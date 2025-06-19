#ifndef INTERNALS_H
#define INTERNALS_H

    #define low 0
    #define high 1


// Raspberry PI GPIO definition
#define    GP_03   2  /*!< B+, Pin J8-03 */
#define    GP_05   3  /*!< B+, Pin J8-05 */
#define    GP_07   4  /*!< B+, Pin J8-07 */
//#define    GP_08  14  /*!< B+, Pin J8-08, defaults to alt function 0 UART0_TXD */
//#define    GP_10  15  /*!< B+, Pin J8-10, defaults to alt function 0 UART0_RXD */
//#define    GP_11  17  /*!< B+, Pin J8-11 */
#define    GP_12  18  /*!< B+, Pin J8-12, can be PWM channel 0 in ALT FUN 5 */
#define    GP_13  27  /*!< B+, Pin J8-13 */
#define    GP_15  22  /*!< B+, Pin J8-15 */
#define    GP_16  23  /*!< B+, Pin J8-16 */
#define    GP_18  24  /*!< B+, Pin J8-18 */
#define    GP_19  10  /*!< B+, Pin J8-19, MOSI when SPI0 in use */
#define    GP_21   9  /*!< B+, Pin J8-21, MISO when SPI0 in use */
#define    GP_22  25  /*!< B+, Pin J8-22 */
#define    GP_23  11  /*!< B+, Pin J8-23, CLK when SPI0 in use */
#define    GP_24   8  /*!< B+, Pin J8-24, CE0 when SPI0 in use */
#define    GP_26   7  /*!< B+, Pin J8-26, CE1 when SPI0 in use */
#define    GP_29   5  /*!< B+, Pin J8-29  */
#define    GP_31   6  /*!< B+, Pin J8-31  */
#define    GP_32  12  /*!< B+, Pin J8-32  */
#define    GP_33  13  /*!< B+, Pin J8-33  */
#define    GP_35  19  /*!< B+, Pin J8-35, can be PWM channel 1 in ALT FUN 5 */
#define    GP_36  16  /*!< B+, Pin J8-36  */
#define    GP_37  26  /*!< B+, Pin J8-37  */
#define    GP_38  20  /*!< B+, Pin J8-38  */
#define    GP_40  21   /*!< B+, Pin J8-40  */

// SPI port definition (GPIO)
#define SPI_PORT        spi0
#define GPIO_MISO        16
#define GPIO_CS          17
#define GPIO_SCK         18
#define GPIO_MOSI        19
#define GPIO_RESET       20
#define GPIO_INT         21

// PICO pin to GPIO
#define PIN_1 0
#define PIN_2 1
#define PIN_4 2
#define PIN_5 3
#define PIN_6 4
#define PIN_7 5
#define PIN_9 6
#define PIN_10 7
#define PIN_11 8
#define PIN_12 9
#define PIN_14 10
#define PIN_15 11
#define PIN_16 12
#define PIN_17 13
#define PIN_19 14
#define PIN_20 15
#define PIN_21 GPIO_MISO
#define PIN_22 GPIO_CS
#define PIN_23 GPIO_SCK
#define PIN_25 GPIO_MOSI
#define PIN_26 GPIO_RESET
#define PIN_27 GPIO_INT
//#define PIN_26 20
//#define PIN_27 21
#define PIN_29 22
#define PIN_31 26
#define PIN_32 27
#define PIN_34 28 

#define LED_GPIO         PICO_DEFAULT_LED_PIN

#define IMR_RECV      0x04
#define Sn_IMR_RECV   0x04
#define Sn_IR_RECV    0x04
#define SOCKET_DHCP   0


#endif // INTERNALS_H