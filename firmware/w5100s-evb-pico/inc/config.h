#ifndef CONFIG_H
#define CONFIG_H

    #define stepgens 4

    // if you want to use the module with pwm output, set this to 1
    #define use_pwm 1 // use pwm output removes 1 encoder

    // if you want to use the module with outputs, set this to 1
    #define use_outputs 1 // use outputs removes 1 encoder

    #if use_pwm == 0 && use_outputs == 0
        #define encoders 4
    #elif use_pwm == 1 && use_outputs == 0
        #define encoders 3
    #elif use_pwm == 0 && use_outputs == 1
        #define encoders 2
    #elif use_pwm == 1 && use_outputs == 1
        #define encoders 2
    #endif

    #define in_pins {22, 26, 27, 28} // Free GPIO pins for inputs (GPIO 22-28)
    
    #if use_outputs == 1
        #define out_pins_3 {12, 13, 15} // output pins with pwm
        #define out_pins_4 {12, 13, 14, 15} // output pins without pwm
    #endif

    #define pwm_GP 14 // PWM pin for the module (GPIO 8)

    #define SPI_PORT        spi0
    #define PIN_MISO        16
    #define PIN_CS          17
    #define PIN_SCK         18
    #define PIN_MOSI        19
    #define PIN_RESET       20

    #define IODIR           0x00
    #define GPIO            0x09

    #define USE_SPI_DMA     1

    #define IRQ_PIN         21
    #define LED_PIN         PICO_DEFAULT_LED_PIN

    #define IMR_RECV      0x04
    #define Sn_IMR_RECV   0x04
    #define Sn_IR_RECV    0x04
    #define SOCKET_DHCP   0

    #define TIMER_INTERVAL_US 1000  // 1ms

    #define INT_PIN 21

#endif