
settings = {
    "default_mac": "{0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}",
    "default_ip": "{192.168.0.177}",
    "port": 8888,
    "gateway": "{192.168.0.1}",
    "subnet": "{255.255.255.0}",
    "timeout": 100000,
    "stepgens": 0,
    "stepgen_steps": "{0, 2, 4, 6}",
    "stepgen_dirs": "{1, 3, 5, 7}",
    "stepgen_invert": "{0, 0, 0, 0}",
    "encoders": 1,
    "encoder_pins": "{8}",
    "enc_indexes": "{10}",
    "index_level_high_low": "{1}",
    "input_pins": "{11, 12, 13}",
    "input_pullups": "{1, 1, 1}",
    "output_pins": "{22, 26,27}",
    "use_pwm": 1,
    "pwm_count": 1,
    "pwm_pins": "{14}",
    "pwm_invert": "{0}"
}

config:str = """
#ifndef CONFIG_H 
#define CONFIG_H 
#include "internals.h" 

    // **************************************************************************
    // ** This file contains the configuration for the stepper ninja project   **
    // ** if you want to use pins instead of GPIO use PIN_1, PIN_2, PIN_4, ... **
    // **************************************************************************

    // default network settings after you flash the PICO
    #define DEFAULT_MAC {default_mac}
    #define DEFAULT_IP {default_ip}
    #define DEFAULT_PORT {port}
    #define DEFAULT_GATEWAY {gateway}
    #define DEFAULT_SUBNET {subnet}
    // timeout for detecting disconnection from linuxcnc
    #define DEFAULT_TIMEOUT {timeout}

    // switch off this option (breakout_board 0) to use custom configuration
    #define breakout_board 0 // 1 = stepper-ninia v1.0 breakout board

    // all pin alias is defined in the internals.h if you want to use instead of using GPIO numbers
    // All GPIO form 0-15 and 22-31 are usable
    #define stepgens {stepgens}
    
    // defined with PINS
    #define stepgen_steps {stepgen_steps}
    #define stepgen_dirs {stepgen_dirs}
    #define step_invert {stepgen_invert} // step pin invert for each stepgen (0 = not inverted, 1 = inverted)
    
    #define default_pulse_width 2000 // default pulse width in nanoseconds (1us) for the stepgen if not specified in the HAL configuration
    #define default_step_scale 1000 // default step scale in steps/unit for the stepgen if not specified in the HAL configuration

    #define encoders {encoders}
    #define enc_pins {encoder_pins} // uses 2 pins, you need to set the first pin (PIN_11 + PIN_12)
    #define enc_index_pins {enc_indexes}  // pin the encoder index is connected (interrupt driven)
    #define enc_index_active_level {index_level_high_low}

    #define in_pins {input_pins} // Free GPIO for inputs (GPIO 22-28)
    #define in_pullup {input_pullups}

    #define out_pins {output_pins}

    // if you want to use the module with pwm output, set this to 1
    #define use_pwm {use_pwm} // use of pwm output
    #define pwm_count {pwm_count}
    #define pwm_pin {pwm_pins} // PWM GPIO for the module (GPIO 13, GPIO 14)
    #define pwm_invert {pwm_invert} // Invert the PWM signal (1 = inverted, 0 = not inverted)
    #define default_pwm_frequency 10000 // default pwm frequency in Hz if not specified in the HAL configuration
    #define default_pwm_maxscale 4096 // default pwm max scale if not specified in the HAL configuration
    #define default_pwm_min_limit 0 // default pwm min limit if not specified in the HAL configuration
"""
config_cont = """
    
    #define raspberry_pi_spi 0 // if you want to use the stepper-ninja with Raspberry Pi SPI interface, set this to 1 (need a normal pico)

    // used gpio for SPI on the RPI: 8, 9, 10, 11
    // used gpio for SPI on the PICO: 16, 17, 18, 19
    // available GPIO left side:  2,3,4,17,27,33,0,5,6,13,19,26
    // available GPIO right side: 14,15,18,23,24,25,1,12,16,20,21
    #define raspi_int_out 25
    #define raspi_inputs {2, 3, 4, 14, 15, 16, 17, 18, 20, 21, 22, 23, 24, 27}
    #define raspi_input_pullups {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    #define raspi_outputs {0, 1, 5, 6, 12, 13, 19, 26}
    // if you are using raspberry pi SPI instead of Wizchip you get the GP20, GP21 free on the PICO

    #define KBMATRIX

#include "footer.h"
#include "kbmatrix.h"
#endif
"""

def apply_settings():
    global settings, config
    return config.format(**settings) + config_cont
