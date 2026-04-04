/**
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef quadrature_encoder_substep_h
#define quadrature_encoder_substep_h

#include "hardware/pio.h"

typedef struct {
    uint calibration_data[4];
    uint clocks_per_us;
    uint idle_stop_samples;
    PIO pio;
    uint sm;
    uint prev_trans_pos, prev_trans_us;
    uint prev_step_us;
    uint prev_low, prev_high;
    uint idle_stop_sample_count;
    int speed_2_20;
    int stopped;
    int speed;
    uint position;
    uint raw_step;
} substep_state_t;

void substep_init_state(PIO pio, int sm, int pin_a, substep_state_t *state);
void substep_update(substep_state_t *state);
void substep_calibrate_phases(PIO pio, uint sm);
void substep_set_calibration_data(substep_state_t *state, uint phase0, uint phase1, uint phase2);

#endif // quadrature_encoder_substep_h
