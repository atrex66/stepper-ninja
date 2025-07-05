#ifndef INTERNALS_H
#define INTERNALS_H

#ifndef __linux__
#include "hardware/i2c.h"
#endif

#define version "1.1"

#define low 0
#define high 1

// defines for rp2040/rp2350
#define GP_NULL 255
#define GP00 0
#define GP01 1
#define GP02 2
#define GP03 3
#define GP04 4
#define GP05 5
#define GP06 6
#define GP07 7
#define GP08 8
#define GP09 9
#define GP10 10
#define GP11 11
#define GP12 12
#define GP13 13
#define GP14 14
#define GP15 15
#define GP16 16
#define GP17 17
#define GP18 18
#define GP19 19
#define GP20 20
#define GP21 21
#define GP22 22
#define GP23 23
#define GP24 24
#define GP25 25
#define GP26 26
#define GP27 27
#define GP28 28
#define GP29 29
#define GP30 30
#define GP31 31
// defines only for rp2350 (qfn-80)
#define GP32 32
#define GP33 33
#define GP34 34
#define GP35 35
#define GP36 36
#define GP37 38
#define GP38 38
#define GP39 39
#define GP40 40
#define GP41 41
#define GP42 42
#define GP43 43
#define GP44 44
#define GP45 45
#define GP46 46
#define GP47 47

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
#define GPIO_MISO       GP16
#define GPIO_CS         GP17
#define GPIO_SCK        GP18
#define GPIO_MOSI       GP19
#define GPIO_RESET      GP20
#define GPIO_INT        GP21

// PICO pin to GPIO
#define PIN_NULL GP_NULL
#define PIN_1 GP00
#define PIN_2 GP01
#define PIN_4 GP02
#define PIN_5 GP03
#define PIN_6 GP04
#define PIN_7 GP05
#define PIN_9 GP06
#define PIN_10 GP07
#define PIN_11 GP08
#define PIN_12 GP09
#define PIN_14 GP10
#define PIN_15 GP11
#define PIN_16 GP12
#define PIN_17 GP13
#define PIN_19 GP14
#define PIN_20 GP15
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

#define LED_GPIO      GP25

#define IMR_RECV      0x04
#define Sn_IMR_RECV   0x04
#define Sn_IR_RECV    0x04
#define SOCKET_DHCP   0

#endif // INTERNALS_H