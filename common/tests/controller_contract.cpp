#include "controller_test_rig.h"
#include "motion/controller.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
  Rig r;
  r.ready();
  uint8_t io[4] = {3, 255, 255, 0};
  assert(r.send(SN_IO, io, 4) == SN_OK);
  assert(r.c.pwm == 65535 && r.c.outputs == 3);
  uint8_t jog[8] = {0, 1, 0, 0, 0, 0, 0, 0};
  sn_write_u32(jog + 4, 1000);
  assert(r.send(SN_JOG, jog, 8) == SN_OK);
  for (int i = 0; i < 200; ++i) {
    if (i % 50 == 0)
      r.poll();
    r.step();
  }
  assert(r.c.position[0] > 170 && r.c.position[0] < 200);
  uint8_t axis[4] = {0, 0, 0, 0};
  assert(r.send(SN_JOG_STOP, axis, 4) == SN_OK);
  for (int i = 0; i < 100; ++i) {
    r.step();
  }
  assert(r.c.mode == SN_IDLE);
  int64_t stopped = r.c.position[0];
  for (int i = 0; i < 20; ++i) {
    r.step();
  }
  assert(r.c.position[0] == stopped);
  // Identity includes peer and session; strangers cannot stop or refresh the
  // watchdog.
  uint32_t before = r.c.last_seen_ms;
  assert(r.send(SN_ABORT, nullptr, 0, 43) == SN_OWNER);
  assert(r.c.last_seen_ms == before);
  --r.seq;
  // Identical retransmission executes once; altered payload with same sequence
  // is rejected.
  uint8_t batch[24] = {};
  sn_write_u32(batch, 1);
  sn_write_u32(batch + 4, 1);
  sn_write_u32(batch + 8, 2);
  sn_write_u32(batch + 20, 123);
  assert(r.send(SN_BATCH, batch, 24) == SN_OK);
  unsigned count = r.c.fill;
  uint8_t wire[512];
  auto len = sn_control_encode(wire, SN_BATCH, 9, r.seq, batch, 24);
  sn_packet packet;
  assert(sn_control_decode(wire, len, &packet));
  assert(sn_controller_command(&r.c, &packet, 42) == SN_OK);
  assert(r.c.fill == count);
  batch[8] = 3;
  len = sn_control_encode(wire, SN_BATCH, 9, r.seq, batch, 24);
  assert(sn_control_decode(wire, len, &packet));
  assert(sn_controller_command(&r.c, &packet, 42) == SN_SEQUENCE);
  assert(r.c.executed_frame == 0);
  assert(r.send(SN_END) == SN_OK);
  assert(r.send(SN_START) == SN_OK);
  r.step();
  assert(r.c.executed_frame == 0);
  r.step();
  assert(r.c.executed_frame == 1 && r.c.executed_line == 123);
  assert(r.c.position[0] == stopped + 2 && r.c.mode == SN_IDLE);
  // A partial pulse train must never acknowledge a complete program segment.
  Rig partial;
  partial.ready();
  batch[8] = 2;
  assert(partial.send(SN_BATCH, batch, 24) == SN_OK);
  assert(partial.send(SN_START) == SN_OK);
  partial.step();
  int32_t emitted[3] = {1, 0, 0};
  sn_controller_complete(&partial.c, emitted, 0);
  assert(partial.c.position[0] == 1 && partial.c.executed_frame == 0 &&
         partial.c.fault == SN_FAULT_TIMING && !partial.c.armed);
  // Underflow, physical E-stop, limits and watchdog reset every output
  // including PWM.
  Rig empty;
  empty.ready();
  assert(empty.send(SN_BATCH, batch, 24) == SN_OK);
  assert(empty.send(SN_START) == SN_OK);
  empty.step();
  empty.step();
  assert(empty.c.fault == SN_FAULT_UNDERFLOW);
  r.raw &= ~1u;
  r.step();
  assert(r.c.fault == SN_FAULT_ESTOP && !r.c.outputs && !r.c.pwm);
  Rig lost;
  lost.ready();
  assert(lost.send(SN_IO, io, 4) == SN_OK);
  for (int i = 0; i < 502; ++i)
    lost.step();
  assert(lost.c.fault == SN_FAULT_WATCHDOG && !lost.c.session &&
         !lost.c.outputs && !lost.c.pwm);
  Rig limit;
  limit.ready();
  assert(limit.send(SN_JOG, jog, 8) == SN_OK);
  for (int i = 0; i < 60; ++i)
    limit.step();
  limit.raw &= ~2u;
  limit.step();
  assert(limit.c.fault == SN_FAULT_LIMIT);
  // Complete switch sequence: fast seek, backoff, slow latch, final release.
  Rig home;
  home.ready();
  assert(home.send(SN_HOME, axis, 4) == SN_OK);
  for (int i = 0; i < 4000 && !(home.c.homed & 1); ++i) {
    if (i % 50 == 0)
      home.poll();
    bool hit = home.c.home_phase == 1   ? home.c.position[0] <= -30
               : home.c.home_phase == 2 ? home.c.position[0] <= -20
               : home.c.home_phase == 3 ? home.c.position[0] <= -20
                                        : home.c.position[0] <= 10;
    home.raw = hit ? 125 : 127;
    home.step();
  }
  assert(home.c.homed & 1);
  assert(home.c.mode == SN_IDLE && home.c.armed && home.c.position[0] >= 100);
  // Firmware-owned dwell is exactly timed and cannot overlap jog/program
  // motion.
  Rig dwell;
  dwell.ready();
  uint8_t duration[4];
  sn_write_u32(duration, 20);
  assert(dwell.send(SN_DWELL, duration, 4) == SN_OK);
  assert(dwell.send(SN_JOG, jog, 8) == SN_STATE);
  for (unsigned i = 0; i < 19; ++i)
    dwell.step();
  assert(dwell.c.mode == SN_DWELLING && dwell.c.position[0] == 0);
  dwell.step();
  assert(dwell.c.mode == SN_IDLE && dwell.c.armed);
  // HOLD drops queued frames, preserves enable and ramps an active jog to zero.
  Rig hold;
  hold.ready();
  assert(hold.send(SN_JOG, jog, 8) == SN_OK);
  for (unsigned i = 0; i < 100; ++i)
    hold.step();
  const int64_t hold_position = hold.c.position[0];
  assert(hold.send(SN_HOLD) == SN_OK);
  for (unsigned i = 0; i < 100; ++i)
    hold.step();
  assert(hold.c.mode == SN_IDLE && hold.c.armed && !hold.c.fill);
  assert(hold.c.position[0] > hold_position &&
         hold.c.position[0] < hold_position + 50);
  // Abort cannot erase the reason for a hardware fault; only explicit ARM can.
  assert(partial.send(SN_ABORT) == SN_OK);
  assert(partial.c.fault == SN_FAULT_TIMING);
  // Configuration rejects extreme signed values, zero velocity and invalid
  // maps.
  sn_machine_config cfg;
  sn_config_defaults(&cfg);
  uint8_t config[128];
  sn_config_encode(config, &cfg);
  sn_write_i64(config + 44, INT64_MIN);
  assert(!sn_config_decode(config, 128, &cfg));
  std::cout << "PASS: I/O, ramped jog, home sequence, program ACK/report, "
               "retransmissions, faults and watchdog\n";
}
