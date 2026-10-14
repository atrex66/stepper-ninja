#ifndef SN_SERIAL_TRANSPORT_H
#define SN_SERIAL_TRANSPORT_H
#include <stdint.h>
#include <string>
#include "../../common/protocol/mach3_usb.h"
namespace sn {
class SerialTransport {
public:
  SerialTransport() = default;
  SerialTransport(const SerialTransport &) = delete;
  SerialTransport &operator=(const SerialTransport &) = delete;
  ~SerialTransport() { close(); }
  std::string open(const std::string &port);
  void close();
  bool connected() const { return handle_ != -1; }
  bool send(const uint8_t *data, size_t size, unsigned timeout_ms);
  int receive(uint8_t *data, size_t capacity, unsigned timeout_ms);
private:
  intptr_t handle_ = -1;
  sn_usb_decoder decoder_ = {};
  uint8_t pending_[1024];
  size_t pending_at_ = 0, pending_size_ = 0;
};
}
#endif
