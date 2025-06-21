#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/multicore.h"
#include "pico/divider.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/regs/pll.h"
#include "hardware/structs/pll.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "serial_terminal.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "jump_table.h"
#include "transmission.h"
#include "flash_config.h"
#include "pio_settings.h"
#include "pwm.h"
#include "freq_generator.pio.h"
#if use_stepcounter == 0
#include "quadrature_encoder.pio.h"
#else
#include "step_counter.pio.h"
#endif

#if brakeout_board > 0
#include "hardware/i2c.h"
#endif 

#define core1_running 1
// Low-pass filter parameters
#define ALPHA 0.25f // Smoothing factor (0.0 to 1.0, lower = more smoothing)

// -------------------------------------------
// Globális változók a magok közötti kommunikációhoz
// -------------------------------------------
// Függvény deklarációk
#if brakeout_board > 0
    void i2c_setup(void);
    bool i2c_check_address(i2c_inst_t *i2c, uint8_t addr);
    uint8_t mcp_read_register(uint8_t i2c_addr, uint8_t reg);
    void mcp_write_register(uint8_t i2c_addr, uint8_t reg, uint8_t value);
#endif // brakeout_board > 0
void stepgen_update_handler();
static void alarm_irq(void);
void jump_table_checksum();
void jump_table_checksum_in();
void i2c_setup(void);
void cs_select();
void cs_deselect();
uint8_t spi_read();
void reset_with_watchdog();
void spi_write(uint8_t data);
int32_t _sendto(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t port);
int32_t _recvfrom(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t *port);
void handle_udp();
void w5100s_interrupt_init();
void w5500_interrupt_init();
void w5100s_init();
void network_init();
uint8_t xor_checksum(const uint8_t *data, uint8_t len);
void printbuf(uint8_t *buf, size_t len);
void core1_entry();
void gpio_callback(uint gpio, uint32_t events);

static void spi_write_burst(uint8_t *pBuf, uint16_t len);
static void spi_read_burst(uint8_t *pBuf, uint16_t len);
static void spi_read_fulldup(uint8_t *pBuf, uint8_t *sBuf, uint16_t len);



#endif // MAIN_H