#ifndef CONFIG_H
#define CONFIG_H
#include "internals.h"


    // *****************************************************************************
    // ** This file contains the configuration for the stepper ninja project      **
    // ** Expatria PicoBOB Deluxe A3: LinuxCNC profile, RP2040 + W5500          **
    // *****************************************************************************
    // Hardware reference:
    // https://github.com/Expatria-Technologies/PicoBOB-DLX
    // Use this file as firmware/inc/config.h, then rebuild firmware and HAL.
    // hal-driver/config.h links to that header; if using a separate copy,
    // update it to exactly the same configuration before rebuilding.

    // The Deluxe routes W5500 RESET/INT differently from WIZnet EVB boards.
    // Override only this board's defaults, including its status LED.
    #undef SPI_PORT
    #define SPI_PORT spi0
    #undef GPIO_RESET
    #define GPIO_RESET GP21
    #undef GPIO_INT
    #define GPIO_INT GP20
    #undef LED_GPIO
    #define LED_GPIO GP07

    // default network settings after you flash the PICO
    #define DEFAULT_MAC {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}
    #define DEFAULT_IP {192, 168, 0, 177}
    #define DEFAULT_PORT 8888
    #define DEFAULT_GATEWAY {192, 168, 0, 1}
    #define DEFAULT_SUBNET {255, 255, 255, 0}
    // timeout for detecting disconnection from linuxcnc
    #define DEFAULT_TIMEOUT 1000000

    // X, Y, Z, A, B. GPIO25 is A STEP, not the status LED on this board.
    #define stepgens 5
    #define stepgen_steps {22, 23, 24, 25, 26}
    #define stepgen_dirs {9, 10, 11, 12, 13}
    #define step_invert {0, 0, 0, 0, 0} // step pin invert for each stepgen (0 = not inverted, 1 = inverted)

    #define encoders 0
    #define enc_pins {}
    #define enc_index_pins {}
    #define enc_index_active_level {}

    // Y limit, X limit, HALT, probe, Z limit, pendant HALT.
    #define in_pins {1, 2, 3, 4, 5, 15}
    #define in_pullup {1, 1, 1, 1, 1, 1}

    // GPIO8: shared step-enable/coolant output. GPIO14 is reserved for PWM.
    // GPIO13 is B DIR; on common 5-axis BOBs it also controls the spindle relay.
    // To use that relay, reduce stepgens to 4, remove B's STEP/DIR/invert entries,
    // and add GPIO13 to out_pins. B DIR and relay cannot be independent outputs.
    #define out_pins {8}

   // if you want to use the module with pwm output, set this to 1
    #define use_pwm 1 // use of pwm output
    #define pwm_count 1
    #define pwm_pin {GP14} // DB25 pin 1, spindle PWM (external BOB converts to 0-10 V)
    #define pwm_invert {0} // Invert the PWM signal (1 = inverted, 0 = not inverted)
    #define default_pwm_frequency 10000 // default pwm frequency in Hz if not specified in the HAL configuration
    #define default_pwm_maxscale 4096 // default pwm max scale if not specified in the HAL configuration
    #define default_pwm_min_limit 0 // default pwm min limit if not specified in the HAL configuration

    #define default_pulse_width 2000 // default pulse width in nanoseconds (2us) unless set in HAL
    #define default_step_scale 1000 // default step scale in steps/unit for the stepgen if not specified in the HAL configuration

    #define raspberry_pi_spi 0 // if you want to use the stepper-ninja with Raspberry Pi SPI interface, set this to 1 (need a normal pico)
 
    #define breakout_board 0 // custom GPIO configuration, no StepperNinja expander board
    #define io_expanders 0 // how many IO expander you connected to the breakout board (16 Input + 8 output each max 3 io_expanders)

    #define use_timer_interrupt 0 // Use timer interrupt for the stepgen starting, maybe eliminates servo-thread jitter experimental
    #define encoder_pio_version ENCODER_PIO_SUBSTEP // 0 = old quadrature encoder PIO, 1 = substep encoder PIO

    #include "footer.h"

#endif
