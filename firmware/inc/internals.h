#ifndef INTERNALS_H
#define INTERNALS_H

    #define low 0
    #define high 1

    #define SPI_PORT        spi0
    #define PIN_MISO        16
    #define PIN_CS          17
    #define PIN_SCK         18
    #define PIN_MOSI        19
    #define PIN_RESET       20
    #define INT_PIN         21
 
    #define IODIR           0x00
    #define GPIO            0x09

    #define IRQ_PIN         21
    #define LED_PIN         PICO_DEFAULT_LED_PIN

    #define IMR_RECV      0x04
    #define Sn_IMR_RECV   0x04
    #define Sn_IR_RECV    0x04
    #define SOCKET_DHCP   0

#endif