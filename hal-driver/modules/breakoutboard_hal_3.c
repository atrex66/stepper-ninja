// ============================================================
// Breakout Board HAL Module - Type 3 (Analog-Ninja)
// Hardware: up to 4x MCP4725 (12-bit bipolar DAC) +
//           MCP23008 (8 digital enable outputs) +
//           4x quadrature encoder counters (Pico PIO)
//
// Firmware counterpart: firmware/modules/breakoutboard_3.c
//   (symlinked as: hal-driver/modules/breakoutboard_3.c)
//
// This module provides the board-specific HAL pin registration and
// analog send/receive logic.
//
// This file is included directly into analog-ninja.c after module_data_t
// and the global rx_buffer / tx_buffer pointers are defined.
// ============================================================

#if breakout_board == 3

// Helper: scale a bipolar float to a 12-bit DAC value (0–4095)
// A value of 0.0 maps to mid-scale (2047); min_limit → 0; max_limit → 4095.
static inline uint16_t bb3_bipolar_to_dac(float input,
                                           float min_limit,
                                           float max_limit,
                                           int8_t _offset)
{
    if (input < min_limit) input = min_limit;
    if (input > max_limit) input = max_limit;

    float max_abs = (max_limit > -min_limit) ? max_limit : -min_limit;
    float scaled  = input / max_abs;

    int32_t dac = (int32_t)(2047.0f + scaled * 2047.0f) + _offset;
    if (dac < 0)    dac = 0;
    if (dac > 4095) dac = 4095;

    return (uint16_t)dac;
}

// ------------------------------------------------------------
// bb_hal_3_setup_pins
// Create ANALOG_CH analog output HAL pins for breakout board 3:
//   - analog.X.enable
//   - analog.X.minimum
//   - analog.X.maximum
//   - analog.X.value
// Returns 0 on success, negative HAL error code on failure.
// ------------------------------------------------------------
static int bb_hal_setup_pins(module_data_t *d, int j, int comp_id,
                             char *name, uint32_t nsize)
{
    int r;

    for (int i = 0; i < ANALOG_CH; i++) {
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.analog.%d.enable", j, i);
        r = hal_pin_bit_newf(HAL_IN, &d->analog_enable[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: analog enable pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        *d->analog_enable[i] = 0;

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.analog.%d.minimum", j, i);
        r = hal_pin_float_newf(HAL_IN, &d->analog_min[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: analog minimum pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        *d->analog_min[i] = 0.0;

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.analog.%d.maximum", j, i);
        r = hal_pin_float_newf(HAL_IN, &d->analog_max[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: analog maximum pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        *d->analog_max[i] = 0.0;

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.analog.%d.offset", j, i);
        r = hal_pin_s32_newf(HAL_IN, &d->analog_offset[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: analog offset pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        *d->analog_offset[i] = 0;

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.analog.%d.value", j, i);
        r = hal_pin_float_newf(HAL_IN, &d->analog_value[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: analog value pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        *d->analog_value[i] = 0.0;
    }

    return 0;
}

// ------------------------------------------------------------
// bb_hal_3_process_recv
// Board type 3 has no digital inputs – nothing to unpack from
// rx_buffer.  Called from udp_io_process_recv().
// ------------------------------------------------------------
static void bb_hal_process_recv(module_data_t *d)
{
    (void)d; // no digital inputs on this board
}

// ------------------------------------------------------------
// bb_hal_3_process_send
// Pack ANALOG_CH analog values (bipolar DAC) and enable bits
// into tx_buffer.  Called from udp_io_process_send().
//
// tx_buffer->analog_out[i]  : 12-bit DAC word for channel i
// tx_buffer->outputs[0]     : bitfield – bit N = analog_enable[N]
// ------------------------------------------------------------
static void bb_hal_process_send(module_data_t *d)
{
    tx_buffer->outputs[0] = 0;

    for (int i = 0; i < ANALOG_CH; i++) {
        tx_buffer->analog_out[i] = bb3_bipolar_to_dac(
            *d->analog_value[i],
            *d->analog_min[i],
            *d->analog_max[i],
            (int8_t)*d->analog_offset[i]);

        if (*d->analog_enable[i]) {
            tx_buffer->outputs[0] |= (1u << i);
        }
    }
}

#endif // breakout_board == 3
