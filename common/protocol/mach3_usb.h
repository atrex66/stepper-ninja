#ifndef SN_MACH3_USB_H
#define SN_MACH3_USB_H
#include "mach3_control.h"
#ifdef __cplusplus
extern "C" {
#endif
/* SLIP boundaries also distinguish a binary claim from console text. */
#define SN_USB_END 0xc0u
#define SN_USB_ESC 0xdbu
#define SN_USB_MAX_FRAME (2u * SN_CONTROL_MAX_PACKET + 2u)
typedef struct {
  uint8_t data[SN_CONTROL_MAX_PACKET];
  size_t size;
  uint8_t active, escaped, invalid;
} sn_usb_decoder;
/* A returned packet remains valid until the next feed call. */
int sn_usb_feed(sn_usb_decoder *d, uint8_t byte, sn_packet *packet);
size_t sn_usb_encode(uint8_t *out, const uint8_t *packet, size_t size);
#ifdef __cplusplus
}
#endif
#endif
