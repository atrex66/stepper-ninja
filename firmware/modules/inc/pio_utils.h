#ifndef PIO_UTILS_H
#define PIO_UTILS_H
#include "hardware/pio.h"

#define encoder_len 24
#define stepgen_len 13

typedef struct {
    PIO pio;
    uint8_t pio_blk;
    uint8_t sm;
    uint32_t program_address;
} PIO_def_t;


PIO_def_t get_next_pio(uint8_t length);


#endif