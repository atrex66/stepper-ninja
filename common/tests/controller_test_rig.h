#ifndef SN_CONTROLLER_TEST_RIG_H
#define SN_CONTROLLER_TEST_RIG_H
#include "motion/controller.h"
#include <cassert>
#include <cstring>
struct Rig {
  sn_controller c;
  uint32_t seq = 0;
  uint8_t raw = 127;
  Rig() {
    sn_controller_init(&c);
    step();
  }
  uint32_t send(unsigned command, const uint8_t *p = nullptr, unsigned n = 0,
                uint64_t peer = 42, uint32_t session = 9) {
    uint8_t wire[512];
    size_t length = sn_control_encode(wire, command, session, ++seq, p, n);
    sn_packet packet;
    assert(sn_control_decode(wire, length, &packet));
    return sn_controller_command(&c, &packet, peer);
  }
  void step() {
    int32_t actual[3] = {0, 0, 0};
    if (c.pending)
      std::memcpy(actual, c.current.steps, sizeof(actual));
    sn_controller_complete(&c, actual, 1);
    int32_t steps[3];
    sn_controller_tick(&c, raw, steps);
  }
  void ready() {
    assert(send(SN_CLAIM) == SN_OK);
    sn_machine_config cfg;
    sn_config_defaults(&cfg);
    uint8_t p[128];
    sn_config_encode(p, &cfg);
    assert(send(SN_CONFIG, p, sizeof(p)) == SN_OK);
    assert(send(SN_ARM) == SN_OK);
  }
  void poll() { assert(send(SN_POLL) == SN_OK); }
};
#endif
