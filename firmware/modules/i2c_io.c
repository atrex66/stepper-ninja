#include "i2c_io.h"
#if I2C_IO_ENABLE || I2C_DAC_ENABLE
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/critical_section.h"
#include "hardware/i2c.h"

static const sn_i2c_bus_config buses[] = I2C_IO_BUSES;
#if I2C_IO_ENABLE
static const sn_i2c_device_config devices[] = I2C_IO_DEVICES;
#endif
_Static_assert(sizeof(buses) / sizeof(*buses) == I2C_IO_BUS_COUNT, "I2C bus count mismatch");
#if I2C_IO_ENABLE
_Static_assert(sizeof(devices) / sizeof(*devices) == I2C_IO_DEVICE_COUNT, "I2C device count mismatch");

static struct {
    bool configured, online, valid;
    uint16_t sample;
    uint64_t next_us;
} state[I2C_IO_DEVICE_COUNT];
#endif
#if I2C_DAC_ENABLE
static const sn_i2c_dac_channel dac_channels[] = I2C_DAC_CHANNELS;
_Static_assert(sizeof(dac_channels) / sizeof(*dac_channels) == I2C_DAC_CHANNEL_COUNT,
               "I2C DAC channel count mismatch");
static struct {
    bool initialized, valid;
    uint64_t next_us;
} dac_state[I2C_DAC_CHANNEL_COUNT];
static uint32_t dac_commands[I2C_DAC_CHANNEL_COUNT];
#endif
static bool enabled, was_connected;
static critical_section_t snapshot_lock;
static uint32_t commands, published_inputs, published_status;

static i2c_inst_t *port(unsigned bus)
{ return buses[bus].controller ? i2c1 : i2c0; }

#if I2C_IO_ENABLE
/* Each SDK operation has a finite deadline. No bus transfers in packet/IRQ code. */
static bool write_reg(unsigned n, uint8_t reg, uint8_t value)
{
    const sn_i2c_device_config *d = &devices[n];
    uint8_t bytes[] = {reg, value};
    return i2c_write_timeout_us(port(d->bus), d->address, bytes, 2, false,
                                buses[d->bus].timeout_us) == 2;
}

static bool read_reg(unsigned n, uint8_t reg, uint8_t *value)
{
    const sn_i2c_device_config *d = &devices[n];
    if (i2c_write_timeout_us(port(d->bus), d->address, &reg, 1, true,
                              buses[d->bus].timeout_us) != 1) return false;
    return i2c_read_timeout_us(port(d->bus), d->address, value, 1, false,
                               buses[d->bus].timeout_us) == 1;
}

static bool write_pair(unsigned n, uint8_t reg, uint16_t value)
{
    if (!write_reg(n, reg, value & 0xff)) return false;
    return sn_i2c_io_width(&devices[n]) == 8 || write_reg(n, reg + 1, value >> 8);
}

static uint8_t direction_reg(unsigned n)
{ return devices[n].type >= SN_IO_TCA9535 ? 6 : 0; }
static uint8_t output_reg(unsigned n)
{
    if (devices[n].type >= SN_IO_TCA9535) return 2;
    return devices[n].type == SN_IO_MCP23008 ? 0x0a : 0x14; // OLAT, not GPIO
}
static uint8_t input_reg(unsigned n)
{
    if (devices[n].type >= SN_IO_TCA9535) return 0;
    return devices[n].type == SN_IO_MCP23008 ? 0x09 : 0x12;
}
static uint16_t physical_outputs(unsigned n, uint16_t logical)
{
    const sn_i2c_device_config *d = &devices[n];
    return (logical ^ d->output_invert) & ~d->direction;
}

static bool configure(unsigned n)
{
    const sn_i2c_device_config *d = &devices[n];
    // Devices must start in their power-on register bank (MCP BANK=0).
    // Release directions before changing latches on a recovery attempt.
    if (!write_pair(n, direction_reg(n), 0xffff) ||
        !write_pair(n, output_reg(n), physical_outputs(n, d->safe_outputs))) return false;
    uint8_t polarity = d->type >= SN_IO_TCA9535 ? 4 :
                       d->type == SN_IO_MCP23008 ? 1 : 2;
    if (!write_pair(n, polarity, 0)) return false; // inversion is done in software
    if (d->type < SN_IO_TCA9535 &&
        !write_pair(n, d->type == SN_IO_MCP23008 ? 6 : 0x0c, d->pullup)) return false;
    return write_pair(n, direction_reg(n), d->direction);
}

#endif

static bool pin_reserved(unsigned pin)
{
    const uint8_t inputs[] = in_pins, outputs[] = out_pins;
    for (unsigned i = 0; i < sizeof(inputs); ++i) if (inputs[i] == pin) return true;
    for (unsigned i = 0; i < sizeof(outputs); ++i) if (outputs[i] == pin) return true;
#if stepgens > 0
    const uint8_t steps[] = stepgen_steps, dirs[] = stepgen_dirs;
    for (unsigned i = 0; i < stepgens; ++i)
        if (steps[i] == pin || dirs[i] == pin) return true;
#endif
#if encoders > 0
    const uint8_t enc[] = enc_pins, index[] = enc_index_pins;
    for (unsigned i = 0; i < encoders; ++i)
        if (enc[i] == pin || enc[i] + 1u == pin || index[i] == pin) return true;
#endif
#if use_pwm == 1
    const uint8_t pwm[] = pwm_pin;
    for (unsigned i = 0; i < sizeof(pwm); ++i) if (pwm[i] == pin) return true;
#endif
#ifdef KBMATRIX
#if KB_PICO == 1
    const uint8_t rows[] = KB_ROWS_IN, cols[] = KB_COLUMNS_OUT;
    for (unsigned i = 0; i < sizeof(rows); ++i) if (rows[i] == pin) return true;
    for (unsigned i = 0; i < sizeof(cols); ++i) if (cols[i] == pin) return true;
#endif
#endif
    if (pin == LED_GPIO) return true;
#if raspberry_pi_spi == 1
    if (pin == RASPI_INT_OUT_GPIO || pin == RASPI_SPI_CS_GPIO) return true;
#else
    if (pin == GPIO_CS || pin == GPIO_RESET || pin == GPIO_INT) return true;
#endif
    return pin == GPIO_MISO || pin == GPIO_MOSI || pin == GPIO_SCK;
}

void sn_i2c_io_setup(void)
{
    critical_section_init(&snapshot_lock);
    enabled = sn_i2c_buses_valid(buses, I2C_IO_BUS_COUNT);
#if I2C_IO_ENABLE
    enabled = enabled && sn_i2c_io_config_valid(buses, I2C_IO_BUS_COUNT, devices, I2C_IO_DEVICE_COUNT);
#endif
#if I2C_DAC_ENABLE
    enabled = enabled && sn_i2c_dac_config_valid(buses, I2C_IO_BUS_COUNT,
        dac_channels, I2C_DAC_CHANNEL_COUNT,
#if I2C_IO_ENABLE
        devices, I2C_IO_DEVICE_COUNT);
#else
        NULL, 0);
#endif
    for (unsigned n = 0; n < I2C_DAC_CHANNEL_COUNT; ++n)
        dac_commands[n] = dac_channels[n].safe_code;
#endif
    for (unsigned i = 0; enabled && i < I2C_IO_BUS_COUNT; ++i) {
        enabled = buses[i].sda < NUM_BANK0_GPIOS && buses[i].scl < NUM_BANK0_GPIOS &&
                  !pin_reserved(buses[i].sda) && !pin_reserved(buses[i].scl);
    }
    if (!enabled) {
        printf("I2C peripherals: invalid configuration or GPIO conflict; no bus initialized\n");
        return;
    }
    for (unsigned i = 0; i < I2C_IO_BUS_COUNT; ++i) {
        i2c_init(port(i), buses[i].frequency_hz);
        gpio_set_function(buses[i].sda, GPIO_FUNC_I2C);
        gpio_set_function(buses[i].scl, GPIO_FUNC_I2C);
        gpio_pull_up(buses[i].sda);
        gpio_pull_up(buses[i].scl);
    }
}

#if I2C_DAC_ENABLE
static bool dac_write(unsigned n, uint32_t code)
{
    const sn_i2c_dac_channel *d = &dac_channels[n];
    uint8_t bytes[3];
    unsigned size = sn_i2c_dac_encode(d->type, d->channel, code, bytes);
    return size && i2c_write_timeout_us(port(d->bus), d->address, bytes, size,
                                       false, buses[d->bus].timeout_us) == (int)size;
}

void sn_i2c_dac_commands(const uint32_t codes[I2C_DAC_CHANNEL_COUNT])
{
    critical_section_enter_blocking(&snapshot_lock);
    for (unsigned n = 0; n < I2C_DAC_CHANNEL_COUNT; ++n) dac_commands[n] = codes[n];
    critical_section_exit(&snapshot_lock);
}
#endif

/* Called by the main loop on core 1 only; the bus has a single runtime owner. */
void sn_i2c_io_poll(bool connected)
{
    if (!enabled) return;
    uint64_t now = time_us_64();
#if I2C_IO_ENABLE
    uint32_t command;
#endif
#if I2C_DAC_ENABLE
    uint32_t dac_command[I2C_DAC_CHANNEL_COUNT];
#endif
    critical_section_enter_blocking(&snapshot_lock);
#if I2C_IO_ENABLE
    command = commands;
#endif
#if I2C_DAC_ENABLE
    for (unsigned n = 0; n < I2C_DAC_CHANNEL_COUNT; ++n) dac_command[n] = dac_commands[n];
#endif
    critical_section_exit(&snapshot_lock);
    bool disconnect = was_connected && !connected;
    was_connected = connected;
    #if I2C_IO_ENABLE
    for (unsigned n = 0; n < I2C_IO_DEVICE_COUNT; ++n) {
        if (!disconnect && now < state[n].next_us) continue;
        state[n].next_us = now + (uint64_t)devices[n].poll_ms * 1000;
        bool ok = state[n].configured || configure(n);
        state[n].configured = ok;
        uint16_t value = connected ? (command >> (16 * n)) : devices[n].safe_outputs;
        if (ok) ok = write_pair(n, output_reg(n), physical_outputs(n, value));
        uint8_t lo = 0, hi = 0;
        if (ok) ok = read_reg(n, input_reg(n), &lo);
        if (ok && sn_i2c_io_width(&devices[n]) == 16)
            ok = read_reg(n, input_reg(n) + 1, &hi);
        state[n].online = state[n].valid = ok;
        if (ok) state[n].sample = ((lo | ((uint16_t)hi << 8)) ^ devices[n].input_invert) & devices[n].direction;
        else state[n].configured = false; // retry full configuration at next poll
    }
    #endif
    #if I2C_DAC_ENABLE
    for (unsigned n = 0; n < I2C_DAC_CHANNEL_COUNT; ++n) {
        if (!disconnect && now < dac_state[n].next_us) continue;
        dac_state[n].next_us = now + (uint64_t)dac_channels[n].poll_ms * 1000;
        // Always establish the configured startup level before accepting commands.
        bool ok = dac_state[n].initialized || dac_write(n, dac_channels[n].safe_code);
        dac_state[n].initialized = ok;
        uint32_t code = connected ? dac_command[n] : dac_channels[n].safe_code;
        if (ok) ok = dac_write(n, code);
        dac_state[n].valid = ok;
        if (!ok) dac_state[n].initialized = false;
    }
    #endif
    uint32_t inputs = 0, status = 0;
    #if I2C_IO_ENABLE
    for (unsigned n = 0; n < I2C_IO_DEVICE_COUNT; ++n) {
        inputs |= (uint32_t)state[n].sample << (16 * n);
        if (state[n].online) status |= 1u << n;
        if (state[n].valid) status |= 1u << (n + 8);
    }
    #endif
    #if I2C_DAC_ENABLE
    for (unsigned n = 0; n < I2C_DAC_CHANNEL_COUNT; ++n)
        if (dac_state[n].valid) status |= (1u << (16 + n)) | (1u << (24 + n));
    #endif
    critical_section_enter_blocking(&snapshot_lock);
    published_inputs = inputs;
    published_status = status;
    critical_section_exit(&snapshot_lock);
}

/* Packet path copies a bounded snapshot, never touches I2C. */
void sn_i2c_io_packet(uint32_t command, uint32_t *inputs, uint32_t *status)
{
    critical_section_enter_blocking(&snapshot_lock);
    commands = command;
    *inputs = published_inputs;
    *status = published_status;
    critical_section_exit(&snapshot_lock);
}
#endif
