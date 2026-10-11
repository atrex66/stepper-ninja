#include "udp_transport.h"
#include "pico.h"
#include "socket.h"
#include "wizchip_conf.h"
#include <string.h>
extern uint8_t first_send;

int32_t __time_critical_func(_sendto)(uint8_t sn, uint8_t *buf, uint16_t len,
                                      uint8_t *addr, uint16_t port) {

  if (first_send) {
    setSn_DIPR(sn, addr);
    setSn_DPORT(sn, port);
    first_send = 0;
  }

  wiz_send_data(sn, buf, len);
  setSn_CR(sn, Sn_CR_SEND);
  while (getSn_CR(sn))
    ;

  while (1) {
    uint8_t tmp = getSn_IR(sn);
    if (tmp & Sn_IR_SENDOK) {
      setSn_IR(sn, Sn_IR_SENDOK);
      break;
    } else if (tmp & Sn_IR_TIMEOUT) {
      setSn_IR(sn, Sn_IR_TIMEOUT);
      return SOCKERR_TIMEOUT;
    }
  }
  return (int32_t)len;
}

int32_t __time_critical_func(_recvfrom)(uint8_t sn, uint8_t *buf, uint16_t len,
                                        uint8_t *addr, uint16_t *port) {
  static uint8_t head[8];
  static uint16_t pack_len = 0;

  while ((pack_len = getSn_RX_RSR(sn)) == 0) {
    if (getSn_SR(sn) == SOCK_CLOSED)
      return SOCKERR_SOCKCLOSED;
  }

  wiz_recv_data(sn, head, 8);
  setSn_CR(sn, Sn_CR_RECV);
  while (getSn_CR(sn))
    ;

  memset(addr, 0, 4);

  addr[0] = head[0];
  addr[1] = head[1];
  addr[2] = head[2];
  addr[3] = head[3];
  *port = (head[4] << 8) | head[5];

  uint16_t data_len = (head[6] << 8) | head[7];

  if (len < data_len)
    pack_len = len;
  else
    pack_len = data_len;

  wiz_recv_data(sn, buf, pack_len);
  setSn_CR(sn, Sn_CR_RECV);
  while (getSn_CR(sn))
    ;

  return (int32_t)pack_len;
}

/* Datagram API for the Mach3 protocol. Unlike the legacy wrapper, this API
 * rebinds the destination on every reply and drains an oversized datagram. */
int32_t sn_udp_receive(uint8_t sn, uint8_t *buffer, uint16_t capacity,
                       sn_udp_peer *peer) {
  uint8_t header[8];
  if (!getSn_RX_RSR(sn))
    return 0;
  wiz_recv_data(sn, header, 8);
  setSn_CR(sn, Sn_CR_RECV);
  while (getSn_CR(sn))
    ;
  memcpy(peer->ip, header, 4);
  peer->port = (uint16_t)((header[4] << 8) | header[5]);
  uint16_t length = (uint16_t)((header[6] << 8) | header[7]);
  uint16_t copied = length > capacity ? capacity : length;
  if (copied)
    wiz_recv_data(sn, buffer, copied);
  setSn_CR(sn, Sn_CR_RECV);
  while (getSn_CR(sn))
    ;
  uint16_t remaining = length - copied;
  while (remaining) {
    uint8_t discard[32];
    uint16_t chunk = remaining > sizeof(discard) ? sizeof(discard) : remaining;
    wiz_recv_data(sn, discard, chunk);
    setSn_CR(sn, Sn_CR_RECV);
    while (getSn_CR(sn))
      ;
    remaining -= chunk;
  }
  return length > capacity ? -1 : length;
}
int32_t sn_udp_send(uint8_t sn, const uint8_t *buffer, uint16_t length,
                    const sn_udp_peer *peer) {
  setSn_DIPR(sn, (uint8_t *)peer->ip);
  setSn_DPORT(sn, peer->port);
  return _sendto(sn, (uint8_t *)buffer, length, (uint8_t *)peer->ip,
                 peer->port);
}
