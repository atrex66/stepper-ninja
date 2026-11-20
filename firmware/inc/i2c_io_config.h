#ifndef SN_I2C_IO_CONFIG_H
#define SN_I2C_IO_CONFIG_H
#include <stdbool.h>
#include <stdint.h>

enum { SN_IO_MCP23008, SN_IO_MCP23017, SN_IO_TCA9535, SN_IO_TCA9555 };
typedef struct {
    uint8_t controller, sda, scl;
    uint32_t frequency_hz, timeout_us;
} sn_i2c_bus_config;
typedef struct {
    uint8_t type, bus, address;
    uint16_t direction, pullup, input_invert, output_invert, safe_outputs;
    uint32_t poll_ms;
} sn_i2c_device_config;

/* Two fixed 16-bit slots: inputs[2], outputs[1]. Status in inputs[3].
   No packet layout changes, and no overlap with native GPIO input banks. */
#define SN_I2C_IO_MAX_DEVICES 2
static inline unsigned sn_i2c_io_width(const sn_i2c_device_config *d)
{ return d->type == SN_IO_MCP23008 ? 8 : 16; }

static inline bool sn_i2c_buses_valid(const sn_i2c_bus_config *b, unsigned nb)
{
    if (!nb || nb > 2) return false;
    for (unsigned i = 0; i < nb; ++i) {
        if (b[i].controller > 1 || (b[i].sda & 3) != 2 * b[i].controller ||
            (b[i].scl & 3) != 2 * b[i].controller + 1 ||
            b[i].sda == b[i].scl || !b[i].frequency_hz ||
            b[i].frequency_hz > 400000 || !b[i].timeout_us ||
            b[i].timeout_us > 10000) return false;
        for (unsigned j = 0; j < i; ++j)
            if (b[i].controller == b[j].controller || b[i].sda == b[j].sda ||
                b[i].sda == b[j].scl || b[i].scl == b[j].sda ||
                b[i].scl == b[j].scl) return false;
    }
    return true;
}

static inline bool sn_i2c_io_config_valid(const sn_i2c_bus_config *b, unsigned nb,
                                        const sn_i2c_device_config *d, unsigned nd)
{
    if (!sn_i2c_buses_valid(b, nb) || !nd || nd > SN_I2C_IO_MAX_DEVICES) return false;
    for (unsigned i = 0; i < nd; ++i) {
        if (d[i].type > SN_IO_TCA9555 || d[i].bus >= nb ||
            d[i].address < 0x20 || d[i].address > 0x27 ||
            !d[i].poll_ms || d[i].poll_ms > 60000) return false;
        unsigned mask = sn_i2c_io_width(&d[i]) == 8 ? 0xff : 0xffff;
        if ((d[i].direction | d[i].pullup | d[i].input_invert |
             d[i].output_invert | d[i].safe_outputs) & ~mask) return false;
        if (d[i].pullup & ~d[i].direction) return false;
        if (d[i].input_invert & ~d[i].direction) return false;
        if ((d[i].safe_outputs | d[i].output_invert) & d[i].direction) return false;
        if (d[i].type >= SN_IO_TCA9535 && d[i].pullup) return false;
        /* Current Microchip datasheet: GPA7/GPB7 are output-only. */
        if (d[i].type == SN_IO_MCP23017 && (d[i].direction & 0x8080)) return false;
        for (unsigned j = 0; j < i; ++j)
            if (d[i].bus == d[j].bus && d[i].address == d[j].address) return false;
    }
    return true;
}
#endif
