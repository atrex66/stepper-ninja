#include "config.h"
// ============================================================
// Breakout Board HAL Module - Type 0 (Custom GPIO)
//
// This is the fallback board: no expander IC, raw Pico GPIO
// pins which are listed in input_pins[] / output_pins[] arrays
// defined in analog-ninja.c.
//
// This file is included from analog-ninja.c in the #else branch
// of the board-select chain, so it has no #if guard of its own.
// ============================================================

#if breakout_board == 0

static int bb_hal_setup_pins(module_data_t *d, int j, int comp_id,
                             char *name, uint32_t nsize)
{
    int r;

    for (int i = 0; i < in_pins_no; i++) {
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.input.gp%d", j, input_pins[i]);
        r = hal_pin_bit_newf(HAL_OUT, &d->input[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            return r;
        }

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.input.gp%d-not", j, input_pins[i]);
        r = hal_pin_bit_newf(HAL_OUT, &d->input_not[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            return r;
        }
    }

    for (int i = 0; i < out_pins_no; i++) {
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.output.gp%d", j, output_pins[i]);
        r = hal_pin_bit_newf(HAL_IN, &d->output[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            return r;
        }
        *d->output[i] = 0;
    }

    return 0;
}

static void bb_hal_process_recv(module_data_t *d)
{
    for (uint8_t i = 0; i < in_pins_no; i++) {
        if (input_pins[i] < 32) {
            *d->input[i] = (rx_buffer->inputs[0] >> (input_pins[i] & 31)) & 1;
        } else {
            *d->input[i] = (rx_buffer->inputs[1] >> ((input_pins[i] - 32) & 31)) & 1;
        }
        *d->input_not[i] = !(*d->input[i]);
    }
}

static void bb_hal_process_send(module_data_t *d)
{
    uint32_t outs0 = 0;
    uint32_t outs1 = 0;

    for (uint8_t i = 0; i < out_pins_no; i++) {
        if (i < 32) {
            outs0 |= *d->output[i] == 1 ? 1u << i : 0;
        } else {
            outs1 |= *d->output[i] == 1 ? 1u << (i & 31) : 0;
        }
    }

    tx_buffer->outputs[0] = outs0;
    tx_buffer->outputs[1] = outs1;
}

#endif // breakout_board == 0