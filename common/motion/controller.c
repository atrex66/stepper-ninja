#include "controller.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static int32_t signed32(uint32_t v) {
  return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)(UINT32_MAX - v);
}
void sn_config_defaults(sn_machine_config *c) {
  memset(c, 0, sizeof(*c));
  c->watchdog_ms = 500;
  c->pwm_hz = 10000;
  c->pulse_ns = 2500;
  c->dir_setup_ns = 5000;
  c->input_invert = 0x7f;
  for (unsigned i = 0; i < 3; ++i) {
    c->axis[i].max_velocity = 10000;
    c->axis[i].acceleration = 20000;
    c->axis[i].home_fast = 1000;
    c->axis[i].home_slow = 100;
    c->axis[i].backoff = 100;
    c->axis[i].home_travel = 100000;
    c->axis[i].home_direction = -1;
    c->axis[i].home_input = (uint8_t)(i + 1);
    c->axis[i].limit_input = (uint8_t)(i + 1);
  }
}
void sn_config_encode(uint8_t p[128], const sn_machine_config *c) {
  memset(p, 0, 128);
  sn_write_u32(p, c->watchdog_ms);
  sn_write_u32(p + 4, c->pwm_hz);
  sn_write_u32(p + 8, c->pulse_ns);
  sn_write_u32(p + 12, c->dir_setup_ns);
  p[16] = c->input_invert;
  p[17] = c->output_invert;
  p[18] = c->dir_invert;
  for (unsigned i = 0; i < 3; ++i) {
    uint8_t *a = p + 20 + i * 36;
    const sn_axis_config *v = &c->axis[i];
    sn_write_u32(a, v->max_velocity);
    sn_write_u32(a + 4, v->acceleration);
    sn_write_u32(a + 8, v->home_fast);
    sn_write_u32(a + 12, v->home_slow);
    sn_write_u32(a + 16, v->backoff);
    sn_write_u32(a + 20, v->home_travel);
    sn_write_i64(a + 24, v->home_offset);
    a[32] = (uint8_t)v->home_direction;
    a[33] = v->home_input;
    a[34] = v->limit_input;
  }
}
int sn_config_decode(const uint8_t *p, size_t size, sn_machine_config *c) {
  if (!p || !c || size != 128 || p[19] || p[16] > 127 || p[17] > 63 ||
      p[18] > 7)
    return 0;
  memset(c, 0, sizeof(*c));
  c->watchdog_ms = sn_read_u32(p);
  c->pwm_hz = sn_read_u32(p + 4);
  c->pulse_ns = sn_read_u32(p + 8);
  c->dir_setup_ns = sn_read_u32(p + 12);
  c->input_invert = p[16];
  c->output_invert = p[17];
  c->dir_invert = p[18];
  if (c->watchdog_ms < 100 || c->watchdog_ms > 2000 || c->pwm_hz < 4000 ||
      c->pwm_hz > 100000 || c->pulse_ns < 1000 || c->pulse_ns > 10000 ||
      c->dir_setup_ns < 1000 || c->dir_setup_ns > 20000)
    return 0;
  for (unsigned i = 0; i < 3; ++i) {
    const uint8_t *a = p + 20 + i * 36;
    sn_axis_config *v = &c->axis[i];
    v->max_velocity = sn_read_u32(a);
    v->acceleration = sn_read_u32(a + 4);
    v->home_fast = sn_read_u32(a + 8);
    v->home_slow = sn_read_u32(a + 12);
    v->backoff = sn_read_u32(a + 16);
    v->home_travel = sn_read_u32(a + 20);
    v->home_offset = sn_read_i64(a + 24);
    v->home_direction = a[32] == 255 ? -1 : (int8_t)a[32];
    v->home_input = a[33];
    v->limit_input = a[34];
    if (a[35] || !v->max_velocity || v->max_velocity > 80000 ||
        !v->acceleration || v->acceleration > 10000000 || !v->home_fast ||
        v->home_fast > v->max_velocity || !v->home_slow ||
        v->home_slow > v->home_fast || !v->backoff ||
        v->backoff > v->home_travel || v->home_travel > 100000000 ||
        (v->home_offset < -1000000000000LL ||
         v->home_offset > 1000000000000LL) ||
        (v->home_direction != -1 && v->home_direction != 1) ||
        v->home_input < 1 || v->home_input > 6 ||
        (v->limit_input != 255 && (v->limit_input < 1 || v->limit_input > 6)))
      return 0;
    /* Guaranteed frame fit, including low time and PIO overhead. */
    if ((uint64_t)v->max_velocity * (c->pulse_ns * 2u + 1000u) > 850000000u)
      return 0;
  }
  return 1;
}
static void clear_motion(sn_controller *c) {
  c->head = c->tail = c->fill = 0;
  c->mode = SN_IDLE;
  c->pending = 0;
  c->end = 0;
  c->home_phase = 0;
  c->home_hit = 0;
  c->dwell_ticks = 0;
  c->probe_cancel = 0;
  if (c->probe_state == SN_PROBE_ACTIVE)
    c->probe_state = SN_PROBE_CANCELLED;
  memset(c->velocity, 0, sizeof(c->velocity));
  memset(c->target, 0, sizeof(c->target));
  memset(c->fraction, 0, sizeof(c->fraction));
  ++c->epoch;
}
void sn_controller_init(sn_controller *c) {
  memset(c, 0, sizeof(*c));
  sn_config_defaults(&c->config);
}
void sn_controller_fault(sn_controller *c, uint8_t fault) {
  clear_motion(c);
  c->fault = fault;
  c->armed = 0;
  c->outputs = 0;
  c->pwm = 0;
  if (fault != SN_FAULT_NONE)
    c->homed = 0;
}
static int idle(const sn_controller *c) {
  return c->mode == SN_IDLE && !c->pending && !c->fill;
}
static uint32_t execute(sn_controller *c, const sn_packet *r) {
  const uint8_t *p = r->payload;
  size_t n = r->size;
  switch (r->command) {
  case SN_CONFIG: {
    sn_machine_config next;
    if (c->armed || !idle(c))
      return SN_STATE;
    if (!sn_config_decode(p, n, &next))
      return SN_RANGE;
    c->config = next;
    c->configured = 1;
    c->inputs = c->raw_inputs ^ next.input_invert;
    return SN_OK;
  }
  case SN_ARM:
    if (n)
      return SN_RANGE;
    if (!c->configured || !idle(c) || (c->inputs & 1))
      return SN_STATE;
    c->fault = 0;
    c->armed = 1;
    c->outputs = 1;
    return SN_OK;
  case SN_DISARM:
  case SN_ABORT:
    if (n)
      return SN_RANGE;
    sn_controller_fault(c, c->fault);
    return SN_OK;
  case SN_POLL:
    return n ? SN_RANGE : SN_OK;
  case SN_RELEASE:
    if (n)
      return SN_RANGE;
    sn_controller_fault(c, SN_FAULT_NONE);
    c->session = 0;
    c->peer = 0;
    c->configured = 0;
    return SN_OK;
  case SN_DWELL:
    if (n != 4 || !c->armed || !idle(c))
      return SN_STATE;
    if (!sn_read_u32(p) || sn_read_u32(p) > 3600000)
      return SN_RANGE;
    c->dwell_ticks = sn_read_u32(p);
    c->mode = SN_DWELLING;
    return SN_OK;
  case SN_HOLD: {
    if (n || !c->armed)
      return SN_STATE;
    double speed[3];
    for (unsigned i = 0; i < 3; ++i)
      speed[i] =
          c->mode == SN_STREAM ? c->current.steps[i] * 1000.0 : c->velocity[i];
    clear_motion(c);
    memcpy(c->velocity, speed, sizeof(speed));
    c->mode = SN_STOPPING;
    return SN_OK;
  }
  case SN_MOVE: {
    if (n != 16 || p[0] > 2 || p[1] || p[2] || p[3])
      return SN_RANGE;
    int64_t distance = sn_read_i64(p + 4);
    uint32_t speed = sn_read_u32(p + 12);
    if (distance < -1000000000LL || distance > 1000000000LL || !distance ||
        !speed || speed > c->config.axis[p[0]].max_velocity)
      return SN_RANGE;
    if (!c->armed || !idle(c))
      return SN_STATE;
    c->move_axis = p[0];
    c->move_target = c->position[p[0]] + distance;
    c->target[p[0]] = distance > 0 ? speed : -(double)speed;
    c->mode = SN_POSITIONING;
    return SN_OK;
  }
  case SN_PROBE: {
    /* Absolute XYZ step endpoint, dominant-axis speed and logical input index.
     */
    if (n != 32 || p[28] < 1 || p[28] > 6 || p[29] || p[30] || p[31])
      return SN_RANGE;
    if (!c->armed || !idle(c) || (c->inputs & (1u << p[28])))
      return SN_STATE;
    int64_t delta[3];
    unsigned major = 0;
    double length = 0, speed = sn_read_u32(p + 24), acceleration = 1e100;
    if (speed < 1 || speed > 80000)
      return SN_RANGE;
    for (unsigned i = 0; i < 3; ++i) {
      int64_t goal = sn_read_i64(p + 8 * i);
      if (goal < -1000000000000LL || goal > 1000000000000LL)
        return SN_RANGE;
      delta[i] = goal - c->position[i];
      if (delta[i] < -1000000000LL || delta[i] > 1000000000LL)
        return SN_RANGE;
      if (fabs((double)delta[i]) > length) {
        major = i;
        length = fabs((double)delta[i]);
      }
    }
    if (!length)
      return SN_RANGE;
    for (unsigned i = 0; i < 3; ++i) {
      double ratio = fabs((double)delta[i]) / length;
      if (ratio) {
        if (c->config.axis[i].limit_input < 7 &&
            (c->inputs & (1u << c->config.axis[i].limit_input)))
          return SN_STATE;
        double vmax = c->config.axis[i].max_velocity / ratio;
        if (speed > vmax)
          speed = vmax;
        double amax = c->config.axis[i].acceleration / ratio;
        if (acceleration > amax)
          acceleration = amax;
      }
    }
    memcpy(c->probe_start, c->position, sizeof(c->position));
    memcpy(c->probe_delta, delta, sizeof(delta));
    memset(c->probe_position, 0, sizeof(c->probe_position));
    c->probe_id = r->sequence;
    c->probe_input = p[28];
    c->probe_major = (uint8_t)major;
    c->probe_length = length;
    c->probe_progress = c->probe_speed = c->probe_brake_progress = 0;
    c->probe_max_speed = speed;
    c->probe_acceleration = acceleration;
    c->probe_state = SN_PROBE_ACTIVE;
    c->mode = SN_PROBING;
    return SN_OK;
  }
  case SN_PROBE_CANCEL:
    if (n || !c->armed ||
        (c->mode != SN_PROBING && c->mode != SN_PROBE_BRAKING))
      return SN_STATE;
    c->probe_cancel = 1; // Current PIO frame completes before braking starts.
    return SN_OK;
  case SN_SET_POSITION:
    if (n != 24 || !idle(c))
      return SN_STATE;
    for (unsigned i = 0; i < 3; ++i)
      if (sn_read_i64(p + i * 8) < -1000000000000LL ||
          sn_read_i64(p + i * 8) > 1000000000000LL)
        return SN_RANGE;
    for (unsigned i = 0; i < 3; ++i)
      c->position[i] = sn_read_i64(p + i * 8);
    return SN_OK;
  case SN_IO:
    if (n != 4 || p[0] > 63 || p[3])
      return SN_RANGE;
    if (!c->armed)
      return SN_STATE;
    c->outputs = (uint8_t)((p[0] & 62) | 1);
    c->pwm = (uint16_t)(p[1] | p[2] << 8);
    if (!(c->outputs & 2))
      c->pwm = 0;
    return SN_OK;
  case SN_JOG: {
    if (n != 8 || p[0] > 2 || (p[1] != 1 && p[1] != 255) || p[2] || p[3])
      return SN_RANGE;
    unsigned axis = p[0];
    uint32_t v = sn_read_u32(p + 4);
    if (!v || v > c->config.axis[axis].max_velocity)
      return SN_RANGE;
    if (!c->armed || c->fill || (c->mode != SN_IDLE && c->mode != SN_JOGGING))
      return SN_STATE;
    if (c->config.axis[axis].limit_input < 7 &&
        (c->inputs & (1u << c->config.axis[axis].limit_input)))
      return SN_STATE;
    c->target[axis] = (p[1] == 1 ? 1.0 : -1.0) * v;
    c->mode = SN_JOGGING;
    return SN_OK;
  }
  case SN_JOG_STOP:
    if (n != 4 || p[0] > 2 || p[1] || p[2] || p[3])
      return SN_RANGE;
    if (c->mode != SN_JOGGING)
      return SN_STATE;
    c->target[p[0]] = 0;
    return SN_OK;
  case SN_HOME:
    if (n != 4 || p[0] > 2 || p[1] || p[2] || p[3])
      return SN_RANGE;
    if (!c->armed || !idle(c))
      return SN_STATE;
    c->home_axis = p[0];
    c->homed &= (uint8_t)~(1u << p[0]);
    c->home_origin = c->position[p[0]];
    c->home_phase =
        (c->inputs & (1u << c->config.axis[p[0]].home_input)) ? 2 : 1;
    c->mode = SN_HOMING;
    return SN_OK;
  case SN_BATCH: {
    if (n < 8 || !c->armed || (c->mode != SN_IDLE && c->mode != SN_STREAM) ||
        c->end)
      return SN_STATE;
    uint32_t first = sn_read_u32(p), count = sn_read_u32(p + 4);
    if (!first || first > UINT32_MAX - count + 1u || !count || count > 20 ||
        n != 8 + count * 16u || first != c->accepted_frame + 1u)
      return SN_RANGE;
    if (c->fill + count > SN_QUEUE_CAPACITY)
      return SN_FULL;
    for (unsigned i = 0; i < count; ++i)
      for (unsigned a = 0; a < 3; ++a) {
        int32_t delta = signed32(sn_read_u32(p + 8 + i * 16 + a * 4));
        if (delta < -SN_MAX_STEPS_TICK || delta > SN_MAX_STEPS_TICK ||
            fabs((double)delta) * 1000 > c->config.axis[a].max_velocity)
          return SN_RANGE;
      }
    for (unsigned i = 0; i < count; ++i) {
      sn_segment *s = &c->queue[c->head];
      s->id = first + i;
      s->line = sn_read_u32(p + 8 + i * 16 + 12);
      for (unsigned a = 0; a < 3; ++a)
        s->steps[a] = signed32(sn_read_u32(p + 8 + i * 16 + a * 4));
      c->head = (c->head + 1) % SN_QUEUE_CAPACITY;
      ++c->fill;
    }
    c->accepted_frame = first + count - 1;
    return SN_OK;
  }
  case SN_START:
    if (n || !c->armed || !c->fill || c->mode != SN_IDLE)
      return SN_STATE;
    memset(c->current.steps, 0, sizeof(c->current.steps));
    c->mode = SN_STREAM;
    return SN_OK;
  case SN_END:
    if (n || !c->armed || (c->mode != SN_STREAM && c->mode != SN_IDLE))
      return SN_STATE;
    c->end = 1;
    return SN_OK;
  default:
    return SN_UNSUPPORTED;
  }
}
uint32_t sn_controller_command(sn_controller *c, const sn_packet *r,
                               uint64_t peer) {
  if (!r->sequence)
    return SN_SEQUENCE;
  if (r->command == SN_CLAIM) {
    if (r->size || !r->session || r->sequence != 1)
      return SN_RANGE;
    if (c->session && (c->peer != peer || c->session != r->session))
      return SN_OWNER;
    if (!c->session) {
      sn_controller_fault(c, SN_FAULT_NONE);
      c->session = r->session;
      c->peer = peer;
      c->configured = 0;
      c->sequence = 0;
      c->accepted_frame = c->executed_frame = c->executed_line = 0;
      c->probe_state = SN_PROBE_NONE;
      c->probe_id = 0;
      memset(c->probe_position, 0, sizeof(c->probe_position));
    }
  }
  if (!c->session || c->session != r->session || c->peer != peer)
    return SN_OWNER;
  if (r->sequence == c->sequence) {
    if (c->fingerprint != r->fingerprint)
      return SN_SEQUENCE;
    c->last_seen_ms = c->now_ms;
    return c->result;
  }
  if (r->sequence != c->sequence + 1u)
    return SN_SEQUENCE;
  uint32_t result = r->command == SN_CLAIM ? SN_OK : execute(c, r);
  c->sequence = r->sequence;
  c->fingerprint = r->fingerprint;
  c->result = result;
  c->last_seen_ms = c->now_ms;
  return result;
}
void sn_controller_complete(sn_controller *c, const int32_t actual[3],
                            int complete) {
  for (unsigned i = 0; i < 3; ++i)
    c->position[i] += actual[i];
  if (!c->pending)
    return;
  int valid = complete;
  for (unsigned i = 0; i < 3; ++i)
    if (actual[i] != c->current.steps[i])
      valid = 0;
  if (!valid) {
    sn_controller_fault(c, SN_FAULT_TIMING);
    return;
  }
  if (c->current.id) {
    c->executed_frame = c->current.id;
    c->executed_line = c->current.line;
  }
  c->pending = 0;
}
void sn_controller_probe_hit(sn_controller *c, const int32_t residual[3]) {
  if (!c->armed || c->mode != SN_PROBING || c->probe_state != SN_PROBE_ACTIVE)
    return;
  for (unsigned i = 0; i < 3; ++i) {
    c->position[i] += residual[i];
    c->probe_position[i] = c->probe_brake_start[i] = c->position[i];
  }
  c->probe_state = SN_PROBE_HIT;
  c->pending = 0;
  c->mode = SN_PROBE_BRAKING;
  c->probe_progress = fabs(
      (double)(c->position[c->probe_major] - c->probe_start[c->probe_major]));
  c->probe_brake_progress = 0;
}
static void probe_tick(sn_controller *c, int32_t steps[3]) {
  const int braking = c->mode == SN_PROBE_BRAKING;
  if (!braking && c->probe_progress >= c->probe_length) {
    c->probe_state = SN_PROBE_MISS;
    sn_controller_fault(c, SN_FAULT_PROBE_MISS);
    return;
  }
  if (braking && !c->probe_speed) {
    clear_motion(c); /* Preserve HIT, its token and captured coordinates. */
    return;
  }
  double old = c->probe_speed;
  double change = c->probe_acceleration * .001;
  double remaining = c->probe_length - c->probe_progress;
  double desired =
      braking ? 0
              : fmin(c->probe_max_speed,
                     sqrt(2 * c->probe_acceleration * fmax(0, remaining)));
  double diff = desired - old;
  if (diff > change)
    diff = change;
  if (diff < -change)
    diff = -change;
  c->probe_speed += diff;
  double distance = (old + c->probe_speed) * .0005;
  if (distance >= remaining) {
    distance = fmax(0, remaining);
    c->probe_speed = 0;
  }
  c->probe_progress += distance;
  if (braking)
    c->probe_brake_progress += distance;
  double progress = braking ? c->probe_brake_progress : c->probe_progress;
  for (unsigned i = 0; i < 3; ++i) {
    int64_t start = braking ? c->probe_brake_start[i] : c->probe_start[i];
    int64_t goal =
        start + (int64_t)round(c->probe_delta[i] * progress / c->probe_length);
    int64_t end = c->probe_start[i] + c->probe_delta[i];
    if (c->probe_delta[i] > 0 && goal > end)
      goal = end;
    if (c->probe_delta[i] < 0 && goal < end)
      goal = end;
    int64_t difference = goal - c->position[i];
    if (difference < -SN_MAX_STEPS_TICK || difference > SN_MAX_STEPS_TICK) {
      sn_controller_fault(c, SN_FAULT_TIMING);
      memset(steps, 0, 3 * sizeof(*steps));
      return;
    }
    steps[i] = c->current.steps[i] = (int32_t)difference;
  }
  c->current.id = c->current.line = 0;
  c->pending = 1;
}
static void home_tick(sn_controller *c) {
  unsigned a = c->home_axis;
  sn_axis_config *v = &c->config.axis[a];
  int active = c->home_hit || (c->inputs & (1u << v->home_input)) != 0;
  uint64_t distance = (uint64_t)llabs(c->position[a] - c->home_origin);
  if (distance > v->home_travel) {
    sn_controller_fault(c, SN_FAULT_HOME_TRAVEL);
    return;
  }
  if (c->home_phase == 1 && active) {
    c->home_hit = 0;
    c->home_phase = 2;
    c->velocity[a] = c->fraction[a] = 0;
    c->home_origin = c->position[a];
  } else if ((c->home_phase == 2 || c->home_phase == 4) && !active &&
             distance >= v->backoff) {
    if (c->home_phase == 4) {
      c->homed |= (uint8_t)(1u << a);
      clear_motion(c);
      return;
    }
    c->home_phase = 3;
    c->velocity[a] = c->fraction[a] = 0;
    c->home_origin = c->position[a];
  } else if (c->home_phase == 3 && active) {
    c->position[a] = v->home_offset;
    c->home_origin = c->position[a];
    c->home_hit = 0;
    c->home_phase = 4;
    c->velocity[a] = c->fraction[a] = 0;
  }
  double speed = c->home_phase == 3 ? v->home_slow : v->home_fast;
  c->target[a] = speed * v->home_direction *
                 ((c->home_phase == 2 || c->home_phase == 4) ? -1 : 1);
}
void sn_controller_tick(sn_controller *c, uint8_t raw, int32_t steps[3]) {
  memset(steps, 0, 3 * sizeof(*steps));
  ++c->now_ms;
  c->raw_inputs = raw & 127;
  c->inputs = c->raw_inputs ^ c->config.input_invert;
  if (c->session && c->now_ms - c->last_seen_ms > c->config.watchdog_ms) {
    sn_controller_fault(c, SN_FAULT_WATCHDOG);
    c->session = 0;
    c->peer = 0;
    c->configured = 0;
    return;
  }
  if (c->armed && (c->inputs & 1)) {
    sn_controller_fault(c, SN_FAULT_ESTOP);
    return;
  }
  if (!c->armed || c->pending)
    return;
  if (c->mode == SN_HOMING)
    home_tick(c);
  if (!c->armed)
    return;
  if (c->mode == SN_DWELLING) {
    if (c->dwell_ticks)
      --c->dwell_ticks;
    if (!c->dwell_ticks)
      clear_motion(c);
    return;
  }
  if (c->mode == SN_POSITIONING) {
    unsigned a = c->move_axis;
    double remaining = (double)(c->move_target - c->position[a]);
    if (!remaining) {
      clear_motion(c);
      return;
    }
    double limit = sqrt(2 * c->config.axis[a].acceleration * fabs(remaining));
    if (fabs(c->target[a]) > limit)
      c->target[a] = remaining > 0 ? limit : -limit;
  }
  if (c->probe_cancel &&
      (c->mode == SN_PROBING || c->mode == SN_PROBE_BRAKING)) {
    if (c->mode == SN_PROBING) {
      const int32_t residual[3] = {0, 0, 0};
      sn_controller_probe_hit(c, residual);
    }
    c->probe_state = SN_PROBE_CANCELLED;
    memset(c->probe_position, 0, sizeof(c->probe_position));
    c->probe_cancel = 0;
  }
  if (c->mode == SN_PROBING && (c->inputs & (1u << c->probe_input))) {
    const int32_t residual[3] = {0, 0, 0};
    sn_controller_probe_hit(c, residual);
  }
  if (c->mode == SN_PROBING || c->mode == SN_PROBE_BRAKING) {
    probe_tick(c, steps);
  } else if (c->mode == SN_STREAM) {
    if (!c->fill) {
      if (c->end) {
        clear_motion(c);
        return;
      }
      sn_controller_fault(c, SN_FAULT_UNDERFLOW);
      return;
    }
    c->current = c->queue[c->tail];
    c->tail = (c->tail + 1) % SN_QUEUE_CAPACITY;
    --c->fill;
    memcpy(steps, c->current.steps, 3 * sizeof(*steps));
    c->pending = 1;
  } else if (c->mode == SN_JOGGING || c->mode == SN_HOMING ||
             c->mode == SN_STOPPING || c->mode == SN_POSITIONING) {
    int moving = 0;
    for (unsigned i = 0; i < 3; ++i) {
      double change = c->config.axis[i].acceleration * .001,
             diff = c->target[i] - c->velocity[i];
      double old = c->velocity[i];
      if (diff > change)
        diff = change;
      else if (diff < -change)
        diff = -change;
      c->velocity[i] += diff;
      c->fraction[i] += (old + c->velocity[i]) * .0005;
      steps[i] = (int32_t)c->fraction[i];
      c->fraction[i] -= steps[i];
      if (c->velocity[i] || c->target[i])
        moving = 1;
      c->current.steps[i] = steps[i];
    }
    c->current.id = 0;
    c->current.line = 0;
    c->pending = 1;
    if (c->mode == SN_POSITIONING) {
      unsigned a = c->move_axis;
      int64_t remaining = c->move_target - c->position[a];
      if ((remaining > 0 && steps[a] > remaining) ||
          (remaining < 0 && steps[a] < remaining)) {
        steps[a] = (int32_t)remaining;
        c->current.steps[a] = steps[a];
        c->velocity[a] = c->fraction[a] = 0;
      }
    }
    if (!moving && (c->mode == SN_JOGGING || c->mode == SN_STOPPING))
      c->mode = SN_IDLE;
  }
  for (unsigned i = 0; i < 3; ++i) {
    if (c->config.axis[i].limit_input < 7 &&
        (c->inputs & (1u << c->config.axis[i].limit_input)) && steps[i] &&
        !(c->mode == SN_HOMING && c->home_axis == i &&
          c->config.axis[i].limit_input == c->config.axis[i].home_input)) {
      sn_controller_fault(c, SN_FAULT_LIMIT);
      memset(steps, 0, 3 * sizeof(*steps));
      return;
    }
  }
}
void sn_controller_status(const sn_controller *c,
                          uint8_t p[SN_CONTROL_STATUS_SIZE], uint32_t result) {
  memset(p, 0, SN_CONTROL_STATUS_SIZE);
  sn_write_u32(p, result);
  sn_write_u32(p + 4, c->sequence);
  sn_write_u32(p + 8, c->accepted_frame);
  sn_write_u32(p + 12, c->executed_frame);
  sn_write_u32(p + 16, c->executed_line);
  sn_write_u32(p + 20, c->epoch);
  sn_write_u32(p + 24, c->now_ms);
  p[28] = c->fill & 255;
  p[29] = (uint8_t)(c->fill >> 8);
  p[30] = (SN_QUEUE_CAPACITY - c->fill) & 255;
  p[31] = (uint8_t)((SN_QUEUE_CAPACITY - c->fill) >> 8);
  p[32] = c->mode;
  p[33] = c->fault;
  p[34] = c->armed;
  p[35] = c->configured;
  p[36] = c->inputs;
  p[37] = c->outputs;
  p[38] = c->homed;
  p[39] = c->home_phase;
  p[40] = (uint8_t)c->pwm;
  p[41] = (uint8_t)(c->pwm >> 8);
  p[42] = c->end;
  p[43] = c->pending;
  for (unsigned i = 0; i < 3; ++i) {
    sn_write_i64(p + 48 + i * 8, c->position[i]);
    sn_write_i64(p + 72 + i * 8, c->probe_position[i]);
  }
  sn_write_u32(p + 96, c->probe_id);
  p[100] = c->probe_state;
}
