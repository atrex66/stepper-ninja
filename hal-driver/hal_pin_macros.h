#ifndef HAL_PIN_MACROS_H
#define HAL_PIN_MACROS_H

#define PIN_BIT(ptr, dir, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = hal_pin_bit_newf(dir, ptr, comp_id, name, j); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
    } while(0)

#define PIN_BIT_INIT(ptr, dir, init_val, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = hal_pin_bit_newf(dir, ptr, comp_id, name, j); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
        **ptr = init_val; \
    } while(0)

#define PIN_S32(ptr, dir, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = hal_pin_s32_newf(dir, ptr, comp_id, name, j); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
    } while(0)

#define PIN_S32_INIT(ptr, dir, init_val, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = hal_pin_s32_newf(dir, ptr, comp_id, name, j); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
        **ptr = init_val; \
    } while(0)

#define PIN_U32(ptr, dir, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = hal_pin_u32_newf(dir, ptr, comp_id, name, j); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
    } while(0)

#define PIN_U32_INIT(ptr, dir, init_val, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = hal_pin_u32_newf(dir, ptr, comp_id, name, j); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
        **ptr = init_val; \
    } while(0)

#define PIN_FLOAT(ptr, dir, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = hal_pin_float_newf(dir, ptr, comp_id, name, j); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
    } while(0)

#define PIN_FLOAT_INIT(ptr, dir, init_val, fmt, ...) \
    do { \
        memset(name, 0, nsize); \
        snprintf(name, nsize, fmt, ##__VA_ARGS__); \
        r = hal_pin_float_newf(dir, ptr, comp_id, name, j); \
        if (r < 0) { \
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin export failed with err=%i\n", j, r); \
            hal_exit(comp_id); return r; \
        } \
        **ptr = init_val; \
    } while(0)

#endif
