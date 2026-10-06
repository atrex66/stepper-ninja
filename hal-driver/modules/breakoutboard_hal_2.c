// ============================================================
// Breakout Board HAL Module - Type 2
// Hardware: 6x MCP23017 for 96 digital inputs
//           2x MCP23017 for 32 digital outputs
//           Optional toolchanger BCD encoder (4-bit + strobe + parity)
//
// Firmware counterpart: firmware/modules/breakoutboard_2.c
//   (symlinked as: hal-driver/modules/breakoutboard_2.c)
//
// This file is included directly into io-ninja.c after module_data_t
// and the global rx_buffer / tx_buffer pointers are defined.
// ============================================================

#if breakout_board == 2

enum {
    BB2_IN_PINS_NO = 96,
    BB2_OUT_PINS_NO = 32,
};

// ------------------------------------------------------------
// bb_hal_2_setup_pins
// Create all HAL pins for breakout board type 2:
//   - out_pins_no digital output bits  (io-ninja.N.output.X)
//   - in_pins_no  digital input bits   (io-ninja.N.input.X / input.X-not)
//   - toolchanger BCD encoder pins     (io-ninja.N.sauter.*)
// Returns 0 on success, negative HAL error code on failure.
// ------------------------------------------------------------
static int bb_hal_setup_pins(module_data_t *d, int j, int comp_id,
                             char *name, uint32_t nsize)
{
    int r;

    // --- Digital outputs ---
    for (int i = 0; i < BB2_OUT_PINS_NO; i++) {
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.output.%d", j, i);
        r = sn_new_bit(HAL_IN, &d->output[i], comp_id, name);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: output pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        sn_set_bit(d->output[i], 0);
    }

    // --- Digital inputs + inverted ---
    for (int i = 0; i < BB2_IN_PINS_NO; i++) {
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.input.%d", j, i);
        r = sn_new_bit(HAL_IN, &d->input[i], comp_id, name);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: input pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.input.%d-not", j, i);
        r = sn_new_bit(HAL_IN, &d->input_not[i], comp_id, name);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ".%d: ERROR: input-not pin %d export failed err=%i\n",
                j, i, r);
            return r;
        }
    }

    // --- Toolchanger BCD encoder pins (optional) ---
#if toolchanger_encoder == 1
    memset(name, 0, nsize);
    snprintf(name, nsize, module_name ".%d.sauter.bit0", j);
    r = sn_new_bit(HAL_IN, &d->toolchanger_bit0, comp_id, name);
    if (r < 0) { return r; }

    memset(name, 0, nsize);
    snprintf(name, nsize, module_name ".%d.sauter.bit1", j);
    r = sn_new_bit(HAL_IN, &d->toolchanger_bit1, comp_id, name);
    if (r < 0) { return r; }

    memset(name, 0, nsize);
    snprintf(name, nsize, module_name ".%d.sauter.bit2", j);
    r = sn_new_bit(HAL_IN, &d->toolchanger_bit2, comp_id, name);
    if (r < 0) { return r; }

    memset(name, 0, nsize);
    snprintf(name, nsize, module_name ".%d.sauter.bit3", j);
    r = sn_new_bit(HAL_IN, &d->toolchanger_bit3, comp_id, name);
    if (r < 0) { return r; }

    memset(name, 0, nsize);
    snprintf(name, nsize, module_name ".%d.sauter.strobe", j);
    r = sn_new_bit(HAL_IN, &d->toolchanger_strobe, comp_id, name);
    if (r < 0) { return r; }

    memset(name, 0, nsize);
    snprintf(name, nsize, module_name ".%d.sauter.parity", j);
    r = sn_new_bit(HAL_IN, &d->toolchanger_parity, comp_id, name);
    if (r < 0) { return r; }

    memset(name, 0, nsize);
    snprintf(name, nsize, module_name ".%d.sauter.error", j);
    r = sn_new_bit(HAL_OUT, &d->toolchanger_error, comp_id, name);
    if (r < 0) { return r; }

    memset(name, 0, nsize);
    snprintf(name, nsize, module_name ".%d.sauter.position", j);
    r = sn_new_u32(HAL_OUT, &d->toolchanger_position, comp_id, name);
    if (r < 0) { return r; }

    memset(name, 0, nsize);
    snprintf(name, nsize, module_name ".%d.sauter.even-or-odd-parity", j);
    r = sn_new_bit(HAL_IN, &d->toolchanger_even_or_odd_parity, comp_id, name);
    if (r < 0) { return r; }
#endif // toolchanger_encoder == 1

    return 0;
}

// ------------------------------------------------------------
// bb_hal_2_process_recv
// Map received UDP packet inputs (rx_buffer->inputs[0..2]) onto
// HAL input pins.  Also decodes toolchanger BCD encoder.
// Called from udp_io_process_recv().
// ------------------------------------------------------------
static void bb_hal_process_recv(module_data_t *d)
{
    // --- 96 digital inputs spread across three 32-bit words ---
    for (uint8_t k = 0; k < 3; k++) {
        uint32_t value = rx_buffer->inputs[k];
        for (uint8_t i = 0; i < 32; i++) {
            uint8_t idx = k * 32 + i;
            uint8_t bit = (value >> i) & 1;
            sn_set_bit(d->input[idx], bit);
            sn_set_bit(d->input_not[idx], !bit);
        }
    }

    // --- Toolchanger BCD encoder: latch position on rising strobe ---
#if toolchanger_encoder == 1
    if (sn_get_bit(d->toolchanger_strobe) == 1) {
        sn_set_u32(d->toolchanger_position, sn_get_bit(d->toolchanger_bit0) * 1 +
            sn_get_bit(d->toolchanger_bit1) * 2 +
            sn_get_bit(d->toolchanger_bit2) * 4 +
            sn_get_bit(d->toolchanger_bit3) * 8);

        uint8_t parity = (sn_get_bit(d->toolchanger_bit0) +
                          sn_get_bit(d->toolchanger_bit1) +
                          sn_get_bit(d->toolchanger_bit2) +
                          sn_get_bit(d->toolchanger_bit3)) % 2;

        if (parity != sn_get_bit(d->toolchanger_parity)) {
            sn_set_bit(d->toolchanger_error, 1);
        } else {
            sn_set_bit(d->toolchanger_error, 0);
        }
        sn_set_bit(d->toolchanger_even_or_odd_parity, parity);
    }
#endif // toolchanger_encoder == 1
}

// ------------------------------------------------------------
// bb_hal_2_process_send
// Pack HAL output pin states into tx_buffer->outputs[0].
// Called from udp_io_process_send().
// ------------------------------------------------------------
static void bb_hal_process_send(module_data_t *d)
{
    uint32_t outs = 0;
    for (uint8_t i = 0; i < BB2_OUT_PINS_NO; i++) {
        if (sn_get_bit(d->output[i])) {
            outs |= (1u << i);
        }
    }
    tx_buffer->outputs[0] = outs;
}

#endif // breakout_board == 2
