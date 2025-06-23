#include <stdint.h>
#include <stdio.h>
#include "pio_utils.h"

// length of pio programs in PIO words (max program length = 32)

#define sm_per_pio_block 4

// defines for different pico boards
#ifdef PICO_RP2040
#define max_pio_blocks 2
const PIO pio_blocks[max_pio_blocks] = {pio0, pio1};
#elif defined(PICO_RP2350)
#define max_pio_blocks 3
const PIO pio_blocks[max_pio_blocks] = {pio0, pio1, pio2};
#endif

static uint8_t pio_memory[max_pio_blocks] = {0};
static uint8_t pio_claimed[max_pio_blocks * sm_per_pio_block] = {0,};

PIO_def_t get_next_pio(uint8_t length){
    PIO_def_t retval;
    retval.sm = 255;
    for (uint8_t p=0;p<max_pio_blocks;p++){
        for (uint8_t s=0;s<sm_per_pio_block; s++){
            if (pio_memory[p] == 0){
                pio_memory[p] = length;
            }
            if (pio_memory[p] == length){
                uint8_t offset = p * sm_per_pio_block + s;
                if (pio_claimed[offset] == 0){
                    pio_claimed[offset] = 1;
                    retval.pio_blk = p;
                    retval.pio = pio_blocks[p];
                    retval.sm = s;
                    return retval;
                }
            }
        }
    }
    return retval;
}