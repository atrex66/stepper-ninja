#include "transmission.h"
#include "jump_table.h"
#include <stdint.h>
#include <stdbool.h>

const uint8_t input_pins[in_pins_no] = in_pins;

#if use_outputs == 1 && use_pwm == 0
const uint8_t output_pins[4] = out_pins_4; // Example output pins
#elif use_outputs == 1 && use_pwm == 1
const uint8_t output_pins[3] = out_pins_3; // Example output pins
#endif

#if use_pwm == 1
const uint8_t pwm_pin = pwm_GP; // Example PWM pins + 1 for direction
#endif

uint16_t pwm_calculate_wrap(uint32_t freq) {
    // Rendszer órajel lekérése (alapértelmezetten 125 MHz az RP2040 esetén)
    uint32_t sys_clock = 125000000;
    
    // Wrap kiszámítása fix 1.0 divider-rel
    uint32_t wrap = (uint32_t)(sys_clock/ freq);

    if (freq < 1908){
        wrap = 65535; // 65535 is the maximum wrap value for 16-bit PWM
    }

    return (uint16_t)wrap;
}

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

