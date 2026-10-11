#include "controller_test_rig.h"
#include "probe_motion.h"
#include <cmath>
#include <iostream>
static void settle(Rig &r) {
  for (unsigned i = 0; i < 3000 && (r.c.mode != SN_IDLE || r.c.pending); ++i) {
    if (i % 50 == 0)
      r.poll();
    r.step();
  }
  assert(r.c.mode == SN_IDLE && !r.c.pending);
}
static void packet(uint8_t p[32], int64_t x, int64_t y, int64_t z,
                   unsigned speed = 1000) {
  std::memset(p, 0, 32);
  sn_write_i64(p, x);
  sn_write_i64(p + 8, y);
  sn_write_i64(p + 16, z);
  sn_write_u32(p + 24, speed);
  p[28] = 4;
}
int main() {
  Rig hit;
  hit.ready();
  uint8_t p[32];
  packet(p, 1000, -500, -1000, 2000);
  assert(hit.send(SN_PROBE, p, 32) == SN_OK);
  const uint32_t token = hit.c.probe_id;
  for (unsigned i = 0; i < 10; ++i)
    hit.step();
  // Lost start ACK: identical retry cannot restart the move or change its
  // token.
  uint8_t wire[512];
  size_t n = sn_control_encode(wire, SN_PROBE, 9, hit.seq, p, 32);
  sn_packet retry;
  assert(sn_control_decode(wire, n, &retry));
  double progress = hit.c.probe_progress;
  assert(sn_controller_command(&hit.c, &retry, 42) == SN_OK);
  assert(hit.c.probe_id == token && hit.c.probe_progress == progress);
  for (unsigned i = 0; i < 100; ++i) {
    if (i % 50 == 0)
      hit.poll();
    hit.step();
  }
  assert(hit.c.pending && hit.c.probe_speed > 1000);
  int64_t before[3];
  std::memcpy(before, hit.c.position, sizeof(before));
  // GPIO IRQ midway through an active frame; only these actual residual pulses
  // were emitted. They must be accounted once, without a false TIMING fault.
  int32_t partial[3] = {1, 0, -1};
  sn_controller_probe_hit(&hit.c, partial);
  assert(hit.c.probe_state == SN_PROBE_HIT && hit.c.mode == SN_PROBE_BRAKING);
  for (unsigned i = 0; i < 3; ++i)
    assert(hit.c.probe_position[i] == before[i] + partial[i]);
  int64_t latched[3];
  std::memcpy(latched, hit.c.probe_position, sizeof(latched));
  settle(hit);
  assert(hit.c.armed && !hit.c.fault && hit.c.probe_state == SN_PROBE_HIT);
  assert(hit.c.position[0] > latched[0] && hit.c.position[2] < latched[2]);
  assert(hit.c.position[0] < 1000 && hit.c.position[2] > -1000);
  uint8_t report[112];
  sn_controller_status(&hit.c, report, SN_OK);
  assert(sn_read_u32(report + 96) == token && report[100] == SN_PROBE_HIT);
  for (unsigned i = 0; i < 3; ++i)
    assert(sn_read_i64(report + 72 + i * 8) == latched[i]);
  for (unsigned i = 0; i < 20; ++i)
    hit.step();
  assert(!std::memcmp(latched, hit.c.probe_position, sizeof(latched)));
  // Already active contact never starts motion.
  Rig active;
  active.ready();
  active.raw &= ~16u;
  active.step();
  assert(active.send(SN_PROBE, p, 32) == SN_STATE && active.c.mode == SN_IDLE);
  assert(!active.c.position[0] && !active.c.probe_id);
  // No contact: stop exactly at the 3D endpoint; preserve the MISS diagnostic,
  // disable outputs, and never report the endpoint as a successful probe.
  Rig miss;
  miss.ready();
  packet(p, 30, -15, 10);
  assert(miss.send(SN_PROBE, p, 32) == SN_OK);
  settle(miss);
  assert(miss.c.position[0] == 30 && miss.c.position[1] == -15 &&
         miss.c.position[2] == 10);
  assert(miss.c.fault == SN_FAULT_PROBE_MISS && !miss.c.armed &&
         miss.c.probe_state == SN_PROBE_MISS);
  assert(!miss.c.outputs && !miss.c.pwm);
  // Probe cancellation finishes the current hardware frame then brakes, without
  // returning a HIT or executing the remainder of G31.
  Rig cancel;
  cancel.ready();
  packet(p, 1000, 500, 0, 2000);
  assert(cancel.send(SN_PROBE, p, 32) == SN_OK);
  for (unsigned i = 0; i < 100; ++i) {
    if (i % 50 == 0)
      cancel.poll();
    cancel.step();
  }
  const int64_t at_cancel = cancel.c.position[0];
  assert(cancel.send(SN_PROBE_CANCEL) == SN_OK);
  settle(cancel);
  assert(cancel.c.position[0] > at_cancel && cancel.c.position[0] < 1000);
  assert(cancel.c.probe_state == SN_PROBE_CANCELLED && cancel.c.armed &&
         !cancel.c.fault);
  // Polling fallback still captures a contact if a GPIO edge was missed.
  Rig polled;
  polled.ready();
  packet(p, 0, 0, -500);
  assert(polled.send(SN_PROBE, p, 32) == SN_OK);
  for (unsigned i = 0; i < 70; ++i) {
    if (i % 50 == 0)
      polled.poll();
    polled.step();
  }
  polled.raw &= ~16u;
  polled.step();
  settle(polled);
  assert(polled.c.probe_state == SN_PROBE_HIT &&
         polled.c.probe_position[2] < 0);
  // Probe result is not a success after E-stop; no resumed motion is possible.
  Rig estop;
  estop.ready();
  assert(estop.send(SN_PROBE, p, 32) == SN_OK);
  estop.step();
  estop.raw &= ~1u;
  estop.step();
  assert(estop.c.fault == SN_FAULT_ESTOP &&
         estop.c.probe_state == SN_PROBE_CANCELLED);
  // Watchdog and moving-axis limits apply to local probing as to program
  // motion.
  Rig watchdog;
  watchdog.ready();
  packet(p, 10000, 0, 0);
  assert(watchdog.send(SN_PROBE, p, 32) == SN_OK);
  for (unsigned i = 0; i < 600; ++i)
    watchdog.step();
  assert(watchdog.c.fault == SN_FAULT_WATCHDOG && !watchdog.c.armed);
  assert(watchdog.c.probe_state == SN_PROBE_CANCELLED);
  Rig limit;
  limit.ready();
  assert(limit.send(SN_PROBE, p, 32) == SN_OK);
  for (unsigned i = 0; i < 20; ++i)
    limit.step();
  limit.raw &= ~2u;
  for (unsigned i = 0; i < 10 && !limit.c.fault; ++i)
    limit.step();
  assert(limit.c.fault == SN_FAULT_LIMIT && !limit.c.armed);
  assert(limit.c.probe_state == SN_PROBE_CANCELLED);
  // PC mapping uses path length in machine units despite anisotropic scaling.
  double goal[3] = {5, 2.5, -2.5}, scale[3] = {100, 200, 400};
  int64_t zero[3] = {};
  assert(sn::probe_packet(p, goal, scale, zero, 60).empty());
  assert(sn_read_i64(p) == 500 && sn_read_i64(p + 8) == 500 &&
         sn_read_i64(p + 16) == -1000);
  assert(sn_read_u32(p + 24) == 163 && p[28] == 4);
  double origin[3] = {2, 3, -1}, work[3];
  int64_t capture[3] = {500, 500, -1000};
  assert(sn::probe_work_position(work, capture, scale, origin).empty());
  assert(work[0] == 3 && work[1] == -.5 && work[2] == -1.5);
  goal[0] = goal[1] = goal[2] = 0;
  assert(!sn::probe_packet(p, goal, scale, zero, 60).empty());
  scale[0] = 0;
  assert(!sn::probe_work_position(work, capture, scale, origin).empty());
  std::cout
      << "PASS: coordinated G31, IRQ residual latch, braking vs contact, "
         "missed/active/cancelled probe, duplicate ACK and work coordinates\n";
}
