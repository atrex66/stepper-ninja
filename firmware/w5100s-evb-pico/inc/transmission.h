#ifndef TRANSMISSION_H
#define TRANSMISSION_H
#include <stdint.h>

#define stepgens 4
#define encoders 4

#pragma pack(push, 1)
// transmission structure from PC to Pico
typedef struct{
    uint32_t stepgen_command[stepgens];
    uint8_t pio_timing;
    uint8_t checksum;
} transmission_pc_pico_t;
#pragma pack(pop)

#pragma pack(push, 1)
// transmission structure from Pico to PC
typedef struct{
    uint32_t encoder_counter[encoders];
    uint8_t checksum;
} transmission_pico_pc_t;
#pragma pack(pop)

#endif 