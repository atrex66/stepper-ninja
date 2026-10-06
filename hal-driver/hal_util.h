#ifndef HAL_UTIL_H
#define HAL_UTIL_H
#include "hal_compat.h"                /* HAL public API decls */

void create_bit(sn_bit_pin *pin, sn_pin_dir direction, const char *name);
void create_s32(sn_s32_pin *pin, sn_pin_dir direction, const char *name);
void create_u32(sn_u32_pin *pin, sn_pin_dir direction, const char *name);
void create_float(sn_float_pin *pin, sn_pin_dir direction, const char *name);
void create_process(const char *name, void (*func)(void *, long), void *arg);
#endif