#ifndef CONFIG_H
#define CONFIG_H


    // ************************************************************************
    // ** This file contains the configuration for the stepper ninja project **
    // ** Do not change this file unless you know what you are doing         **
    // ** If you change this file, you may break the module functionality    **
    // ************************************************************************

    #define raspberry_pi_spi 0 // if you want to use the stepper-ninja with Raspberry Pi SPI interface, set this to 1 (need a normal pico)

    #define stepgens 4

    // if you want to use the module with pwm output, set this to 1
    #define use_pwm 1 // use pwm output removes 1 encoder

    // if you want to use the module with outputs, set this to 1
    #define use_outputs 1 // use outputs removes 1 encoder

    #define brakeout_board 0 // 1 = stepper-ninia v1.0 breakout board do not change this value the beakout board has not ready

    #define default_pulse_width 2000 // default pulse width in nanoseconds (1us) for the stepgen if not specified in the HAL configuration
    #define default_step_scale 1000 // default step scale in steps/unit for the stepgen if not specified in the HAL configuration
    #define default_pwm_frequency 10000 // default pwm frequency in Hz if not specified in the HAL configuration
    #define default_pwm_maxscale 4096 // default pwm max scale if not specified in the HAL configuration
    #define default_pwm_min_limit 0 // default pwm min limit if not specified in the HAL configuration

    #define pwm_GP 14 // PWM pin for the module (GPIO 8)
    #define pwm_invert 0 // Invert the PWM signal (1 = inverted, 0 = not inverted)

    #define step_invert ((const uint8_t[]){0, 0, 0, 0}) // step pin invert for each stepgen (0 = not inverted, 1 = inverted)

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
    #else
        #if use_pwm == 0 && use_outputs == 0
            #define encoders 4
        #elif use_pwm == 1 && use_outputs == 0
            #define encoders 3
        #elif use_pwm == 0 && use_outputs == 1
            #define encoders 2
        #elif use_pwm == 1 && use_outputs == 1
            #define encoders 1  // added 2 extra inputs (GPIO10-GPIO11)
        #endif

        #if encoders == 1
            #define in_pins {10, 11, 22, 26, 27, 28} // Free GPIO pins for inputs (GPIO 22-28)
            #define in_pins_no 6
        #elif encoders == 2
            #define in_pins {22, 26, 27, 28} // Free GPIO pins for inputs (GPIO 22-28)
            #define in_pins_no 4
        #endif // encoders < 2

    #endif // brakeout_board > 0

    #if use_outputs == 1
        #if use_pwm == 1
            #define out_pins {12, 13, 15} // output pins with pwm
            #define out_pins_no 3
        #else
            #define out_pins {12, 13, 14, 15} // output pins without pwm
            #define out_pins_no 4
        #endif // use_pwm == 1
    #endif // use_outputs == 1

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

#endif