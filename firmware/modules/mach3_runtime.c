#include "mach3_runtime.h"
#include "../../common/motion/controller.h"
#include "config.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "mach3_step.pio.h"
#include "pico/stdlib.h"
#include "step_counter.pio.h"
#include "udp_transport.h"
static sn_controller controller;
static struct repeating_timer timer;
static const uint8_t step_pins[3] = SN_MACH3_STEP_PINS,
                     dir_pins[3] = SN_MACH3_DIR_PINS;
static const uint8_t in_pins_map[7] = SN_MACH3_INPUT_PINS,
                     out_pins_map[6] = SN_MACH3_OUTPUT_PINS;
static uint32_t previous_count[3], last_pwm_hz;
static uint active_mask, program_offset;
static uint64_t last_tick_us, last_stop_us;
static volatile bool armed_status;
static uint8_t input_bits(void) {
  uint32_t raw = gpio_get_all();
  uint8_t bits = 0;
  for (unsigned i = 0; i < 7; ++i)
    bits |= (uint8_t)(((raw >> in_pins_map[i]) & 1u) << i);
  return bits;
}
static void stop_steps(void) {
  pio_set_sm_mask_enabled(pio0, 7, false);
  for (unsigned i = 0; i < 3; ++i) {
    pio_sm_clear_fifos(pio0, i);
    pio_sm_restart(pio0, i);
    pio_sm_exec(pio0, i, pio_encode_set(pio_pins, 0));
    pio_sm_exec(pio0, i, pio_encode_jmp(program_offset));
    pio_interrupt_clear(pio0, i);
  }
  active_mask = 0;
  last_stop_us = time_us_64();
}
static void apply_io(void) {
  for (unsigned i = 0; i < 6; ++i)
    gpio_put(out_pins_map[i],
             ((controller.outputs ^ controller.config.output_invert) >> i) &
                 1u);
  uint slice = pwm_gpio_to_slice_num(SN_MACH3_PWM_PIN);
  if (last_pwm_hz != controller.config.pwm_hz) {
    pwm_set_enabled(slice, false);
    pwm_set_clkdiv(slice, 1.0f);
    pwm_set_wrap(
        slice,
        (uint16_t)(clock_get_hz(clk_sys) / controller.config.pwm_hz - 1));
    last_pwm_hz = controller.config.pwm_hz;
  }
  uint32_t period = clock_get_hz(clk_sys) / last_pwm_hz;
  uint16_t level =
      (uint16_t)((period * (uint64_t)controller.pwm + 32767) / 65535);
  pwm_set_gpio_level(SN_MACH3_PWM_PIN, level);
  pwm_set_enabled(slice, controller.armed && controller.pwm != 0);
  armed_status = controller.armed != 0;
}
/* Read a fresh PIO1 sample after FIFO drain. Used by both timer and probe IRQ.
 */
static void sample_steps(int32_t actual[3], int *done) {
  *done = 1;
  for (unsigned i = 0; i < 3; ++i) {
    uint32_t count = (uint32_t)step_counter_get_count(pio1, i);
    uint32_t difference = count - previous_count[i];
    int32_t delta = difference <= INT32_MAX
                        ? (int32_t)difference
                        : -1 - (int32_t)(UINT32_MAX - difference);
    previous_count[i] = count;
    actual[i] = (controller.config.dir_invert & (1u << i)) ? delta : -delta;
    if ((active_mask & (1u << i)) && !pio_interrupt_get(pio0, i))
      *done = 0;
  }
}
static void input_irq(uint gpio, uint32_t events) {
  (void)gpio;
  (void)events;
  if (!controller.armed)
    return;
  uint8_t active = input_bits() ^ controller.config.input_invert;
  if (active & 1) {
    sn_controller_fault(&controller, SN_FAULT_ESTOP);
    stop_steps();
    apply_io();
    return;
  }
  for (unsigned i = 0; i < 3; ++i) {
    sn_axis_config *c = &controller.config.axis[i];
    if (controller.mode == SN_HOMING && controller.home_axis == i &&
        (controller.home_phase == 1 || controller.home_phase == 3) &&
        (active & (1u << c->home_input))) {
      stop_steps();
      controller.pending = 0;
      controller.home_hit = 1;
      controller.velocity[i] = controller.fraction[i] = 0;
    } else if (c->limit_input < 7 && (active & (1u << c->limit_input)) &&
               ((controller.pending && controller.current.steps[i]) ||
                controller.velocity[i] ||
                ((controller.mode == SN_PROBING ||
                  controller.mode == SN_PROBE_BRAKING) &&
                 controller.probe_delta[i])) &&
               !(controller.mode == SN_HOMING && controller.home_axis == i &&
                 c->limit_input == c->home_input)) {
      sn_controller_fault(&controller, SN_FAULT_LIMIT);
      stop_steps();
      apply_io();
      return;
    }
  }
  if (controller.mode == SN_PROBING &&
      (active & (1u << controller.probe_input))) {
    /* Stop all STEP generators first: counter snapshots then share a stable
     * endpoint. Read and advance previous_count here, so timer cannot double
     * count the partial pulse train or falsely ACK it as a complete frame. */
    stop_steps();
    int32_t residual[3];
    int done;
    sample_steps(residual, &done);
    sn_controller_probe_hit(&controller, residual);
  }
}
static bool tick(struct repeating_timer *t) {
  (void)t;
  uint64_t now = time_us_64();
  int done = 1;
  int32_t actual[3];
  sample_steps(actual, &done);
  sn_controller_complete(&controller, actual, done);
  if (last_tick_us && now - last_tick_us > 1500)
    sn_controller_fault(&controller, SN_FAULT_TIMING);
  last_tick_us = now;
  int32_t steps[3];
  sn_controller_tick(&controller, input_bits(), steps);
  if (!controller.armed) {
    stop_steps();
    apply_io();
    return true;
  }
  active_mask = 0;
  uint32_t high_count = (controller.config.pulse_ns + 99) / 100 - 2;
  /* Keep each burst within the first 900 us of this timer callback, including
   * its computation time. Reserve 30 us for FIFO/GPIO staging. Normal frame
   * boundaries provide >=100 us DIR hold; after an asynchronous stop enforce
   * at least 20 us before changing direction. */
  uint64_t stopped_elapsed = time_us_64() - last_stop_us;
  if (last_stop_us && stopped_elapsed < 20)
    busy_wait_us_32((uint32_t)(20 - stopped_elapsed));
  uint64_t elapsed = time_us_64() - now;
  uint32_t setup_us = (controller.config.dir_setup_ns + 999) / 1000;
  if (elapsed + setup_us + 30 >= 900) {
    sn_controller_fault(&controller, SN_FAULT_TIMING);
    stop_steps();
    apply_io();
    return true;
  }
  uint32_t available = (uint32_t)(900 - elapsed - setup_us - 30) * 10;
  for (unsigned i = 0; i < 3; ++i) {
    if (!steps[i])
      continue;
    uint32_t count = (uint32_t)(steps[i] < 0 ? -steps[i] : steps[i]);
    uint32_t period = available / count;
    if (period < high_count + 7 + (controller.config.pulse_ns + 99) / 100) {
      sn_controller_fault(&controller, SN_FAULT_TIMING);
      stop_steps();
      apply_io();
      return true;
    }
    pio_sm_set_enabled(pio0, i, false);
    pio_interrupt_clear(pio0, i);
    gpio_put(dir_pins[i],
             (steps[i] > 0) ^ ((controller.config.dir_invert >> i) & 1));
    pio_sm_put(pio0, i, count - 1);
    pio_sm_put(pio0, i, high_count);
    pio_sm_put(pio0, i, period - high_count - 7);
    active_mask |= 1u << i;
  }
  if (active_mask) {
    busy_wait_us_32((controller.config.dir_setup_ns + 999) / 1000);
    pio_enable_sm_mask_in_sync(pio0, active_mask);
  }
  apply_io();
  return true;
}
void sn_mach3_runtime_init(void) {
  sn_controller_init(&controller);
  program_offset = pio_add_program(pio0, &mach3_step_program);
  pio_add_program_at_offset(pio1, &step_counter_program, 0);
  for (unsigned i = 0; i < 3; ++i) {
    pio_sm_claim(pio0, i);
    pio_sm_claim(pio1, i);
    gpio_init(dir_pins[i]);
    gpio_set_dir(dir_pins[i], GPIO_OUT);
    gpio_put(dir_pins[i], 0);
    pio_gpio_init(pio0, step_pins[i]);
    pio_sm_config pulse = mach3_step_program_get_default_config(program_offset);
    sm_config_set_set_pins(&pulse, step_pins[i], 1);
    sm_config_set_clkdiv(&pulse, clock_get_hz(clk_sys) / 10000000.0f);
    pio_sm_set_consecutive_pindirs(pio0, i, step_pins[i], 1, true);
    pio_sm_init(pio0, i, program_offset, &pulse);
    /* PIO1 only samples pads: it must not steal PIO0's GPIO mux. */
    pio_sm_config counter = step_counter_program_get_default_config(0);
    sm_config_set_in_pins(&counter, step_pins[i]);
    sm_config_set_in_shift(&counter, false, false, 32);
    pio_sm_init(pio1, i, 0, &counter);
    pio_sm_exec(pio1, i, pio_encode_set(pio_y, 0));
    pio_sm_set_enabled(pio1, i, true);
    previous_count[i] = (uint32_t)step_counter_get_count(pio1, i);
  }
  gpio_set_function(SN_MACH3_PWM_PIN, GPIO_FUNC_PWM);
  stop_steps();
  apply_io();
  gpio_set_irq_enabled_with_callback(
      in_pins_map[0], GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, input_irq);
  for (unsigned i = 1; i < 7; ++i)
    gpio_set_irq_enabled(in_pins_map[i],
                         GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  if (!add_repeating_timer_us(-1000, tick, NULL, &timer))
    panic("Mach3 timer allocation failed");
}
bool sn_mach3_runtime_armed(void) { return armed_status; }
void sn_mach3_runtime_poll(void) {
  static uint8_t rx[SN_CONTROL_MAX_PACKET], tx[SN_CONTROL_MAX_PACKET];
  sn_udp_peer peer;
  int32_t size = sn_udp_receive(0, rx, sizeof(rx), &peer);
  if (size <= 0)
    return;
  if (sn_mach3_probe_reply(rx, (size_t)size, tx)) {
    sn_udp_send(0, tx, SN_MACH3_PROBE_REPLY_SIZE, &peer);
    return;
  }
  sn_packet packet;
  if (!sn_control_decode(rx, (size_t)size, &packet))
    return;
  uint64_t identity =
      (uint64_t)sn_read_u32(peer.ip) | ((uint64_t)peer.port << 32);
  uint8_t report[SN_CONTROL_STATUS_SIZE];
  uint32_t irq = save_and_disable_interrupts();
  controller.raw_inputs = input_bits();
  controller.inputs = controller.raw_inputs ^ controller.config.input_invert;
  uint32_t before_epoch = controller.epoch;
  uint32_t result = sn_controller_command(&controller, &packet, identity);
  if (controller.epoch != before_epoch)
    stop_steps();
  apply_io();
  sn_controller_status(&controller, report, result);
  restore_interrupts(irq);
  size_t length =
      sn_control_encode(tx, packet.command | 0x8000u, packet.session,
                        packet.sequence, report, sizeof(report));
  sn_udp_send(0, tx, (uint16_t)length, &peer);
}
