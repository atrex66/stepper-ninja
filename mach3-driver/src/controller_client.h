#ifndef SN_CONTROLLER_CLIENT_H
#define SN_CONTROLLER_CLIENT_H
#include "../../common/motion/controller.h"
#include "settings.h"
#include "serial_transport.h"
#include <string>
namespace sn {
struct Report {
  uint32_t result = 0, acknowledged = 0, accepted = 0, executed = 0, line = 0,
           epoch = 0, probe_id = 0;
  unsigned fill = 0, free = 0;
  uint8_t mode = 0, fault = 0, armed = 0, configured = 0, inputs = 0,
          outputs = 0, homed = 0, home_phase = 0, probe_state = 0;
  bool pending = false, end = false;
  int64_t position[3] = {0, 0, 0}, probe_position[3] = {0, 0, 0};
};
class ControllerClient {
public:
  ControllerClient();
  ~ControllerClient();
  ControllerClient(const ControllerClient &) = delete;
  ControllerClient &operator=(const ControllerClient &) = delete;
  std::string open(const Settings &settings);
  void close();
  bool connected() const;
  std::string command(uint16_t type, const uint8_t *payload = nullptr,
                      uint16_t size = 0);
  const Report &report() const { return report_; }

private:
  SerialTransport serial_;
  intptr_t socket_ = -1;
  bool winsock_ = false;
  uint32_t session_ = 0, sequence_ = 0;
  unsigned timeout_ms_ = 100;
  Report report_;
};
} // namespace sn
#endif
