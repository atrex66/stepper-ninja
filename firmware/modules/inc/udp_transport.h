#ifndef SN_UDP_TRANSPORT_H
#define SN_UDP_TRANSPORT_H
#include <stdint.h>
typedef struct { uint8_t ip[4]; uint16_t port; } sn_udp_peer;
int32_t _sendto(uint8_t sn, uint8_t *buffer, uint16_t length, uint8_t *ip, uint16_t port);
int32_t _recvfrom(uint8_t sn, uint8_t *buffer, uint16_t length, uint8_t *ip, uint16_t *port);
int32_t sn_udp_receive(uint8_t sn, uint8_t *buffer, uint16_t capacity, sn_udp_peer *peer);
int32_t sn_udp_send(uint8_t sn, const uint8_t *buffer, uint16_t length, const sn_udp_peer *peer);
#endif
