/* Exercise lubrication state transitions with real HAL headers, no live HAL. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "hal_compat.h"
#ifndef LUBRICATION_SOURCE
#define LUBRICATION_SOURCE "lubrication-guard.c"
#endif
#include LUBRICATION_SOURCE
#define TEST_MODULE_NAME "lubrication-guard"
#include "hal_test_runtime.h"

static void tick(long period) { test_callback(test_callback_arg, period); }
static void expect(unsigned state, int motor, int busy, int fault, int timeout)
{
    assert(sn_get_u32(hal_data->state_dbg_out) == state);
    assert(sn_get_bit(hal_data->motor_out) == motor);
    assert(sn_get_bit(hal_data->busy_out) == busy);
    assert(sn_get_bit(hal_data->fault_out) == fault);
    assert(sn_get_bit(hal_data->pressure_timeout_out) == timeout);
}

int main(int argc, char **argv)
{
    if (argc == 2) {
        if (!strcmp(argv[1], "export")) test_fail_export = 1;
        else if (!strcmp(argv[1], "ready")) test_fail_ready = 1;
        else test_fail_pin = atoi(argv[1]);
    }
    int result = rtapi_app_main();
    if (argc == 2) {
        assert(result == (test_fail_pin >= 0 ? -ENOMEM : -EINVAL));
        assert(test_exit_count == 1);
        free(test_storage);
        return 0;
    }
    assert(result == 0 && test_pin_count == 13 && test_export_count == 1);
    expect(0, 0, 0, 0, 0);
    assert(sn_get_float(hal_data->hold_seconds) == 1.0);
    assert(sn_get_float(hal_data->timeout_seconds) == 5.0);
    assert(!sn_get_bit(inv_enable) && !sn_get_bit(inv_pressure_ok) && !sn_get_bit(inv_fault_reset));
    test_snapshot("defaults");

    sn_set_bit(hal_data->enable_in, 1);
    tick(250000000); expect(1, 0, 0, 0, 0);
    tick(250000000); expect(1, 1, 1, 0, 0);
    sn_set_bit(hal_data->pressure_ok_in, 1);
    tick(250000000); expect(2, 1, 1, 0, 0);
    /* A running hold completes even if the enable input is removed. */
    sn_set_bit(hal_data->enable_in, 0);
    tick(500000000); expect(2, 1, 1, 0, 0);
    tick(500000000); expect(0, 0, 0, 0, 0);
    test_snapshot("completed-hold");

    sn_set_bit(hal_data->pressure_ok_in, 0);
    sn_set_bit(hal_data->enable_in, 1);
    sn_set_float(hal_data->timeout_seconds, 0.5);
    tick(250000000); expect(1, 0, 0, 0, 0);
    tick(250000000); expect(1, 1, 1, 0, 0);
    tick(250000000); expect(0, 0, 0, 1, 1);
    tick(250000000); expect(0, 0, 0, 1, 1);
    test_snapshot("latched-timeout");
    sn_set_bit(hal_data->enable_in, 0);
    sn_set_bit(hal_data->fault_reset_in, 1);
    tick(250000000); expect(0, 0, 0, 0, 0);
    test_snapshot("reset");

    sn_set_bit(inv_enable, 1);
    sn_set_bit(inv_pressure_ok, 1);
    sn_set_bit(inv_fault_reset, 1);
    /* Raw enable/pressure=0 and reset=1 become active/active/inactive. */
    tick(250000000); expect(1, 0, 0, 0, 0);
    tick(250000000); expect(2, 1, 1, 0, 0);
    tick(500000000); expect(2, 1, 1, 0, 0);
    tick(500000000); expect(0, 0, 0, 0, 0);
    sn_set_bit(hal_data->fault_out, 1);
    sn_set_bit(hal_data->pressure_timeout_out, 1);
    sn_set_bit(hal_data->enable_in, 1);
    sn_set_bit(hal_data->fault_reset_in, 0);
    tick(250000000); expect(0, 0, 0, 0, 0);
    test_snapshot("inverted-inputs-and-reset");

    sn_set_bit(inv_enable, 0);
    sn_set_bit(inv_pressure_ok, 0);
    sn_set_bit(inv_fault_reset, 0);
    sn_set_float(hal_data->hold_seconds, -1);
    sn_set_float(hal_data->timeout_seconds, -1);
    tick(0); expect(1, 0, 0, 0, 0);
    tick(0); expect(0, 0, 0, 1, 1);
    sn_set_bit(hal_data->enable_in, 0);
    sn_set_bit(hal_data->fault_reset_in, 1);
    tick(0); expect(0, 0, 0, 0, 0);
    sn_set_bit(hal_data->fault_reset_in, 0);
    sn_set_bit(hal_data->enable_in, 1);
    sn_set_bit(hal_data->pressure_ok_in, 1);
    tick(0); expect(1, 0, 0, 0, 0);
    tick(0); expect(2, 1, 1, 0, 0);
    tick(0); expect(0, 0, 0, 0, 0);
    test_snapshot("clamped-negative-times");
    rtapi_app_exit();
    assert(test_exit_count == 1);
    free(test_storage);
    return 0;
}
