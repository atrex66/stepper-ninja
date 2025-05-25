#include "transmission.h"
#include "jump_table.h"
#include <stdint.h>
#include <stdbool.h>

const uint8_t input_pins[4] = {22, 26, 27, 28};

uint8_t calculate_checksum(void *buffer, uint8_t len) {
    uint8_t checksum = 0;
    char *bytes = (char *)buffer;
    for (int i = 0; i < len; i++) {
        checksum += jump_table[(uint8_t)bytes[i]];
    }
    return checksum;
}

bool rx_checksum_ok(transmission_pc_pico_t *rx_buffer) {
    uint8_t len = sizeof(transmission_pc_pico_t) - 1; // Exclude checksum byte
    return calculate_checksum(rx_buffer, len) == rx_buffer->checksum;
}

bool tx_checksum_ok(transmission_pico_pc_t *tx_buffer) {
    uint8_t len = sizeof(transmission_pico_pc_t) - 1; // Exclude checksum byte
    return calculate_checksum(tx_buffer, len) == tx_buffer->checksum;
}

