#include "controller_client.h"
#include <iostream>

// Claims and polls an idle controller, then releases it. Never configures or arms it.
int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sn-mach3-usb-check COM12\n";
    return 2;
  }
  sn::Settings settings;
  settings.usb = true;
  settings.serial_port = argv[1];
  settings.timeout_ms = 1000;
  sn::ControllerClient client;
  std::string error = client.open(settings);
  if (!error.empty()) {
    std::cerr << error << '\n';
    return 1;
  }
  error = client.command(SN_POLL);
  if (error.empty()) {
    const sn::Report &r = client.report();
    std::cout << "USB protocol " << SN_CONTROL_VERSION << ", port " << argv[1]
              << ": mode=" << unsigned(r.mode) << ", armed=" << unsigned(r.armed)
              << ", fault=" << unsigned(r.fault) << ", inputs=" << unsigned(r.inputs)
              << ", max_step_rate=" << r.max_step_rate << ", raw_inputs=" << unsigned(r.raw_inputs)
              << ", queue=" << r.fill << ", free=" << r.free << '\n';
    if (r.armed || r.mode != SN_IDLE || r.fill || r.pending)
      error = "Controller was not idle and disarmed.";
  }
  if (client.connected()) {
    const std::string release_error = client.command(SN_RELEASE);
    if (error.empty()) error = release_error;
  }
  client.close();
  if (!error.empty()) {
    std::cerr << error << '\n';
    return 1;
  }
  std::cout << "PASS: CLAIM / POLL / RELEASE; no motion or ARM commands sent.\n";
  return 0;
}