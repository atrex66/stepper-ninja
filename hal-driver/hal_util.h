#ifndef HAL_UTIL_H
#define HAL_UTIL_H
#include "hal.h"                /* HAL public API decls */

void create_bit(hal_bit_t **pin, int direction, const char *name);
void create_s32(hal_s32_t **pin, int direction, const char *name);
void create_u32(hal_u32_t **pin, int direction, const char *name);
void create_float(hal_float_t **pin, int direction, const char *name);
void create_process(const char *name, void (*func)(void *, long), void *arg);
#endif