#include <stdlib.h>
#include <string.h>
#include "ringbuffer.h"

#if udp_buffering == 1

    void ringbuffer_init(void) {
        ring_buffer = malloc(sizeof(transmission_pc_pico_t) * ringbuffer_size);
        buffer_index_read = 0;
        buffer_index_write = 0;
    }

    void ringbuffer_push(transmission_pc_pico_t *data) {
        buffer_index_write++;
        if (buffer_index_write > 32){
            buffer_index_write = 0; // Reset write index if it exceeds buffer size
        }
        if (buffer_index_write != buffer_index_read){
            memcpy(&ring_buffer[buffer_index_write], data, sizeof(transmission_pc_pico_t));
        }
        else {
            // Buffer is full, handle overflow (e.g., overwrite oldest data or ignore)
            // For now, we will just ignore the new data if the buffer is full
            // You can also implement a policy to overwrite the oldest data if needed
        }
    }

    transmission_pc_pico_t *ringbuffer_pop(void) {
        buffer_index_read++;
        if (buffer_index_read > 32){
            buffer_index_read = 0; // Reset read index if it exceeds buffer size
        }
        if (buffer_index_read == buffer_index_write) {
            return NULL; // Buffer is empty
        }
        transmission_pc_pico_t *data = &ring_buffer[buffer_index_read];
        return data;
    }

    void ringbuffer_free(void) {
        for (int i = 0; i < 32; i++) {
            if (&ring_buffer[i] != NULL) {
                free(&ring_buffer[i]);
            }
        }
        free(ring_buffer);
        ring_buffer = NULL;
    }
#endif