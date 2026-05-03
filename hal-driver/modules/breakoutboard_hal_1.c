#include "config.h"

// ============================================================
// Breakout Board HAL Module - Type 1
// Hardware: MCP23008 (8 digital outputs) + MCP23017 (16 digital inputs)
//           + optional MCP4725 (2x 12-bit analog DAC outputs)
//
// Firmware counterpart: firmware/modules/breakoutboard_1.c
//   (symlinked as: hal-driver/modules/breakoutboard_1.c)
//
// This file is included directly into io-ninja.c after module_data_t
// and the global rx_buffer / tx_buffer pointers are defined.
// ============================================================

#if breakout_board == 1

enum {
    BB1_IN_PINS_NO = 16,
    BB1_OUT_PINS_NO = 8,
};

// ------------------------------------------------------------
// bb_hal_1_setup_pins
// Create all HAL pins for breakout board type 1:
//   - 8 digital output bits  (io-ninja.N.outputs.X)
//   - 16 digital input bits  (io-ninja.N.inputs.X / inputs.X-not)
//   - 2 analog output channels (io-ninja.N.analog.X.{enable,minimum,maximum,value})
// Returns 0 on success, negative HAL error code on failure.
// ------------------------------------------------------------
static int bb_hal_setup_pins(module_data_t *d, int j, int comp_id,
                             char *name, uint32_t nsize)
{
    int r;

    // --- Digital outputs (8 bits via MCP23008) ---
    for (int i = 0; i < BB1_OUT_PINS_NO; i++) {
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.outputs.%d", j, i);
        r = hal_pin_bit_newf(HAL_IN, &d->output[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: output pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        *d->output[i] = 0;
    }

    // --- Digital inputs (16 bits via MCP23017) ---
    for (int i = 0; i < BB1_IN_PINS_NO; i++) {
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.inputs.%d", j, i);
        r = hal_pin_bit_newf(HAL_OUT, &d->input[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: input pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.inputs.%d-not", j, i);
        r = hal_pin_bit_newf(HAL_OUT, &d->input_not[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: input-not pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
    }

    // --- Analog outputs (2 channels via MCP4725 DAC) ---
    for (int i = 0; i < 2; i++) {
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
                module_name ".%d: ERROR: analog min pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        *d->analog_min[i] = 0.0;

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.analog.%d.maximum", j, i);
        r = hal_pin_float_newf(HAL_IN, &d->analog_max[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: analog max pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        *d->analog_max[i] = 0.0;

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
// bb_hal_1_process_recv
// Map received UDP packet inputs (rx_buffer->inputs[2/3]) onto
// the HAL input pins.  Called from udp_io_process_recv().
// ------------------------------------------------------------
static void bb_hal_process_recv(module_data_t *d)
{
    for (uint8_t i = 0; i < BB1_IN_PINS_NO; i++) {
        if (i < 32) {
            *d->input[i] = (rx_buffer->inputs[2] >> i) & 1;
        } else {
            *d->input[i] = (rx_buffer->inputs[3] >> (i - 32)) & 1;
        }
        *d->input_not[i] = !(*d->input[i]);
    }
}

// ------------------------------------------------------------
// bb_hal_1_process_send
// Pack HAL output pins + analog values into tx_buffer fields.
// Called from udp_io_process_send().
// ------------------------------------------------------------
static void bb_hal_process_send(module_data_t *d)
{
    // --- Digital outputs → tx_buffer->outputs[0] (lower 8 bits) ---
    uint8_t outs = 0;
    for (uint8_t i = 0; i < BB1_OUT_PINS_NO; i++) {
        outs |= (*d->output[i] == 1) ? (1 << i) : 0;
    }
    tx_buffer->outputs[0] = outs;

    // --- Analog outputs → tx_buffer->analog_out (2 x 12-bit packed) ---
    uint32_t at0 = 0;
    for (int i = 0; i < ANALOG_CH; i++) {
        float val = *d->analog_value[i];

        if (val < *d->analog_min[i]) val = *d->analog_min[i];
        if (val > *d->analog_max[i]) val = *d->analog_max[i];

        // Scale float to 0–4095 DAC range
        uint16_t dac = (uint16_t)((4095.0f / *d->analog_max[i]) * val);
        at0 |= dac << (16 * i);
    }
    tx_buffer->analog_out[0] = at0;
}

#endif // breakout_board == 1
