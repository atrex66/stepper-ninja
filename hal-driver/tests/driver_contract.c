/* SPDX-License-Identifier: MIT
 * Adapted from FredericM88/linuxcnc-sim (MIT), Copyright (c) 2026 Frederic Müller.
 * Execute the complete driver with real HAL headers and test-only HAL/network
 * storage. No HAL module is loaded and no network packet leaves this process.
 */
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "hal_compat.h"

static ssize_t capture_send(int fd, const void *buffer, size_t size, int flags,
                            const void *address, socklen_t length)
{
    (void)fd; (void)flags; (void)address; (void)length;
    printf("wire %zu ", size);
    for (size_t i = 0; i < size; ++i) printf("%02x", ((const unsigned char *)buffer)[i]);
    puts("");
    return (ssize_t)size;
}
static unsigned char incoming[256];
static ssize_t incoming_size;
static ssize_t supply_recv(int fd, void *buffer, size_t size, int flags,
                           void *address, socklen_t *length)
{
    (void)fd; (void)flags; (void)address; (void)length;
    assert(incoming_size <= (ssize_t)size);
    if (incoming_size > 0) memcpy(buffer, incoming, (size_t)incoming_size);
    return incoming_size;
}
#define sendto capture_send
#define recvfrom supply_recv
#include DRIVER_SOURCE
#undef sendto
#undef recvfrom

/* Storage is test-owned. Only HAL's real inline accessors interpret API-1 refs. */
static struct {
    char name[128];
    int direction;
    char kind;
    union { bool bit; int32_t s32; uint32_t u32; double real; int64_t sint; uint64_t uint; } value;
} pins[128];
static int pin_count, export_count, exit_count;
static int fail_at = -1;
static void *component_storage;
static int allocate_pin(int dir, int id, char kind, const char *fmt, va_list args)
{
    assert(id == 42 && pin_count < 128);
    if (pin_count == fail_at) return -ENOMEM;
    pins[pin_count].direction = dir;
    pins[pin_count].kind = kind;
    vsnprintf(pins[pin_count].name, sizeof(pins[pin_count].name), fmt, args);
    return pin_count++;
}
#if defined(HAL_API_VERSION) && HAL_API_VERSION >= 1
#define NEW_PIN(api, ref_type, value_type, member, kind) \
    int hal_pin_new_##api(int id, hal_pdir_t dir, ref_type *ref, value_type def, const char *fmt, ...) { \
        va_list args; va_start(args, fmt); int n = allocate_pin(dir, id, kind, fmt, args); va_end(args); \
        if (n < 0) return n; \
        pins[n].value.member = def; *ref = (ref_type)&pins[n].value; return 0; }
NEW_PIN(bool, hal_bool_t, rtapi_bool, uint, 'b')
NEW_PIN(si32, hal_sint_t, rtapi_s32, sint, 's')
NEW_PIN(ui32, hal_uint_t, rtapi_u32, uint, 'u')
NEW_PIN(real, hal_real_t, rtapi_real, real, 'f')
#else
#define NEW_PIN(api, value_type, member, kind) \
    int hal_pin_##api##_newf(hal_pin_dir_t dir, value_type **ref, int id, const char *fmt, ...) { \
        va_list args; va_start(args, fmt); int n = allocate_pin(dir, id, kind, fmt, args); va_end(args); \
        if (n < 0) return n; \
        *ref = (void *)&pins[n].value.member; return 0; }
NEW_PIN(bit, hal_bit_t, bit, 'b')
NEW_PIN(s32, hal_s32_t, s32, 's')
NEW_PIN(u32, hal_u32_t, u32, 'u')
NEW_PIN(float, hal_float_t, real, 'f')
#endif
#undef NEW_PIN
int hal_init(const char *name) { assert(strcmp(name, "stepgen-ninja") == 0); return 42; }
void *hal_malloc(long size) { component_storage = calloc(1, (size_t)size); return component_storage; }
int hal_exit(int id) { assert(id == 42); ++exit_count; return 0; }
int hal_ready(int id) { assert(id == 42 && export_count == 3); return 0; }
void rtapi_print_msg(msg_level_t level, const char *fmt, ...) { (void)level; (void)fmt; }
int rtapi_set_msg_level(int level) { return level; }
#if defined(HAL_API_VERSION) && HAL_API_VERSION >= 1
int hal_export_funct(const char *name, void (*funct)(void *, long), void *arg, int reentrant, int id)
#else
int hal_export_funct(const char *name, void (*funct)(void *, long), void *arg, int uses_fp, int reentrant, int id)
#endif
{
#if !defined(HAL_API_VERSION)
    assert(uses_fp == 1);
#endif
    assert(funct && arg == hal_data && reentrant == 1 && id == 42);
    ++export_count;
    printf("export %s %d\n", name, reentrant);
    return 0;
}
static void snapshot(const char *label)
{
    printf("snapshot %s %d\n", label, pin_count);
    for (int i = 0; i < pin_count; ++i) {
        printf("%s %s %c ", pins[i].name, pins[i].direction == HAL_IN ? "in" : pins[i].direction == HAL_IO ? "io" : "out", pins[i].kind);
        switch (pins[i].kind) {
        case 'b': printf("%d", pins[i].value.bit); break;
        case 's': printf("%" PRId32, pins[i].value.s32); break;
        case 'u': printf("%" PRIu32, pins[i].value.u32); break;
        case 'f': printf("%a", pins[i].value.real); break;
        }
        puts("");
    }
}
static void receive_packet(transmission_pico_pc_t *packet)
{
    packet->protocol_magic = SN_PROTOCOL_MAGIC;
    packet->checksum = calculate_checksum(packet, sizeof(*packet) - 1);
    memcpy(incoming, packet, sizeof(*packet));
    incoming_size = sizeof(*packet);
    udp_io_process_recv(hal_data, 1000000);
}
int main(int argc, char **argv)
{
    if (argc == 2) fail_at = atoi(argv[1]);
    ip_address = "127.0.0.1:0"; /* Ephemeral local socket; send/receive are replaced above. */
    int result = rtapi_app_main();
    if (fail_at >= 0) {
        assert(result == -ENOMEM && exit_count == 1);
        close(hal_data[0].sockfd);
        free(tx_buffer); free(rx_buffer); free(component_storage);
        return 0;
    }
    assert(result == 0 && pin_count == 66);
    snapshot("defaults");
    module_data_t *d = hal_data;
    sn_set_u32(d->period, 1000000);
    sn_set_bit(d->io_ready_in, 1);
    sn_set_bit(d->output[0], 1);
    for (int i = 0; i < stepgens; ++i) {
        sn_set_bit(d->enable[i], 1);
        sn_set_float(d->scale[i], 400);
    }
    watchdog_process(d, 1000000);
    udp_io_process_send(d, 1000000);
    for (int cycle = 1; cycle <= 4; ++cycle) {
        for (int i = 0; i < stepgens; ++i) sn_set_float(d->command[i], cycle * (i % 2 ? -0.025 : 0.025));
        udp_io_process_send(d, 1000000);
    }
    snapshot("position");
    for (int i = 0; i < stepgens; ++i) {
        sn_set_bit(d->mode[i], 1);
        sn_set_float(d->command[i], i % 2 ? -10 : 10);
    }
    udp_io_process_send(d, 1000000);
    snapshot("velocity");
    sn_set_bit(d->debug_steps_reset, 1);
    sn_set_bit(d->enable[2], 0);
    sn_set_u32(d->pulse_width, 5000);
    udp_io_process_send(d, 1000000);
    snapshot("reset-disabled-pulse");
    transmission_pico_pc_t packet = {0};
    packet.jitter = 1250;
    packet.step_ring_fill = 2;
    packet.step_ring_status = STEP_RING_STATUS_ACTIVE | STEP_RING_STATUS_UNDERFLOW | STEP_RING_STATUS_OVERFLOW;
    for (int i = 0; i < encoders; ++i) {
        packet.encoder_counter[i] = -200 + i;
        packet.encoder_timestamp[i] = 1000;
        sn_set_float(d->enc_scale[i], 100);
    }
    receive_packet(&packet);
    for (unsigned bit = 0; bit < 128; ++bit) {
        memset(packet.inputs, 0, sizeof(packet.inputs));
        packet.inputs[bit / 32] = 1u << (bit % 32);
        receive_packet(&packet);
        for (unsigned i = 0; i < in_pins_no; ++i) {
            assert(sn_get_bit(d->input[i]) == (bit == input_pins[i]));
            assert(sn_get_bit(d->input_not[i]) == (bit != input_pins[i]));
        }
    }
    for (int i = 0; i < encoders; ++i) {
        packet.encoder_counter[i] += 30;
        packet.encoder_timestamp[i] += 1000;
        packet.encoder_velocity[i] = 30;
    }
    sn_set_bit(d->enc_reset[0], 1);
    receive_packet(&packet);
    snapshot("encoder-feedback");
    for (int i=0;i<pin_count;i++) if (strstr(pins[i].name, ".index-enable")) assert(pins[i].direction == HAL_IO);
    sn_set_bit(d->enc_index[0], 1);
    udp_io_process_send(d, 1000000);
    assert(tx_buffer->enc_control & 1);
    packet.encoder_index_tag[0]=tx_buffer->encoder_index_tag[0];
    packet.encoder_counter[0]+=30;packet.encoder_timestamp[0]+=1000;
    packet.encoder_index_count[0]=packet.encoder_counter[0]-7;
    packet.interrupt_data = 1;
    receive_packet(&packet);
    assert(sn_get_bit(d->enc_index[0]) == 0);
    assert(d->delta_count[0] == 30 && sn_get_float(d->enc_velocity[0]) > 0);
    assert(sn_get_s32(d->raw_count[0]) == packet.encoder_counter[0]);
    assert(fabs(sn_get_float(d->enc_position[0])-7/sn_get_float(d->enc_scale[0])) < 1e-9);
    int32_t index_offset=d->enc_offset[0];
    /* A new HAL request must wait for firmware's low acknowledgement. */
    sn_set_bit(d->enc_index[0], 1);
    receive_packet(&packet); /* repeated old event cannot clear the new request */
    assert(sn_get_bit(d->enc_index[0]) == 1 && d->enc_offset[0] == index_offset);
    udp_io_process_send(d, 1000000);assert(!(tx_buffer->enc_control & 1));
    packet.interrupt_data = 0;
    receive_packet(&packet);
    udp_io_process_send(d, 1000000);assert(tx_buffer->enc_control & 1);
    uint8_t next_tag=tx_buffer->encoder_index_tag[0];
    packet.interrupt_data=1; /* delayed old-generation event must be ignored */
    receive_packet(&packet);assert(sn_get_bit(d->enc_index[0]) == 1);
    packet.encoder_index_tag[0]=next_tag;
    d->enc_timestamp[0]=0; /* first feedback sample still completes index */
    receive_packet(&packet);assert(sn_get_bit(d->enc_index[0]) == 0);
    packet.interrupt_data = 0;
    for (int i = 0; i < encoders; ++i) packet.encoder_timestamp[i] += 3000000;
    receive_packet(&packet);
    snapshot("encoder-index-timeout");
    sn_set_s32(d->raw_count[0], INT32_MIN);
    assert(sn_get_s32(d->raw_count[0]) == INT32_MIN);
    sn_set_s32(d->raw_count[0], INT32_MAX);
    sn_set_u32(d->period, UINT32_MAX);
    assert(sn_get_u32(d->period) == UINT32_MAX);
    snapshot("integer-boundaries");
#if defined(HAL_API_VERSION) && HAL_API_VERSION >= 1
    /* HAL-1 storage is 64-bit; the driver's integer values must remain 32-bit. */
    uint64_t wide = UINT64_C(0x100000005);
    assert(sn_get_u32((sn_u32_pin)&wide) == 5);
    sn_set_u32((sn_u32_pin)&wide, UINT32_MAX);
    assert(wide == UINT32_MAX);
    int64_t signed_wide = INT64_C(0x1ffffffff);
    assert(sn_get_s32((sn_s32_pin)&signed_wide) == -1);
    sn_set_s32((sn_s32_pin)&signed_wide, INT32_MIN);
    assert(signed_wide == INT32_MIN);
#endif
    d->watchdog_expired = 1;
    udp_io_process_recv(d, 1000000);
    udp_io_process_send(d, 1000000);
    snapshot("watchdog");
    /* A corrupt packet must clear the value without destroying the reference. */
    d->watchdog_expired = 0;
    incoming[rx_size - 1] ^= 1;
    udp_io_process_recv(d, 1000000);
    assert(d->connected != NULL && sn_get_bit(d->connected) == 0 && d->checksum_error == 1);
    snapshot("checksum-error");
    incoming[rx_size - 1] ^= 1;
    udp_io_process_recv(d, 1000000);
    assert(d->connected != NULL && sn_get_bit(d->connected) == 1);
    snapshot("checksum-recovery");
    sn_set_bit(d->connected,1);
    packet.protocol_magic=0;
    packet.checksum=calculate_checksum(&packet,sizeof(packet)-1);
    memcpy(incoming,&packet,sizeof(packet));incoming_size=sizeof(packet);
    udp_io_process_recv(d,1000000);
    assert(sn_get_bit(d->connected)==0 && sn_get_bit(d->io_ready_out)==0 && d->protocol_error==1);
    rtapi_app_exit();
    free(tx_buffer); free(rx_buffer); free(component_storage);
    return 0;
}
