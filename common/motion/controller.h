#ifndef SN_CONTROLLER_H
#define SN_CONTROLLER_H
#include "../protocol/mach3_control.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  uint32_t max_velocity, acceleration, home_fast, home_slow, backoff,
      home_travel;
  int64_t home_offset;
  int8_t home_direction;
  uint8_t home_input, limit_input;
} sn_axis_config;
typedef struct {
  uint32_t watchdog_ms, pwm_hz, pulse_ns, dir_setup_ns;
  uint8_t input_invert, output_invert, dir_invert;
  sn_axis_config axis[3];
} sn_machine_config;
typedef struct {
  int32_t steps[3];
  uint32_t id, line;
} sn_segment;
typedef struct {
  sn_machine_config config;
  uint64_t peer;
  uint32_t session, sequence, fingerprint, last_seen_ms, now_ms, result;
  uint32_t accepted_frame, executed_frame, executed_line, epoch;
  uint16_t head, tail, fill;
  uint8_t configured, armed, mode, fault, inputs, raw_inputs, outputs, homed,
      end;
  uint16_t pwm;
  int64_t position[3], home_origin;
  double velocity[3], target[3], fraction[3];
  uint8_t home_axis, home_phase, home_hit, move_axis;
  uint32_t dwell_ticks;
  int64_t move_target;
  uint8_t probe_state, probe_input, probe_major, probe_cancel;
  uint32_t probe_id;
  int64_t probe_position[3], probe_start[3], probe_delta[3],
      probe_brake_start[3];
  double probe_length, probe_progress, probe_speed, probe_max_speed,
      probe_acceleration, probe_brake_progress;
  uint8_t pending;
  sn_segment current, queue[SN_QUEUE_CAPACITY];
} sn_controller;
void sn_config_defaults(sn_machine_config *config);
void sn_config_encode(uint8_t out[128], const sn_machine_config *config);
int sn_config_decode(const uint8_t *payload, size_t size,
                     sn_machine_config *config);
void sn_controller_init(sn_controller *controller);
void sn_controller_fault(sn_controller *controller, uint8_t fault);
uint32_t sn_controller_command(sn_controller *controller,
                               const sn_packet *packet, uint64_t peer);
void sn_controller_complete(sn_controller *controller, const int32_t actual[3],
                            int complete);
void sn_controller_tick(sn_controller *controller, uint8_t raw_inputs,
                        int32_t steps[3]);
/* Hardware calls after stopping STEP PIOs and sampling their residual counts.
 */
void sn_controller_probe_hit(sn_controller *controller,
                             const int32_t residual[3]);
void sn_controller_status(const sn_controller *controller,
                          uint8_t status[SN_CONTROL_STATUS_SIZE],
                          uint32_t result);
#ifdef __cplusplus
}
#endif
#endif
