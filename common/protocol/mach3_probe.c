#include "mach3_probe.h"
#include "../profiles/mach3_3axis.h"
#include <string.h>
uint32_t sn_read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
void sn_write_u32(uint8_t *p, uint32_t v) {
    unsigned i; for (i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8));
}
uint32_t sn_crc32(const uint8_t *p, size_t n) {
    uint32_t crc = 0xffffffffu;
    size_t i; unsigned bit;
    for (i = 0; i < n; ++i) {
        crc ^= p[i];
        for (bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
static void finish(uint8_t *p, size_t n) { sn_write_u32(p + n - 4, sn_crc32(p, n - 4)); }
static int valid(const uint8_t *p, size_t n, size_t expected, uint8_t type) {
    return p && n == expected && memcmp(p, "SNM3", 4) == 0 &&
        p[4] == SN_MACH3_PROBE_VERSION && p[5] == 0 && p[6] == type && p[7] == 0 &&
        sn_read_u32(p + n - 4) == sn_crc32(p, n - 4);
}
void sn_mach3_probe_request(uint8_t *p, uint32_t nonce) {
    memset(p, 0, SN_MACH3_PROBE_REQUEST_SIZE); memcpy(p, "SNM3", 4);
    p[4] = SN_MACH3_PROBE_VERSION; p[6] = 1;
    sn_write_u32(p + 8, nonce); finish(p, SN_MACH3_PROBE_REQUEST_SIZE);
}
int sn_mach3_probe_reply(const uint8_t *p, size_t n, uint8_t *reply) {
    size_t i;
    if (!reply || !valid(p, n, SN_MACH3_PROBE_REQUEST_SIZE, 1)) return 0;
    for (i = 12; i < n - 4; ++i) if (p[i]) return 0;
    memset(reply, 0, SN_MACH3_PROBE_REPLY_SIZE); memcpy(reply, p, 12); reply[6] = 2;
    sn_write_u32(reply + 12, SN_MACH3_PROFILE_ID);
    reply[16] = SN_MACH3_AXES; reply[17] = SN_MACH3_INPUTS;
    reply[18] = SN_MACH3_OUTPUTS; reply[19] = SN_MACH3_PWM_CHANNELS;
    sn_write_u32(reply + 20, SN_MACH3_CAP_DIAGNOSTICS);
    finish(reply, SN_MACH3_PROBE_REPLY_SIZE); return 1;
}
int sn_mach3_probe_validate(const uint8_t *p, size_t n, uint32_t nonce) {
    return valid(p, n, SN_MACH3_PROBE_REPLY_SIZE, 2) && sn_read_u32(p + 8) == nonce &&
        sn_read_u32(p + 12) == SN_MACH3_PROFILE_ID &&
        p[16] == SN_MACH3_AXES && p[17] == SN_MACH3_INPUTS &&
        p[18] == SN_MACH3_OUTPUTS && p[19] == SN_MACH3_PWM_CHANNELS &&
        sn_read_u32(p + 20) == SN_MACH3_CAP_DIAGNOSTICS;
}
