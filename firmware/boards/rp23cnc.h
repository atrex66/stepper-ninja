// SPDX-License-Identifier: MIT
// Phil Barrett RP23CNC V1.0 (RP2350B) / V2 (RP2354B), 48 GPIO, 2 MiB flash.
// This SDK board header is also read by CMake and assembler.
#ifndef _BOARDS_STEPPER_NINJA_RP23CNC_H
#define _BOARDS_STEPPER_NINJA_RP23CNC_H

pico_board_cmake_set(PICO_PLATFORM, rp2350)
#define STEPPER_NINJA_RP23CNC 1
#define PICO_RP2350A 0

#define PICO_DEFAULT_UART 0
#define PICO_DEFAULT_UART_TX_PIN 0
#define PICO_DEFAULT_UART_RX_PIN 1
#define PICO_DEFAULT_I2C 1
#define PICO_DEFAULT_I2C_SDA_PIN 30
#define PICO_DEFAULT_I2C_SCL_PIN 31
#define PICO_DEFAULT_SPI 1
#define PICO_DEFAULT_SPI_SCK_PIN 46
#define PICO_DEFAULT_SPI_TX_PIN 43
#define PICO_DEFAULT_SPI_RX_PIN 44
#define PICO_DEFAULT_SPI_CSN_PIN 41

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1
#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif
pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (2 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif
pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, 1)
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

// No GPIO status LED. Do not inherit Pico 2's LED/SMPS/VSYS pins: these are EN/I/O.
#endif
