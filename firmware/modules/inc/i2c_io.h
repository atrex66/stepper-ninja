#ifndef SN_I2C_IO_H
#define SN_I2C_IO_H
#include "config.h"
#if I2C_IO_ENABLE
void sn_i2c_io_setup(void);
void sn_i2c_io_poll(bool connected);
void sn_i2c_io_packet(uint32_t command, uint32_t *inputs, uint32_t *status);
#endif
#endif
