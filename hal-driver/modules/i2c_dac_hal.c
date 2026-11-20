/* Included by the custom-board HAL module, independently of digital expanders. */
#if I2C_DAC_ENABLE
static const sn_i2c_bus_config dac_buses[] = I2C_IO_BUSES;
static const sn_i2c_dac_channel dac_config[] = I2C_DAC_CHANNELS;
#if I2C_IO_ENABLE
static const sn_i2c_device_config dac_io_config[] = I2C_IO_DEVICES;
#endif
_Static_assert(sizeof(dac_buses) / sizeof(*dac_buses) == I2C_IO_BUS_COUNT, "I2C bus count mismatch");
_Static_assert(sizeof(dac_config) / sizeof(*dac_config) == I2C_DAC_CHANNEL_COUNT,
               "I2C DAC channel count mismatch");

static int dac_hal_setup(module_data_t *d, int j, int comp_id, char *name, uint32_t nsize)
{
    if (!sn_i2c_dac_config_valid(dac_buses, I2C_IO_BUS_COUNT,
        dac_config, I2C_DAC_CHANNEL_COUNT,
#if I2C_IO_ENABLE
        dac_io_config, I2C_IO_DEVICE_COUNT)) {
#else
        NULL, 0)) {
#endif
        rtapi_print_msg(RTAPI_MSG_ERR, "Invalid I2C DAC configuration\n");
        return -EINVAL;
    }
    int r;
    for (unsigned n = 0; n < I2C_DAC_CHANNEL_COUNT; ++n) {
#define DAC_PIN(kind, member, direction, suffix, initial) do { \
        snprintf(name, nsize, module_name ".%d.dac.%u." suffix, j, n); \
        r = sn_new_##kind(direction, &d->member[n], comp_id, name); \
        if (r < 0) return r; \
        sn_set_##kind(d->member[n], initial); \
    } while (0)
        DAC_PIN(bit, dac_enable, HAL_IN, "enable", 0);
        DAC_PIN(float, dac_value, HAL_IN, "value", 0.0);
        DAC_PIN(float, dac_scale, HAL_IN, "scale", 1.0);
        DAC_PIN(u32, dac_raw, HAL_OUT, "raw-command", dac_config[n].safe_code);
        DAC_PIN(bit, dac_online, HAL_OUT, "online", 0);
        DAC_PIN(bit, dac_valid, HAL_OUT, "valid", 0);
#undef DAC_PIN
    }
    return 0;
}

static void dac_hal_send(module_data_t *d)
{
    for (unsigned n = 0; n < I2C_DAC_CHANNEL_COUNT; ++n) {
        double scale = sn_get_float(d->dac_scale[n]);
        double value = sn_get_float(d->dac_value[n]);
        unsigned code = dac_config[n].safe_code;
        if (sn_get_bit(d->dac_enable[n]) && isfinite(scale) && scale > 0 && isfinite(value)) {
            if (value <= 0) code = 0;
            else if (value >= scale) code = sn_i2c_dac_max(dac_config[n].type);
            else code = (unsigned)((value / scale) * sn_i2c_dac_max(dac_config[n].type) + 0.5);
        }
        tx_buffer->analog_out[n] = code;
        sn_set_u32(d->dac_raw[n], code);
    }
}

static void dac_hal_recv(module_data_t *d)
{
    for (unsigned n = 0; n < I2C_DAC_CHANNEL_COUNT; ++n) {
        sn_set_bit(d->dac_online[n], (rx_buffer->inputs[3] >> (16 + n)) & 1);
        sn_set_bit(d->dac_valid[n], (rx_buffer->inputs[3] >> (24 + n)) & 1);
    }
}
#endif
