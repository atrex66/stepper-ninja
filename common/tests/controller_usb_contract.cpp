#include "controller_client.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <thread>
#include <unistd.h>
#include <cstdlib>
int main() {
  int master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
  assert(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
  sn::Settings settings;
  settings.usb = true;
  settings.serial_port = ptsname(master);
  settings.ip = "unused for USB";
  settings.port = 0;
  settings.timeout_ms = 200;
  assert(sn::validate_settings(settings).empty());
  std::atomic<bool> running(true), drop(true);
  std::atomic<unsigned> duplicates(0), releases(0);
  std::thread firmware([&] {
    sn_controller controller;
    sn_controller_init(&controller);
    sn_usb_decoder decoder = {};
    auto next_tick = std::chrono::steady_clock::now();
    while (running) {
      while (next_tick <= std::chrono::steady_clock::now()) {
        int32_t steps[3];
        sn_controller_tick(&controller, 127, steps);
        next_tick += std::chrono::milliseconds(1);
      }
      pollfd p = {master, POLLIN, 0};
      if (poll(&p, 1, 1) <= 0) continue;
      uint8_t data[1024];
      ssize_t size = read(master, data, sizeof(data));
      for (ssize_t i = 0; i < size; ++i) {
        sn_packet packet;
        if (!sn_usb_feed(&decoder, data[i], &packet)) continue;
        if (controller.session && controller.sequence == packet.sequence) ++duplicates;
        uint32_t result = sn_controller_command(&controller, &packet, UINT64_C(1) << 63);
        if (packet.command == SN_RELEASE && result == SN_OK) ++releases;
        if (drop.exchange(false)) continue; // Lost CLAIM response: retry must be idempotent.
        uint8_t status[SN_CONTROL_STATUS_SIZE], raw[SN_CONTROL_MAX_PACKET], frame[SN_USB_MAX_FRAME];
        sn_controller_status(&controller, status, result);
        size_t n = sn_control_encode(raw, packet.command | 0x8000u, packet.session,
                                    packet.sequence, status, sizeof(status));
        n = sn_usb_encode(frame, raw, n);
        // Prefix noise and fragmentation exercise stream resynchronization.
        const char noise[] = "old boot message\n";
        assert(write(master, noise, sizeof(noise) - 1) == sizeof(noise) - 1);
        for (size_t at = 0; at < n;) {
          size_t chunk = n - at > 7 ? 7 : n - at;
          ssize_t sent = write(master, frame + at, chunk);
          if (sent > 0) at += static_cast<size_t>(sent);
          else std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    }
  });
  sn::ControllerClient client;
  assert(client.open(settings).empty());
  assert(duplicates >= 1);
  assert(client.command(SN_POLL).empty());
  sn_machine_config config;
  sn_config_defaults(&config);
  uint8_t payload[128];
  sn_config_encode(payload, &config);
  assert(client.command(SN_CONFIG, payload, sizeof(payload)).empty());
  assert(client.command(SN_ARM).empty() && client.report().armed);
  assert(client.command(SN_DISARM).empty() && !client.report().armed);
  assert(client.command(SN_RELEASE).empty());
  client.close();
  assert(releases == 1);
  assert(client.open(settings).empty());
  assert(client.command(SN_RELEASE).empty());
  client.close();
  assert(releases == 2);
  running = false;
  firmware.join();
  // A silent endpoint must time out and close the client, not hang.
  settings.timeout_ms = 100;
  assert(!client.open(settings).empty());
  assert(!client.connected());
  close(master);
}
