#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "breakoutboard.h"

// ============================================================
// Breakout Board Firmware Template (USER)
//
// How to use:
// 1) Copy this file to: breakoutboard_<ID>.c
// 2) Change the compile guard below to: #if breakout_board == <ID>
// 3) Add the new file to firmware/CMakeLists.txt
// 4) Add board-specific config/macros in firmware/inc/footer.h
// 5) Implement the 4 required callbacks below
// ============================================================

#if breakout_board == 255

extern transmission_pc_pico_t *rx_buffer;
extern transmission_pico_pc_t *tx_buffer;
extern volatile uint32_t input_buffer[4];
extern volatile uint32_t output_buffer;

void breakout_board_setup(void)
{
    // TODO: Initialize board peripherals here
    // Example: i2c_setup(); mcp4725_port_setup(); gpio_init(...);
}

void breakout_board_disconnected_update(void)
{
    // TODO: Put outputs to a safe state when host communication is lost
}

void breakout_board_connected_update(void)
{
    // TODO: Apply outputs from rx_buffer to board hardware
    // Example: DAC writes, expander outputs, enable bits...
    (void)rx_buffer;
}

void breakout_board_handle_data(void)
{
    // TODO: Map board input/output data to tx/rx packet fields
    tx_buffer->inputs[0] = input_buffer[0];
    tx_buffer->inputs[1] = input_buffer[1];
    tx_buffer->inputs[2] = input_buffer[2];
    tx_buffer->inputs[3] = input_buffer[3];

    output_buffer = rx_buffer->outputs[0];
}

#endif // breakout_board == 255
