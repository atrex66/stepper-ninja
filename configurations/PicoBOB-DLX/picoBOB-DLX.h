#ifndef CONFIG_H
#define CONFIG_H
#include "internals.h"


    // *****************************************************************************
    // ** This file contains the configuration for the stepper ninja project      **
    // ** if you want to use pins instead of GPIO use like PPIN_1, PPIN_2, PPIN_4 **
    // *****************************************************************************
    // default network settings after you flash the PICO
    #define DEFAULT_MAC {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}
    #define DEFAULT_IP {192, 168, 0, 177}
    #define DEFAULT_PORT 8888
    #define DEFAULT_GATEWAY {192, 168, 0, 1}
    #define DEFAULT_SUBNET {255, 255, 255, 0}
    // timeout for detecting disconnection from linuxcnc
    #define DEFAULT_TIMEOUT 1000000

    // all pin alias is defined in the internals.h is you want to use instead of using GPIO numbers

     // All GPIO form 0-15 and 22-31 are usable
    #define stepgens 5
    #define stepgen_steps {22, 23, 24, 25, 26}
    #define stepgen_dirs {9, 10, 11, 12, 13}
    #define step_invert {0, 0, 0, 0, 0} // step pin invert for each stepgen (0 = not inverted, 1 = inverted)

    #define encoders 0
    #define enc_pins {} // uses 2 pins, you need to set the first pin (8 + 9)

    #define in_pins {1, 2, 3, 4, 5, 15} // Free GPIO pins for inputs (GPIO 22-28)
    #define in_pullup {1, 1, 1, 1, 1, 1}

    #define out_pins {14, 8} // output GPIO

   // if you want to use the module with pwm output, set this to 1
    #define use_pwm 1 // use of pwm output
    #define pwm_count 1
    #define pwm_pin {GP14} // PWM GPIO for the module (GPIO 13, GPIO 14)
    #define pwm_invert {0} // Invert the PWM signal (1 = inverted, 0 = not inverted)
    #define default_pwm_frequency 10000 // default pwm frequency in Hz if not specified in the HAL configuration
    #define default_pwm_maxscale 4096 // default pwm max scale if not specified in the HAL configuration
    #define default_pwm_min_limit 0 // default pwm min limit if not specified in the HAL configuration

    #define default_pulse_width 2000 // default pulse width in nanoseconds (1us) for the stepgen if not specified in the HAL configuration
    #define default_step_scale 1000 // default step scale in steps/unit for the stepgen if not specified in the HAL configuration

    #define raspberry_pi_spi 0 // if you want to use the stepper-ninja with Raspberry Pi SPI interface, set this to 1 (need a normal pico)
 
    #define brakeout_board 0 // 1 = stepper-ninia v1.0 breakout board do not change this value the beakout board has not ready
    #define io_expanders 0 // how many IO expander you connected to the breakout board (16 Input + 8 output each max 3 io_expanders)

    #define use_stepcounter 0 // Use step counter for the stepgen
    #define use_timer_interrupt 0 // Use timer interrupt for the stepgen starting, maybe eliminates servo-thread jitter experimental
    #define debug_mode 0    // only used in Raspberry PI communications

#endif
