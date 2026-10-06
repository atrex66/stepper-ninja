/* SPDX-License-Identifier: MIT
 * LinuxCNC HAL API adapter for the separately obtained Stepper-Ninja driver.
 * Copyright (c) 2026 Frederic Müller
 */
#ifndef STEPPER_NINJA_HAL_COMPAT_H
#define STEPPER_NINJA_HAL_COMPAT_H

#include "hal.h"

#if defined(HAL_API_VERSION) && HAL_API_VERSION >= 1
typedef hal_bool_t sn_bit_pin;
typedef hal_sint_t sn_s32_pin;
typedef hal_uint_t sn_u32_pin;
typedef hal_real_t sn_float_pin;
typedef hal_pdir_t sn_pin_dir;

#define sn_get_bit(pin) hal_get_bool(pin)
#define sn_get_s32(pin) hal_get_si32(pin)
#define sn_get_u32(pin) hal_get_ui32(pin)
#define sn_get_float(pin) hal_get_real(pin)
#define sn_set_bit(pin, value) hal_set_bool((pin), (value))
#define sn_set_s32(pin, value) hal_set_si32((pin), (value))
#define sn_set_u32(pin, value) hal_set_ui32((pin), (value))
#define sn_set_float(pin, value) hal_set_real((pin), (value))

/* The si32/ui32 API deliberately retains the old 32-bit value semantics. */
#define SN_NEW_PIN(kind, api) \
    static inline int sn_new_##kind(sn_pin_dir dir, sn_##kind##_pin *pin, \
                                   int comp_id, const char *name) \
    { return hal_pin_new_##api(comp_id, dir, pin, 0, "%s", name); }
SN_NEW_PIN(bit, bool)
SN_NEW_PIN(s32, si32)
SN_NEW_PIN(u32, ui32)
SN_NEW_PIN(float, real)
#undef SN_NEW_PIN

static inline int sn_export_funct(const char *name, void (*funct)(void *, long),
                                 void *arg, int uses_fp, int reentrant, int comp_id)
{
    (void)uses_fp;
    return hal_export_funct(name, funct, arg, reentrant, comp_id);
}
#else
typedef hal_bit_t *sn_bit_pin;
typedef hal_s32_t *sn_s32_pin;
typedef hal_u32_t *sn_u32_pin;
typedef hal_float_t *sn_float_pin;
typedef hal_pin_dir_t sn_pin_dir;

#define sn_get_bit(pin) (*(pin))
#define sn_get_s32(pin) (*(pin))
#define sn_get_u32(pin) (*(pin))
#define sn_get_float(pin) (*(pin))
#define sn_set_bit(pin, value) (*(pin) = (value))
#define sn_set_s32(pin, value) (*(pin) = (value))
#define sn_set_u32(pin, value) (*(pin) = (value))
#define sn_set_float(pin, value) (*(pin) = (value))

#define SN_NEW_PIN(kind) \
    static inline int sn_new_##kind(sn_pin_dir dir, sn_##kind##_pin *pin, \
                                   int comp_id, const char *name) \
    { return hal_pin_##kind##_newf(dir, pin, comp_id, "%s", name); }
SN_NEW_PIN(bit)
SN_NEW_PIN(s32)
SN_NEW_PIN(u32)
SN_NEW_PIN(float)
#undef SN_NEW_PIN

static inline int sn_export_funct(const char *name, void (*funct)(void *, long),
                                 void *arg, int uses_fp, int reentrant, int comp_id)
{
    return hal_export_funct(name, funct, arg, uses_fp, reentrant, comp_id);
}
#endif
#endif
