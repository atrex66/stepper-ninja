#include <stdio.h>
#include "breakoutboard.h"
#include "config.h"

// ============================================================
// Breakout Board Firmware (Version 1)
//   - supports 16 digital inputs via MCP23017
//   - supports 8 digital outputs via MCP23008
//   - supports 2 analog outputs via MCP4725 (optional)
//   - supports 4 Step generator
//   - supports 2 Quadrature encoder inputs
//   - includes helper functions for I2C and MCP register access
//   - maps incoming packet fields to HAL pins and vice versa
//   - ideal for small CNC machines with simple breakout board needs
// ============================================================

#if breakout_board == 1

#ifndef io_expanders
#define io_expanders 0
#endif

extern transmission_pc_pico_t *rx_buffer;
extern transmission_pico_pc_t *tx_buffer;
extern volatile uint32_t input_buffer[4];
extern volatile uint32_t output_buffer;

void breakout_board_setup(void) {
    i2c_setup();

    #if defined(MCP4725_BASE)
    mcp4725_port_setup();
    #endif

    printf("Detecting MCP23008 (Outputs) starting on %#x address\n", MCP23008_ADDR);

    uint8_t io_base_output = MCP23008_ADDR;
    for (int i = 0; i < io_expanders + 1; i += 2) {
        if (i2c_check_address(I2C_PORT, io_base_output + i)) {
            printf("MCP23008:%d (Outputs) Init\n", i);
            mcp_write_register(io_base_output + i, 0x00, 0x00);
            mcp_write_register(io_base_output + i, 0x09, 0x00);
        } else {
            printf("No MCP23008:%d (Outputs) found on %#x address.\n", i, io_base_output + i);
        }
    }

    printf("Detecting MCP23017 (Inputs) starting on %#x address\n", MCP23017_ADDR);
    uint8_t io_base_input = MCP23017_ADDR;
    for (int i = 0; i < io_expanders + 1; i += 2) {
        if (i2c_check_address(I2C_PORT, io_base_input + i)) {
            printf("MCP23017:%d (Inputs) Init\n", i);
            mcp_write_register(io_base_input + i, 0x00, 0xff);
            mcp_write_register(io_base_input + i, 0x01, 0xff);
            mcp_write_register(io_base_input + i, 0x02, 0xff);
            mcp_write_register(io_base_input + i, 0x03, 0xff);
        } else {
            printf("No MCP23017:%d (Inputs) found on %#x address.\n", i, io_base_input + i);
        }
    }

    #if defined(MCP4725_BASE)
    for (int i = 0; i < ANALOG_CH; i++) {
        printf("Detecting MCP4725 (analog) on %#x address\n", MCP4725_BASE + i);
        if (i2c_check_address(MCP4725_PORT, MCP4725_BASE + i)) {
            printf("MCP4725 (analog) Init\n");
            mcp4725_write_data(MCP4725_BASE + i, 0);
        } else {
            printf("No MCP4725 (analog) found on %#x address.\n", MCP4725_BASE + i);
        }
    }
    #endif
}

void breakout_board_disconnected_update(void) {
    #if defined(MCP4725_BASE)
    mcp4725_write_data(MCP4725_BASE, 0);
    mcp4725_write_data(MCP4725_BASE + 1, 0);
    #endif

    mcp_write_register(MCP23008_ADDR, 0x09, 0);

    #if io_expanders + 1 > 0
    mcp_write_register(MCP23008_ADDR + 0, 0x09, 0);
    #endif
    #if io_expanders + 1 > 1
    mcp_write_register(MCP23008_ADDR + 2, 0x09, 0);
    #endif
    #if io_expanders + 1 > 2
    mcp_write_register(MCP23008_ADDR + 4, 0x09, 0);
    #endif
    #if io_expanders + 1 > 3
    mcp_write_register(MCP23008_ADDR + 6, 0x09, 0);
    #endif
}

void breakout_board_connected_update(void) {
    #if io_expanders + 1 > 0
    input_buffer[0] = mcp_read_register(MCP23017_ADDR + 0, 0x13) | mcp_read_register(MCP23017_ADDR + 0, 0x12) << 8;
    mcp_write_register(MCP23008_ADDR + 0, 0x09, (output_buffer >> 0) & 0xff);
    #endif
    #if io_expanders + 1 > 1
    input_buffer[0] |= (mcp_read_register(MCP23017_ADDR + 2, 0x13) | mcp_read_register(MCP23017_ADDR + 2, 0x12) << 8) << 16;
    mcp_write_register(MCP23008_ADDR + 2, 0x09, (output_buffer >> 8) & 0xff);
    #endif
    #if io_expanders + 1 > 2
    input_buffer[1] = mcp_read_register(MCP23017_ADDR + 4, 0x13) | mcp_read_register(MCP23017_ADDR + 4, 0x12) << 8;
    mcp_write_register(MCP23008_ADDR + 4, 0x09, (output_buffer >> 16) & 0xff);
    #endif
    #if io_expanders + 1 > 3
    input_buffer[1] |= (mcp_read_register(MCP23017_ADDR + 6, 0x13) | mcp_read_register(MCP23017_ADDR + 6, 0x12) << 8) << 16;
    mcp_write_register(MCP23008_ADDR + 6, 0x09, (output_buffer >> 24) & 0xff);
    #endif

    #if defined(MCP4725_BASE)
    mcp4725_write_data(MCP4725_BASE + 0, rx_buffer->analog_out[0] & 0xfff);
    mcp4725_write_data(MCP4725_BASE + 1, rx_buffer->analog_out[1] & 0xfff);
    #endif
}

void breakout_board_handle_data(void) {
    tx_buffer->inputs[2] = input_buffer[0];
    tx_buffer->inputs[3] = input_buffer[1];
    output_buffer = rx_buffer->outputs[0];
}

#endif

