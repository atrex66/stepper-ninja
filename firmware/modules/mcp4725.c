#include "config.h"
#if breakout_board > 0
#include "mcp4725.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"


void mcp4725_write_data(uint8_t address, uint16_t data) {
    uint8_t buf[3];
    uint8_t data_low = ((data & 0xfff) & 0x0f) << 4;
    uint8_t data_high = (data & 0xfff) >> 4;
    buf[0] = MCP4725_DAC;
    buf[1] = data_high;
    buf[2] = data_low;
    i2c_write_blocking(MCP4725_PORT, address, buf, 3, false);
}

#endif