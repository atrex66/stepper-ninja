"""Minimal Pico SDK declarations for host-side I2C driver contracts."""

def make_fake_sdk(temp):
    (temp / "pico").mkdir()
    (temp / "hardware").mkdir()
    (temp / "pico/stdlib.h").write_text("""
#include <stdint.h>
#define NUM_BANK0_GPIOS 30
#define GPIO_FUNC_I2C 3
uint64_t time_us_64(void);
void gpio_set_function(unsigned, unsigned);
void gpio_pull_up(unsigned);
""")
    (temp / "pico/critical_section.h").write_text("""
typedef struct { int unused; } critical_section_t;
static inline void critical_section_init(critical_section_t *p) {(void)p;}
static inline void critical_section_enter_blocking(critical_section_t *p) {(void)p;}
static inline void critical_section_exit(critical_section_t *p) {(void)p;}
""")
    (temp / "hardware/i2c.h").write_text("""
#ifndef FAKE_I2C_H
#define FAKE_I2C_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { unsigned id; } i2c_inst_t;
extern i2c_inst_t fake_ports[2];
#define i2c0 (&fake_ports[0])
#define i2c1 (&fake_ports[1])
unsigned i2c_init(i2c_inst_t *, unsigned);
int i2c_write_timeout_us(i2c_inst_t *, uint8_t, const uint8_t *, size_t, bool, unsigned);
int i2c_read_timeout_us(i2c_inst_t *, uint8_t, uint8_t *, size_t, bool, unsigned);
#endif
""")
