/* Included by the custom-GPIO HAL module only. */
#if I2C_IO_ENABLE
static const sn_i2c_bus_config i2c_buses[] = I2C_IO_BUSES;
static const sn_i2c_device_config i2c_devices[] = I2C_IO_DEVICES;
_Static_assert(sizeof(i2c_buses) / sizeof(*i2c_buses) == I2C_IO_BUS_COUNT, "I2C bus count mismatch");
_Static_assert(sizeof(i2c_devices) / sizeof(*i2c_devices) == I2C_IO_DEVICE_COUNT, "I2C device count mismatch");

static int i2c_hal_setup(module_data_t *d, int j, int comp_id, char *name, uint32_t nsize)
{
    if (!sn_i2c_io_config_valid(i2c_buses, I2C_IO_BUS_COUNT, i2c_devices, I2C_IO_DEVICE_COUNT)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Invalid I2C IO configuration\n");
        return -EINVAL;
    }
    int r;
    for (unsigned n = 0; n < I2C_IO_DEVICE_COUNT; ++n) {
        snprintf(name, nsize, module_name ".%d.expander.%u.online", j, n);
        r = sn_new_bit(HAL_OUT, &d->i2c_online[n], comp_id, name);
        if (r < 0) return r;
        sn_set_bit(d->i2c_online[n], 0);
        snprintf(name, nsize, module_name ".%d.expander.%u.valid", j, n);
        r = sn_new_bit(HAL_OUT, &d->i2c_valid[n], comp_id, name);
        if (r < 0) return r;
        sn_set_bit(d->i2c_valid[n], 0);
        for (unsigned p = 0; p < sn_i2c_io_width(&i2c_devices[n]); ++p) {
            if (i2c_devices[n].direction & (1u << p)) {
                snprintf(name, nsize, module_name ".%d.expander.%u.input.%u", j, n, p);
                r = sn_new_bit(HAL_OUT, &d->i2c_input[n][p], comp_id, name);
                if (r < 0) return r;
                sn_set_bit(d->i2c_input[n][p], 0);
                snprintf(name, nsize, module_name ".%d.expander.%u.input.%u-not", j, n, p);
                r = sn_new_bit(HAL_OUT, &d->i2c_input_not[n][p], comp_id, name);
                if (r < 0) return r;
                sn_set_bit(d->i2c_input_not[n][p], 1);
            } else {
                snprintf(name, nsize, module_name ".%d.expander.%u.output.%u", j, n, p);
                r = sn_new_bit(HAL_IN, &d->i2c_output[n][p], comp_id, name);
                if (r < 0) return r;
                sn_set_bit(d->i2c_output[n][p], (i2c_devices[n].safe_outputs >> p) & 1);
            }
        }
    }
    return 0;
}

static void i2c_hal_recv(module_data_t *d)
{
    for (unsigned n = 0; n < I2C_IO_DEVICE_COUNT; ++n) {
        sn_set_bit(d->i2c_online[n], (rx_buffer->inputs[3] >> n) & 1);
        sn_set_bit(d->i2c_valid[n], (rx_buffer->inputs[3] >> (n + 8)) & 1);
        for (unsigned p = 0; p < sn_i2c_io_width(&i2c_devices[n]); ++p)
            if (i2c_devices[n].direction & (1u << p)) {
                unsigned value = (rx_buffer->inputs[2] >> (n * 16 + p)) & 1;
                sn_set_bit(d->i2c_input[n][p], value);
                sn_set_bit(d->i2c_input_not[n][p], !value);
            }
    }
}

static void i2c_hal_send(module_data_t *d)
{
    uint32_t word = 0;
    for (unsigned n = 0; n < I2C_IO_DEVICE_COUNT; ++n)
        for (unsigned p = 0; p < sn_i2c_io_width(&i2c_devices[n]); ++p)
            if (!(i2c_devices[n].direction & (1u << p)) && sn_get_bit(d->i2c_output[n][p]))
                word |= 1u << (n * 16 + p);
    tx_buffer->outputs[1] = word;
}
#endif
