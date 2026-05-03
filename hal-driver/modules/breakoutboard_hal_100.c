// ============================================================
// Breakout Board HAL Module - Type 100
// Hardware: 2x MCP23017 for 32 digital inputs
//           1x MCP23017 for 16 digital outputs
//
// Firmware counterpart: firmware/modules/breakoutboard_100.c
// ============================================================

#if breakout_board == 100

enum {
    BB100_IN_PINS_NO = 32,
    BB100_OUT_PINS_NO = 16,
};

static int bb_hal_setup_pins(module_data_t *d, int j, int comp_id,
                             char *name, uint32_t nsize)
{
    int r;

    for (int i = 0; i < BB100_OUT_PINS_NO; i++) {
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.output.%d", j, i);
        r = hal_pin_bit_newf(HAL_IN, &d->output[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: output pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        *d->output[i] = 0;
    }

    for (int i = 0; i < BB100_IN_PINS_NO; i++) {
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.input.%d", j, i);
        r = hal_pin_bit_newf(HAL_OUT, &d->input[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: input pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.input.%d-not", j, i);
        r = hal_pin_bit_newf(HAL_OUT, &d->input_not[i], comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: input-not pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
    }

    return 0;
}

static void bb_hal_process_recv(module_data_t *d)
{
    uint32_t value = rx_buffer->inputs[0];

    for (uint8_t i = 0; i < BB100_IN_PINS_NO; i++) {
        uint8_t bit = (value >> i) & 1u;
        *d->input[i] = bit;
        *d->input_not[i] = !bit;
    }
}

static void bb_hal_process_send(module_data_t *d)
{
    uint32_t outs = 0;

    for (uint8_t i = 0; i < BB100_OUT_PINS_NO; i++) {
        if (*d->output[i]) {
            outs |= (1u << i);
        }
    }

    tx_buffer->outputs[0] = outs;
}

#endif // breakout_board == 100
