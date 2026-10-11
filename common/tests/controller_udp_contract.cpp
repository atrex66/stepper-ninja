#include "controller_client.h"
#include "streamer.h"
#include "waypoints.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>
// Model Mach3's native planner: running records, a Feed Hold deceleration Stop
// boundary, then fresh acceleration records exposed only after Cycle Start.
struct NativePlanner : sn::TrajectorySource {
  std::vector<sn::Waypoint> points;
  unsigned read = 0, visible = 40;
  NativePlanner() {
    for (unsigned i = 0; i < 162; ++i) {
      sn::Waypoint p;
      if (i < 40)
        p.delta[0] = 4;
      else if (i < 80)
        p.delta[0] = 4 - ((i - 40) / 10);
      else if (i < 82)
        p.delta[0] = 0;
      else {
        unsigned k = i - 82;
        p.delta[0] = k < 30 ? -(double)(k / 10 + 1) : -4;
      }
      p.delta[1] = -p.delta[0];
      p.line = i < 82 ? 100 : 200;
      p.stop = i == 81 || i == 161;
      points.push_back(p);
    }
  }
  unsigned available() const override { return visible - read; }
  sn::Waypoint peek(unsigned offset) const override {
    return points.at(read + offset);
  }
  void consume(unsigned count) override {
    assert(count <= available());
    read += count;
  }
  bool ending() const override { return false; } // Stop markers, not EOF.
};
int main() {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  assert(sock >= 0);
  sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  assert(bind(sock, reinterpret_cast<sockaddr *>(&address), sizeof(address)) ==
         0);
  socklen_t size = sizeof(address);
  assert(getsockname(sock, reinterpret_cast<sockaddr *>(&address), &size) == 0);
  std::atomic<bool> stop(false);
  std::atomic<unsigned> duplicate_batches(0), destructive_stops(0);
  std::atomic<bool> probe_input(false);
  std::thread firmware([&]() {
    sn_controller c;
    sn_controller_init(&c);
    bool lost_ack = false;
    uint32_t last_batch = 0;
    auto deadline = std::chrono::steady_clock::now();
    while (!stop.load()) {
      auto now = std::chrono::steady_clock::now();
      while (deadline <= now) {
        int32_t actual[3] = {};
        if (c.pending)
          std::memcpy(actual, c.current.steps, sizeof(actual));
        sn_controller_complete(&c, actual, 1);
        int32_t next[3];
        uint8_t raw = (probe_input && c.position[2] <= -40) ? 111 : 127;
        sn_controller_tick(&c, raw, next);
        deadline += std::chrono::milliseconds(1);
      }
      fd_set ready;
      FD_ZERO(&ready);
      FD_SET(sock, &ready);
      timeval wait = {0, 1000};
      if (select(sock + 1, &ready, nullptr, nullptr, &wait) <= 0)
        continue;
      uint8_t rx[512], tx[512], status[SN_CONTROL_STATUS_SIZE];
      sockaddr_in peer = {};
      socklen_t peer_size = sizeof(peer);
      int length = recvfrom(sock, rx, sizeof(rx), 0,
                            reinterpret_cast<sockaddr *>(&peer), &peer_size);
      sn_packet request;
      if (length <= 0 || !sn_control_decode(rx, length, &request))
        continue;
      uint64_t identity =
          uint64_t(peer.sin_addr.s_addr) | (uint64_t(peer.sin_port) << 32);
      uint32_t result = sn_controller_command(&c, &request, identity);
      if (request.command == SN_ABORT || request.command == SN_HOLD)
        ++destructive_stops;
      if (request.command == SN_BATCH) {
        if (request.sequence == last_batch)
          ++duplicate_batches;
        last_batch = request.sequence;
        if (!lost_ack) {
          lost_ack = true;
          continue;
        }
      }
      sn_controller_status(&c, status, result);
      auto count =
          sn_control_encode(tx, request.command | 0x8000, request.session,
                            request.sequence, status, sizeof(status));
      // Client must ignore a stale sequence response before the real report.
      if (request.command == SN_POLL) {
        uint8_t stale[512];
        auto bad =
            sn_control_encode(stale, request.command | 0x8000, request.session,
                              request.sequence - 1, status, sizeof(status));
        sendto(sock, stale, bad, 0, reinterpret_cast<sockaddr *>(&peer),
               peer_size);
      }
      assert(sendto(sock, tx, count, 0, reinterpret_cast<sockaddr *>(&peer),
                    peer_size) == static_cast<int>(count));
    }
  });
  sn::Settings settings;
  settings.ip = "127.0.0.1";
  settings.port = ntohs(address.sin_port);
  settings.timeout_ms = 300;
  sn::ControllerClient client;
  assert(client.open(settings).empty());
  sn_machine_config cfg;
  sn_config_defaults(&cfg);
  uint8_t config[128];
  sn_config_encode(config, &cfg);
  assert(client.command(SN_CONFIG, config, sizeof(config)).empty());
  assert(client.command(SN_ARM).empty());
  uint8_t io[4] = {3, 255, 255, 0};
  assert(client.command(SN_IO, io, 4).empty());
  uint8_t batch[24] = {};
  sn_write_u32(batch, 1);
  sn_write_u32(batch + 4, 1);
  sn_write_u32(batch + 8, 5);
  sn_write_u32(batch + 20, 456);
  assert(client.command(SN_BATCH, batch, sizeof(batch)).empty());
  assert(duplicate_batches == 1);
  assert(client.report().accepted == 1 && client.report().executed == 0 &&
         client.report().position[0] == 0);
  assert(client.command(SN_END).empty());
  assert(client.command(SN_START).empty());
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  assert(client.command(SN_POLL).empty());
  assert(client.report().executed == 1 && client.report().line == 456 &&
         client.report().position[0] == 5);
  assert(client.report().mode == SN_IDLE && !client.report().pending);
  uint8_t move[16] = {0, 0, 0, 0};
  sn_write_i64(move + 4, -20);
  sn_write_u32(move + 12, 1000);
  assert(client.command(SN_MOVE, move, sizeof(move)).empty());
  for (unsigned i = 0; i < 50 && (i == 0 || client.report().mode != SN_IDLE);
       ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(client.command(SN_POLL).empty());
  }
  assert(client.report().mode == SN_IDLE && client.report().position[0] == -15);
  NativePlanner planner;
  sn::Streamer streamer;
  assert(streamer.update(client, planner).empty());
  assert(planner.read == 40 && client.report().mode == SN_STREAM);
  // Feed Hold arrives while running; keep transmitting the planner's braking
  // data. No firmware HOLD, purge or duplicate of already executed records.
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  planner.visible = 82;
  for (unsigned i = 0; i < 100; ++i) {
    assert(client.command(SN_POLL).empty());
    assert(streamer.update(client, planner).empty());
    if (planner.read == 82 && client.report().mode == SN_IDLE)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  assert(planner.read == 82 && client.report().position[0] == 245 &&
         client.report().position[1] == -260);
  assert(client.report().executed == 83 && client.report().line == 100);
  for (unsigned i = 0; i < 20; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(client.command(SN_POLL).empty());
    assert(streamer.update(client, planner).empty());
    assert(client.report().mode == SN_IDLE &&
           client.report().position[0] == 245);
  }
  assert(client.report().armed && client.report().outputs == 3);
  // Cycle Start: Mach3 exposes fresh startup records from the exact stop point.
  planner.visible = 162;
  for (unsigned i = 0; i < 100; ++i) {
    assert(client.command(SN_POLL).empty());
    assert(streamer.update(client, planner).empty());
    if (planner.read == 162 && client.report().mode == SN_IDLE)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  assert(client.report().position[0] == -15 &&
         client.report().position[1] == 0);
  assert(client.report().executed == 163 && client.report().accepted == 163 &&
         client.report().line == 200);
  assert(destructive_stops == 0);
  // The actual UDP client carries a distinct, persistent probe latch report.
  uint8_t probe[32] = {};
  sn_write_i64(probe, -15);
  sn_write_i64(probe + 16, -500);
  sn_write_u32(probe + 24, 1000);
  probe[28] = 4;
  probe_input = true;
  assert(client.command(SN_PROBE, probe, sizeof(probe)).empty());
  uint32_t probe_token = client.report().probe_id;
  for (unsigned i = 0; i < 100; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(client.command(SN_POLL).empty());
    if (client.report().mode == SN_IDLE)
      break;
  }
  assert(client.report().probe_id == probe_token &&
         client.report().probe_state == SN_PROBE_HIT);
  assert(client.report().probe_position[2] <= -40 &&
         client.report().position[2] < client.report().probe_position[2]);
  probe_input = false;
  assert(client.command(SN_DISARM).empty());
  assert(!client.report().armed && !client.report().outputs);
  assert(client.command(SN_RELEASE).empty());
  client.close();
  stop = true;
  firmware.join();
  close(sock);
  sn::WaypointAccumulator acc;
  int32_t steps[3], total = 0;
  double delta[3] = {0.25, -0.25, 0};
  for (unsigned i = 0; i < 400; ++i) {
    assert(acc.convert(delta, steps));
    total += steps[0];
    assert(steps[0] == -steps[1]);
  }
  assert(total == 100);
  double bad[3] = {0, 100, 0};
  assert(!acc.convert(bad, steps));
  std::cout << "PASS: real UDP client, lost ACK retry, stale report rejection, "
               "measured reports, native Feed Hold/Cycle Start, G31 and "
               "bounded move\n";
}
