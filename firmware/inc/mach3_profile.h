#ifndef SN_FIRMWARE_MACH3_PROFILE_H
#define SN_FIRMWARE_MACH3_PROFILE_H
#include "../../common/profiles/mach3_3axis.h"
/* Selected only by -DSTEPPER_NINJA_PROFILE=mach3-3axis. */
#undef breakout_board
#define breakout_board 0
#undef raspberry_pi_spi
#define raspberry_pi_spi 0
#undef io_expanders
#define io_expanders 0
#undef stepgens
#define stepgens SN_MACH3_AXES
#undef stepgen_steps
#define stepgen_steps SN_MACH3_STEP_PINS
#undef stepgen_dirs
#define stepgen_dirs SN_MACH3_DIR_PINS
#undef step_invert
#define step_invert {0, 0, 0}
#undef encoders
#define encoders 0
#undef enc_pins
#define enc_pins {}
#undef enc_index_pins
#define enc_index_pins {}
#undef in_pins
#define in_pins SN_MACH3_INPUT_PINS
#undef in_pullup
#define in_pullup {1, 1, 1, 1, 1, 1, 1}
#undef out_pins
#define out_pins SN_MACH3_OUTPUT_PINS
#undef use_pwm
#define use_pwm 1
#undef pwm_count
#define pwm_count SN_MACH3_PWM_CHANNELS
#undef pwm_pin
#define pwm_pin {SN_MACH3_PWM_PIN}
#undef pwm_invert
#define pwm_invert {0}
#undef use_timer_interrupt
#define use_timer_interrupt 0
#undef KBMATRIX
#endif
