#ifndef TRANSMISSION_H
#define TRANSMISSION_H
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#pragma pack(push, 1)
// transmission structure from PC to Pico
typedef struct{
    uint32_t stepgen_command[stepgens];
    uint32_t outputs;
    uint32_t pwm_duty[pwm_count];
    uint32_t pwm_frequency[pwm_count];
    uint16_t pio_timing;
    uint8_t packet_id;
    uint8_t checksum;
} transmission_pc_pico_t;
#pragma pack(pop)

#pragma pack(push, 1)
// transmission structure from Pico to PC
typedef struct{
    #if encoders < 2
        uint32_t encoder_counter[2];        // i dont know yet why but this generates checksum error if encoder_counter size is < 2
    #else
        uint32_t encoder_counter[encoders];
    #endif
    uint32_t inputs[2]; // Two 32-bit inputs
    uint32_t jitter;
    uint8_t interrupt_data;
    uint8_t packet_id;
    uint8_t checksum;
} transmission_pico_pc_t;
#pragma pack(pop)

uint16_t pwm_calculate_wrap(uint32_t freq);
bool rx_checksum_ok(transmission_pc_pico_t *rx_buffer);
bool tx_checksum_ok(transmission_pico_pc_t *tx_buffer);
uint8_t calculate_checksum(void *buffer, uint8_t len);

#endif 