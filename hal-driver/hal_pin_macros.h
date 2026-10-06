#ifndef HAL_PIN_MACROS_H
#define HAL_PIN_MACROS_H

#include "hal_compat.h"

#define PIN_BIT(ptr, dir, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = sn_new_bit(dir, ptr, comp_id, name); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
    } while(0)

#define PIN_BIT_INIT(ptr, dir, init_val, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = sn_new_bit(dir, ptr, comp_id, name); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
        sn_set_bit(*(ptr), (init_val)); \
    } while(0)

#define PIN_S32(ptr, dir, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = sn_new_s32(dir, ptr, comp_id, name); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
    } while(0)

#define PIN_S32_INIT(ptr, dir, init_val, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = sn_new_s32(dir, ptr, comp_id, name); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
        sn_set_s32(*(ptr), (init_val)); \
    } while(0)

#define PIN_U32(ptr, dir, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = sn_new_u32(dir, ptr, comp_id, name); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
    } while(0)

#define PIN_U32_INIT(ptr, dir, init_val, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = sn_new_u32(dir, ptr, comp_id, name); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
        sn_set_u32(*(ptr), (init_val)); \
    } while(0)

#define PIN_FLOAT(ptr, dir, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = sn_new_float(dir, ptr, comp_id, name); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
    } while(0)

#define PIN_FLOAT_INIT(ptr, dir, init_val, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = sn_new_float(dir, ptr, comp_id, name); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
        sn_set_float(*(ptr), (init_val)); \
    } while(0)

#endif
