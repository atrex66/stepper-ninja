#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "breakoutboard.h"

#if breakout_board > 0
void i2c_setup(void) {
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCK, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCK);
}

#if defined(MCP4725_BASE)
void mcp4725_port_setup(void) {
    i2c_init(MCP4725_PORT, 400 * 1000);
    gpio_set_function(MCP4725_SDA, GPIO_FUNC_I2C);
    gpio_set_function(MCP4725_SCL, GPIO_FUNC_I2C);
}
#endif

uint8_t mcp_read_register(uint8_t i2c_addr, uint8_t reg) {
    uint8_t data;
    int ret;

    ret = i2c_write_blocking(I2C_PORT, i2c_addr, &reg, 1, true);
    if (ret < 0) {
        return 0xFF;
    }

    ret = i2c_read_blocking(I2C_PORT, i2c_addr, &data, 1, false);
    if (ret < 0) {
        return 0xFF;
    }

    return data;
}

void mcp_write_register(uint8_t i2c_addr, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    int ret = i2c_write_blocking(I2C_PORT, i2c_addr, data, 2, false);

    if (ret < 0) {
        printf("I2C write failed %02X\n", i2c_addr);
        asm("nop");
    }
}

bool i2c_check_address(i2c_inst_t *i2c, uint8_t addr) {
    uint8_t buffer[1] = {0x00};
    int ret = i2c_write_blocking_until(i2c, addr, buffer, 1, false, make_timeout_time_us(100));

    if (ret != PICO_ERROR_GENERIC) {
        return true;
    }

    return false;
}
#endif
