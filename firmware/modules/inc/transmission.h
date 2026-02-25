#ifndef TRANSMISSION_H
#define TRANSMISSION_H
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// first encoder index
#define CTRL_SPINDEX 0

// Auto-discovery protocol constants
#define DISCOVERY_MAGIC        0x4E494E4A  // "NINJ"
#define DISCOVERY_MULTICAST_IP "239.192.1.100"
#define DISCOVERY_PORT         5007
#define DISCOVERY_TIMEOUT_SEC  10

#pragma pack(push, 1)
// Discovery/announcement packet sent by Pico via multicast
typedef struct {
    uint32_t magic;    // DISCOVERY_MAGIC
    uint8_t  ip[4];    // Pico's IP address
    uint16_t port;     // Pico's UDP data port
    uint8_t  mac[6];   // Pico's MAC address
    uint8_t  checksum; // Simple sum checksum
} discovery_packet_t;
#pragma pack(pop)


#pragma pack(push, 1)
// transmission structure from PC to Pico
typedef struct{
    #if stepgens > 0
    uint32_t stepgen_command[stepgens];
    #endif
    uint32_t outputs[2];
    uint32_t pwm_duty[pwm_count];
    uint32_t pwm_frequency[pwm_count];
    uint16_t pio_timing;
    #if encoders > 0
    uint8_t enc_control;  // enables encoder index 1st bit encoder 0 2nd encoder 1
    #endif
    #if breakout_board > 0
    uint32_t analog_out;
    #endif
    uint8_t packet_id;
    uint8_t checksum;
} transmission_pc_pico_t;

// transmission structure from Pico to PC
typedef struct{
    #if encoders > 0
    int32_t encoder_counter[encoders];
    uint32_t encoder_timestamp[encoders];
    int32_t encoder_latched[encoders];
    #endif
    uint32_t inputs[4];
    uint32_t jitter;
    uint8_t interrupt_data;
    uint8_t dummy;
    uint8_t packet_id;
    uint8_t checksum;
} transmission_pico_pc_t;
#pragma pack(pop)

uint16_t pwm_calculate_wrap(uint32_t freq);
bool rx_checksum_ok(transmission_pc_pico_t *rx_buffer);
bool tx_checksum_ok(transmission_pico_pc_t *tx_buffer);
uint8_t calculate_checksum(void *buffer, uint8_t len);

#endif
