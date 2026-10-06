/* Test-owned HAL storage with the selected installation's real HAL accessors. */
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hal_compat.h"

static struct {
    char name[128];
    int direction;
    char kind;
    union { bool bit; int32_t s32; uint32_t u32; double real; int64_t sint; uint64_t uint; } value;
} test_pins[32];
static int test_pin_count, test_exit_count, test_export_count;
static int test_fail_pin = -1, test_fail_export, test_fail_ready;
static void *test_storage, *test_callback_arg;
static void (*test_callback)(void *, long);

static int test_allocate_pin(int dir, int id, char kind, const char *fmt, va_list args)
{
    assert(id == 42 && test_pin_count < 32);
    if (test_pin_count == test_fail_pin) return -ENOMEM;
    test_pins[test_pin_count].direction = dir;
    test_pins[test_pin_count].kind = kind;
    vsnprintf(test_pins[test_pin_count].name, sizeof(test_pins[0].name), fmt, args);
    return test_pin_count++;
}
#if defined(HAL_API_VERSION) && HAL_API_VERSION >= 1
#define TEST_NEW_PIN(api, ref_type, value_type, member, kind) \
    int hal_pin_new_##api(int id, hal_pdir_t dir, ref_type *ref, value_type def, const char *fmt, ...) { \
        va_list args; va_start(args, fmt); int n = test_allocate_pin(dir, id, kind, fmt, args); va_end(args); \
        if (n < 0) return n; \
        test_pins[n].value.member = def; *ref = (ref_type)&test_pins[n].value; return 0; }
TEST_NEW_PIN(bool, hal_bool_t, rtapi_bool, uint, 'b')
TEST_NEW_PIN(si32, hal_sint_t, rtapi_s32, sint, 's')
TEST_NEW_PIN(ui32, hal_uint_t, rtapi_u32, uint, 'u')
TEST_NEW_PIN(real, hal_real_t, rtapi_real, real, 'f')
#else
#define TEST_NEW_PIN(api, value_type, member, kind) \
    int hal_pin_##api##_newf(hal_pin_dir_t dir, value_type **ref, int id, const char *fmt, ...) { \
        va_list args; va_start(args, fmt); int n = test_allocate_pin(dir, id, kind, fmt, args); va_end(args); \
        if (n < 0) return n; \
        *ref = (void *)&test_pins[n].value.member; return 0; }
TEST_NEW_PIN(bit, hal_bit_t, bit, 'b')
TEST_NEW_PIN(s32, hal_s32_t, s32, 's')
TEST_NEW_PIN(u32, hal_u32_t, u32, 'u')
TEST_NEW_PIN(float, hal_float_t, real, 'f')
#endif
#undef TEST_NEW_PIN

int hal_init(const char *name) { assert(strcmp(name, TEST_MODULE_NAME) == 0); return 42; }
void *hal_malloc(long size) { test_storage = calloc(1, (size_t)size); return test_storage; }
int hal_exit(int id) { assert(id == 42); ++test_exit_count; return 0; }
int hal_ready(int id) { assert(id == 42 && test_export_count == 1); return test_fail_ready ? -EINVAL : 0; }
void rtapi_print_msg(msg_level_t level, const char *fmt, ...) { (void)level; (void)fmt; }
int rtapi_set_msg_level(int level) { return level; }
#if defined(HAL_API_VERSION) && HAL_API_VERSION >= 1
int hal_export_funct(const char *name, void (*fn)(void *, long), void *arg, int reentrant, int id)
#else
int hal_export_funct(const char *name, void (*fn)(void *, long), void *arg, int uses_fp, int reentrant, int id)
#endif
{
#if !defined(HAL_API_VERSION) || HAL_API_VERSION < 1
    assert(uses_fp == 1);
#endif
    assert(fn && arg && reentrant == 0 && id == 42);
    if (test_fail_export) return -EINVAL;
    test_callback = fn; test_callback_arg = arg; ++test_export_count;
    printf("export %s %d\n", name, reentrant);
    return 0;
}

static void test_snapshot(const char *label)
{
    puts(label);
    for (int i = 0; i < test_pin_count; ++i) {
        printf("%s %d %c ", test_pins[i].name, test_pins[i].direction, test_pins[i].kind);
        switch (test_pins[i].kind) {
        case 'b': printf("%d", sn_get_bit((sn_bit_pin)&test_pins[i].value)); break;
        case 's': printf("%" PRId32, sn_get_s32((sn_s32_pin)&test_pins[i].value)); break;
        case 'u': printf("%" PRIu32, sn_get_u32((sn_u32_pin)&test_pins[i].value)); break;
        case 'f': printf("%a", sn_get_float((sn_float_pin)&test_pins[i].value)); break;
        }
        puts("");
    }
}
