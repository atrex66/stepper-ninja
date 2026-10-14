#!/usr/bin/env python3
"""Exercise the firmware's actual USB poll code with CDC/GPIO/time substitutes.
This validates mode transitions locally, not the SDK USB stack or hardware.
"""
import pathlib
import subprocess
import sys
import tempfile
root = pathlib.Path(__file__).resolve().parents[2]
runtime = (root / 'firmware/modules/mach3_runtime.c').read_text()
globals_code = runtime[runtime.index('#define USB_PEER'):runtime.index('static sn_controller controller;')]
poll_code = runtime[runtime.index('void sn_mach3_usb_poll(void)'):]
stubs = r'''
#include "common/protocol/mach3_usb.h"
#include "common/motion/controller.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
static sn_controller controller;
static uint8_t rx[4096], tx[4096];
static size_t rx_at, rx_size, tx_size;
static uint64_t now = 200000;
static bool connected = true, console_enabled = true;
static unsigned commands, stops;
static char last_command[64];
static int read_chars(char *p, int n) {
  (void)n;
  if (rx_at == rx_size || !connected) return -1;
  *p = (char)rx[rx_at++]; return 1;
}
static void write_chars(const char *p, int n) {
  assert(tx_size + n <= sizeof(tx));
  memcpy(tx + tx_size, p, (size_t)n); tx_size += (size_t)n;
}
static struct {int (*in_chars)(char *, int); void (*out_chars)(const char *, int);}
  stdio_usb = {read_chars, write_chars};
#define stdio_set_driver_enabled(driver, enabled) (console_enabled = (enabled))
static uint32_t save_and_disable_interrupts(void) { return 0; }
static void restore_interrupts(uint32_t x) { (void)x; }
static bool stdio_usb_connected(void) { return connected; }
static uint64_t time_us_64(void) { return now; }
static uint8_t input_bits(void) { return 127; }
static void stop_steps(void) { ++stops; }
static void apply_io(void) {}
static void process_command(char *p) {
  assert(!controller.session && console_enabled);
  ++commands; strcpy(last_command, p);
}
'''
tests = r'''
static void bytes(const void *p, size_t n) {
  assert(n <= sizeof(rx)); memcpy(rx, p, n); rx_at = 0; rx_size = n;
  sn_mach3_usb_poll();
}
static void text(const char *p) { bytes(p, strlen(p)); }
static void command(uint16_t cmd, uint32_t session, uint32_t sequence) {
  uint8_t raw[SN_CONTROL_MAX_PACKET], frame[SN_USB_MAX_FRAME];
  size_t n = sn_control_encode(raw, cmd, session, sequence, NULL, 0);
  n = sn_usb_encode(frame, raw, n);
  tx_size = 0;
  // Feed every possible USB fragment boundary, including split header/CRC.
  for (size_t i = 0; i < n; ++i) bytes(frame + i, 1);
}
static uint32_t reply(void) {
  sn_usb_decoder decoder = {0}; sn_packet packet; int found = 0;
  uint32_t result = 999;
  for (size_t i = 0; i < tx_size; ++i) {
    if (sn_usb_feed(&decoder, tx[i], &packet)) {
      result = sn_read_u32(packet.payload); ++found;
    }
  }
  assert(found == 1); return result;
}
static void quiet(void) {
  now += 100001; rx_at = rx_size = 0; sn_mach3_usb_poll();
}
int main(void) {
  sn_controller_init(&controller);
  text("check\r"); assert(commands == 1 && !strcmp(last_command, "check"));
  text("half-command");
  command(SN_CLAIM, 123, 1);
  assert(reply() == SN_OK && controller.session == 123 && !console_enabled);
  assert(!controller.armed); // E-stop/idle must not unlock console.
  text("save\rreset\r"); assert(commands == 1);
  command(SN_POLL, 123, 2); assert(reply() == SN_OK);
  command(SN_RELEASE, 123, 3);
  assert(reply() == SN_OK && !controller.session && !console_enabled);
  text("save\r"); assert(commands == 1); // Discard binary-session residue.
  quiet(); assert(console_enabled);
  text("help\n"); assert(commands == 2 && !strcmp(last_command, "help"));
  command(SN_CLAIM, 234, 1); assert(reply() == SN_OK);
  for (unsigned i = 0; i < 501; ++i) {
    int32_t step[3]; sn_controller_tick(&controller, 127, step); now += 1000;
  }
  quiet();
  assert(!controller.session && controller.fault == SN_FAULT_WATCHDOG && console_enabled);
  command(SN_CLAIM, 345, 1); assert(reply() == SN_OK);
  connected = false;
  unsigned previous_stops = stops;
  sn_mach3_usb_poll();
  assert(!controller.session && !controller.armed && stops > previous_stops);
  connected = true; quiet(); assert(console_enabled);
  // UDP owner also locks USB console and rejects USB acquisition.
  uint8_t raw[SN_CONTROL_MAX_PACKET]; sn_packet packet;
  size_t n = sn_control_encode(raw, SN_CLAIM, 456, 1, NULL, 0);
  assert(sn_control_decode(raw, n, &packet));
  assert(sn_controller_command(&controller, &packet, 42) == SN_OK);
  sn_mach3_usb_poll(); assert(!console_enabled);
  command(SN_CLAIM, 567, 1); assert(reply() == SN_OWNER);
  assert(controller.session == 456 && controller.peer == 42);
  connected = false; sn_mach3_usb_poll();
  assert(controller.session == 456); // Closing USB cannot stop a UDP owner.
  connected = true;
  for (unsigned i = 0; i < 501; ++i) {
    int32_t step[3]; sn_controller_tick(&controller, 127, step); now += 1000;
  }
  quiet(); assert(console_enabled);
  // Invalid protocol or CRC must never switch modes or execute console text.
  n = sn_control_encode(raw, SN_CLAIM, 678, 1, NULL, 0); raw[4] ^= 1;
  uint8_t frame[SN_USB_MAX_FRAME]; n = sn_usb_encode(frame, raw, n);
  bytes(frame, n); assert(!controller.session && commands == 2);
  quiet(); text("check\r"); assert(commands == 3);
  char overflow[100]; memset(overflow, 'a', sizeof(overflow));
  bytes(overflow, sizeof(overflow)); text("\r"); assert(commands == 3);
  quiet(); text("check\r"); assert(commands == 4);
  puts("USB claim/release, console isolation, watchdog, DTR and UDP ownership passed");
}
'''
with tempfile.TemporaryDirectory(prefix='sn-usb-console-') as directory:
    path = pathlib.Path(directory)
    source = path / 'test.c'
    source.write_text(stubs + globals_code + poll_code + tests)
    exe = path / 'test'
    subprocess.run([sys.argv[1], '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(root), str(source),
                    *[str(root / p) for p in ('common/protocol/mach3_usb.c',
                        'common/protocol/mach3_control.c', 'common/protocol/mach3_probe.c',
                        'common/motion/controller.c')], '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
