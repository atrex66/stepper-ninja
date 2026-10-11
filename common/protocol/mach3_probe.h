#ifndef SN_MACH3_PROBE_H
#define SN_MACH3_PROBE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Bootstrap diagnostic protocol only. All multi-byte fields are little endian.
 * Request: magic[4], version:u16, type:u16, nonce:u32, reserved[20], crc32:u32.
 * Reply: same first 12 bytes, profile:u32, counts[4], capabilities:u32, crc32:u32.
 * CRC32 is IEEE (reflected), excluding the final four bytes.
 * No motion, output writes or watchdog renewal is possible through this API. */
#define SN_MACH3_PROBE_REQUEST_SIZE 36u
#define SN_MACH3_PROBE_REPLY_SIZE 28u
#define SN_MACH3_PROBE_VERSION 1u
#define SN_MACH3_CAP_DIAGNOSTICS 1u
uint32_t sn_read_u32(const uint8_t *p);
void sn_write_u32(uint8_t *p, uint32_t value);
uint32_t sn_crc32(const uint8_t *data, size_t size);
void sn_mach3_probe_request(uint8_t *request, uint32_t nonce);
int sn_mach3_probe_reply(const uint8_t *request, size_t size, uint8_t *reply);
int sn_mach3_probe_validate(const uint8_t *reply, size_t size, uint32_t nonce);
#ifdef __cplusplus
}
#endif
#endif
