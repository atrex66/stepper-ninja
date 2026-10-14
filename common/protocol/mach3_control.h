#ifndef SN_MACH3_CONTROL_H
#define SN_MACH3_CONTROL_H
#include "../profiles/mach3_3axis.h"
#include "mach3_probe.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SN_CONTROL_VERSION 3
#define SN_CONTROL_HEADER 24u
#define SN_CONTROL_MAX_PACKET 512u
#define SN_CONTROL_STATUS_SIZE 112u
/* Optional status extension; older firmware leaves these bytes zero. */
#define SN_STATUS_CAP_RAW_INPUTS 0x01u
#define SN_STATUS_CAP_STEP_RATE 0x02u
#define SN_QUEUE_CAPACITY 256u
#define SN_MAX_STEPS_TICK 200
#define SN_MAX_STEP_RATE (SN_MAX_STEPS_TICK * 1000u)
#define SN_TICK_US 1000u
/* All payloads and headers are little endian; no native packed structs. */
enum sn_command {
  SN_CLAIM = 1,
  SN_CONFIG,
  SN_ARM,
  SN_DISARM,
  SN_POLL,
  SN_IO,
  SN_JOG,
  SN_JOG_STOP,
  SN_HOME,
  SN_ABORT,
  SN_BATCH,
  SN_START,
  SN_END,
  SN_RELEASE,
  SN_SET_POSITION,
  SN_DWELL,
  SN_HOLD,
  SN_MOVE,
  SN_PROBE,
  SN_PROBE_CANCEL
};
enum sn_result {
  SN_OK,
  SN_BAD_PACKET,
  SN_OWNER,
  SN_SEQUENCE,
  SN_STATE,
  SN_RANGE,
  SN_FULL,
  SN_UNSUPPORTED
};
enum sn_mode {
  SN_IDLE,
  SN_JOGGING,
  SN_HOMING,
  SN_STREAM,
  SN_STOPPING,
  SN_DWELLING,
  SN_POSITIONING,
  SN_PROBING,
  SN_PROBE_BRAKING
};
enum sn_fault {
  SN_FAULT_NONE,
  SN_FAULT_ESTOP,
  SN_FAULT_WATCHDOG,
  SN_FAULT_LIMIT,
  SN_FAULT_HOME_TRAVEL,
  SN_FAULT_UNDERFLOW,
  SN_FAULT_TIMING,
  SN_FAULT_PROBE_MISS
};
enum sn_probe_state {
  SN_PROBE_NONE,
  SN_PROBE_ACTIVE,
  SN_PROBE_HIT,
  SN_PROBE_MISS,
  SN_PROBE_CANCELLED
};
typedef struct {
  uint16_t command;
  uint32_t session, sequence, fingerprint;
  const uint8_t *payload;
  uint16_t size;
} sn_packet;
size_t sn_control_encode(uint8_t *out, uint16_t command, uint32_t session,
                         uint32_t sequence, const uint8_t *payload,
                         uint16_t size);
int sn_control_decode(const uint8_t *data, size_t size, sn_packet *packet);
int64_t sn_read_i64(const uint8_t *p);
void sn_write_i64(uint8_t *p, int64_t value);
#ifdef __cplusplus
}
#endif
#endif
