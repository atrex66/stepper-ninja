#ifndef MCP4725_H
#define MCP4725_H
#include <stdint.h>

//  registerMode
#define MCP4725_DAC             0x40
#define MCP4725_DACEEPROM       0x60

void mcp4725_write_data(uint8_t address, uint16_t data);

#endif