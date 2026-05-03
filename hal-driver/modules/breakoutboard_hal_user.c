// ============================================================
// Breakout Board HAL Template (USER)
//
// How to use:
// 1) Copy this file to: breakoutboard_hal_<ID>.c
// 2) Change the compile guard below to: #if breakout_board == <ID>
// 3) Add a matching #elif branch in analog-ninja.c:
//      #elif breakout_board == <ID>
//      #include "modules/breakoutboard_hal_<ID>.c"
//
// Each board module defines exactly three functions with these
// common names (no board-number suffix needed):
//   bb_hal_setup_pins(), bb_hal_process_recv(), bb_hal_process_send()
//
// This file is expected to be included into analog-ninja.c after
// module_data_t and global rx_buffer/tx_buffer are defined.
// ============================================================

#if breakout_board == 255

enum {
    BB_USER_IN_PINS_NO = 0,
    BB_USER_OUT_PINS_NO = 0,
};

static int bb_hal_setup_pins(module_data_t *d, int j, int comp_id,
                             char *name, uint32_t nsize)
{
    int r = 0;

    // TODO: Export all board-specific HAL pins here
    // Example:
    //   r = hal_pin_bit_newf(HAL_IN, &d->output[0], comp_id,
    //                        module_name ".%d.output.0", j);
    //   if (r < 0) return r;

    (void)d;
    (void)j;
    (void)comp_id;
    (void)name;
    (void)nsize;
    return r;
}

static void bb_hal_process_recv(module_data_t *d)
{
    // TODO: Map incoming packet fields from rx_buffer to HAL pins
    // Example: *d->input[i] = (rx_buffer->inputs[word] >> bit) & 1;

    (void)d;
}

static void bb_hal_process_send(module_data_t *d)
{
    // TODO: Map HAL pin values to outgoing tx_buffer fields
    // Example: tx_buffer->outputs[0] bits, tx_buffer->analog_out[*], etc.

    (void)d;
}

#endif // breakout_board == 255
