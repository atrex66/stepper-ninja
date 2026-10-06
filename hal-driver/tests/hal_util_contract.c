/* hal_util.c is included by modules after defining module_name and comp_id. */
#include "rtapi.h"
#include "hal_util.h"
#define module_name "hal-util-test"
static int comp_id = 42;
#ifndef HAL_UTIL_SOURCE
#define HAL_UTIL_SOURCE "hal_util.c"
#endif
#include HAL_UTIL_SOURCE
#define TEST_MODULE_NAME module_name
#include "hal_test_runtime.h"

static void callback(void *arg, long period) { (void)period; ++*(int *)arg; }
int main(int argc, char **argv)
{
    sn_bit_pin bit = 0;
    sn_s32_pin sint = 0;
    sn_u32_pin uint = 0;
    sn_float_pin real = 0;
    int calls = 0;
    if (argc == 2) {
        if (!strcmp(argv[1], "export")) test_fail_export = 1;
        else test_fail_pin = atoi(argv[1]);
    }
    create_bit(&bit, HAL_IN, "bit");
    if (test_fail_pin == 0) goto failed;
    create_s32(&sint, HAL_OUT, "s32");
    if (test_fail_pin == 1) goto failed;
    create_u32(&uint, HAL_IO, "u32");
    if (test_fail_pin == 2) goto failed;
    create_float(&real, HAL_IN, "float");
    if (test_fail_pin == 3) goto failed;
    sn_set_bit(bit, 1);
    sn_set_s32(sint, INT32_MIN);
    sn_set_u32(uint, UINT32_MAX);
    sn_set_float(real, 1.25);
    assert(sn_get_bit(bit) == 1 && sn_get_s32(sint) == INT32_MIN);
    assert(sn_get_u32(uint) == UINT32_MAX && sn_get_float(real) == 1.25);
    create_process("process", callback, &calls);
    if (test_fail_export) goto failed;
    assert(test_pin_count == 4 && test_export_count == 1 && !test_exit_count);
    test_callback(test_callback_arg, 1000000);
    assert(calls == 1);
    test_snapshot("utility-pins");
    return 0;
failed:
    assert(test_exit_count == 1 && test_export_count == 0);
    return 0;
}
