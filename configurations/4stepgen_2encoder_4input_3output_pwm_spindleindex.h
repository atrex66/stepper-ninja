#ifndef CONFIG_H
#define CONFIG_H

    #define low 0
    #define high 1

    // ************************************************************************
    // ** This file contains the configuration for the stepper ninja project **
    // ** Do not change this file unless you know what you are doing         **
    // ** If you change this file, you may break the module functionality    **
    // ************************************************************************

    #define stepgens 4
    #define step_invert ((const uint8_t[]){0, 0, 0, 0}) // step pin invert for each stepgen (0 = not inverted, 1 = inverted)

    // if you want to use the module with pwm output, set this to 1
    #define use_pwm 1 // use pwm output removes 1 encoder
    #define pwm_GP 22 // PWM pin for the module (GPIO 22)
    #define pwm_invert 0 // Invert the PWM signal (1 = inverted, 0 = not inverted)

    // if you want to use the module with outputs, set this to 1
    #define use_outputs 1 // use outputs removes 1 encoder

    #define encoders 2  // added 2 extra inputs (GPIO10-GPIO11)
    #define spindle_encoder_index_GPIO 10
    #define spindle_encoder_active_level high

    #define in_pins {22, 26, 27, 28} // Free GPIO pins for inputs (GPIO 22-28)
    #define in_pins_no 4

    #define out_pins {12, 13, 15} // output pins with pwm
    #define out_pins_no 3

    #define default_pulse_width 2000 // default pulse width in nanoseconds (1us) for the stepgen if not specified in the HAL configuration
    #define default_step_scale 1000 // default step scale in steps/unit for the stepgen if not specified in the HAL configuration
    #define default_pwm_frequency 10000 // default pwm frequency in Hz if not specified in the HAL configuration
    #define default_pwm_maxscale 4096 // default pwm max scale if not specified in the HAL configuration
    #define default_pwm_min_limit 0 // default pwm min limit if not specified in the HAL configuration

    // **********************************************************************************
    // ** the following code cunfigures the rest of the module please do not change it **
    // ** if you not know exactly what you are doing, it can break the module          **
    // **********************************************************************************
    #define SPI_PORT        spi0
    #define PIN_MISO        16
    #define PIN_CS          17
    #define PIN_SCK         18
    #define PIN_MOSI        19
    #define PIN_RESET       20
    #define INT_PIN         21
 
    #define IODIR           0x00
    #define GPIO            0x09

    #define USE_SPI_DMA     1

    #define IRQ_PIN         21
    #define LED_PIN         PICO_DEFAULT_LED_PIN

    #define IMR_RECV      0x04
    #define Sn_IMR_RECV   0x04
    #define Sn_IR_RECV    0x04
    #define SOCKET_DHCP   0

    #define use_stepcounter 0 // Use step counter for the stepgen
    #define use_timer_interrupt 0 // Use timer interrupt for the stepgen starting, maybe eliminates servo-thread jitter experimental
    #define raspberry_pi_spi 0 // if you want to use the stepper-ninja with Raspberry Pi SPI interface, set this to 1 (need a normal pico)
    #define brakeout_board 0 // 1 = stepper-ninia v1.0 breakout board do not change this value the beakout board has not ready


#endif