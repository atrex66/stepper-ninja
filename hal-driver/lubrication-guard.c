#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_errno.h"
#include "hal.h"

#include <stdio.h>
#include <string.h>

#define module_name "lubrication-guard"

/* module information */
MODULE_AUTHOR("Viola Zsolt");
MODULE_DESCRIPTION(module_name " HAL driver");
MODULE_LICENSE("MIT");

typedef struct {
    /* inputs */
    hal_bit_t *enable_in;            /* request lubrication cycle */
    hal_bit_t *pressure_ok_in;       /* pressure switch */
    hal_bit_t *fault_reset_in;       /* reset latched fault */

    /* outputs */
    hal_bit_t *motor_out;            /* lubrication motor command */
    hal_bit_t *fault_out;            /* latched fault */
    hal_bit_t *pressure_timeout_out; /* timeout event status */
    hal_bit_t *busy_out;             /* cycle active */

    /* parameters */
    hal_float_t *hold_seconds;       /* lubrication hold time after pressure ok */
    hal_float_t *timeout_seconds;    /* pressure wait timeout */

    /* internal state */
    hal_u32_t *state_dbg_out;        /* 0=IDLE,1=WAIT_PRESSURE,2=HOLD */
} module_data_t;

static int comp_id = -1;
static module_data_t *hal_data = 0;

typedef enum {
    LUBE_IDLE = 0,
    LUBE_WAIT_PRESSURE = 1,
    LUBE_HOLD = 2
} lube_state_t;

static lube_state_t lube_state = LUBE_IDLE;
static double pressure_wait_elapsed_s = 0.0;
static double hold_elapsed_s = 0.0;

/* Optional per-signal inversion like cycle-start-guard style */
static hal_bit_t *inv_enable = 0;
static hal_bit_t *inv_pressure_ok = 0;
static hal_bit_t *inv_fault_reset = 0;

static int read_inverted_bit(hal_bit_t *value_pin, hal_bit_t *invert_pin) {
    int value = (*value_pin != 0);
    int invert = (*invert_pin != 0);
    return invert ? !value : value;
}

static int create_bit_pin(hal_bit_t **pin, int dir, const char *suffix) {
    int r;
    char name[96];

    memset(name, 0, sizeof(name));
    snprintf(name, sizeof(name), module_name ".%s", suffix);
    r = hal_pin_bit_newf(dir, pin, comp_id, name);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": failed to create pin %s (err=%d)\n", name, r);
        return r;
    }
    return 0;
}

static int create_u32_pin(hal_u32_t **pin, int dir, const char *suffix) {
    int r;
    char name[96];

    memset(name, 0, sizeof(name));
    snprintf(name, sizeof(name), module_name ".%s", suffix);
    r = hal_pin_u32_newf(dir, pin, comp_id, name);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": failed to create pin %s (err=%d)\n", name, r);
        return r;
    }
    return 0;
}

static int create_float_pin(hal_float_t **pin, int dir, const char *suffix) {
    int r;
    char name[96];

    memset(name, 0, sizeof(name));
    snprintf(name, sizeof(name), module_name ".%s", suffix);
    r = hal_pin_float_newf(dir, pin, comp_id, name);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": failed to create pin %s (err=%d)\n", name, r);
        return r;
    }
    return 0;
}

static int create_pins(void) {
    int r;

    r = create_bit_pin(&hal_data->enable_in, HAL_IN, "inputs.enable");
    if (r < 0) return r;

    r = create_bit_pin(&hal_data->pressure_ok_in, HAL_IN, "inputs.pressure-ok");
    if (r < 0) return r;

    r = create_bit_pin(&hal_data->fault_reset_in, HAL_IN, "inputs.reset-fault");
    if (r < 0) return r;

    r = create_bit_pin(&hal_data->motor_out, HAL_OUT, "outputs.motor");
    if (r < 0) return r;

    r = create_bit_pin(&hal_data->fault_out, HAL_OUT, "outputs.fault");
    if (r < 0) return r;

    r = create_bit_pin(&hal_data->pressure_timeout_out, HAL_OUT, "outputs.pressure-timeout");
    if (r < 0) return r;

    r = create_bit_pin(&hal_data->busy_out, HAL_OUT, "outputs.busy");
    if (r < 0) return r;

    r = create_u32_pin(&hal_data->state_dbg_out, HAL_OUT, "outputs.state");
    if (r < 0) return r;

    r = create_float_pin(&hal_data->hold_seconds, HAL_IN, "params.hold-seconds");
    if (r < 0) return r;

    r = create_float_pin(&hal_data->timeout_seconds, HAL_IN, "params.timeout-seconds");
    if (r < 0) return r;

    r = create_bit_pin(&inv_enable, HAL_IN, "params.invert-enable");
    if (r < 0) return r;

    r = create_bit_pin(&inv_pressure_ok, HAL_IN, "params.invert-pressure-ok");
    if (r < 0) return r;

    r = create_bit_pin(&inv_fault_reset, HAL_IN, "params.invert-reset-fault");
    if (r < 0) return r;

    /* defaults */
    *hal_data->motor_out = 0;
    *hal_data->fault_out = 0;
    *hal_data->pressure_timeout_out = 0;
    *hal_data->busy_out = 0;
    *hal_data->state_dbg_out = 0;

    *hal_data->hold_seconds = 1.0f;    /* default hold */
    *hal_data->timeout_seconds = 5.0f; /* default timeout */

    *inv_enable = 0;
    *inv_pressure_ok = 0;
    *inv_fault_reset = 0;

    return 0;
}

static void set_state(lube_state_t s) {
    lube_state = s;
    pressure_wait_elapsed_s = 0.0;
    hold_elapsed_s = 0.0;
}

static void process(void *arg, long period) {
    module_data_t *d = (module_data_t *)arg;

    const double dt_s = ((double)period) * 1e-9;
    int enable = read_inverted_bit(d->enable_in, inv_enable);
    int pressure_ok = read_inverted_bit(d->pressure_ok_in, inv_pressure_ok);
    int reset_fault = read_inverted_bit(d->fault_reset_in, inv_fault_reset);

    /* clamp parameters */
    double hold_s = (double)(*d->hold_seconds);
    double timeout_s = (double)(*d->timeout_seconds);
    if (hold_s < 0.0) hold_s = 0.0;
    if (timeout_s < 0.0) timeout_s = 0.0;

    /* reset latched fault only by explicit reset input */
    if (reset_fault) {
        *d->fault_out = 0;
        *d->pressure_timeout_out = 0;
        if (!enable) {
            set_state(LUBE_IDLE);
        }
    }

    /* Fault latched: force motor off until reset */
    if (*d->fault_out) {
        *d->motor_out = 0;
        *d->busy_out = 0;
        *d->state_dbg_out = (hal_u32_t)LUBE_IDLE;
        return;
    }

    switch (lube_state) {
        case LUBE_IDLE:
            *d->motor_out = 0;
            *d->busy_out = 0;

            if (enable) {
                *d->pressure_timeout_out = 0;
                set_state(LUBE_WAIT_PRESSURE);
            }
            break;

        case LUBE_WAIT_PRESSURE:
            *d->motor_out = 1;
            *d->busy_out = 1;

            if (pressure_ok) {
                set_state(LUBE_HOLD);
                break;
            }

            pressure_wait_elapsed_s += dt_s;
            if (pressure_wait_elapsed_s >= timeout_s) {
                *d->fault_out = 1;
                *d->pressure_timeout_out = 1;
                *d->motor_out = 0;
                *d->busy_out = 0;
                rtapi_print_msg(
                    RTAPI_MSG_ERR,
                    module_name ": pressure did not arrive before timeout (timeout=%.3fs)\n",
                    timeout_s);
                set_state(LUBE_IDLE);
            }
            break;

        case LUBE_HOLD:
            *d->motor_out = 1;
            *d->busy_out = 1;

            hold_elapsed_s += dt_s;
            if (hold_elapsed_s >= hold_s) {
                *d->motor_out = 0;
                *d->busy_out = 0;
                set_state(LUBE_IDLE);
            }
            break;

        default:
            set_state(LUBE_IDLE);
            *d->motor_out = 0;
            *d->busy_out = 0;
            break;
    }

    *d->state_dbg_out = (hal_u32_t)lube_state;
}

int rtapi_app_main(void) {
    int r;
    char funct_name[64];

    rtapi_set_msg_level(RTAPI_MSG_INFO);

    comp_id = hal_init(module_name);
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_init failed (%d)\n", comp_id);
        return comp_id;
    }

    hal_data = hal_malloc(sizeof(module_data_t));
    if (!hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_malloc failed\n");
        hal_exit(comp_id);
        return -ENOMEM;
    }

    memset(hal_data, 0, sizeof(module_data_t));
    set_state(LUBE_IDLE);

    r = create_pins();
    if (r < 0) {
        hal_exit(comp_id);
        return r;
    }

    memset(funct_name, 0, sizeof(funct_name));
    snprintf(funct_name, sizeof(funct_name), module_name ".process");
    r = hal_export_funct(funct_name, process, hal_data, 1, 0, comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_export_funct failed (%d)\n", r);
        hal_exit(comp_id);
        return r;
    }

    r = hal_ready(comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_ready failed (%d)\n", r);
        hal_exit(comp_id);
        return r;
    }

    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": loaded\n");
    return 0;
}

void rtapi_app_exit(void) {
    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": unloading\n");
    if (comp_id >= 0) {
        hal_exit(comp_id);
    }
}
