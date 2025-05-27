#ifndef RINGBUFFER_H
#define RINGBUFFER_H
#include "config.h"

#if udp_buffering == 1

#include "transmission.h"
#include <stdint.h>
#include <strings.h>

transmission_pc_pico_t *ring_buffer; // Ring buffer for transmission_pc_pico_t
uint8_t buffer_index_read;
uint8_t buffer_index_write;

void ringbuffer_init(void);
void ringbuffer_push(transmission_pc_pico_t *data);
transmission_pc_pico_t *ringbuffer_pop(void);
void ringbuffer_free(void);

#endif // buffering == 1
#endif // RINGBUFFER_H