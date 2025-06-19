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

    #define encoders 0
    #define enc_pins {}

    #define in_pins {PIN_14, PIN_15, PIN_16, PIN_29, PIN_31, PIN_32}
    #define in_pullup {1, 1, 1, 1, 1, 1}

    #define spindle_encoder_index_GPIO PIN_32
    #define spindle_encoder_active_level high

    #define out_pins {PIN_17, PIN_20}

    #define use_pwm 1 
    #define pwm_GP PIN_14
    #define pwm_invert 0

    #define raspberry_pi_spi 0 // if you want to use the stepper-ninja with Raspberry Pi SPI interface, set this to 1 (need a normal pico)
    #if raspberry_pi_spi == 1
        #pragma message("Build for Raspberry PI SPI communication!")
        // used gpio for SPI on the RPI: 8, 9, 10, 11
        // used gpio for SPI on the PICO: 16, 17, 18, 19
        // available GPIO left side:  2,3,4,17,27,33,0,5,6,13,19,26
        // available GPIO right side: 14,15,18,23,24,25,1,12,16,20,21
        #define raspi_inputs {GP_03}
        #define raspi_outputs {GP_05}
    #endif
    // if you are using raspberry pi SPI instead of Wizchip you get the GP20, GP21 free on the PICO

    #define default_pulse_width 3000 // default pulse width in nanoseconds (1us) for the stepgen if not specified in the HAL configuration
    #define default_step_scale 1000 // default step scale in steps/unit for the stepgen if not specified in the HAL configuration
    #define default_pwm_frequency 10000 // default pwm frequency in Hz if not specified in the HAL configuration
    #define default_pwm_maxscale 4096 // default pwm max scale if not specified in the HAL configuration
    #define default_pwm_min_limit 0 // default pwm min limit if not specified in the HAL configuration
    
    #define brakeout_board 0 // 1 = stepper-ninia v1.0 breakout board do not change this value the beakout board has not ready

    // **********************************************************************************
    // ** the following code cunfigures the rest of the module please do not change it **
    // ** if you not know exactly what you are doing, it can break the module          **
    // **********************************************************************************

    #if brakeout_board > 0
        // if you use the breakout board, you need to change the pin configuration (4 stepgen, 2 encoder, 4 fast input, 1 pwm output,8 output, 16 inputs)
        #define MCP23017_ADDR   0x21
        #define MCP23008_ADDR   0x20
        #define MCP_ALL_RESET   22
        #define encoders        2
        #define in_pins         {12, 13, 15, 28} // Free GPIO pins for inputs (GPIO 22-28)
        #define in_pins_no      4 // Number of input pins
        #define I2C_SDA         26
        #define I2C_SCK         27
        #undef use_outputs
        #define use_outputs 0
    #endif // brakeout_board > 0

    #define use_stepcounter 0 // Use step counter for the stepgen
    #define use_timer_interrupt 0 // Use timer interrupt for the stepgen starting, maybe eliminates servo-thread jitter experimental
    #define debug_mode 1    // only used in Raspberry PI communications

#endif