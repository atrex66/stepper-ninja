#include "rtapi.h"              /* RTAPI realtime OS API */
#include "rtapi_app.h"          /* RTAPI realtime module decls */
#include "rtapi_errno.h"        /* EINVAL etc */
#include "hal_compat.h"                /* HAL public API decls */

#include <string.h>
#include <stdio.h>

void create_bit(sn_bit_pin *pin, sn_pin_dir direction, const char *name) {
    char nm[64] = {0};
    int r;
    memset(nm, 0, sizeof(nm));
    snprintf(nm, sizeof(nm), module_name ".%s", name);
    r = sn_new_bit(direction, pin, comp_id, nm);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin %s export failed with err=%i\n", 0, name, r);
        hal_exit(comp_id);
        return;
    }
}

void create_s32(sn_s32_pin *pin, sn_pin_dir direction, const char *name) {
    char nm[64] = {0};
    int r;
    memset(nm, 0, sizeof(nm));
    snprintf(nm, sizeof(nm), module_name ".%s", name);
    r = sn_new_s32(direction, pin, comp_id, nm);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin %s export failed with err=%i\n", 0, name, r);
        hal_exit(comp_id);
        return;
    }
}

void create_u32(sn_u32_pin *pin, sn_pin_dir direction, const char *name) {
    char nm[64] = {0};
    int r;
    memset(nm, 0, sizeof(nm));
    snprintf(nm, sizeof(nm), module_name ".%s", name);
    r = sn_new_u32(direction, pin, comp_id, nm);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin %s export failed with err=%i\n", 0, name, r);
        hal_exit(comp_id);
        return;
    }
}

void create_float(sn_float_pin *pin, sn_pin_dir direction, const char *name) {
    char nm[64] = {0};
    int r;
    memset(nm, 0, sizeof(nm));
    snprintf(nm, sizeof(nm), module_name ".%s", name);
    r = sn_new_float(direction, pin, comp_id, nm);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin %s export failed with err=%i\n", 0, name, r);
        hal_exit(comp_id);
        return;
    }
}

void create_process(const char *name, void (*func)(void *, long), void *arg) {
    char funct_name[64] = {0};
    int r;
    memset(funct_name, 0, sizeof(funct_name));
    snprintf(funct_name, sizeof(funct_name), module_name ".%s", name);
    r = sn_export_funct(funct_name, func, arg, 1, 0, comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_export_funct failed (%d)\n", r);
        hal_exit(comp_id);
        return;
    }
}
