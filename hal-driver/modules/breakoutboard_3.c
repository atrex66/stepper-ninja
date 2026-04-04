#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "breakoutboard.h"

// ============================================================
// Breakout Board Firmware for the analog ninja
//   - supports 4 drive enable relays trough an MCP23008 4x 12bit bipolar analog outputs trough MCP4725
//   - includes helper functions for I2C and MCP register access
//   - maps incoming packet analog_out values to DAC outputs and output bits to expander
// ============================================================

#if breakout_board == 3

extern transmission_pc_pico_t *rx_buffer;
extern transmission_pico_pc_t *tx_buffer;
extern volatile uint32_t input_buffer[4];
extern volatile uint32_t output_buffer;

void breakout_board_setup(void) {
    i2c_setup();
    mcp4725_port_setup();

    gpio_init(MCP_ALL_RESET);
    gpio_set_dir(MCP_ALL_RESET, GPIO_OUT);
    gpio_put(MCP_ALL_RESET, 0);
    sleep_ms(100);
    gpio_put(MCP_ALL_RESET, 1);

    for (int i = 0; i < DA_CHANNELS; i++) {
        printf("Detecting MCP4725 (analog) on %#x address\n", MCP4725_BASE + i);
        if (i2c_check_address(MCP4725_PORT, MCP4725_BASE + i)) {
            printf("MCP4725 (analog) Init\n");
            mcp4725_write_data(MCP4725_BASE + i, 1023);
        } else {
            printf("No MCP4725 (analog) found on %#x address.\n", MCP4725_BASE + i);
        }
    }

    mcp_write_register(MCP23008_ADDR, 0x00, 0x00);
    mcp_write_register(MCP23008_ADDR, 0x09, 0);
}

void breakout_board_disconnected_update(void) {
    for (int i = 0; i < DA_CHANNELS; i++) {
        mcp4725_write_data(MCP4725_BASE + i, 2047);
    }
    mcp_write_register(MCP23008_ADDR, 0x09, 0);
}

void breakout_board_connected_update(void) {
    for (int i = 0; i < DA_CHANNELS; i++) {
        mcp4725_write_data(MCP4725_BASE + i, rx_buffer->analog_out[i] & 0xfff);
    }
    mcp_write_register(MCP23008_ADDR, 0x09, rx_buffer->outputs[0] & 0xff);
}

void breakout_board_handle_data(void) {
    tx_buffer->inputs[0] = input_buffer[0];
    tx_buffer->inputs[1] = input_buffer[1];
    tx_buffer->inputs[2] = input_buffer[2];
    tx_buffer->inputs[3] = input_buffer[3];
    output_buffer = rx_buffer->outputs[0];
}

#endif
