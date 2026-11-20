#ifndef SN_I2C_DAC_CONFIG_H
#define SN_I2C_DAC_CONFIG_H
#include "i2c_io_config.h"

enum {
    SN_DAC_MCP4725, SN_DAC_DAC5571, SN_DAC_DAC6571, SN_DAC_DAC7571,
    SN_DAC_AD5337, SN_DAC_AD5338, SN_DAC_AD5339
};
#define SN_I2C_DAC_MAX_CHANNELS 4
typedef struct {
    uint8_t type, bus, address, channel;
    uint16_t safe_code;
    uint32_t poll_ms;
} sn_i2c_dac_channel;

static inline unsigned sn_i2c_dac_bits(unsigned type)
{
    switch (type) {
    case SN_DAC_DAC5571: case SN_DAC_AD5337: return 8;
    case SN_DAC_DAC6571: case SN_DAC_AD5338: return 10;
    default: return 12;
    }
}
static inline unsigned sn_i2c_dac_max(unsigned type)
{ return (1u << sn_i2c_dac_bits(type)) - 1; }

/* Native codes in, complete volatile-write payload out (address is separate).
   Never writes EEPROM, sends general-call commands, or globally clears a DAC. */
static inline unsigned sn_i2c_dac_encode(unsigned type, unsigned channel,
                                         uint32_t code, uint8_t bytes[3])
{
    if (type > SN_DAC_AD5339 || channel > (type >= SN_DAC_AD5337 ? 1u : 0u)) return 0;
    if (code > sn_i2c_dac_max(type)) code = sn_i2c_dac_max(type);
    if (type == SN_DAC_MCP4725) {
        bytes[0] = 0x40; // volatile DAC register, normal operation; no EEPROM
        bytes[1] = code >> 4;
        bytes[2] = (code & 0xf) << 4;
        return 3;
    }
    uint16_t word = code << (12 - sn_i2c_dac_bits(type));
    if (type >= SN_DAC_AD5337) {
        // PD=00, CLR=1, LDAC=0: normal operation and immediate update.
        word |= 0x2000;
        bytes[0] = 1u << channel; // DACA bit 0, DACB bit 1
        bytes[1] = word >> 8;
        bytes[2] = word & 0xff;
        return 3;
    }
    bytes[0] = word >> 8; // TI PD=00, data left-aligned within 12 data bits
    bytes[1] = word & 0xff;
    return 2;
}

static inline bool sn_i2c_dac_config_valid(const sn_i2c_bus_config *b, unsigned nb,
    const sn_i2c_dac_channel *d, unsigned nd,
    const sn_i2c_device_config *io, unsigned nio)
{
    if (!sn_i2c_buses_valid(b, nb) || !nd || nd > SN_I2C_DAC_MAX_CHANNELS) return false;
    for (unsigned i = 0; i < nd; ++i) {
        if (d[i].type > SN_DAC_AD5339 || d[i].bus >= nb ||
            d[i].channel > (d[i].type >= SN_DAC_AD5337 ? 1u : 0u) ||
            d[i].safe_code > sn_i2c_dac_max(d[i].type) ||
            !d[i].poll_ms || d[i].poll_ms > 60000) return false;
        unsigned address = d[i].address;
        if (d[i].type == SN_DAC_MCP4725) {
            if (address < 0x60 || address > 0x67) return false;
        } else if (d[i].type < SN_DAC_AD5337) {
            if (address != 0x4c && address != 0x4d) return false;
        } else if (address != 0x0c && address != 0x0d &&
                   !(d[i].type == SN_DAC_AD5338 && (address == 0x22 || address == 0x23))) return false;
        for (unsigned j = 0; j < nio; ++j)
            if (d[i].bus == io[j].bus && address == io[j].address) return false;
        for (unsigned j = 0; j < i; ++j) {
            if (d[i].bus != d[j].bus || address != d[j].address) continue;
            if (d[i].type != d[j].type || d[i].channel == d[j].channel) return false;
            // Two channels of the same AD533x are allowed on one address.
        }
    }
    return true;
}
#endif
