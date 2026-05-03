#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "breakoutboard.h"

// ============================================================
// Breakout Board Firmware (Version 100) (RASPI SPI VERSION)
//   - 4 stepgen + 2 encoder (configured in footer.h)
//   - 32 digital inputs via 2x MCP23017
//   - 16 digital outputs via 1x MCP23017
// ============================================================

#if breakout_board == 100

extern transmission_pc_pico_t *rx_buffer;
extern transmission_pico_pc_t *tx_buffer;
extern volatile uint32_t input_buffer[4];
extern volatile uint32_t output_buffer;

static inline uint16_t read_mcp23017_16(uint8_t addr)
{
    uint16_t a = mcp_read_register(addr, 0x12);
    uint16_t b = mcp_read_register(addr, 0x13);
    return (uint16_t)(a | (b << 8));
}

void breakout_board_setup(void)
{
    i2c_setup();

    gpio_init(MCP_ALL_RESET);
    gpio_set_dir(MCP_ALL_RESET, GPIO_OUT);
    gpio_put(MCP_ALL_RESET, 0);
    sleep_ms(100);
    gpio_put(MCP_ALL_RESET, 1);

    printf("Detecting MCP23017 input expanders starting on %#x address\n", MCP23017_ADDR);
    for (int i = 0; i < IN_EXPANDER_COUNT; i++) {
        uint8_t addr = (uint8_t)(MCP23017_ADDR + i);
        if (i2c_check_address(I2C_PORT, addr)) {
            printf("MCP23017 input expander %d found at %#x\n", i, addr);
            mcp_write_register(addr, 0x00, 0xff); // IODIRA input
            mcp_write_register(addr, 0x01, 0xff); // IODIRB input
            mcp_write_register(addr, 0x0c, 0xff); // GPPUA pull-up
            mcp_write_register(addr, 0x0d, 0xff); // GPPUB pull-up
        } else {
            printf("No MCP23017 input expander %d found on %#x\n", i, addr);
        }
    }

    printf("Detecting MCP23017 output expanders starting on %#x address\n", MCP23017_ADDR_output);
    for (int i = 0; i < output_expander_count; i++) {
        uint8_t addr = (uint8_t)(MCP23017_ADDR_output + i);
        if (i2c_check_address(I2C_PORT, addr)) {
            printf("MCP23017 output expander %d found at %#x\n", i, addr);
            mcp_write_register(addr, 0x00, 0x00); // IODIRA output
            mcp_write_register(addr, 0x01, 0x00); // IODIRB output
            mcp_write_register(addr, 0x12, 0x00); // GPIOA low
            mcp_write_register(addr, 0x13, 0x00); // GPIOB low
        } else {
            printf("No MCP23017 output expander %d found on %#x\n", i, addr);
        }
    }
}

void breakout_board_disconnected_update(void)
{
    for (int i = 0; i < output_expander_count; i++) {
        uint8_t addr = (uint8_t)(MCP23017_ADDR_output + i);
        mcp_write_register(addr, 0x12, 0x00);
        mcp_write_register(addr, 0x13, 0x00);
    }
}

void breakout_board_connected_update(void)
{
    uint32_t in0 = 0;

    in0 |= (uint32_t)read_mcp23017_16((uint8_t)(MCP23017_ADDR + 0));
    in0 |= (uint32_t)read_mcp23017_16((uint8_t)(MCP23017_ADDR + 1)) << 16;
    input_buffer[0] = in0;

    mcp_write_register(MCP23017_ADDR_output, 0x12, (uint8_t)(output_buffer & 0xff));
    mcp_write_register(MCP23017_ADDR_output, 0x13, (uint8_t)((output_buffer >> 8) & 0xff));
}

void breakout_board_handle_data(void)
{
    tx_buffer->inputs[0] = input_buffer[0];
    tx_buffer->inputs[1] = 0;
    tx_buffer->inputs[2] = 0;
    tx_buffer->inputs[3] = 0;

    output_buffer = rx_buffer->outputs[0];
}

#endif // breakout_board == 100
