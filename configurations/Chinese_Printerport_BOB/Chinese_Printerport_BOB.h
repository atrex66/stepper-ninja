#ifndef CONFIG_H
#define CONFIG_H
#include "internals.h"

    // **************************************************************************    
    // **           Chinese printerport breakout board settings                **
    // **************************************************************************
    #define stepgens 4
    
    #define stepgen_steps {PIN_1, PIN_4, PIN_6, PIN_9}
    #define stepgen_dirs {PIN_2, PIN_5, PIN_7, PIN_10}
    #define step_invert {0, 0, 0, 0}

    #define default_pulse_width 3000 // default pulse width in nanoseconds (1us) for the stepgen if not specified in the HAL configuration
    #define default_step_scale 1000 // default step scale in steps/unit for the stepgen if not specified in the HAL configuration

    #define encoders 0
    #define enc_pins {}
    #define enc_index_pins {PIN_NULL}  // pin the encoder index is connected (interrupt driven)
    #define enc_index_active_level {high}

    #define in_pins {PIN_14, PIN_15, PIN_16, PIN_29, PIN_31, PIN_32}
    #define in_pullup {1, 1, 1, 1, 1, 1}

    #define out_pins {PIN_17, PIN_20}

    #define use_pwm 1 
    #define pwm_count 1
    #define pwm_pin {PIN_14} // PWM GPIO for the module (GPIO 13, GPIO 14)
    #define pwm_invert {0} // Invert the PWM signal (1 = inverted, 0 = not inverted)
    #define default_pwm_frequency 10000 // default pwm frequency in Hz if not specified in the HAL configuration
    #define default_pwm_maxscale 4096 // default pwm max scale if not specified in the HAL configuration
    #define default_pwm_min_limit 0 // default pwm min limit if not specified in the HAL configuration

    #define raspberry_pi_spi 0 // if you want to use the stepper-ninja with Raspberry Pi SPI interface, set this to 1 (need a normal pico)
    #define brakeout_board 0 // 1 = stepper-ninia v1.0 breakout board do not change this value the beakout board has not ready

    #define use_stepcounter 0 // Use step counter for the stepgen
    #define use_timer_interrupt 0 // Use timer interrupt for the stepgen starting, maybe eliminates servo-thread jitter experimental
    #define debug_mode 0    // only used in Raspberry PI communications

#endif