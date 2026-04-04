#ifndef BREAKOUTBOARD_H
#define BREAKOUTBOARD_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "hardware/i2c.h"
#include "mcp4725.h"
#include "transmission.h"

void breakout_board_setup(void);
void breakout_board_disconnected_update(void);
void breakout_board_connected_update(void);
void breakout_board_handle_data(void);

#if breakout_board > 0
void i2c_setup(void);
bool i2c_check_address(i2c_inst_t *i2c, uint8_t addr);
uint8_t mcp_read_register(uint8_t i2c_addr, uint8_t reg);
void mcp_write_register(uint8_t i2c_addr, uint8_t reg, uint8_t value);

#if defined(MCP4725_BASE)
void mcp4725_port_setup(void);
#endif
#endif

#endif
