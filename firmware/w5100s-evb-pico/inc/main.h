#ifndef MAIN_H
#define MAIN_H

// -------------------------------------------
// Hardware Defines (Módosítsd a pinjeidnek megfelelően!)
// -------------------------------------------
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

// Interrupt konfiguráció
#define INT_PIN 21
#define core1_running 1
// Low-pass filter parameters
#define ALPHA 0.25f // Smoothing factor (0.0 to 1.0, lower = more smoothing)

// -------------------------------------------
// Globális változók a magok közötti kommunikációhoz
// -------------------------------------------
// Függvény deklarációk
void jump_table_checksum();
void jump_table_checksum_in();
void i2c_setup(void);
void cs_select();
void cs_deselect();
uint8_t spi_read();
void reset_with_watchdog();
void spi_write(uint8_t data);
int32_t _sendto(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t port);
int32_t _recvfrom(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t *port);
void handle_udp();
void w5100s_interrupt_init();
void w5100s_init();
void network_init();
uint8_t xor_checksum(const uint8_t *data, uint8_t len);
void core1_entry();

#if USE_SPI_DMA
static void wizchip_write_burst(uint8_t *pBuf, uint16_t len);
static void wizchip_read_burst(uint8_t *pBuf, uint16_t len);
#endif

#endif // MAIN_H