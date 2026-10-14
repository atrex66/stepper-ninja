#ifndef SN_STEP_TIMING_H
#define SN_STEP_TIMING_H
#include <stdint.h>
/* mach3_step.pio runs at 10 MHz. Exact interpreter-tested cycle accounting:
 * high = H, period = high_counter + low_counter + 7; burst = 6 + N*period.
 * The five low-side control cycles already belong to STEP low time. */
#define SN_STEP_CLOCK_HZ 10000000u
#define SN_STEP_BURST_END_US 900u
#define SN_STEP_STAGE_US 30u
#define SN_STEP_COMPUTE_US 25u
#define SN_STEP_START_CYCLES 6u
static inline uint32_t sn_step_high_cycles(uint32_t pulse_ns) {
  return (pulse_ns + 99u) / 100u;
}
static inline int sn_step_plan(uint32_t pulse_ns, uint32_t count,
                               uint32_t budget_cycles,
                               uint32_t *high, uint32_t *low) {
  if (!count || budget_cycles <= SN_STEP_START_CYCLES) return 0;
  uint32_t h = sn_step_high_cycles(pulse_ns);
  uint32_t period = (budget_cycles - SN_STEP_START_CYCLES) / count;
  if (h < 5u || period < 2u * h) return 0;
  *high = h - 2u;
  *low = period - *high - 7u;
  return 1;
}
static inline int sn_step_rate_fits(uint32_t rate, uint32_t pulse_ns,
                                   uint32_t dir_setup_ns) {
  uint32_t setup_us = (dir_setup_ns + 999u) / 1000u;
  if (setup_us + SN_STEP_STAGE_US + SN_STEP_COMPUTE_US >= SN_STEP_BURST_END_US)
    return 0;
  uint32_t cycles = (SN_STEP_BURST_END_US - setup_us -
                    SN_STEP_STAGE_US - SN_STEP_COMPUTE_US) * 10u;
  uint32_t high, low;
  return sn_step_plan(pulse_ns, (rate + 999u) / 1000u, cycles, &high, &low);
}
#endif
