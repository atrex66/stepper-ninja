#ifndef INTERNALS_H
#define INTERNALS_H

    #define low 0
    #define high 1

/* RPi B+ J8 header, also RPi 2 40 pin GPIO header */
#define    GP_03   2  /*!< B+, Pin J8-03 */
#define    GP_05   3  /*!< B+, Pin J8-05 */
#define    GP_07   4  /*!< B+, Pin J8-07 */
#define    GP_08  14  /*!< B+, Pin J8-08, defaults to alt function 0 UART0_TXD */
#define    GP_10  15  /*!< B+, Pin J8-10, defaults to alt function 0 UART0_RXD */
#define    GP_11  17  /*!< B+, Pin J8-11 */
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

#endif // INTERNALS_H