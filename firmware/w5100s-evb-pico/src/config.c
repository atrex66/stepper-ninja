#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "wizchip_conf.h" // W5100S/W5500 könyvtárból
#include "hardware/flash.h"
#include "pico/flash.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "wizchip_conf.h"
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "config.h"

extern void reset_with_watchdog();

configuration_t default_config = {
    .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56},
    .ip = {192, 168, 0, 177},
    .sn = {255, 255, 255, 0},
    .gw = {192, 168, 0, 1},
    .dns = {8, 8, 8, 8},
    .dhcp = 1,
    .port = 8888,
    .timeout = 1000000,
    .checksum = 0
};

uint16_t port = 0;
int16_t adc_offset = 0;
uint16_t adc_min = 0;
uint16_t adc_max = 0;
configuration_t *flash_config = NULL;

#define FLASH_TARGET_OFFSET (1024 * 1024) // 1mb offset
#define FLASH_DATA_SIZE (sizeof(configuration_t))

uint8_t calculate_checksum(configuration_t *config) {
    uint8_t checksum = 0;
    for (int i = 0; i < sizeof(configuration_t) - 1; i++) {
        checksum += ((uint8_t *)config)[i];
    }
    return checksum;
}

void __time_critical_func(save_config_to_flash)() {
    if (flash_config == NULL) {
        printf("Nothing to save flash_config is empty.\n");
    }
    uint8_t *data;
    data = (uint8_t *)malloc(FLASH_SECTOR_SIZE);
    if (data == NULL) {
        printf("Failed to allocate memory for flash data.\n");
        return;
    }
    
    uint core_id = get_core_num();

    if (core_id == 1)
    {
        while (!multicore_fifo_wready()) {
            tight_loop_contents();
        }
        multicore_fifo_push_blocking(0xCAFEBABE);

        while(!multicore_fifo_rvalid()) {
            tight_loop_contents();
        }
        uint32_t signal = multicore_fifo_pop_blocking();
        if (signal != 0xFEEDFACE) {
            printf("Core0 is not ready, aborting flash write...\n");
            free(data);
            return;
        }
        printf("Core0 is ready, proceeding with flash write...\n");
    }

    flash_config->checksum = calculate_checksum(flash_config);

    memset(data, 0xFF, FLASH_SECTOR_SIZE);
    memcpy(data, flash_config, sizeof(configuration_t));
    uint32_t ints = save_and_disable_interrupts();
    flash_safe_execute_core_deinit();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, data, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    free(data);
    if (core_id == 1){
    multicore_fifo_push_blocking(0xDEADBEEF);
    while (1) {
        tight_loop_contents();
        }
    }    
}

void load_config_from_flash() {
    if (flash_config == NULL) {
        flash_config = (configuration_t *)malloc(sizeof(configuration_t));
    }
    memcpy(flash_config, (configuration_t *)(XIP_BASE + FLASH_TARGET_OFFSET), sizeof(configuration_t));
    uint8_t checksum = calculate_checksum(flash_config);
    if (checksum != flash_config->checksum){
        printf("Invalid checksum restoring default configuration.\n");
        printf("Checksum: %02X, Flash Checksum: %02X\n", checksum, flash_config->checksum);
        for (int i = 0; i < sizeof(configuration_t); i++) {
            printf(" %02X", ((uint8_t *)flash_config)[i]);
        }
        printf("\n");
        restore_default_config();
        save_config_to_flash();
    }
}

void restore_default_config() {
    if (flash_config == NULL) {
        flash_config = (configuration_t *)malloc(sizeof(configuration_t));
    }
    memcpy(flash_config, &default_config, sizeof(configuration_t));
    // calculate checksum
    flash_config->checksum = calculate_checksum(flash_config);
    save_config_to_flash();
}
