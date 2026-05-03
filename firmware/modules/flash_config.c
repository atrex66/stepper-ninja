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
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "flash_config.h"

extern void reset_with_watchdog();

const configuration_t default_config = {
    .mac = DEFAULT_MAC,
    .ip = DEFAULT_IP,
    .sn = DEFAULT_SUBNET,
    .gw = DEFAULT_GATEWAY,
    .dns = {8, 8, 8, 8},
    .dhcp = 1,
    .port = DEFAULT_PORT,
    .timeout = DEFAULT_TIMEOUT,
    .checksum = 0
};

uint16_t port = 0;
int16_t adc_offset = 0;
uint16_t adc_min = 0;
uint16_t adc_max = 0;
configuration_t *flash_config = NULL;
static volatile bool save_requested = false;

#define FLASH_TARGET_OFFSET (1024 * 1024) // 1mb offset
#define FLASH_DATA_SIZE (sizeof(configuration_t))

uint8_t calculate_checksum_flash(configuration_t *config) {
    uint8_t checksum = 0;
    for (int i = 0; i < sizeof(configuration_t) - 1; i++) {
        checksum += ((uint8_t *)config)[i];
    }
    return checksum;
}

static void call_flash_range_erase(void *param) {
    uint32_t offset = (uint32_t)(uintptr_t)param;
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
}

static void call_flash_range_program(void *param) {
    uintptr_t *params = (uintptr_t *)param;
    uint32_t offset = (uint32_t)params[0];
    const uint8_t *data = (const uint8_t *)params[1];
    uint32_t len = (uint32_t)params[2];
    flash_range_program(offset, data, len);
}

void request_save_config_to_flash() {
    save_requested = true;
}

bool consume_save_config_request() {
    if (!save_requested) {
        return false;
    }
    save_requested = false;
    return true;
}

void __time_critical_func(save_config_to_flash)() {
    if (flash_config == NULL) {
        printf("Nothing to save flash_config is empty.\n");
        return;
    }

    // flash_safe_execute must run on core0 in this firmware architecture.
    if (get_core_num() != 0) {
        request_save_config_to_flash();
        printf("Save request queued for core0.\n");
        return;
    }

    uint8_t *data;
    data = (uint8_t *)malloc(FLASH_SECTOR_SIZE);
    if (data == NULL) {
        printf("Failed to allocate memory for flash data.\n");
        return;
    }

    flash_config->checksum = calculate_checksum_flash(flash_config);

    memset(data, 0xFF, FLASH_SECTOR_SIZE);
    memcpy(data, flash_config, sizeof(configuration_t));
    int rc = flash_safe_execute(call_flash_range_erase, (void *)(uintptr_t)FLASH_TARGET_OFFSET, UINT32_MAX);
    if (rc != PICO_OK) {
        printf("flash_safe_execute erase failed: %d\n", rc);
        free(data);
        return;
    }

    uintptr_t params[] = {(uintptr_t)FLASH_TARGET_OFFSET, (uintptr_t)data, (uintptr_t)FLASH_SECTOR_SIZE};
    rc = flash_safe_execute(call_flash_range_program, params, UINT32_MAX);
    if (rc != PICO_OK) {
        printf("flash_safe_execute program failed: %d\n", rc);
        free(data);
        return;
    }

    free(data);
    printf("Configuration saved to flash.\n");
    reset_with_watchdog();
}

void load_config_from_flash() {
    if (flash_config == NULL) {
        flash_config = (configuration_t *)malloc(sizeof(configuration_t));
    }
    memcpy(flash_config, (configuration_t *)(XIP_BASE + FLASH_TARGET_OFFSET), sizeof(configuration_t));
    uint8_t checksum = calculate_checksum_flash(flash_config);
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
    flash_config->checksum = calculate_checksum_flash(flash_config);
    save_config_to_flash();
}
