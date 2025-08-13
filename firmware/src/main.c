#include "main.h"

// Name: Stepper-Ninja

//    ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣤⠤⠤⠤⠤⣤⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⠶⠋⠉⠀⠀⠀⠀⠀⠀⠀⠀⠉⠑⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡞⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⣤⣶⡶⣶⢦⠀⠀⠀⠀⠀⠀⠀⡾⠀⠀⠀⢀⣠⠀⠀⠀⠀⠀⠀⠀⠠⢤⣀⠀⠀⠸⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠸⡏⠁⣠⡤⠾⣆⠀⠀⠀⠀⠀⢸⡇⢠⠖⣾⣭⣀⡀⠀⠀⠀⠀⠀⠀⣀⣠⣼⡷⢶⡀⢻⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⢹⣿⠋⠈⣩⢿⡄⠀⠀⠀⠀⣾⠀⣿⠀⠀⠉⠉⠚⠻⠿⠶⠾⠿⠛⢋⠉⠁⠀⠈⡇⠘⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⠈⢯⣉⠟⠛⣲⢷⡀⠀⠀⠀⡿⠀⢻⣄⡀⠀⠀⠀⠀⢀⡴⠂⣀⠴⠃⠀⠀⢀⣰⠇⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⠀⠘⡏⢒⡶⠧⢬⣧⠀⠀⠀⡇⠀⢸⡏⠉⣷⣶⣲⠤⢤⣶⣯⡥⠴⣶⣶⣎⠉⢻⠀⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⠀⠀⠻⡷⠶⣤⣏⣙⡆⠀⠀⡇⠀⢸⡆⠀⠑⢆⣩⡿⣖⣀⣰⡶⢯⣁⡴⠃⠀⣸⡄⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⠀⠀⠀⢧⠾⢥⣰⣟⣹⡄⠀⣧⠀⣼⠘⢦⣠⠴⠛⠋⠉⠉⠉⠉⠛⠳⢤⣀⡴⢻⡇⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⠀⠀⠀⠘⣇⣼⣋⣩⡏⣳⠀⢻⡀⢸⡴⠚⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠳⣼⡇⣶⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⠀⠀⠀⠀⠸⣤⢾⣡⣾⣽⣷⢼⡇⢿⡄⠀⠀⠀⠀⠠⣄⡀⢀⡀⠀⠀⠀⠀⠀⢸⠇⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⠀⠀⠀⢀⣀⣿⠿⣓⣋⡥⣿⠟⢷⠘⣿⣆⠀⠀⠀⠀⠀⠉⠉⠀⠀⠀⠀⠀⣰⡿⢀⡏⢳⡦⣤⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⠀⢀⣴⣯⣁⣠⣿⠁⠀⠀⠻⡄⠀⠀⠸⣎⢧⡀⠀⠀⠀⠀⠀⢰⣿⣷⣀⣼⣫⠃⠀⠀⣸⠇⠀⠀⠙⣦⣀⣀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⢰⠞⣍⡍⠉⠉⣿⢹⡀⠀⠀⠀⠱⡄⠀⠀⠈⠳⢽⣦⣀⠀⠀⢰⣿⣿⣟⣿⠕⠋⠀⠀⡰⠃⠀⠀⠀⣼⠃⠀⠉⢉⣭⠽⢶⠀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⢨⡷⠋⠙⢷⡀⠹⣾⣧⠀⠀⠀⠀⠈⠦⣄⠀⠀⠀⠀⠈⠉⠉⣿⣾⠁⣼⡇⠀⠀⡠⠞⠁⠀⠀⠀⣸⠏⠀⠀⣠⡞⠉⠻⣾⡀⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⢰⡟⠀⠀⠀⠀⢻⡄⢷⠙⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡏⡏⠀⠈⡇⠀⠀⠀⠀⠀⠀⠀⣴⠏⠀⠀⢠⡟⠀⠀⠀⠘⣷⠀⠀⠀⠀⠀⠀⠀
//    ⠀⠀⣾⡇⠀⠀⠀⠀⠀⢿⡘⣇⠙⣧⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⢱⠁⠐⠲⡇⢀⡀⠀⠀⠀⠀⣼⠃⠀⠀⢀⡾⠀⠀⠀⠀⠀⢹⣧⠀⠀⠀⠀⠀⠀
//    ⠀⢠⣿⡇⠀⠀⠀⠀⠀⣨⡇⠸⡄⠈⠻⣦⡀⠀⠀⠀⠀⠀⠀⠀⢀⡏⠘⣀⣀⣀⡟⠉⣿⠀⠀⢠⡞⠁⠀⠀⠀⣸⣃⠀⠀⠀⠀⠀⠘⡏⢧⠀⠀⠀⠀⠀
//    ⠀⣿⠀⢻⡀⠀⣠⠔⠛⠉⠻⣄⠹⡄⠀⠈⠙⠶⢤⣀⡀⠀⠀⢀⣿⣟⡻⣷⠛⠛⠋⢉⣉⣻⡖⠋⠀⠀⠀⠀⡀⡏⢉⠟⠦⣀⠀⠀⠀⢧⠈⣇⠀⠀⠀⠀
//    ⢸⠃⠀⠈⣷⠈⢁⡄⠀⠀⠀⠘⢦⠘⣦⠀⠀⠀⠀⠈⠫⣭⣽⠟⠁⠈⢳⠈⡇⠀⠀⠀⠈⠉⠙⣷⠀⠀⠀⠀⢸⡽⠃⠀⠀⠈⡀⠀⠀⠘⢧⡸⡆⠀⠀⠀
//    ⢸⠀⠀⠀⠁⠀⡞⠀⠀⠀⠀⠀⠘⣇⠈⢳⡀⠀⠀⠀⣠⣾⡌⣆⠀⠀⢸⠀⣧⣀⣹⠤⣶⡀⢀⣿⠀⠀⠀⠀⣾⠁⠀⠀⠀⠀⣇⠀⠀⠀⠈⠈⢹⡀⠀⠀
//    ⢸⠀⡀⠀⠀⢸⠁⠀⢀⣠⡤⠶⠒⠛⠁⠉⠉⠉⠉⠉⠁⠈⣿⠘⢦⢀⡞⢰⣇⡀⢹⡧⣄⣩⡽⠃⠀⠀⠀⢸⠃⠀⠀⠀⠀⠀⢸⡆⠀⠀⠀⠐⡄⣧⠀⠀
//    ⢸⡀⣇⠀⠀⢸⣦⡞⢻⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠈⢷⡈⠻⣄⣾⠁⢉⡿⠁⠀⠀⠀⠀⠀⠀⠀⢸⠀⠀⠀⠀⠀⢀⣠⡇⠀⠀⠀⠀⠹⣻⠀⠀
//    ⠀⣇⢸⠀⢀⣴⣿⡇⠘⡆⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⠞⠁⠀⠈⠛⣦⣿⡿⠒⢯⣀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣇⠀⣀⣤⡴⢿⡉⠙⢶⣄⣀⡀⠀⠹⡄⠀
//    ⠀⠘⣾⡇⢸⠏⠀⣧⠀⢻⡀⠀⠀⠀⣀⣠⠶⠚⠉⠀⠀⠀⠀⠀⣠⡞⠁⠀⠀⠀⠙⠳⣄⠀⠀⠀⠀⠀⠀⢀⡿⠊⠁⠀⠀⠀⢱⡄⠈⣧⠉⠓⢄⠀⢧⠀
//    ⠀⠀⢹⡇⢸⠀⠀⠸⣇⠀⢳⡀⠈⠉⠀⠀⠀⠀⠀⠀⠀⠀⢀⡼⠿⡶⢞⣛⣛⣓⣒⣾⣶⣿⣲⣦⣤⠤⠚⠁⠀⠀⠀⠀⠀⠀⠀⢳⠀⢸⡆⠀⠀⠀⢿⡆
//    ⠀⠀⢸⡇⠀⠀⠀⠀⠙⣷⡀⠙⣆⠀⠀⠀⠀⠀⠀⢀⣠⡾⣋⠁⡾⣱⠋⠁⠀⠀⠀⠈⠙⣮⢷⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⡄⠘⡇⠀⠀⠀⢸⡇
//    ⠀⠀⠈⢿⡀⠀⠀⠀⠀⠈⢳⣄⠈⠱⣦⡀⣀⡠⠖⠋⡽⠛⢁⣾⡇⡇⠀⠀⠀⠀⠀⠀⠀⢸⡏⣷⠀⠰⠦⠤⢤⣤⣀⣤⠤⠴⠖⠂⡇⠀⡇⠀⠀⢀⡾⠁
//    ⠀⠀⠀⠘⠻⢦⣄⣀⣀⣀⣀⣈⣿⠦⠤⠿⠁⠀⠀⣼⡧⠔⢛⢿⡇⣇⠀⠀⠀⠀⠀⠀⠀⠀⡇⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡏⠀⡇⠀⣠⠟⠁⠀
//    ⠀⠀⠀⠀⠀⠀⠀⠈⠉⠉⠉⠀⠀⠀⠀⠀⠀⠀⠀⣿⣠⠴⠋⢀⣧⡸⣦⣀⣀⣀⣀⣀⣠⣼⣇⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⠇⢰⠗⠋⠁⠀⠀⠀
//    ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣇⡀⠀⠘⠙⣻⡶⠭⠭⠭⠽⠟⠒⠛⠛⠉⠉⠑⠒⠒⠒⠒⠒⠒⠒⠋⠉⠙⠒⠉⠀⠀⠀⠀⠀⠀
//    ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠉⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀

// Author:Viola Zsolt (atrex66@gmail.com)
// Date: 2025
// Description: stepper-ninja driver for W5100S-EVB-PICO and rpi4 with pico
// License: MIT
// Description: This code is a driver for the W5100S-EVB-PICO board (ethernet udp) and pico (spi and rpi4).
// It also includes a serial terminal interface for configuration and debugging.
// The code is designed to run on the Raspberry Pi Pico and uses the Pico SDK for hardware access.
// The code is using DMA for SPI (burst) communication with the W5100S chip, which allows for high-speed data transfer.
// The code works with normal pico + W5500-lite module (cmake -DWIZCHIP_TYPE=W5500 ..)
// The code works with normal pico + raspberry Pi4 (spi communication)
// The code uses the Wiznet W5100S library for network communication (for W5100S-EVB-PICO, and W5500-Lite).
// The code includes functions for initializing the hardware, handling network communication, and processing commands from the serial terminal.
// The code is designed to be modular and extensible, allowing for easy addition of new features and functionality.
// The code is also designed to be efficient and responsive, with low latency and high throughput.
// The code supports pulse width settings for the step-generators trough HAL pin.
// The code can make 1Mhz step pulse frequency with 1mS servo-thread (1024 steps/servo-thread).
// The code supports PWM generation up to 1Mhz (7bit resolution),with a minimum frequency 1907Hz (16bit resolution)
// Note: checksum algorithm is used to ensure data integrity, and the code includes error handling for network communication. (timeout + jumpcode checksum)
// Note: The code is disables the terminal when the HAL driver connected and LinuxCNC running and enables when not running.


// -------------------------------------------
// GLOBAL VARIABLES
// -------------------------------------------

extern wiz_NetInfo default_net_info;
extern uint16_t port;
extern configuration_t *flash_config;

wiz_NetInfo net_info;
wiz_PhyConf *phy_conf;

transmission_pc_pico_t *rx_buffer;
transmission_pico_pc_t *tx_buffer;

#if breakout_board > 0
    volatile uint32_t input_buffer[2];
    volatile uint32_t output_buffer;
#else
    const uint8_t input_pins[] = in_pins;
    const uint8_t output_pins[] = out_pins; // Example output pins
#endif

uint8_t first_send = 1;
volatile bool first_data = true;

uint dma_tx;
uint dma_rx;
dma_channel_config dma_channel_config_tx;
dma_channel_config dma_channel_config_rx;

#if stepgens > 0
uint32_t step_pin[stepgens] = stepgen_steps;
uint32_t dir_pin[stepgens] = stepgen_dirs;
uint32_t total_steps[stepgens] = {0,};
#endif

uint8_t buffer_index = 0;

int32_t *command;
uint8_t *src_ip;
uint16_t src_port;
uint8_t rx_counter=0;

uint8_t rx_size = sizeof(transmission_pc_pico_t);
uint8_t tx_size = sizeof(transmission_pico_pc_t);

uint8_t sety = 0;
uint8_t nop = 0;
uint8_t old_sety = 0;
uint8_t old_nop = 0;

uint8_t connected = 0;
uint8_t timer_started = 0;
absolute_time_t first_time;
uint alarm_num = 0;

uint8_t checksum_error = 0;
uint8_t timeout_error = 1;
uint32_t last_time = 0;
absolute_time_t last_packet_time;
uint32_t TIMEOUT_US = 100000;
uint32_t time_diff;
uint8_t checksum_index = 1;
uint8_t checksum_index_in = 1;

uint32_t pwm_freq_buffer[pwm_count];
const uint8_t pwm_pins[pwm_count] = pwm_pin;
const uint8_t pwm_inverts[pwm_count] = pwm_invert;
uint32_t old_pwm_frequency[pwm_count];

PIO_def_t stepgen_pio[stepgens];

#if encoders > 0

    uint8_t encoder_base[encoders] = enc_pins;
    uint32_t encoder[encoders] = {0,};
    uint32_t encoder_latched[encoders] = {0, };

    static uint8_t encoder_indexes[encoders] = enc_index_pins;
    static uint8_t indexes = sizeof(encoder_indexes);
    static uint8_t enc_index_lvl[encoders] = enc_index_active_level;
    static uint8_t enc_index_enabled[encoders] = {0,};
    PIO_def_t encoder_pio[encoders];

#endif

#if stepgens > 0
// -------------------------------------------
// Pulse generation setup
// -------------------------------------------
void __time_critical_func(stepgen_update_handler)() {
    if (checksum_error){
        return;
    }

    for (int i = 0; i < stepgens; i++) {
        command[i] = rx_buffer->stepgen_command[i];
        if (command[i] != 0){
            gpio_put(dir_pin[i], (command[i] >> 31));
            pio_sm_put_blocking(stepgen_pio[i].pio, stepgen_pio[i].sm, command[i] & 0x7fffffff);
            }
    }
}
#endif

// -------------------------------------------
// Core 1 Entry Point
// -------------------------------------------
void core1_entry() {

    memset(src_ip, 0, 4);
    sleep_ms(500);

    gpio_init(LED_GPIO);
    gpio_set_dir(LED_GPIO, GPIO_OUT);

    // initialize encoder index pins
    #if encoders > 0
        for (int i=0;i<indexes;i++){
            if (encoder_indexes[i]!=PIN_NULL){
                gpio_init(encoder_indexes[i]);
                gpio_set_dir(encoder_indexes[i], false);
                printf("Encoder index %d pin %d initialized\n", i, encoder_indexes[i]);
            }
        }
    #endif

    // initialization routine with breakout board
    #if breakout_board > 0

        i2c_setup();
        mcp4725_port_setup();
        printf("Detecting MCP23008 (Outputs) starting on %#x address\n", MCP23008_ADDR);
        
        // output base + expanders initialization 
        uint8_t io_base_output = MCP23008_ADDR;
        for (int i = 0; i < io_expanders + 1; i+=2) {
            if (i2c_check_address(I2C_PORT, io_base_output + i)) {
                printf("MCP23008:%d (Outputs) Init\n", i);
                mcp_write_register(io_base_output + i, 0x00, 0x00);
            }
            else {
                printf("No MCP23008:%d (Outputs) found on %#x address.\n", io_base_output + i, i);
            }
        }
        // input base + expanders initialization 
        printf("Detecting MCP23017 (Inputs) starting on %#x address\n", MCP23017_ADDR);
        uint8_t io_base_input = MCP23017_ADDR;
        for (int i = 0; i < io_expanders + 1; i+=2){
            if (i2c_check_address(I2C_PORT, io_base_input + i)) {
                printf("MCP23017:%d (Inputs) Init\n", i);
                mcp_write_register(io_base_input + i, 0x00, 0xff);
                mcp_write_register(io_base_input + i, 0x01, 0xff);
                mcp_write_register(io_base_input + i, 0x02, 0xff);
                mcp_write_register(io_base_input + i, 0x03, 0xff);
            }
            else {
                printf("No MCP23017:%d (Inputs) found on %#x address.\n", i, io_base_input + i);
            }
        }

        #ifdef MCP4725_BASE
        printf("Detecting MCP4725 (analog) on %#x address\n", MCP4725_BASE);
        if (i2c_check_address(MCP4725_PORT, MCP4725_BASE)) {
            printf("MCP4725 (analog) Init\n");
            mcp4725_write_data(MCP4725_BASE, 0);
        }
        else {
            printf("No MCP4725 (analog) found on %#x address.\n", MCP4725_BASE);
        }
        printf("Detecting MCP4725 (analog) on %#x address\n", MCP4725_BASE + 1);
        if (i2c_check_address(MCP4725_PORT, MCP4725_BASE + 1)) {
            printf("MCP4725 (analog) Init\n");
            mcp4725_write_data(MCP4725_BASE + 1, 0);
        }
        else {
            printf("No MCP4725 (analog) found on %#x address.\n", MCP4725_BASE + 1);
        }
        #endif
    #endif

    while(1){
        gpio_put(LED_GPIO, !timeout_error);
        time_diff = (uint32_t)absolute_time_diff_us(last_packet_time, get_absolute_time());
        if (time_diff > TIMEOUT_US) {
            if (timeout_error == 0){
                printf("Disconnected from linuxcnc.\n");
                connected = 0;
                stop_timer();
#if encoders >0
                    for (int i = 0; i < encoders; i++) {
                        pio_sm_exec(encoder_pio[i].pio, encoder_pio[i].sm, pio_encode_set(pio_y, 0));
                        // pio_sm_set_enabled(encoder_pio[i].pio, encoder_pio[i].sm, true);
    #if use_stepcounter == 0
                        printf("Encoder %d reset\n", i);
    #else
                        printf("Step counter %d reset\n", i);
    #endif
                    }
#endif
#if breakout_board > 0
                    // reset the analog outputs
                    mcp4725_write_data(MCP4725_BASE, 0);
                    mcp4725_write_data(MCP4725_BASE + 1, 0);
                    mcp_write_register(MCP23008_ADDR, 0x09, 0); // Set outputs
#endif
                rx_counter = 0;
                timeout_error = 1;
                checksum_index = 1;
                checksum_index_in = 1;
                checksum_error = 0;
                first_data = true;
                first_send = 1;
                #ifdef out_pins
                    if (sizeof(output_pins) > 0){
                        for (int i = 0; i < sizeof(output_pins); i++) {
                            gpio_put(output_pins[i], 0); // reset outputs
                        }
                    }
                #endif
            }
        // terminal handling only when not connected to the linuxcnc
        handle_serial_input();
        }
        else {
            timeout_error = 0;
            connected = 1;
            // enable disable encoder index interrupts based on the enc_control pins
            #if encoders > 0
                if (indexes > 0){
                    for (int i=0;i<indexes;i++){
                        if (encoder_indexes[i]!=PIN_NULL){
                            if (rx_buffer->enc_control >> i == 1){
                                if (enc_index_enabled[i] == 0){
                                    printf("Index-enable %d enabled\n", i);
                                    if (enc_index_lvl[i] == high){
                                        gpio_set_irq_enabled_with_callback(encoder_indexes[i], GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
                                    }
                                    else{
                                        gpio_set_irq_enabled_with_callback(encoder_indexes[i], GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
                                    }
                                    enc_index_enabled[i] = 1;
                                }
                            }else {
                                if (enc_index_enabled[i] == 1){
                                    printf("Index-enable %d disabled\n", i);
                                    if (enc_index_lvl[i] == high){
                                        gpio_set_irq_enabled_with_callback(encoder_indexes[i], GPIO_IRQ_EDGE_RISE, false, &gpio_callback);
                                    }
                                    else{
                                        gpio_set_irq_enabled_with_callback(encoder_indexes[i], GPIO_IRQ_EDGE_FALL, false, &gpio_callback);
                                    }
                                    enc_index_enabled[i] = 0;
                                }
                            }
                        }
                    }
                }
            #endif

            #if stepgens > 0
            sety = pio_settings[rx_buffer->pio_timing].sety & 31;
            nop = pio_settings[rx_buffer->pio_timing].nop & 31;

            if (old_sety != sety || old_nop != nop) {
                if (rx_buffer->pio_timing < sizeof(pio_settings)){
                    for (int i=0; i<stepgens; i++){
                        pio_sm_exec(stepgen_pio[i].pio, stepgen_pio[i].sm, pio_encode_jmp(1));
                        stepgen_pio[i].pio->instr_mem[4] = pio_encode_set(pio_y, sety);
                        stepgen_pio[i].pio->instr_mem[5] = pio_encode_nop() | pio_encode_delay(nop);
                    }
                    float cycle_time_ns = 1.0f / pico_clock * 1000000000.0f; // Ciklusidő nanoszekundumban
                    printf("New pulse width set: %d\n", pio_settings[rx_buffer->pio_timing].high_cycles * (uint8_t)cycle_time_ns);
                    old_sety = sety;
                    old_nop = nop;
                }
            }
            #endif

            // read the inputs in the breakout board and write the outputs
            #if breakout_board > 0
                #if io_expanders + 1 > 0  // always true just for the code readability
                    input_buffer[0] = mcp_read_register(MCP23017_ADDR + 0, 0x13) | mcp_read_register(MCP23017_ADDR + 0, 0x12) << 8;
                    mcp_write_register(MCP23008_ADDR + 0, 0x09, (output_buffer >> 0) & 0xff);
                #endif
                #if io_expanders + 1 > 1
                    input_buffer[0] |= (mcp_read_register(MCP23017_ADDR + 2, 0x13) | mcp_read_register(MCP23017_ADDR + 2, 0x12) << 8) << 16;
                    mcp_write_register(MCP23008_ADDR + 2, 0x09, (output_buffer >> 8) & 0xff);
                #endif
                #if io_expanders + 1 > 2
                    input_buffer[1] = mcp_read_register(MCP23017_ADDR + 4, 0x13) | mcp_read_register(MCP23017_ADDR + 4, 0x12) << 8;
                    mcp_write_register(MCP23008_ADDR + 4, 0x09, (output_buffer >> 16) & 0xff);
                #endif
                #if io_expanders + 1 > 3
                    input_buffer[1] |= (mcp_read_register(MCP23017_ADDR + 6, 0x13) | mcp_read_register(MCP23017_ADDR + 6, 0x12) << 8) << 16;
                    mcp_write_register(MCP23008_ADDR + 6, 0x09, (output_buffer >> 24) & 0xff);
                #endif
                // write the analog outputs
                mcp4725_write_data(MCP4725_BASE + 0, rx_buffer->analog_out & 0xfff);
                mcp4725_write_data(MCP4725_BASE + 1, (rx_buffer->analog_out >> 16) & 0xfff);
            #endif

            #if use_pwm == 1
            for (int i=0; i<pwm_count; i++){
                if (old_pwm_frequency[i] != pwm_freq_buffer[i]){
                    old_pwm_frequency[i] = pwm_freq_buffer[i];
                    ninja_pwm_set_frequency(pwm_pins[i], rx_buffer->pwm_frequency[i]);
                }
            }
            #endif
        }
    }
}

int main() {

    // stdio_init_all();
    stdio_usb_init();
    gpio_init(LED_GPIO);
    gpio_set_dir(LED_GPIO, GPIO_OUT);

    sleep_ms(2000);

    src_ip = (uint8_t *)malloc(4);
    command = malloc(sizeof(int32_t) * stepgens);
    rx_buffer = (transmission_pc_pico_t *)malloc(rx_size);
    if (rx_buffer == NULL) {
        printf("rx_buffer allocation failed\n");
        return -1;
    }
    tx_buffer = (transmission_pico_pc_t *)malloc(tx_size);
    if (tx_buffer == NULL) {
        printf("tx_buffer allocation failed\n");
        return -1;
    }

    printf("\033[2J");
    printf("\033[H");
    
    printf("\n\n--- stepper-ninja ---\n");
    printf("https://github.com/atrex66/stepper-ninja\n");
    printf("E-mail:atrex66@gmail.com\n");
    printf("\n");

    if (!set_sys_clock_khz(pico_clock / 1000, true))
    {
        printf("Clock can not configure to %d");
        while (1)
        {
            sleep_ms(1000);
        }
    }
    
    #if raspberry_pi_spi == 0

        //clock_configure(clk_peri,
        //                0,
        //                CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
        //                clock_get_hz(clk_sys),
        //                clock_get_hz(clk_sys));

        spi_init(spi0, 40000000);

        // force spi clock speed
        #if pico_clock == 125000000
            hw_write_masked(&spi_get_hw(spi0)->cr0, (0) << SPI_SSPCR0_SCR_LSB, SPI_SSPCR0_SCR_BITS); // SCR = 0
            hw_write_masked(&spi_get_hw(spi0)->cpsr, 4, SPI_SSPCPSR_CPSDVSR_BITS); // CPSDVSR = 4
        #endif
    #endif
    
    // load network config from the flash
    load_configuration();

    #if raspberry_pi_spi == 0
        #if _WIZCHIP_==W5100S
            #pragma message "W5100S init"
            w5100s_init();
            w5100s_interrupt_init();
        #endif
        #if _WIZCHIP_==W5500
            #pragma message "W5500 init"
            w5100s_init();
            w5500_interrupt_init();
        #endif
    network_init();
    #else
    printf("Raspberry PI spi communication.\n");
    spi_init(SPI_PORT, 1000 * 1000);
    spi_set_slave(SPI_PORT, true);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(GPIO_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(GPIO_SCK, GPIO_FUNC_SPI);
    gpio_set_function(GPIO_MISO, GPIO_FUNC_SPI);
    gpio_set_function(GPIO_CS, GPIO_FUNC_SPI);
    gpio_set_pulls(GPIO_CS, true, false);

    dma_tx = dma_claim_unused_channel(true);
    dma_rx = dma_claim_unused_channel(true);

    dma_channel_config_tx = dma_channel_get_default_config(dma_tx);
    channel_config_set_transfer_data_size(&dma_channel_config_tx, DMA_SIZE_8);
    channel_config_set_dreq(&dma_channel_config_tx, DREQ_SPI0_TX);
    channel_config_set_read_increment(&dma_channel_config_tx, true);
    channel_config_set_write_increment(&dma_channel_config_tx, false);

    dma_channel_config_rx = dma_channel_get_default_config(dma_rx);
    channel_config_set_transfer_data_size(&dma_channel_config_rx, DMA_SIZE_8);
    channel_config_set_dreq(&dma_channel_config_rx, DREQ_SPI0_RX);
    channel_config_set_read_increment(&dma_channel_config_rx, false);
    channel_config_set_write_increment(&dma_channel_config_rx, true);

    #endif
    
    multicore_launch_core1(core1_entry);

    PIO pio = pio0;
    int p = 0;
    #ifdef out_pins
    printf("Output GPIO: ");
    for (int i = 0; i < sizeof(output_pins); i++) {
        gpio_init(output_pins[i]);
        gpio_set_dir(output_pins[i], GPIO_OUT);
        gpio_put(output_pins[i], 0);
        printf("%d ", output_pins[i]);
    }
    printf("\n");
    #endif

    #ifdef in_pins
    printf("Input GPIO: ");
    int8_t pullups[sizeof(input_pins)] = in_pullup;
    for (int i = 0; i < sizeof(input_pins); i++) {
        gpio_init(input_pins[i]);
        gpio_set_dir(input_pins[i], GPIO_IN);
        printf("%d ", input_pins[i]);
        if (pullups[i] == -1){
            // buggy, its an rp2040 and rp2350 bug, the internal pulldown not enough to pull down the pin its latching need and external ~8Kohm resistor if you really need to pull down the pin.
            gpio_set_pulls(input_pins[i], false, true);
        } else if (pullups[i] == 1){
            gpio_set_pulls(input_pins[i], true, false);
        }
    }
    printf("\n");
    #endif

    #if use_pwm == 1
        printf("Pwm GPIO:");
        for (int i=0;i<pwm_count;i++){
            ninja_pwm_init(pwm_pins[i]);
            if (pwm_inverts[i] == 1){
                gpio_set_outover(pwm_pins[i], GPIO_OVERRIDE_INVERT); // Invert the PWM signal
                printf("_");
            }
            printf("%d ", pwm_pins[i]);
        }
        printf("\n");
    #endif

    #if stepgens > 0

    uint32_t offset[2] = {0, };
    uint8_t step_inverts[] = step_invert;

    offset[0] = pio_add_program_at_offset(pio0, &freq_generator_program, 0);

    uint32_t sm = 0;
    uint8_t o = 0;
    for (uint32_t i = 0; i < stepgens; i++){
        stepgen_pio[i] = get_next_pio(stepgen_len);
        if (stepgen_pio[i].sm == 255){
            printf("Not enough pio state machines, check the config.h \n");
        }
        pio = stepgen_pio[i].pio;
        sm = stepgen_pio[i].sm;
        pio_gpio_init(pio, step_pin[i]);
        gpio_init(dir_pin[i]);
        if (step_inverts[i] == 1){
           gpio_set_outover(step_pin[i], GPIO_OVERRIDE_INVERT);
        }
        gpio_set_dir(step_pin[i]+1, GPIO_OUT);
        pio_sm_set_consecutive_pindirs(pio, sm, step_pin[i], 1, true);
        pio_sm_config c = freq_generator_program_get_default_config(offset[o]);
        sm_config_set_set_pins(&c, step_pin[i], 1);
        pio_sm_init(pio, sm, offset[o], &c);
        pio_sm_set_enabled(pio, sm, true);
        printf("stepgen%d. pio:%d sm:%d init done...\n", i, stepgen_pio[i].pio_blk, sm);
    }
    #endif

    #if encoders > 0
        #if use_stepcounter == 0
            uint8_t fpio = 0;
            for (int i = 0; i < encoders; i++) {
                gpio_init(encoder_base[i]);
                gpio_init(encoder_base[i]+1);
                gpio_set_dir(encoder_base[i], GPIO_IN);
                gpio_set_dir(encoder_base[i]+1, GPIO_IN);
                gpio_pull_up(encoder_base[i]);
                gpio_pull_up(encoder_base[i]+1);
                tx_buffer->encoder_latched[i] = 0;
                encoder_pio[i] = get_next_pio(encoder_len);
                if (encoder_pio[i].sm == 255){
                    printf("Not enough pio state machines, check the config.h \n");
                }
                // todo: handle multiple pio banks
                if (fpio == 0){
                    pio_add_program_at_offset(encoder_pio[i].pio, &quadrature_encoder_program, 0);
                    fpio = 1;
                }
                quadrature_encoder_program_init(encoder_pio[i].pio, encoder_pio[i].sm, encoder_base[i], 0);
                printf("encoder%d. pio:%d sm:%d init done...\n", i, encoder_pio[i].pio_blk, encoder_pio[i].sm);
            }
        #else
            for (int i = 0; i < encoders; i++) {
                gpio_init(encoder_base[i]);
                gpio_init(encoder_base[i]+1);
                gpio_set_dir(encoder_base[i], GPIO_IN);
                gpio_set_dir(encoder_base[i]+1, GPIO_IN);
                gpio_pull_up(encoder_base[i]);
                gpio_pull_up(encoder_base[i]+1);
            }
            pio_add_program(pio1, &step_counter_program);
            for (int i = 0; i < encoders; i++) {
                step_counter_program_init(pio1, i, encoder_base[i], 0);
                printf("step counter %d init done...\n", i);
            }
        #endif
    #endif //encoders>0

    // init the firsat tx_buffer checksum because of the full duplex communication with the pico.
    tx_buffer->checksum = calculate_checksum(tx_buffer, tx_size - 1);

    printf("Pio init done....\n");

    handle_udp();
}

void reset_with_watchdog() {
    watchdog_enable(1, 1);
    while(1);
}

void __time_critical_func(cs_select)() {
    gpio_put(GPIO_CS, 0);
    asm volatile("nop \n nop \n nop");
}

void __time_critical_func(cs_deselect)() {
    gpio_put(GPIO_CS, 1);
}

uint8_t __time_critical_func(spi_read)() {
    uint8_t data;
    spi_read_blocking(SPI_PORT, 0x00, &data, 1);
    return data;
}

void __time_critical_func(spi_write)(uint8_t data) {
    spi_write_blocking(SPI_PORT, &data, 1);
}

int32_t __time_critical_func(_sendto)(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t port) {
    uint16_t freesize;
    uint32_t taddr;

    if (first_send) {
        setSn_DIPR(sn, addr);
        setSn_DPORT(sn, port);
        first_send = 0;
    }

    wiz_send_data(sn, buf, len);
    setSn_CR(sn, Sn_CR_SEND);
    while (getSn_CR(sn));

    while (1) {
        uint8_t tmp = getSn_IR(sn);
        if (tmp & Sn_IR_SENDOK) {
            setSn_IR(sn, Sn_IR_SENDOK);
            break;
        } else if (tmp & Sn_IR_TIMEOUT) {
            setSn_IR(sn, Sn_IR_TIMEOUT);
            return SOCKERR_TIMEOUT;
        }
    }
    return (int32_t)len;
}

#if breakout_board > 0
void i2c_setup(void) {
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCK, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCK);
}

void mcp4725_port_setup(void) {
    i2c_init(MCP4725_PORT, 400 * 1000); // 400 kHz
    gpio_set_function(MCP4725_SDA, GPIO_FUNC_I2C);
    gpio_set_function(MCP4725_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(MCP4725_SCL);
    gpio_pull_up(MCP4725_SDA);
}

uint8_t mcp_read_register(uint8_t i2c_addr, uint8_t reg) {
    uint8_t data;
    int ret;
    ret = i2c_write_blocking(I2C_PORT, i2c_addr, &reg, 1, true);
    if (ret < 0) return 0xFF;
    ret = i2c_read_blocking(I2C_PORT, i2c_addr, &data, 1, false);
    if (ret < 0) return 0xFF;
    return data;
}

void mcp_write_register(uint8_t i2c_addr, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    int ret = i2c_write_blocking(I2C_PORT, i2c_addr, data, 2, false);
    if (ret < 0) {
        printf("I2C write failed %02X\n", i2c_addr);
        asm("nop");
    }
}

bool i2c_check_address(i2c_inst_t *i2c, uint8_t addr) {
    uint8_t buffer[1] = {0x00};
    int ret = i2c_write_blocking_until(i2c, addr, buffer, 1, false, make_timeout_time_us(1000));
    if (ret != PICO_ERROR_GENERIC) {
        return true;
    } else {
        return false;
    }
}
#endif // breakout_board > 0


void __not_in_flash_func(core0_wait)(void) {
    while (!multicore_fifo_wready()) {
        tight_loop_contents();
    }
    multicore_fifo_push_blocking(0xFEEDFACE);
    printf("Core0 is ready to write...\n");
    uint32_t signal = multicore_fifo_pop_blocking();
    if (signal == 0xDEADBEEF) {
        watchdog_reboot(0, 0, 1000);
    }
}

int32_t __time_critical_func(_recvfrom)(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t *port) {
    static uint8_t head[8];
    static uint16_t pack_len = 0;

    while ((pack_len = getSn_RX_RSR(sn)) == 0) {
        if (getSn_SR(sn) == SOCK_CLOSED) return SOCKERR_SOCKCLOSED;
    }

    wiz_recv_data(sn, head, 8);
    setSn_CR(sn, Sn_CR_RECV);
    while (getSn_CR(sn));

    memset(addr, 0, 4);

    addr[0] = head[0];
    addr[1] = head[1];
    addr[2] = head[2];
    addr[3] = head[3];
    *port = (head[4] << 8) | head[5];

    uint16_t data_len = (head[6] << 8) | head[7];

    if (len < data_len) pack_len = len;
    else pack_len = data_len;

    wiz_recv_data(sn, buf, pack_len);
    setSn_CR(sn, Sn_CR_RECV);
    while (getSn_CR(sn));

    return (int32_t)pack_len;
}

void handle_data(){
    tx_buffer->jitter = get_absolute_time() - last_packet_time;
    //printf("%d Received bytes: %d\n", rx_counter, len);
    last_packet_time = get_absolute_time();

    if (rx_buffer->packet_id != rx_counter ) {
        printf("packet loss: %d != %d  syncronizing.... \n", rx_buffer->packet_id, rx_counter);
        // try to syncronize packet counter
        rx_counter = rx_buffer->packet_id;
    }
    if (!rx_checksum_ok(rx_buffer)) {
        printf("Checksum error: %d != %d\n", rx_buffer->checksum, checksum_index_in);
        checksum_error = 1;
    }

    if (!checksum_error) {

        #if stepgens > 0
        // update stepgens
        stepgen_update_handler();
        #endif

#if use_pwm == 1
        #if pwm_count < 1
            #pragma error "Defined to use pwm but not defined any PWM pins"
        #endif
        // update pwm
        for (int i=0;i<pwm_count;i++){
            pwm_freq_buffer[i] = rx_buffer->pwm_frequency[i];
            ninja_pwm_set_duty(pwm_pins[i], (uint16_t)rx_buffer->pwm_duty[i]);
        }
    }
#else
    // no pwm
    }
#endif

    #if encoders > 0
        #if use_stepcounter == 0
        // update encoders
            for (int i = 0; i < encoders; i++) {
                tx_buffer->encoder_counter[i] = quadrature_encoder_get_count(encoder_pio[i].pio, encoder_pio[i].sm);
                tx_buffer->encoder_timestamp[i] = time_us_32();
            }
        #else
            // update step counters
            for (int i = 0; i < encoders; i++) {
                encoder[i] = step_counter_get_count(pio1, i);
                tx_buffer->encoder_counter[i] = encoder[i];
            }
        #endif
    #endif

    #if breakout_board > 0
        tx_buffer->inputs[2] = input_buffer[0]; // Read MCP23017 inputs
        tx_buffer->inputs[3] = input_buffer[1];
        output_buffer = rx_buffer->outputs[0];
    #else
        tx_buffer->inputs[0] = gpio_get_all64() & 0xFFFFFFFF; // Read all GPIO inputs
        tx_buffer->inputs[1] = gpio_get_all64() >> 32;
        if (sizeof(output_pins)>0){
            //set output pins
            for (uint8_t i = 0; i < sizeof(output_pins); i++) {
                if (output_pins[i] < 32){
                    gpio_put(output_pins[i], (rx_buffer->outputs[0] >> i) & 1);
                } else {
                    gpio_put(output_pins[i], (rx_buffer->outputs[1] >> (i & 31)) & 1);
                }
            }
        }
    #endif
    
    tx_buffer->packet_id = rx_counter;
    tx_buffer->checksum = calculate_checksum(tx_buffer, tx_size - 1);
}

void printbuf(uint8_t *buf, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        printf("%02x", buf[i]);
    }
    putchar('\n');
}

#if encoders > 0
void __not_in_flash_func(gpio_callback)(uint gpio, uint32_t events) {
    for (int i=0;i<indexes;i++){
        if (gpio == encoder_indexes[i]) {
            if (events & GPIO_IRQ_EDGE_RISE) {
                tx_buffer->encoder_latched[i] = quadrature_encoder_get_count(encoder_pio[i].pio, encoder_pio[i].sm);
                // pio_sm_exec(encoder_pio[i].pio, encoder_pio[i].sm, pio_encode_set(pio_y, 0));
            }
        }
    }
}
#endif

#if use_timer_interrupt == 1
// Timer interrupt kezelő függvény
void timer_callback(uint alarm_num) {
    hw_clear_bits(&timer_hw->intr, 1u << alarm_num);
    first_time += 1000000; // 1 másodperc = 1 000 000 us
    timer_hw->alarm[alarm_num] = (uint32_t)(first_time & 0xFFFFFFFF); // Alsó 32 bit

    handle_data();
    if (!checksum_error){
    _sendto(0, (uint8_t *)tx_buffer, tx_size, src_ip, src_port);
    rx_counter += 1;
    }

}
#endif

// Timer leállítása
void stop_timer() {
        // Timer interrupt kikapcsolása
        hardware_alarm_set_callback(alarm_num, NULL); // Callback törlése
        hw_clear_bits(&timer_hw->inte, 1u << alarm_num); // Interrupt engedélyezés törlése

        // Timer csatorna felszabadítása
        hardware_alarm_unclaim(alarm_num);
        printf("Timer stopped\n");
}

// -------------------------------------------
// UDP handler
// -------------------------------------------
void __not_in_flash_func(handle_udp)() {
    gpio_pull_up(GPIO_INT);
    uint8_t *packet_buffer;
    packet_buffer = malloc(rx_size);
    memset(packet_buffer, 0, rx_size);
    last_packet_time = get_absolute_time();
    while (1){
        if (!gpio_get(GPIO_INT)){
            #if raspberry_pi_spi == 0
                setSn_IR(0, Sn_IR_RECV);
                int len = _recvfrom(0, (uint8_t *)rx_buffer, rx_size, src_ip, &src_port);
            #else 
                memset(packet_buffer, 0, rx_size);
                memcpy(packet_buffer, (uint8_t *)tx_buffer, tx_size);
                spi_read_fulldup((uint8_t *)rx_buffer, packet_buffer, rx_size);
                int len = rx_size; // for compatibility
            #endif
            if (len == rx_size) {
                #if use_timer_interrupt
                    if (timer_started == 0){
                        sleep_us(250);
                        hardware_alarm_claim(alarm_num);
                        hardware_alarm_set_callback(alarm_num, timer_callback);
                        first_time = make_timeout_time_us(1000000); // 1 másodperc
                        timer_hw->alarm[alarm_num] = (uint32_t)(to_us_since_boot(first_time) & 0xFFFFFFFF);
                        timer_started = 1;
                    }
                #else
                    handle_data();
                    #if raspberry_pi_spi == 0
                        if (!checksum_error){
                        _sendto(0, (uint8_t *)tx_buffer, tx_size, src_ip, src_port);
                        }
                    #endif
                #endif
                rx_counter += 1;
            }
        }
        if (multicore_fifo_rvalid()) {
            uint32_t signal = multicore_fifo_pop_blocking();
            printf("Core1 signal: %08X\n", signal);
            if (signal == 0xCAFEBABE) {
                core0_wait();
            }
        }
    }
}

static void spi_read_fulldup(uint8_t *pBuf, uint8_t *sBuf,  uint16_t len)
{
    channel_config_set_read_increment(&dma_channel_config_tx, true);
    channel_config_set_write_increment(&dma_channel_config_tx, false);
    channel_config_set_dreq(&dma_channel_config_tx, DREQ_SPI0_TX);
    dma_channel_configure(dma_tx, &dma_channel_config_tx,
                          &spi_get_hw(SPI_PORT)->dr,
                          sBuf,
                          len,                      
                          false);                   

    channel_config_set_read_increment(&dma_channel_config_rx, false);
    channel_config_set_write_increment(&dma_channel_config_rx, true);
    channel_config_set_dreq(&dma_channel_config_tx, DREQ_SPI0_RX);
    dma_channel_configure(dma_rx, &dma_channel_config_rx,
                          pBuf,                     
                          &spi_get_hw(SPI_PORT)->dr,
                          len,                      
                          false);                   

    dma_start_channel_mask((1u << dma_tx) | (1u << dma_rx));
    dma_channel_wait_for_finish_blocking(dma_rx);
}

#if _WIZCHIP_ == W5100S
void w5100s_interrupt_init() {
    gpio_init(GPIO_INT);
    gpio_set_dir(GPIO_INT, GPIO_IN);
    gpio_pull_up(GPIO_INT);
    gpio_set_input_hysteresis_enabled(GPIO_INT, false); 

    uint8_t sn_imr = Sn_IMR_RECV;  
    
    setIMR(0x01);
    setIMR2(0x00);
    setSn_IMR(SOCKET_DHCP, sn_imr);
}
#endif

#if _WIZCHIP_ == W5500
void w5500_interrupt_init() {
    gpio_init(GPIO_INT);
    gpio_set_dir(GPIO_INT, GPIO_IN);
    gpio_pull_up(GPIO_INT);
    gpio_set_input_hysteresis_enabled(GPIO_INT, false); 
    setSIMR(0x01);
    setSn_IMR(SOCKET_DHCP, Sn_IMR_RECV);
}
#endif
// -------------------------------------------
// W5100S Init
// -------------------------------------------
void w5100s_init() {

    gpio_init(GPIO_CS);
    gpio_set_dir(GPIO_CS, GPIO_OUT);
    cs_deselect();

    gpio_init(GPIO_RESET);
    gpio_set_dir(GPIO_RESET, GPIO_OUT);
    gpio_put(GPIO_RESET, 1);

    gpio_set_function(GPIO_SCK, GPIO_FUNC_SPI);
    gpio_set_function(GPIO_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(GPIO_MISO, GPIO_FUNC_SPI);
   
    reg_wizchip_cs_cbfunc(cs_select, cs_deselect);
    reg_wizchip_spi_cbfunc(spi_read, spi_write);
    
    reg_wizchip_spiburst_cbfunc(spi_read_burst, spi_write_burst);

    gpio_put(GPIO_RESET, 0);
    sleep_ms(100);
    gpio_put(GPIO_RESET, 1);
    sleep_ms(500);

    dma_tx = dma_claim_unused_channel(true);
    dma_rx = dma_claim_unused_channel(true);

    dma_channel_config_tx = dma_channel_get_default_config(dma_tx);
    channel_config_set_transfer_data_size(&dma_channel_config_tx, DMA_SIZE_8);
    channel_config_set_dreq(&dma_channel_config_tx, DREQ_SPI0_TX);

    dma_channel_config_rx = dma_channel_get_default_config(dma_rx);
    channel_config_set_transfer_data_size(&dma_channel_config_rx, DMA_SIZE_8);
    channel_config_set_dreq(&dma_channel_config_rx, DREQ_SPI0_RX);
    channel_config_set_read_increment(&dma_channel_config_rx, false);
    channel_config_set_write_increment(&dma_channel_config_rx, true);

    uint8_t tx[_WIZCHIP_SOCK_NUM_] = {0,};
    uint8_t rx[_WIZCHIP_SOCK_NUM_] = {0,};
    memset(tx, 2, _WIZCHIP_SOCK_NUM_);
    memset(rx, 2, _WIZCHIP_SOCK_NUM_);
    wizchip_init(tx, rx);

    wiz_PhyConf phyconf;
    phy_conf = malloc(sizeof(wiz_PhyConf));
    wizphy_getphyconf(phy_conf);
    phy_conf->mode = PHY_MODE_MANUAL;
    phy_conf->speed = PHY_SPEED_100;
    phy_conf->duplex = PHY_DUPLEX_FULL;
    wizphy_setphyconf(phy_conf);
    wizchip_setnetinfo(&net_info);
    printf("Network Init Done\n");
    wizchip_getnetinfo(&net_info);
    wizphy_getphyconf(&phyconf);
    printf("**************Network Info read from WIZCHIP\n");
    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", net_info.mac[0], net_info.mac[1], net_info.mac[2], net_info.mac[3], net_info.mac[4], net_info.mac[5]);
    printf("IP: %d.%d.%d.%d\n", net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);
    printf("Subnet: %d.%d.%d.%d\n", net_info.sn[0], net_info.sn[1], net_info.sn[2], net_info.sn[3]);
    printf("Gateway: %d.%d.%d.%d\n", net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
    printf("DNS: %d.%d.%d.%d\n", net_info.dns[0], net_info.dns[1], net_info.dns[2], net_info.dns[3]);
    printf("DHCP: %d   (1-Static, 2-Dinamic)\n", net_info.dhcp);
    printf("PORT: %d\n", port);
    printf("*******************PHY status**************\n");
    printf("PHY Mode: %s\n", phyconf.mode == PHY_MODE_AUTONEGO ? "Auto" : "Manual");
    printf("PHY Duplex: %s\n", phyconf.duplex == PHY_DUPLEX_FULL ? "Full" : "Half");
    printf("PHY Speed: %s\n", phyconf.speed == PHY_SPEED_100 ? "100Mbps" : "10Mbps");
    printf("*******************************************\n");

}

// -------------------------------------------
// Network Init
// -------------------------------------------
void network_init() {
    setSn_CR(0, Sn_CR_CLOSE);
    setSn_CR(0, Sn_CR_OPEN);
    uint8_t sock_num = 0;
    socket(sock_num, Sn_MR_UDP, port, 0);
    }

static void spi_read_burst(uint8_t *pBuf, uint16_t len)
{
    uint8_t dummy_data = 0xFF;

    channel_config_set_read_increment(&dma_channel_config_tx, false);
    channel_config_set_write_increment(&dma_channel_config_tx, false);
    channel_config_set_transfer_data_size(&dma_channel_config_tx, DMA_SIZE_8);
    dma_channel_configure(dma_tx, &dma_channel_config_tx,
                          &spi_get_hw(SPI_PORT)->dr,
                          &dummy_data,              
                          len,                      
                          false);                   

    channel_config_set_read_increment(&dma_channel_config_rx, false);
    channel_config_set_write_increment(&dma_channel_config_rx, true);
    channel_config_set_transfer_data_size(&dma_channel_config_tx, DMA_SIZE_8);
    dma_channel_configure(dma_rx, &dma_channel_config_rx,
                          pBuf,                     
                          &spi_get_hw(SPI_PORT)->dr,
                          len,                      
                          false);                   

    dma_start_channel_mask((1u << dma_tx) | (1u << dma_rx));
    dma_channel_wait_for_finish_blocking(dma_rx);
}

static void spi_write_burst(uint8_t *pBuf, uint16_t len)
{
    uint8_t dummy_data;

    channel_config_set_read_increment(&dma_channel_config_tx, true);
    channel_config_set_write_increment(&dma_channel_config_tx, false);
    dma_channel_configure(dma_tx, &dma_channel_config_tx,
                          &spi_get_hw(SPI_PORT)->dr,
                          pBuf,                     
                          len,                      
                          false);                   

    channel_config_set_read_increment(&dma_channel_config_rx, false);
    channel_config_set_write_increment(&dma_channel_config_rx, false);
    dma_channel_configure(dma_rx, &dma_channel_config_rx,
                          &dummy_data,              
                          &spi_get_hw(SPI_PORT)->dr,
                          len,                      
                          false);                   

    dma_start_channel_mask((1u << dma_tx) | (1u << dma_rx));
    dma_channel_wait_for_finish_blocking(dma_rx);
}

