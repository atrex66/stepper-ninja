#ifndef TRANSMISSION_H
#define TRANSMISSION_H
#include <stdint.h>
#include <stdbool.h>

#define stepgens 4
#define encoders 4

#pragma pack(push, 1)
// transmission structure from PC to Pico
typedef struct{
    uint32_t stepgen_command[stepgens];
    uint32_t outputs;
    uint8_t pio_timing;
    uint8_t packet_id;
    uint8_t checksum;
} transmission_pc_pico_t;
#pragma pack(pop)

#pragma pack(push, 1)
// transmission structure from Pico to PC
typedef struct{
    uint32_t encoder_counter[encoders];
    uint32_t inputs[2]; // Two 32-bit inputs
    uint8_t packet_id;
    uint8_t checksum;
} transmission_pico_pc_t;
#pragma pack(pop)

bool rx_checksum_ok(transmission_pc_pico_t *rx_buffer);
bool tx_checksum_ok(transmission_pico_pc_t *tx_buffer);
uint8_t calculate_checksum(void *buffer, uint8_t len);

#endif 