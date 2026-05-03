#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "breakoutboard.h"

// ============================================================
// io-ninja breakout board firmware (version 2)
//   - supports 96 inputs and 32 outputs via 6x MCP23017
//   - includes helper functions for I2C and MCP register access
//   - implementation for 2 MCP23017 input and 2 MCP23017 output
//   - support for Sauter toolchanger BCD encoder with parity bit (optional)
// ============================================================

#if breakout_board == 2

extern transmission_pc_pico_t *rx_buffer;
extern transmission_pico_pc_t *tx_buffer;
extern volatile uint32_t input_buffer[4];
extern volatile uint32_t output_buffer;

static inline uint8_t mirror8(uint8_t value) {
	uint8_t mirrored = 0;

	mirrored |= (value >> 0) & 1 ? 1 << 7 : 0;
	mirrored |= (value >> 1) & 1 ? 1 << 6 : 0;
	mirrored |= (value >> 2) & 1 ? 1 << 5 : 0;
	mirrored |= (value >> 3) & 1 ? 1 << 4 : 0;
	mirrored |= (value >> 4) & 1 ? 1 << 3 : 0;
	mirrored |= (value >> 5) & 1 ? 1 << 2 : 0;
	mirrored |= (value >> 6) & 1 ? 1 << 1 : 0;
	mirrored |= (value >> 7) & 1 ? 1 << 0 : 0;

	return mirrored;
}

void breakout_board_setup(void) {
	i2c_setup();

	gpio_init(MCP_ALL_RESET);
	gpio_set_dir(MCP_ALL_RESET, GPIO_OUT);
	gpio_put(MCP_ALL_RESET, 0);
	sleep_ms(100);
	gpio_put(MCP_ALL_RESET, 1);

	printf("Detecting MCP23017 (Outputs) starting on %#x address\n", MCP23017_ADDR_output);
	for (int i = 0; i < output_expander_count; i++) {
		if (i2c_check_address(I2C_PORT, MCP23017_ADDR_output + i)) {
			printf("MCP23017:%d (Outputs) Init %#x\n", i, MCP23017_ADDR_output + i);
			mcp_write_register(MCP23017_ADDR_output + i, 0x00, 0x00);
			mcp_write_register(MCP23017_ADDR_output + i, 0x01, 0x00);
			mcp_write_register(MCP23017_ADDR_output + i, 0x12, 0x00);
			mcp_write_register(MCP23017_ADDR_output + i, 0x13, 0x00);
		} else {
			printf("No MCP23017:%d (Outputs) found on %#x address.\n", i, MCP23017_ADDR_output + i);
		}
	}

	printf("Detecting MCP23017 (Inputs) starting on %#x address\n", MCP23017_ADDR);
	for (int i = 0; i < IN_EXPANDER_COUNT; i++) {
		if (i2c_check_address(I2C_PORT, MCP23017_ADDR + i)) {
			printf("MCP23017:%d (Inputs) Init %#x\n", i, MCP23017_ADDR + i);
			mcp_write_register(MCP23017_ADDR + i, 0x00, 0xff);
			mcp_write_register(MCP23017_ADDR + i, 0x01, 0xff);
			mcp_write_register(MCP23017_ADDR + i, 0x02, 0xff);
			mcp_write_register(MCP23017_ADDR + i, 0x03, 0xff);
		} else {
			printf("No MCP23017:%d (Inputs) found on %#x address.\n", i, MCP23017_ADDR + i);
		}
	}
}

void breakout_board_disconnected_update(void) {
	for (int i = 0; i < output_expander_count; i++) {
		mcp_write_register(MCP23017_ADDR_output + i, 0x12, 0);
		mcp_write_register(MCP23017_ADDR_output + i, 0x13, 0);
	}
}

void breakout_board_connected_update(void) {
	uint32_t buffer;
	uint8_t out0;
	uint8_t out1;
	uint8_t out2;
	uint8_t out3;

	buffer = mirror8((uint8_t)mcp_read_register(MCP23017_ADDR + 0, 0x13)) << 8;
	buffer |= mirror8((uint8_t)(mcp_read_register(MCP23017_ADDR + 0, 0x12)));
	buffer |= mirror8((uint8_t)(mcp_read_register(MCP23017_ADDR + 1, 0x13))) << 24;
	buffer |= mirror8((uint8_t)(mcp_read_register(MCP23017_ADDR + 1, 0x12))) << 16;
	input_buffer[0] = buffer;

	buffer = mirror8((uint8_t)mcp_read_register(MCP23017_ADDR + 2, 0x13)) << 8;
	buffer |= mirror8((uint8_t)(mcp_read_register(MCP23017_ADDR + 2, 0x12)));
	buffer |= mirror8((uint8_t)(mcp_read_register(MCP23017_ADDR + 3, 0x13))) << 24;
	buffer |= mirror8((uint8_t)(mcp_read_register(MCP23017_ADDR + 3, 0x12))) << 16;
	input_buffer[1] = buffer;

	buffer = mirror8((uint8_t)mcp_read_register(MCP23017_ADDR + 4, 0x13)) << 8;
	buffer |= mirror8((uint8_t)(mcp_read_register(MCP23017_ADDR + 4, 0x12)));
	buffer |= mirror8((uint8_t)(mcp_read_register(MCP23017_ADDR + 5, 0x13))) << 24;
	buffer |= mirror8((uint8_t)(mcp_read_register(MCP23017_ADDR + 5, 0x12))) << 16;
	input_buffer[2] = buffer;

	out0 = mirror8((uint8_t)((output_buffer >> 0) & 0xff));
	out1 = mirror8((uint8_t)((output_buffer >> 8) & 0xff));
	out2 = mirror8((uint8_t)((output_buffer >> 16) & 0xff));
	out3 = mirror8((uint8_t)((output_buffer >> 24) & 0xff));

	mcp_write_register(MCP23017_ADDR_output + 0, 0x12, out0);
	mcp_write_register(MCP23017_ADDR_output + 0, 0x13, out1);
	mcp_write_register(MCP23017_ADDR_output + 1, 0x12, out2);
	mcp_write_register(MCP23017_ADDR_output + 1, 0x13, out3);
}

void breakout_board_handle_data(void) {
	tx_buffer->inputs[0] = input_buffer[0];
	tx_buffer->inputs[1] = input_buffer[1];
	tx_buffer->inputs[2] = input_buffer[2];
	tx_buffer->inputs[3] = input_buffer[3];
	output_buffer = rx_buffer->outputs[0];
}

#endif

