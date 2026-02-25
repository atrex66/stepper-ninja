#include "rtapi.h"              /* RTAPI realtime OS API */
#include "rtapi_app.h"          /* RTAPI realtime module decls */
#include "rtapi_errno.h"        /* EINVAL etc */
#include "hal.h"                /* HAL public API decls */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include "transmission.h" // Include the header file for transmission structure
#include "transmission.c"
#include "pio_settings.h" // Include the header file for PIO settings

#if raspberry_pi_spi == 0
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#else
    #include "bcm2835.c"
    #include "bcm2835rt.h"
#endif

// name of the module
#if raspberry_pi_spi == 0
    #pragma message "Ethernet version"
    #define module_name "stepgen-ninja"
    // to parse the modparam
    char *ip_address;
    RTAPI_MP_STRING(ip_address, "Ip address");
#else
    #pragma message "SPI version"
    #define module_name "stepgen-ninja"
    #define SPI_SPEED BCM2835_SPI_CLOCK_DIVIDER_128
    const uint8_t rpi_inputs[] = raspi_inputs;
    const uint8_t rpi_outputs[] = raspi_outputs;
    const uint8_t rpi_input_pullup[] = raspi_input_pullups;
    const uint8_t rpi_inputs_no = sizeof(rpi_inputs);
    const uint8_t rpi_outputs_no = sizeof(rpi_outputs);
#endif

#define debug 1

/* module information */
MODULE_AUTHOR("Viola Zsolt");
MODULE_DESCRIPTION(module_name " driver");
MODULE_LICENSE("MIT");

uint8_t tx_size; // transmit buffer size
uint8_t rx_size; // receive buffer size

// maximum number of channels
#define MAX_CHAN 4

uint32_t total_cycles;

#define ANALOG_MAX 4095

// do not modify
#define dormant_cycles 6

// Add 10,000 mm offset to *d->command[i] to avoid simulator zero-crossing issue
// Not needed on real machine due to homing at axis limits, but not hurts real machines.
#define offset 10000

#if breakout_board == 0
const uint8_t input_pins[] = in_pins;
const uint8_t output_pins[] = out_pins;
const uint8_t in_pins_no = sizeof(input_pins);
const uint8_t out_pins_no = sizeof(output_pins);
#else
    #if io_expanders + 1 > 0
    const uint8_t in_pins_no = 16;
    const uint8_t out_pins_no = 8;
    #elif io_expanders + 1 > 1
    const uint8_t in_pins_no = 32;
    const uint8_t out_pins_no = 16;
    #elif io_expanders + 1 > 2
    const uint8_t in_pins_no = 48;
    const uint8_t out_pins_no = 24;
    #elif io_expanders + 1 > 3
    const uint8_t in_pins_no = 64;
    const uint8_t out_pins_no = 32;
    #endif
#endif

typedef struct {
    char ip[16]; // Holds IPv4 address
    int port;
} IpPort;

typedef struct {
    float y;         // Szűrt érték
    float alpha;     // Előre kiszámolt súlyzó (T / (tau + T))
} LowPassFilter;

// Maximum resource counts for static array sizing
#define MAX_STEPGENS 8
#define MAX_ENCODERS 4
#define MAX_PWM      8
#define MAX_ANALOG   4

typedef struct {
    // Runtime feature counts (populated from discovery or compile-time defaults)
    uint8_t n_stepgens;
    uint8_t n_encoders;
    uint8_t n_inputs;
    uint8_t n_outputs;
    uint8_t n_pwm;
    uint8_t n_analog;
    char device_name[16]; // device name from discovery
    char instance_name[32]; // used for HAL pin naming

    // stepgen HAL pins
    hal_float_t *command[MAX_STEPGENS];
    hal_float_t *feedback[MAX_STEPGENS];
    hal_float_t *scale[MAX_STEPGENS];
    hal_bit_t *mode[MAX_STEPGENS];
    hal_bit_t *enable[MAX_STEPGENS];
    hal_u32_t *pulse_width;

    // encoder HAL pins
    hal_s32_t *raw_count[MAX_ENCODERS];
    hal_s32_t *count_latched[MAX_ENCODERS];
    hal_float_t *enc_scale[MAX_ENCODERS];
    hal_float_t *enc_position[MAX_ENCODERS];
    hal_float_t *enc_position_latched[MAX_ENCODERS];
    hal_float_t *enc_velocity[MAX_ENCODERS];
    hal_bit_t *enc_index[MAX_ENCODERS];
    hal_bit_t *enc_reset[MAX_ENCODERS];

    // pwm HAL pins
    hal_bit_t *pwm_enable[MAX_PWM];
    hal_u32_t *pwm_output[MAX_PWM];
    hal_u32_t *pwm_frequency[MAX_PWM];
    hal_u32_t *pwm_maxscale[MAX_PWM];
    hal_u32_t *pwm_min_limit[MAX_PWM];

    // analog HAL pins (breakout board)
    hal_float_t *analog_value[MAX_ANALOG];
    hal_float_t *analog_min[MAX_ANALOG];
    hal_float_t *analog_max[MAX_ANALOG];
    hal_bit_t *analog_enable[MAX_ANALOG];

    hal_u32_t *jitter;
    // inputs
    hal_bit_t *input[96];
    hal_bit_t *input_not[96];
    hal_bit_t *rpi_input[32];
    hal_bit_t *rpi_input_not[32];

    // outputs
    hal_bit_t *output[64];
    hal_bit_t *rpi_output[32];

    hal_float_t *debug_freq;
    hal_s32_t *debug_steps[MAX_STEPGENS];
    hal_bit_t *debug_steps_reset;

    // hal driver inside working
    hal_u32_t *period;
    hal_bit_t *connected;  
    hal_bit_t *io_ready_in;  
    hal_bit_t *io_ready_out;
#if raspberry_pi_spi == 0
    IpPort *ip_address;
    int sockfd;
    struct sockaddr_in local_addr, remote_addr;
#endif
    long long last_received_time;
    long long watchdog_timeout;
    int watchdog_expired; 
    long long current_time;
    int index;
    uint8_t checksum_index;
    uint8_t checksum_index_in;
    uint8_t checksum_error;
    float enc_prev_pos[MAX_ENCODERS];
    uint32_t enc_timestamp[MAX_ENCODERS];
    int32_t enc_offset[MAX_ENCODERS];
    uint32_t delta_time[MAX_ENCODERS];
    int64_t prev_pos[MAX_STEPGENS];
    int64_t curr_pos[MAX_STEPGENS];
    bool watchdog_running;
    bool error_triggered;
    bool first_data;
    float delta_pos[MAX_ENCODERS];
    int32_t delta_count[MAX_ENCODERS];
} module_data_t;

static int instances = 1; // Példányok száma
static int comp_id = -1; // HAL komponens azonosító
static module_data_t *hal_data; // Pointer a megosztott memóriában lévő adatra

static uint32_t timing[1024] = {0, };
static uint32_t old_pulse_width = 0;

static uint8_t tx_counter = 0;
float cycle_time_ns = 1.0f / pico_clock * 1000000000.0f; // Ciklusidő nanoszekundumban
transmission_pc_pico_t *tx_buffer;
transmission_pico_pc_t *rx_buffer;

LowPassFilter filter[MAX_ENCODERS];
float error_estimate = 0.1;

uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

uint16_t pwm_calculate_wrap(uint32_t freq) {
    // Rendszer órajel lekérése (alapértelmezetten 125 MHz az RP2040 esetén)
    uint32_t sys_clock = pico_clock;
    
    // Wrap kiszámítása fix 1.0 divider-rel
    uint32_t wrap = (uint32_t)(sys_clock/ freq);

    if (freq < 1908){
        wrap = 65535; // 65535 is the maximum wrap value for 16-bit PWM
    }
    return (uint16_t)wrap;
}

void lpf_init(LowPassFilter* f, float tau, float dt) {
    f->alpha = dt / (tau + dt);
    f->y = 0.0f;
}

float lpf_update(LowPassFilter* f, float x) {
    f->y += f->alpha * (x - f->y);
    return f->y;
}

void module_init(void) {
    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": module_init\n");
    tx_size = sizeof(transmission_pc_pico_t);
    rx_size = sizeof(transmission_pico_pc_t);
    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": tx_size: %d\n", tx_size);
    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": rx_size: %d\n", rx_size);

    rx_buffer = (transmission_pico_pc_t *)malloc(rx_size);
    if (rx_buffer == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": rx_buffer allocation failed\n");
        return;
    }
    tx_buffer = (transmission_pc_pico_t *)malloc(tx_size);
    if (tx_buffer == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": tx_buffer allocation failed\n");
        return ;
    }
    for (int i = 0; i < MAX_ENCODERS; i++) {
        lpf_init(&filter[i], 0.1f, 0.0005f);
    }
}

#if raspberry_pi_spi == 0
/*
 * init_socket - Initializes a UDP socket for the io-samurai module.
 *
 * @arg: Pointer to an io_samurai_data_t structure containing socket configuration data.
 *
 * Description:
 *   - Creates a UDP socket and binds it to the local address and port specified in the ip_address field.
 *   - Sets the socket to non-blocking mode.
 *   - Configures the remote address for communication using the provided IP and port.
 *   - Logs errors using rtapi_print_msg and closes the socket on failure.
 *
 * Notes:
 *   - The socket is bound to INADDR_ANY to accept packets from any interface.
 *   - If socket creation, binding, or IP address parsing fails, the socket is closed, and sockfd is set to -1.
 *   - The function assumes the ip_address field in the io_samurai_data_t structure is valid.
 */
static void init_socket(module_data_t *arg) {
    module_data_t *d = arg;
    uint32_t bufsize = 65535;
    
    if ((d->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: socket creation failed: %s\n", 
                       d->index, strerror(errno));
        return;
    }

    d->local_addr.sin_family = AF_INET;
    d->local_addr.sin_port = htons(d->ip_address->port);
    d->local_addr.sin_addr.s_addr = INADDR_ANY;

    rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: binding to %s:%d\n",
                   d->index, d->ip_address->ip, d->ip_address->port);

    if (bind(d->sockfd, (struct sockaddr*)&d->local_addr, sizeof(d->local_addr)) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: bind failed: %s\n",
                       d->index, strerror(errno));
        close(d->sockfd);
        d->sockfd = -1;
        return;
    }
    
    // Set non-blocking
    int flags = fcntl(d->sockfd, F_GETFL, 0);
    fcntl(d->sockfd, F_SETFL, flags | O_NONBLOCK);

    // Setup remote address
    d->remote_addr.sin_family = AF_INET;
    d->remote_addr.sin_port = htons(d->ip_address->port);
    if (inet_pton(AF_INET, d->ip_address->ip, &d->remote_addr.sin_addr) <= 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: invalid IP address: %s\n",
                       d->index, d->ip_address->ip);
        close(d->sockfd);
        d->sockfd = -1;
    }
    setsockopt(d->sockfd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
    setsockopt(d->sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
}
#else
// todo: need to implement the spi initialization
void init_spi(){
    if (!bcm2835_init_rt()) {
        printf("bcm2835 init failed\n");
        return ;
    }
    // SPI inicializálása
    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE3);
    bcm2835_spi_setClockDivider(SPI_SPEED);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

    bcm2835_gpio_fsel(raspi_int_out, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_set(raspi_int_out);
}
#endif

// Watchdog process
void watchdog_process(void *arg, long period) {
    module_data_t *d = arg;

    d->current_time += 1; 
    d->watchdog_running = 1; 
    
    long long elapsed = d->current_time - d->last_received_time;
    if (elapsed < 0) {
        elapsed = 0; 
    }
    if (elapsed > d->watchdog_timeout) {
        if (d->watchdog_expired == 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: No transmission, check connection settings and restart Linuxcnc\n", d->index);
            d->checksum_index_in = 1;  // Reset checksum index
            d->checksum_index = 1;     // Reset checksum index
        }
        d->watchdog_expired = 1; 
    } else {
        d->watchdog_expired = 0; 
    }
}

#if stepgens > 0
// find the nearest setting for the given pulse width
uint16_t nearest(uint16_t period){
    uint16_t min_diff = 65535;
    //float value = (float)period * cycle_time_ns; // Period ciklusokból nanoszekundummá
    uint16_t value = (uint16_t)period / cycle_time_ns;
    int16_t calc = 0;
    uint16_t index = 0;
    if (value < pio_settings[0].high_cycles){
        return 0;
    }
    if (value > pio_settings[298].high_cycles){
        return 298;
    }
    for (uint16_t i = 0; i < 299; i++){
        calc = abs((int)pio_settings[i].high_cycles - (int)value);
        if (calc < min_diff){
            min_diff = calc;
            index = i;
        }
    }
    return index;
}
#endif

#if debug == 1
void printbuf(uint8_t *buf, size_t len){
    size_t i;
    for (i=0;i<len;i++){
        printf("%02x", buf[i]);
    }
    printf("\n");
}
#endif

int _receive(void *arg){
    // full duplex transmission not need to receive
    // printbuf((uint8_t *)rx_buffer, sizeof(transmission_pc_pico_t));
    return sizeof(transmission_pico_pc_t);
}

int _send(void *arg){
    #if raspberry_pi_spi == 0
        module_data_t *d = arg;
        return sendto(d->sockfd, tx_buffer, tx_size, MSG_DONTROUTE | MSG_DONTWAIT, &d->remote_addr, sizeof(d->remote_addr));
    #else
        // working full duplex
        bcm2835_gpio_clr(raspi_int_out);
        static size_t ssize = sizeof(transmission_pc_pico_t);
        char *readbuff = malloc(ssize);
        memset(readbuff, 0, ssize);
        bcm2835_spi_transfernb((char *)tx_buffer, readbuff, ssize);
        memcpy(rx_buffer, readbuff , sizeof(transmission_pico_pc_t));
        bcm2835_gpio_set(raspi_int_out);
        return sizeof(transmission_pc_pico_t);
    #endif
}

void udp_io_process_recv(void *arg, long period) {
    module_data_t *d = arg;
    if (d->watchdog_expired) {
        *d->io_ready_out = 0;
        return;
    }
    #if raspberry_pi_spi == 1
        int len = _receive(d);
    #else
        static socklen_t addrlen = sizeof(d->remote_addr);
        int len = recvfrom(d->sockfd, rx_buffer, rx_size, 0, &d->remote_addr, &addrlen);
    #endif
    if (len == rx_size) {
        // rtapi_print_msg(RTAPI_MSG_INFO, "%d %d\n", tx_counter, rx_buffer->packet_id);
        if (!tx_checksum_ok(rx_buffer) && debug_mode == 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: checksum error: %d != %d\n", 
                           d->index, rx_buffer->checksum, calculate_checksum(&rx_buffer, rx_size - 1));
            printbuf((uint8_t*)rx_buffer, rx_size);
            d->checksum_error = 1;
            d->connected = 0;
            *d->io_ready_out = 0; // set io-ready-out to 0 to break the estop-loop
            return;
        }
        *d->connected = 1;
        d->last_received_time = d->current_time;
        *d->jitter = rx_buffer->jitter; // Set jitter value from received data
        for (uint8_t i = 0; i < d->n_encoders; i++) {
            if (*d->enc_reset[i] == 1) {
                d->enc_offset[i] = rx_buffer->encoder_counter[i];
                *d->enc_reset[i] = 0; // reset the encoder
            }
            d->delta_count[i] = *d->raw_count[i] - rx_buffer->encoder_counter[i];
            *d->raw_count[i] = rx_buffer->encoder_counter[i]; // raw encoder count
            *d->count_latched[i] = rx_buffer->encoder_counter[i] - rx_buffer->encoder_latched[i];
            *d->enc_position[i] = (float)rx_buffer->encoder_counter[i] / *d->enc_scale[i];
            *d->enc_position_latched[i] = (float)((rx_buffer->encoder_counter[i] - rx_buffer->encoder_latched[i]) / *d->enc_scale[i]);
            d->delta_pos[i] = d->enc_prev_pos[i] - *d->enc_position[i];
            if (d->delta_count[i] == 0){
                d->delta_time[i] += rx_buffer->encoder_timestamp[i] - d->enc_timestamp[i];
            } else {
                d->delta_time[i] = rx_buffer->encoder_timestamp[i] - d->enc_timestamp[i];
            }
            if (d->delta_time[i]>250000){
                *d->enc_velocity[i] = 0;
            } else {
                *d->enc_velocity[i] = lpf_update(&filter[i], d->delta_pos[i] * ((float)d->delta_time[i]));
            }
            d->enc_timestamp[i] = rx_buffer->encoder_timestamp[i];
            d->enc_prev_pos[i] = *d->enc_position[i];
        }
        #if raspberry_pi_spi == 1
            for (int i=0; i<rpi_inputs_no;i++){
                *d->rpi_input[i]=bcm2835_gpio_lev(rpi_inputs[i]);
            }
        #endif

        #if breakout_board == 1
            for (uint8_t i = 0; i < d->n_inputs; i++) {
                if (i < 32){
                    *d->input[i] = (rx_buffer->inputs[2] >> i) & 1; // MCP23017 inputs
                } else {
                    *d->input[i] = (rx_buffer->inputs[3] >> (i - 32)) & 1; // MCP23017 inputs
                }
                *d->input_not[i] = !(*d->input[i]); // Inverted inputs
            }
        #else
            // get the inputs defined in config.h
            for (uint8_t i = 0; i < d->n_inputs; i++) {
                if (input_pins[i] < 32){
                    *d->input[i] = (rx_buffer->inputs[0] >> (input_pins[i] & 31)) & 1;
                } else{ // pico2 gpio > 31
                    *d->input[i] = (rx_buffer->inputs[1] >> ((input_pins[i] - 32) & 31)) & 1;
                }
                *d->input_not[i] = !(*d->input[i]); // Inverted inputs
            }
        #endif
    }
}

void print_binary_to_array(uint32_t num) {
    char binary[40];
    int index = 0;
    for (int i = 31; i >= 0; i--) {
        binary[index++] = ((num >> i) & 1) ? '1' : '0';
        if (i % 8 == 0 && i != 0) {
            binary[index++] = ' ';
        }
    }
    binary[index] = '\0';
    rtapi_print_msg(RTAPI_MSG_INFO,"%s\n", binary);
} 
  
static void udp_io_process_send(void *arg, long period) {
    module_data_t *d = arg;
    int16_t steps;
    uint8_t sign = 0;
    total_cycles = (uint32_t)(*d->period * 1000) / 1000;
    memset(tx_buffer, 0, tx_size);

    // if watchdog expired, turn off io-ready-out
    if (d->watchdog_expired) {
        *d->io_ready_out = 0;  // turn off io-ready-out (breaking estop-loop)
        return;
    }
        // handle io-ready-in
    if (*d->io_ready_in == 1) {
        *d->io_ready_out = *d->io_ready_in;  // Seems to be all ok so pass the io-ready-in to io-ready-out
    } else {
        *d->io_ready_out = 0;  // no io-ready-in, no io-ready-out
    }

    // handle control bits (encoder index pulse activation)
    if (d->n_encoders > 0) {
        tx_buffer->enc_control = 0;
        for (int i = 0; i < d->n_encoders; i++) {
            tx_buffer->enc_control |= (uint8_t)(1 * *d->enc_index[i]) << (CTRL_SPINDEX + i);
        }
    }

    if (d->watchdog_running == 1) {
        if (d->n_stepgens > 0) {
        double f_steps[MAX_STEPGENS] = {0,};
        uint32_t max_f = (uint32_t)(1.0 / ((*d->pulse_width * 2) * 1e-9));
        *d->debug_freq = (float)max_f / 1000.0;
        if (old_pulse_width != *d->pulse_width) {
            old_pulse_width = *d->pulse_width;
            uint32_t step_counter;
            uint32_t pio_cmd;
            total_cycles = (uint32_t)((period * (pico_clock / 1000)) / 1000000UL); // pico = 125MHz
            uint16_t pio_index = nearest(*d->pulse_width);
            rtapi_print_msg(RTAPI_MSG_INFO, "Max frequency: %.4f KHz\n", max_f / 1000.0);
            rtapi_print_msg(RTAPI_MSG_INFO, "max pulse_width: %dnS\n", pio_settings[298].high_cycles*(int)cycle_time_ns);
            rtapi_print_msg(RTAPI_MSG_INFO, "min pulse_width: %dnS\n", pio_settings[0].high_cycles*(int)cycle_time_ns);
            rtapi_print_msg(RTAPI_MSG_INFO, "total_cycles: %d\n", total_cycles);
            rtapi_print_msg(RTAPI_MSG_INFO, "high_cycles: %d\n", pio_settings[pio_index].high_cycles);
            rtapi_print_msg(RTAPI_MSG_INFO, "pio_index: %d\n", pio_index);
            memset(timing, 0, sizeof(timing));
            for (uint16_t i=1; i < 1024; i++){
                step_counter = (uint32_t)((float)((total_cycles ) / i) - pio_settings[pio_index].high_cycles) - dormant_cycles;
                pio_cmd = (uint32_t)(step_counter << 10 | (i - 1));
                timing[i] = pio_cmd;
            }
        }

        int32_t cmd[MAX_STEPGENS] = {0,};
        for (int i = 0; i < d->n_stepgens; i++) {
            float f_command = *d->command[i] + offset;
            if (d->first_data) {
                d->prev_pos[i] = f_command * *d->scale[i];
            }
            if (*d->enable[i] == 0) {
                cmd[i] = 0;
                continue;
            }
            if (*d->mode[i] == 0) {
                d->curr_pos[i] = f_command * *d->scale[i];
                f_steps[i] = (d->prev_pos[i] - d->curr_pos[i]);
                steps = (int16_t)f_steps[i];

                *d->debug_steps[i] -= steps;
                if (*d->debug_steps_reset == 1) {
                    *d->debug_steps[i] = 0;
                    if (i == d->n_stepgens - 1){
                        *d->debug_steps_reset = 0;
                    }
                }
                steps = abs(steps);

                if (d->prev_pos[i] < 0 && d->curr_pos[i] > 0) {
                    steps ++;
                }

                sign = 0;
                if (d->prev_pos[i] < d->curr_pos[i]){
                    sign = 1;
                }
                d->prev_pos[i] = d->curr_pos[i];
                if (steps > 0){
                    cmd[i] = (timing[steps] | (sign << 31));
                }
                else{
                    cmd[i] = 0;
                }
            }
            else{
                // velocity mode
                float velocity = *d->command[i];
                float steps_per_sec = velocity * *d->scale[i];
                uint8_t sign = (velocity >= 0) ? 1 : 0;
                steps_per_sec = fabs(steps_per_sec);
                if (steps_per_sec > max_f) {
                    steps_per_sec = max_f; 
                }
                uint32_t steps_per_cycle = (uint32_t)(steps_per_sec * (period / 1000000000.0)); // Lépések/ciklus
                *d->debug_steps[i] += (uint16_t)steps_per_cycle;
                if (*d->debug_steps_reset == 1) {
                    *d->debug_steps[i] = 0;
                    *d->debug_steps_reset = 0;
                }
                if (steps_per_cycle > 0) {
                    cmd[i] = timing[steps_per_cycle] | (sign << 31) ;
                } else {
                    cmd[i] = 0;
                }
            }
            *d->feedback[i] = *d->command[i];
        }
        d->first_data = false;
        for (uint8_t i = 0; i < d->n_stepgens; i++) {
            tx_buffer->stepgen_command[i] = cmd[i];
        }
        tx_buffer->pio_timing = nearest(*d->pulse_width);
        } // n_stepgens > 0

    #if raspberry_pi_spi == 1
        for (int i=0; i<rpi_outputs_no;i++){
            if (*d->rpi_output[i]){
                bcm2835_gpio_set(rpi_outputs[i]);
            } else{
                bcm2835_gpio_clr(rpi_outputs[i]);
            }
        }
    #endif

    #if breakout_board > 0
        uint8_t outs=0;
        for (uint8_t i = 0; i < d->n_outputs; i++) {
            outs |= *d->output[i] == 1 ? 1 << i : 0; // Set the bit if output is high
        }
        tx_buffer->outputs[0]= outs;
        uint32_t at0 = 0;
        for (int i = 0; i < d->n_analog; i++) {
            float val = *d->analog_value[i];

            if (val < *d->analog_min[i]) {
                val = *d->analog_min[i];
            }

            if (val > *d->analog_max[i]) {
                val = *d->analog_max[i];
            }

            // Skálázás a 0–4095 DAC tartományba
            uint16_t dac = (uint16_t)((4095.0f / *d->analog_max[i]) * val);
            at0 |= dac << (16 * i);
        }
        tx_buffer->analog_out = at0;

    #else
        if (d->n_outputs > 0){
            uint32_t outs0=0;
            uint32_t outs1=0;
            for (uint8_t i = 0; i < d->n_outputs; i++) {
                if (i<32){
                    outs0 |= *d->output[i] == 1 ? 1 << i : 0;
                } else {
                    outs1 |= *d->output[i] == 1 ? 1 << (i & 31) : 0;
                }
            }
            tx_buffer->outputs[0] = outs0;
            tx_buffer->outputs[1] = outs1;
        }
    #endif

    for (int i = 0; i < d->n_pwm; i++) {
        if (*d->pwm_enable[i]) {
            if (*d->pwm_frequency[i] > 0) {
                if (*d->pwm_frequency[i] > 1000000) {
                    *d->pwm_frequency[i] = 1000000;
                }
                if (*d->pwm_frequency[i] < 1907) {
                    *d->pwm_frequency[i] = 1907;
                }
                if (*d->pwm_output[i] < *d->pwm_min_limit[i]) {
                    *d->pwm_output[i] = *d->pwm_min_limit[i];
                }
                uint16_t wrap = pwm_calculate_wrap(*d->pwm_frequency[i]);
                uint16_t duty_cycle = (uint16_t)(round(((float)*d->pwm_output[i] / *d->pwm_maxscale[i]) * wrap));
                tx_buffer->pwm_duty[i] = duty_cycle;
            } else {
                tx_buffer->pwm_duty[i] = 0;
            }
        }
        tx_buffer->pwm_frequency[i] = *d->pwm_frequency[i];
    }

    tx_buffer->packet_id = tx_counter;
    tx_buffer->checksum = calculate_checksum(tx_buffer, tx_size - 1); // Calculate checksum excluding the checksum byte itself
    _send(d);
    tx_counter++;
    
    }
    else{
        //if the watchdog is not running, we should not send data (io-samurai side is going to timeout error and turn off outputs)
        if (!d->error_triggered){
            d->error_triggered = true; // Set the error triggered flag to prevent multiple messages
            *d->io_ready_out = 0;      // set the io-ready-out pin to 0 to break the estop-loop
            rtapi_print_msg(RTAPI_MSG_ERR ,module_name ".%d: watchdog not running\n", d->index);
            return;  // No data to send (generate io-samurai side timeout error)
        }
    }
}

int parse_ip_port(const char *input, IpPort *output, int max_count) {
    if (input == NULL || output == NULL || max_count <= 0) {
        return -1;
    }

    char *input_copy = strdup(input);
    if (input_copy == NULL) {
        return -1;
    }

    char *saveptr1;
    char *entry;
    int count = 0;

    for (entry = strtok_r(input_copy, ";", &saveptr1);
         entry != NULL && count < max_count;
         entry = strtok_r(NULL, ";", &saveptr1)) {

        char *colon = strchr(entry, ':');
        if (colon == NULL) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": Invalid entry format: %s\n", entry);
            continue; // Skip invalid entry without a colon
        }

        *colon = '\0';
        char *ip = entry;
        char *port_str = colon + 1;

        // Parse port number
        char *endptr;
        long port = strtol(port_str, &endptr, 10);
        if (*endptr != '\0' || port < 0 || port > 65535) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": Invalid port number: %s\n", port_str);
            continue;
        }

        snprintf(output[count].ip, sizeof(output[count].ip), "%s", ip);
        output[count].port = (int)port;

        count++;
    }

    free(input_copy);
    return count;
}

#if raspberry_pi_spi == 0
/*
 * discover_pico - Discovers a Pico device via multicast auto-detection.
 *
 * Joins the multicast group DISCOVERY_MULTICAST_IP:DISCOVERY_PORT and waits
 * for a discovery announcement packet from a Pico. On success, fills *result
 * with the discovered IP and port. Returns 0 on success, -1 on failure.
 */
/*
 * populate_default_features - fills runtime feature counts from compile-time constants.
 * Used when auto-discovery is not available (manual IP configuration).
 */
static void populate_default_features(module_data_t *d) {
    #if stepgens > 0
    d->n_stepgens = (uint8_t)stepgens;
    #else
    d->n_stepgens = 0;
    #endif
    #if encoders > 0
    d->n_encoders = (uint8_t)encoders;
    #else
    d->n_encoders = 0;
    #endif
    d->n_inputs   = (uint8_t)in_pins_no;
    d->n_outputs  = (uint8_t)out_pins_no;
    #if use_pwm == 1
    d->n_pwm = (uint8_t)pwm_count;
    #else
    d->n_pwm = 0;
    #endif
    #if breakout_board > 0
    d->n_analog = (uint8_t)ANALOG_CH;
    #else
    d->n_analog = 0;
    #endif
    memset(d->device_name, 0, sizeof(d->device_name));
    strncpy(d->device_name, "stepper-ninja", sizeof(d->device_name) - 1);
}

static int discover_pico(IpPort *result, module_data_t *features) {
    int sockfd;
    struct sockaddr_in local_addr;
    struct ip_mreq mreq;
    discovery_packet_t pkt;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": discovery socket creation failed: %s\n",
                       strerror(errno));
        return -1;
    }

    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(DISCOVERY_PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": discovery bind failed: %s\n",
                       strerror(errno));
        close(sockfd);
        return -1;
    }

    memset(&mreq, 0, sizeof(mreq));
    inet_pton(AF_INET, DISCOVERY_MULTICAST_IP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": join multicast group failed: %s\n",
                       strerror(errno));
        close(sockfd);
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = DISCOVERY_TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": waiting for Pico discovery (timeout: %ds)...\n",
                   DISCOVERY_TIMEOUT_SEC);

    int found = 0;
    while (!found) {
        struct sockaddr_in sender;
        socklen_t senderlen = sizeof(sender);
        int len = recvfrom(sockfd, &pkt, sizeof(pkt), 0,
                          (struct sockaddr *)&sender, &senderlen);
        if (len < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": auto-discovery timeout, no Pico found\n");
            setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
            close(sockfd);
            return -1;
        }
        if (len == (int)sizeof(discovery_packet_t) && pkt.magic == DISCOVERY_MAGIC) {
            uint8_t chk = 0;
            for (int i = 0; i < (int)sizeof(discovery_packet_t) - 1; i++)
                chk += ((uint8_t *)&pkt)[i];
            if (chk == pkt.checksum) {
                snprintf(result->ip, sizeof(result->ip), "%d.%d.%d.%d",
                        pkt.ip[0], pkt.ip[1], pkt.ip[2], pkt.ip[3]);
                result->port = pkt.port;
                // extract feature set and device name
                if (features != NULL) {
                    features->n_stepgens = pkt.n_stepgens;
                    features->n_encoders = pkt.n_encoders;
                    features->n_inputs   = pkt.n_inputs;
                    features->n_outputs  = pkt.n_outputs;
                    features->n_pwm      = pkt.n_pwm;
                    features->n_analog   = 0; // not sent in discovery yet
                    memset(features->device_name, 0, sizeof(features->device_name));
                    strncpy(features->device_name, pkt.name, sizeof(features->device_name) - 1);
                }
                rtapi_print_msg(RTAPI_MSG_INFO,
                    module_name ": discovered '%s' at %s:%d "
                    "(sg=%d enc=%d in=%d out=%d pwm=%d)\n",
                    pkt.name[0] ? pkt.name : "?",
                    result->ip, result->port,
                    pkt.n_stepgens, pkt.n_encoders,
                    pkt.n_inputs, pkt.n_outputs, pkt.n_pwm);
                found = 1;
            }
        }
    }

    setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    close(sockfd);
    return 0;
}
#endif

int rtapi_app_main(void) {
    int r;

    rtapi_set_msg_level(RTAPI_MSG_INFO);

    module_init();

    #if raspberry_pi_spi == 0
        IpPort results[MAX_CHAN];
        // Temporary storage for discovered feature info per instance
        module_data_t discovered_features[MAX_CHAN];
        memset(discovered_features, 0, sizeof(discovered_features));

        if (ip_address == NULL || strcmp(ip_address, "auto") == 0) {
            // Auto-discovery mode: find Pico via multicast
            instances = 1;
            if (discover_pico(&results[0], &discovered_features[0]) < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ": auto-discovery failed\n");
                return -EINVAL;
            }
        } else {
            // Manual IP configuration
            instances = parse_ip_port((char *)ip_address, results, 8);

            // print parsed IP addresses and ports
            for (int i = 0; i < instances; i++) {
                rtapi_print_msg(RTAPI_MSG_INFO, "Parsed IP: %s, Port: %d\n", results[i].ip, results[i].port);
            }

            // Check if instances is greater than MAX_CHAN
            if (instances > MAX_CHAN) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ": Too many channels, max %d allowed\n", MAX_CHAN);
                return -1;
            }
            // Populate compile-time defaults for manual IP instances
            for (int i = 0; i < instances; i++) {
                populate_default_features(&discovered_features[i]);
            }
        }
    #endif

    // Allocate memory for hal_data in shared memory
    hal_data = hal_malloc(instances * sizeof(module_data_t));
    if (hal_data == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_data allocation failed\n");
        return -1;
    }

    // Initialize hal component
    comp_id = hal_init(module_name);
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: hal_init failed: %d\n", 0, comp_id);
        return comp_id;
    }

    char name[64] = {0};

    // creating HAL pins for each instance
    for (int j = 0; j < instances; j++) {

        hal_data[j].checksum_index = 1;
        hal_data[j].checksum_index_in = 1;
        hal_data[j].index = j; 
        hal_data[j].watchdog_timeout = 10; // ~10 ms timeout
        hal_data[j].current_time = 0;
        hal_data[j].last_received_time = 0;
        hal_data[j].watchdog_expired = 0;
        hal_data[j].watchdog_running = 0;
        hal_data[j].first_data = true;
        hal_data[j].error_triggered = false;

        // Copy runtime feature counts (from discovery or compile-time defaults)
        #if raspberry_pi_spi == 0
        hal_data[j].n_stepgens  = discovered_features[j].n_stepgens;
        hal_data[j].n_encoders  = discovered_features[j].n_encoders;
        hal_data[j].n_inputs    = discovered_features[j].n_inputs;
        hal_data[j].n_outputs   = discovered_features[j].n_outputs;
        hal_data[j].n_pwm       = discovered_features[j].n_pwm;
        hal_data[j].n_analog    = discovered_features[j].n_analog;
        memcpy(hal_data[j].device_name, discovered_features[j].device_name, sizeof(hal_data[j].device_name));
        #else
        populate_default_features(&hal_data[j]);
        #endif

        // Set instance name: device_name if available, otherwise numeric index
        if (hal_data[j].device_name[0] != '\0') {
            snprintf(hal_data[j].instance_name, sizeof(hal_data[j].instance_name),
                     "%s", hal_data[j].device_name);
        } else {
            snprintf(hal_data[j].instance_name, sizeof(hal_data[j].instance_name),
                     "%d", j);
        }
        rtapi_print_msg(RTAPI_MSG_INFO,
            module_name ": instance '%s': sg=%d enc=%d in=%d out=%d pwm=%d\n",
            hal_data[j].instance_name,
            hal_data[j].n_stepgens, hal_data[j].n_encoders,
            hal_data[j].n_inputs, hal_data[j].n_outputs, hal_data[j].n_pwm);

        #if raspberry_pi_spi == 0
            hal_data[j].ip_address = &results[j];
            rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%s: init_socket\n", hal_data[j].instance_name);
            init_socket(&hal_data[j]);
            rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%s: init_socket ready..\n", hal_data[j].instance_name);
        #else
            rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: init_spi\n", j);
            init_spi();
            rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: init_spi ready..\n", j);
            instances = 1;
        #endif

        uint32_t nsize = sizeof(name);
        const char *iname = hal_data[j].instance_name;

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%s.connected", iname);
        r = hal_pin_bit_newf(HAL_IN, &hal_data[j].connected, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%s: ERROR: pin connected export failed with err=%i\n", iname, r);
            hal_exit(comp_id);
            return r;
        }

        if (hal_data[j].n_stepgens > 0) {
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.stepgen.pulse-width", iname);
            r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pulse_width, comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%s: ERROR: pin pulse-width export failed with err=%i\n", iname, r);
                hal_exit(comp_id);
                return r;
            }
            *hal_data[j].pulse_width = default_pulse_width;

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.stepgen.max-freq-khz", iname);
            r = hal_pin_float_newf(HAL_OUT, &hal_data[j].debug_freq, comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%s: ERROR: pin max-freq-khz export failed with err=%i\n", iname, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.stepgen.debug-steps-reset", iname);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].debug_steps_reset, comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%s: ERROR: pin debug-steps-reset export failed with err=%i\n", iname, r);
                hal_exit(comp_id);
                return r;
            }
        }

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%s.jitter", iname);
        r = hal_pin_u32_newf(HAL_OUT, &hal_data[j].jitter, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%s: ERROR: pin jitter export failed with err=%i\n", iname, r);
            hal_exit(comp_id);
            return r;
        }

        #if raspberry_pi_spi == 1
            for (int i = 0; i < rpi_inputs_no; i++){
                bcm2835_gpio_fsel(rpi_inputs[i], BCM2835_GPIO_FSEL_INPT);
                if (rpi_input_pullup[i]){
                    bcm2835_gpio_set_pud(rpi_inputs[i], BCM2835_GPIO_PUD_UP);
                } else {
                    bcm2835_gpio_set_pud(rpi_inputs[i], BCM2835_GPIO_PUD_DOWN);
                }
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%s.rpi-input.gp%d", iname, rpi_inputs[i]);
                r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].rpi_input[i], comp_id, name, j);
                if (r < 0) { hal_exit(comp_id); return r; }
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%s.rpi-input.gp%d-not", iname, rpi_inputs[i]);
                r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].rpi_input_not[i], comp_id, name, j);
                if (r < 0) { hal_exit(comp_id); return r; }
            }
            for (int i = 0; i < rpi_outputs_no; i++){
                bcm2835_gpio_fsel(rpi_outputs[i], BCM2835_GPIO_FSEL_OUTP);
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%s.rpi-output.gp%d", iname, rpi_outputs[i]);
                r = hal_pin_bit_newf(HAL_IN, &hal_data[j].rpi_output[i], comp_id, name, j);
                if (r < 0) { hal_exit(comp_id); return r; }
                *hal_data[j].rpi_output[i] = 0;
            }
        #endif

        // Input pins: indexed by sequential number
        for (int i = 0; i < hal_data[j].n_inputs; i++) {
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.input.%d", iname, i);
            r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].input[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%s: ERROR: pin input.%d export failed with err=%i\n", iname, i, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.input.%d-not", iname, i);
            r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].input_not[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%s: ERROR: pin input.%d-not export failed with err=%i\n", iname, i, r);
                hal_exit(comp_id);
                return r;
            }
        }

        // Output pins: indexed by sequential number
        for (int i = 0; i < hal_data[j].n_outputs; i++) {
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.output.%d", iname, i);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].output[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%s: ERROR: pin output.%d export failed with err=%i\n", iname, i, r);
                hal_exit(comp_id);
                return r;
            }
            *hal_data[j].output[i] = 0;
        }

        // Analog pins (breakout board)
        for (int i = 0; i < hal_data[j].n_analog; i++) {
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.analog.%d.enable", iname, i);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].analog_enable[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].analog_enable[i] = 0;

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.analog.%d.minimum", iname, i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].analog_min[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].analog_min[i] = 0.0;

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.analog.%d.maximum", iname, i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].analog_max[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].analog_max[i] = 0.0;

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.analog.%d.value", iname, i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].analog_value[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].analog_value[i] = 0.0;
        }

        // PWM pins
        for (int i = 0; i < hal_data[j].n_pwm; i++) {
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.pwm.%d.enable", iname, i);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].pwm_enable[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].pwm_enable[i] = 0;

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.pwm.%d.duty", iname, i);
            r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_output[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.pwm.%d.frequency", iname, i);
            r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_frequency[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].pwm_frequency[i] = default_pwm_frequency;

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.pwm.%d.min-limit", iname, i);
            r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_min_limit[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].pwm_min_limit[i] = 0;

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.pwm.%d.max-scale", iname, i);
            r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_maxscale[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].pwm_maxscale[i] = default_pwm_maxscale;
        }

        // Stepgen pins
        for (int i = 0; i < hal_data[j].n_stepgens; i++) {
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.stepgen.%d.debug-steps", iname, i);
            r = hal_pin_s32_newf(HAL_OUT, &hal_data[j].debug_steps[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].debug_steps[i] = 0;

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.stepgen.%d.command", iname, i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].command[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.stepgen.%d.step-scale", iname, i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].scale[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].scale[i] = default_step_scale;

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.stepgen.%d.feedback", iname, i);
            r = hal_pin_float_newf(HAL_OUT, &hal_data[j].feedback[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.stepgen.%d.mode", iname, i);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].mode[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].mode[i] = 0;

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%s.stepgen.%d.enable", iname, i);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].enable[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].enable[i] = 0;
        }

        // Encoder pins
        for (int i = 0; i < hal_data[j].n_encoders; i++) {
            hal_data[j].delta_time[i] = 0;
            hal_data[j].enc_offset[i] = 0;
            hal_data[j].enc_prev_pos[i] = 0;
            #if use_stepcounter == 1
            const char *e_name = module_name ".%s.stepcounter";
            #else
            const char *e_name = module_name ".%s.encoder";
            #endif

            memset(name, 0, nsize);
            snprintf(name, nsize, e_name, iname);
            int ename_len = strlen(name);
            snprintf(name + ename_len, nsize - ename_len, ".%d.raw-count", i);
            r = hal_pin_s32_newf(HAL_OUT, &hal_data[j].raw_count[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }

            memset(name, 0, nsize);
            snprintf(name, nsize, e_name, iname);
            ename_len = strlen(name);
            snprintf(name + ename_len, nsize - ename_len, ".%d.count-latched", i);
            r = hal_pin_s32_newf(HAL_OUT, &hal_data[j].count_latched[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }

            memset(name, 0, nsize);
            snprintf(name, nsize, e_name, iname);
            ename_len = strlen(name);
            snprintf(name + ename_len, nsize - ename_len, ".%d.position", i);
            r = hal_pin_float_newf(HAL_OUT, &hal_data[j].enc_position[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }

            memset(name, 0, nsize);
            snprintf(name, nsize, e_name, iname);
            ename_len = strlen(name);
            snprintf(name + ename_len, nsize - ename_len, ".%d.position-latched", i);
            r = hal_pin_float_newf(HAL_OUT, &hal_data[j].enc_position_latched[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }

            memset(name, 0, nsize);
            snprintf(name, nsize, e_name, iname);
            ename_len = strlen(name);
            snprintf(name + ename_len, nsize - ename_len, ".%d.scale", i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].enc_scale[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].enc_scale[i] = 1;

            memset(name, 0, nsize);
            snprintf(name, nsize, e_name, iname);
            ename_len = strlen(name);
            snprintf(name + ename_len, nsize - ename_len, ".%d.velocity", i);
            r = hal_pin_float_newf(HAL_OUT, &hal_data[j].enc_velocity[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }

            memset(name, 0, nsize);
            snprintf(name, nsize, e_name, iname);
            ename_len = strlen(name);
            snprintf(name + ename_len, nsize - ename_len, ".%d.index-enable", i);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].enc_index[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }

            memset(name, 0, nsize);
            snprintf(name, nsize, e_name, iname);
            ename_len = strlen(name);
            snprintf(name + ename_len, nsize - ename_len, ".%d.debug-reset", i);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].enc_reset[i], comp_id, name, j);
            if (r < 0) { hal_exit(comp_id); return r; }
            *hal_data[j].enc_reset[i] = 0;
        }

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%s.period", iname);
        r = hal_pin_u32_newf(HAL_IN, &hal_data[j].period, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%s: ERROR: pin period export failed with err=%i\n", iname, r);
            hal_exit(comp_id);
            return r;
        }

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%s.io-ready-in", iname);
        r = hal_pin_bit_newf(HAL_IN, &hal_data[j].io_ready_in, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%s: ERROR: pin io-ready-in export failed with err=%i\n", iname, r);
            hal_exit(comp_id);
            return r;
        }

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%s.io-ready-out", iname);
        r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].io_ready_out, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%s: ERROR: pin io-ready-out export failed with err=%i\n", iname, r);
            hal_exit(comp_id);
            return r;
        }

        char watchdog_name[48] = {0};
        snprintf(watchdog_name, sizeof(watchdog_name), module_name ".%s.watchdog-process", iname);
        r = hal_export_funct(watchdog_name, watchdog_process, &hal_data[j], 1, 1, comp_id);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_export_funct failed for watchdog-process: %d\n", r);
            hal_exit(comp_id);
            return r;
        }

        char process_send[48] = {0};
        snprintf(process_send, sizeof(process_send), module_name ".%s.process-send", iname);
        r = hal_export_funct(process_send, udp_io_process_send, &hal_data[j], 1, 1, comp_id);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_export_funct failed: %d\n", r);
            hal_exit(comp_id);
            return r;
        }

        char process_recv[48] = {0};
        snprintf(process_recv, sizeof(process_recv), module_name ".%s.process-recv", iname);
        r = hal_export_funct(process_recv, udp_io_process_recv, &hal_data[j], 1, 1, comp_id);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_export_funct failed: %d\n", r);
            hal_exit(comp_id);
            return r;
        }
        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%s: pins exported successfully\n", iname);
    }

    r = hal_ready(comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_ready failed: %d\n", r);
        hal_exit(comp_id);
        return r;
    }

    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": component ready.\n");
    return 0;
}

void rtapi_app_exit(void) {
    for (int i = 0; i < instances; i++) {
        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: Exiting component\n", i);
        #if raspberry_pi_spi == 0
        close(hal_data[i].sockfd);
        #else
        bcm2835_spi_end();
        bcm2835_close();
        #endif
    }
    hal_exit(comp_id);
}