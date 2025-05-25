#ifndef TRANSMISSION_H
#define TRANSMISSION_H
#include <stdint.h>
#include <stdbool.h>

#define stepgens 4

// if you want to use the module with pwm output, set this to 1
#define use_pwm 1 // use pwm output removes 1 encoder

// if you want to use the module with outputs, set this to 1
#define use_outputs 1 // use outputs removes 1 encoder

#if use_pwm == 0 && use_outputs == 0
#define encoders 4
#elif use_pwm == 1 && use_outputs == 0
#define encoders 3
#elif use_pwm == 0 && use_outputs == 1
#define encoders 2
#elif use_pwm == 1 && use_outputs == 1
#define encoders 2
#endif

#pragma pack(push, 1)
// transmission structure from PC to Pico
typedef struct{
    uint32_t stepgen_command[stepgens];
    uint32_t outputs;
    uint32_t pwm_duty;
    uint32_t pwm_frequency;
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

uint16_t pwm_calculate_wrap(uint32_t freq);
bool rx_checksum_ok(transmission_pc_pico_t *rx_buffer);
bool tx_checksum_ok(transmission_pico_pc_t *tx_buffer);
uint8_t calculate_checksum(void *buffer, uint8_t len);

#endif 