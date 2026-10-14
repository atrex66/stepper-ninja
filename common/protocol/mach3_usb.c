#include "mach3_usb.h"
int sn_usb_feed(sn_usb_decoder *d, uint8_t b, sn_packet *packet) {
  if (b == SN_USB_END) {
    int valid = d->active && !d->escaped && !d->invalid &&
                sn_control_decode(d->data, d->size, packet);
    d->active = 1;
    d->size = 0;
    d->escaped = d->invalid = 0;
    return valid;
  }
  if (!d->active || d->invalid) return 0;
  if (d->escaped) {
    d->escaped = 0;
    if (b == 0xdc) b = SN_USB_END;
    else if (b == 0xdd) b = SN_USB_ESC;
    else { d->invalid = 1; return 0; }
  } else if (b == SN_USB_ESC) {
    d->escaped = 1;
    return 0;
  }
  if (d->size == sizeof(d->data)) { d->invalid = 1; return 0; }
  d->data[d->size++] = b;
  return 0;
}
size_t sn_usb_encode(uint8_t *out, const uint8_t *packet, size_t n) {
  if (!out || !packet || !n || n > SN_CONTROL_MAX_PACKET) return 0;
  size_t at = 0;
  out[at++] = SN_USB_END;
  for (size_t i = 0; i < n; ++i) {
    if (packet[i] == SN_USB_END || packet[i] == SN_USB_ESC) {
      out[at++] = SN_USB_ESC;
      out[at++] = packet[i] == SN_USB_END ? 0xdc : 0xdd;
    } else out[at++] = packet[i];
  }
  out[at++] = SN_USB_END;
  return at;
}
