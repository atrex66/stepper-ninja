#include "mach3_control.h"
#include <string.h>
size_t sn_control_encode(uint8_t *out, uint16_t command, uint32_t session,
                         uint32_t sequence, const uint8_t *payload,
                         uint16_t size) {
  size_t length = SN_CONTROL_HEADER + size + 4u;
  if (!out || length > SN_CONTROL_MAX_PACKET || (size && !payload))
    return 0;
  memset(out, 0, SN_CONTROL_HEADER);
  memcpy(out, "SNC3", 4);
  out[4] = SN_CONTROL_VERSION;
  out[6] = (uint8_t)command;
  out[7] = (uint8_t)(command >> 8);
  sn_write_u32(out + 8, session);
  sn_write_u32(out + 12, sequence);
  out[16] = (uint8_t)size;
  out[17] = (uint8_t)(size >> 8);
  sn_write_u32(out + 20, SN_MACH3_PROFILE_ID);
  if (size)
    memcpy(out + SN_CONTROL_HEADER, payload, size);
  sn_write_u32(out + length - 4, sn_crc32(out, length - 4));
  return length;
}
int sn_control_decode(const uint8_t *p, size_t n, sn_packet *packet) {
  if (!p || !packet || n < SN_CONTROL_HEADER + 4 || n > SN_CONTROL_MAX_PACKET)
    return 0;
  uint16_t size = (uint16_t)(p[16] | p[17] << 8);
  if (memcmp(p, "SNC3", 4) || p[4] != SN_CONTROL_VERSION || p[5] || p[18] ||
      p[19] || size + SN_CONTROL_HEADER + 4u != n ||
      sn_read_u32(p + 20) != SN_MACH3_PROFILE_ID ||
      sn_read_u32(p + n - 4) != sn_crc32(p, n - 4))
    return 0;
  packet->command = (uint16_t)(p[6] | p[7] << 8);
  packet->session = sn_read_u32(p + 8);
  packet->sequence = sn_read_u32(p + 12);
  packet->fingerprint = sn_read_u32(p + n - 4);
  packet->payload = p + SN_CONTROL_HEADER;
  packet->size = size;
  return 1;
}
int64_t sn_read_i64(const uint8_t *p) {
  uint64_t value = (uint64_t)sn_read_u32(p) | (uint64_t)sn_read_u32(p + 4)
                                                  << 32;
  return value <= INT64_MAX ? (int64_t)value
                            : -1 - (int64_t)(UINT64_MAX - value);
}
void sn_write_i64(uint8_t *p, int64_t value) {
  uint64_t v = (uint64_t)value;
  sn_write_u32(p, (uint32_t)v);
  sn_write_u32(p + 4, (uint32_t)(v >> 32));
}
