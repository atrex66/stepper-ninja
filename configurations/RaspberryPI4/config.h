#ifndef CONFIG_H
#define CONFIG_H
#include "internals.h"

    // **************************************************************************
    // ** This file contains the configuration for the stepper ninja project   **
    // ** if you want to use pins instead of GPIO use PIN_1, PIN_2, PIN_4, ... **
    // **************************************************************************
    // all pin alias is defined in the internals.h if you want to use instead of using GPIO numbers

    // All GPIO form 0-15 and 22-31 are usable
    #define stepgens 4
    
    // defined with PINS
    #define stepgen_steps {PIN_1, PIN_4, PIN_6, PIN_9}
    #define stepgen_dirs {PIN_2, PIN_5, PIN_7, PIN_10}
    #define step_invert {0, 0, 0, 0, 0} // step pin invert for each stepgen (0 = not inverted, 1 = inverted)
    
    #define default_pulse_width 2000 // default pulse width in nanoseconds (1us) for the stepgen if not specified in the HAL configuration
    #define default_step_scale 1000 // default step scale in steps/unit for the stepgen if not specified in the HAL configuration

    #define encoders 1
    #define enc_pins {PIN_11} // uses 2 pins, you need to set the first pin (PIN_11 + PIN_12)
    #define enc_index_pins {PIN_NULL}  // pin the encoder index is connected (interrupt driven)
    #define enc_index_active_level {high}

    #define in_pins {PIN_29, PIN_31, PIN_32, PIN_34} // Free GPIO for inputs (GPIO 22-28)
    #define in_pullup {0, 0, 0, 0}

    #define out_pins {PIN_16, PIN_17, PIN_20}

    // if you want to use the module with pwm output, set this to 1
    #define use_pwm 1 // use of pwm output
    #define pwm_count 1
    #define pwm_pin {PIN_19} // PWM GPIO for the module (GPIO 13, GPIO 14)
    #define pwm_invert {0} // Invert the PWM signal (1 = inverted, 0 = not inverted)
    #define default_pwm_frequency 10000 // default pwm frequency in Hz if not specified in the HAL configuration
    #define default_pwm_maxscale 4096 // default pwm max scale if not specified in the HAL configuration
    #define default_pwm_min_limit 0 // default pwm min limit if not specified in the HAL configuration

    #define raspberry_pi_spi 1 // if you want to use the stepper-ninja with Raspberry Pi SPI interface, set this to 1 (need a normal pico)

    // used gpio for SPI on the RPI: 8, 9, 10, 11
    // used gpio for SPI on the PICO: 16, 17, 18, 19
    // available GPIO left side:  2,3,4,17,27,33,0,5,6,13,19,26
    // available GPIO right side: 14,15,18,23,24,25,1,12,16,20,21
    #define raspi_int_out 25
    #define raspi_inputs {2, 3, 4, 14, 15, 16, 17, 18, 20, 21, 22, 23, 24, 27}
    #define raspi_input_pullups {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    #define raspi_outputs {0, 1, 5, 6, 12, 13, 19, 26}
    // if you are using raspberry pi SPI instead of Wizchip you get the GP20, GP21 free on the PICO
    
    #define brakeout_board 0 // 1 = stepper-ninia v1.0 breakout board do not change this value the beakout board has not ready

#include "footer.h"
#endif
