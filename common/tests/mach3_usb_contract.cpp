#include "protocol/mach3_usb.h"
#include <cassert>
#include <cstring>
#include <vector>
int main() {
  uint8_t payload[256], raw[SN_CONTROL_MAX_PACKET], frame[SN_USB_MAX_FRAME];
  for (unsigned i = 0; i < sizeof(payload); ++i) payload[i] = uint8_t(i);
  size_t n = sn_control_encode(raw, SN_BATCH, 0xdb00c001, 42, payload, sizeof(payload));
  size_t length = sn_usb_encode(frame, raw, n);
  sn_usb_decoder decoder = {};
  sn_packet packet;
  unsigned accepted = 0;
  for (unsigned repeat = 0; repeat < 2; ++repeat)
    for (size_t i = 0; i < length; ++i)
      if (sn_usb_feed(&decoder, frame[i], &packet)) {
        ++accepted;
        assert(packet.session == 0xdb00c001 && packet.sequence == 42);
        assert(packet.size == sizeof(payload));
        assert(!std::memcmp(packet.payload, payload, sizeof(payload)));
      }
  assert(accepted == 2);
  // Corruption, illegal escapes, truncated escapes and overlong frames must
  // all recover at a boundary without accepting any malformed command.
  frame[8] ^= 1;
  for (size_t i = 0; i < length; ++i) assert(!sn_usb_feed(&decoder, frame[i], &packet));
  const uint8_t bad[] = {SN_USB_END, SN_USB_ESC, 1, SN_USB_END,
                        SN_USB_ESC, SN_USB_END};
  for (uint8_t b : bad) assert(!sn_usb_feed(&decoder, b, &packet));
  for (unsigned i = 0; i < SN_CONTROL_MAX_PACKET + 30; ++i)
    assert(!sn_usb_feed(&decoder, 1, &packet));
  assert(!sn_usb_feed(&decoder, SN_USB_END, &packet));
  length = sn_usb_encode(frame, raw, n);
  for (size_t i = 0; i < length; ++i)
    if (sn_usb_feed(&decoder, frame[i], &packet)) ++accepted;
  assert(accepted == 3);
}
