#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_errno.h"
#include "hal.h"

#include <stdio.h>
#include <string.h>

#define module_name "cycle-start-guard"

/*
 * Preconfigured number of blocking inputs.
 * Increase/decrease and rebuild the module as needed.
 */
#define BLOCK_INPUT_COUNT 8

/* module information */
MODULE_AUTHOR("Viola Zsolt");
MODULE_DESCRIPTION(module_name " HAL driver");
MODULE_LICENSE("MIT");

#if (BLOCK_INPUT_COUNT <= 0)
#error "BLOCK_INPUT_COUNT must be greater than 0"
#endif

typedef struct {
    hal_bit_t *cycle_start_in;
    hal_bit_t *cycle_start_out;
    hal_bit_t *cycle_start_invert;

    hal_bit_t *any_blocked_out;
    hal_u32_t *active_blocking_index_out; /* 1..N when blocked, 0 otherwise */

    hal_bit_t *block_in[BLOCK_INPUT_COUNT];
    hal_bit_t *block_invert[BLOCK_INPUT_COUNT];
    hal_u32_t *message_id[BLOCK_INPUT_COUNT];
} module_data_t;

static int comp_id = -1;
static module_data_t *hal_data = 0;

/*
 * Custom messages mapped by message ID.
 * You can edit these strings to fit your machine alarms.
 */
static const char *k_error_messages[] = {
    "unused",
    "Hidraulikanyomás alacsony!",
    "Kenőolaj szint alacsony!",
    "Tokmány szorítás hiba!",
    "A Gép ajtó nyitva!",
    "Kenés nyomás hiba!",
    "Hűtés hiba!",
    "Szerszámcsere hiba!",
    "Szegnyereg nyomás hiba!",
    "Felhasználói zárolás 1!",
    "Felhasználói zárolás 2!",
    "Felhasználói zárolás 3!",
    "Felhasználói zárolás 4!"
};

#define ERROR_MESSAGE_COUNT (sizeof(k_error_messages) / sizeof(k_error_messages[0]))

static uint8_t last_cycle_start_state = 0;

static const char *resolve_message(hal_u32_t id) {
    if (id > 0 && id < ERROR_MESSAGE_COUNT) {
        return k_error_messages[id];
    }
    return "Cycle start blocked: unknown reason";
}

static int read_inverted_bit(hal_bit_t *value_pin, hal_bit_t *invert_pin) {
    int value = (*value_pin != 0);
    int invert = (*invert_pin != 0);
    return invert ? !value : value;
}

static void process(void *arg, long period) {
    module_data_t *d = (module_data_t *)arg;
    int i;
    int blocked_index = -1;
    int cycle_start_val = read_inverted_bit(d->cycle_start_in, d->cycle_start_invert);

    for (i = 0; i < BLOCK_INPUT_COUNT; i++) {
        if (read_inverted_bit(d->block_in[i], d->block_invert[i])) {
            blocked_index = i;
            break;
        }
    }

    if (blocked_index >= 0) {
        hal_u32_t msg_id = *d->message_id[blocked_index];

        *d->cycle_start_out = 0;
        *d->any_blocked_out = 1;
        *d->active_blocking_index_out = (hal_u32_t)(blocked_index + 1);

        if (cycle_start_val && !last_cycle_start_state) {
            rtapi_print_msg(
                RTAPI_MSG_ERR,
                "%s\n",
                resolve_message(msg_id));
        }

        last_cycle_start_state = cycle_start_val;
        return;
    }

    *d->cycle_start_out = cycle_start_val ? 1 : 0;
    *d->any_blocked_out = 0;
    *d->active_blocking_index_out = 0;

    last_cycle_start_state = cycle_start_val;
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

static int create_pins(void) {
    int r;
    int i;
    char pin_name[96];

    r = create_bit_pin(&hal_data->cycle_start_in, HAL_IN, "inputs.cycle-start");
    if (r < 0) return r;

    r = create_bit_pin(&hal_data->cycle_start_out, HAL_OUT, "outputs.cycle-start");
    if (r < 0) return r;

    r = create_bit_pin(&hal_data->cycle_start_invert, HAL_IN, "params.invert-cycle-start");
    if (r < 0) return r;

    *hal_data->cycle_start_invert = 0;

    r = create_bit_pin(&hal_data->any_blocked_out, HAL_OUT, "outputs.blocked");
    if (r < 0) return r;

    r = create_u32_pin(&hal_data->active_blocking_index_out, HAL_OUT, "outputs.active-blocking-index");
    if (r < 0) return r;

    for (i = 0; i < BLOCK_INPUT_COUNT; i++) {
        memset(pin_name, 0, sizeof(pin_name));
        snprintf(pin_name, sizeof(pin_name), "inputs.block-%02d", i + 1);
        r = create_bit_pin(&hal_data->block_in[i], HAL_IN, pin_name);
        if (r < 0) return r;

        memset(pin_name, 0, sizeof(pin_name));
        snprintf(pin_name, sizeof(pin_name), "params.message-id-%02d", i + 1);
        r = create_u32_pin(&hal_data->message_id[i], HAL_IN, pin_name);
        if (r < 0) return r;

        memset(pin_name, 0, sizeof(pin_name));
        snprintf(pin_name, sizeof(pin_name), "params.invert-block-%02d", i + 1);
        r = create_bit_pin(&hal_data->block_invert[i], HAL_IN, pin_name);
        if (r < 0) return r;

        *hal_data->message_id[i] = (hal_u32_t)(i + 1);
        *hal_data->block_invert[i] = 0;
    }

    *hal_data->cycle_start_out = 0;
    *hal_data->any_blocked_out = 0;
    *hal_data->active_blocking_index_out = 0;

    return 0;
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
    last_cycle_start_state = 0;

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

    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": loaded with BLOCK_INPUT_COUNT=%d\n", BLOCK_INPUT_COUNT);
    return 0;
}

void rtapi_app_exit(void) {
    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": unloading\n");
    if (comp_id >= 0) {
        hal_exit(comp_id);
    }
}
