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
    char *ip_address[128] = {0,};
    RTAPI_MP_ARRAY_STRING(ip_address, 128, "Ip address");
#else
    #pragma message "SPI version"
    #define module_name "stepgen-ninja"
    #define SPI_SPEED BCM2835_SPI_CLOCK_DIVIDER_64 // ~1 MHz (250 MHz / 256)
    const uint8_t rpi_inputs[] = raspi_inputs;
    const uint8_t rpi_outputs[] = raspi_outputs;
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

// do not modify
#define dormant_cycles 6

// Add 10,000 mm offset to *d->command[i] to avoid simulator zero-crossing issue
// Not needed on real machine due to homing at axis limits, but not hurts real machines.
#define offset 10000

const uint8_t input_pins[] = in_pins;
const uint8_t output_pins[] = out_pins;
const uint8_t in_pins_no = sizeof(input_pins);
const uint8_t out_pins_no = sizeof(output_pins);

typedef struct {
    char ip[16]; // Holds IPv4 address
    int port;
} IpPort;

typedef struct {
    hal_float_t *command[stepgens];
    hal_float_t *feedback[stepgens];
    hal_float_t *scale[stepgens];
    hal_bit_t *mode[stepgens];
    hal_bit_t *enable[stepgens];
    hal_u32_t *pulse_width;
    hal_bit_t *spindle_index;
    // encoder pins
    hal_s32_t *raw_count[encoders];
    hal_s32_t *scaled_count[encoders];
    hal_float_t *enc_value[encoders];
    hal_float_t *enc_scale[encoders];
    hal_bit_t *enc_reset[encoders];
    // pwm output
    hal_bit_t *pwm_enable[pwm_count];
    hal_u32_t *pwm_output[pwm_count];
    hal_u32_t *pwm_frequency[pwm_count];
    hal_u32_t *pwm_maxscale[pwm_count];
    hal_u32_t *pwm_min_limit[pwm_count];
    hal_u32_t *jitter;
    // inputs
    hal_bit_t *input[32];
    hal_bit_t *input_not[32];
    hal_bit_t *rpi_input[32];
    hal_bit_t *rpi_input_not[32];
    // outputs
    hal_bit_t *output[32];
    hal_bit_t *rpi_output[32];

#if debug == 1
    hal_float_t *debug_freq;
    hal_s32_t *debug_steps[stepgens];
    hal_bit_t *debug_steps_reset;
#endif
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
    int32_t enc_offset[encoders];
    int64_t prev_pos[6];
    int64_t curr_pos[6];
    bool watchdog_running;
    bool error_triggered;
    bool first_data;
} module_data_t;

static int instances = 1; // Példányok száma
static int comp_id = -1; // HAL komponens azonosító
static module_data_t *hal_data; // Pointer a megosztott memóriában lévő adatra
static uint32_t timing[1024] = {0, };
static uint32_t old_pulse_width = 0;
static uint8_t tx_counter = 0;
transmission_pc_pico_t *tx_buffer;
transmission_pico_pc_t *rx_buffer;

uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

uint16_t pwm_calculate_wrap(uint32_t freq) {
    // Rendszer órajel lekérése (alapértelmezetten 125 MHz az RP2040 esetén)
    uint32_t sys_clock = 125000000;
    
    // Wrap kiszámítása fix 1.0 divider-rel
    uint32_t wrap = (uint32_t)(sys_clock/ freq);

    if (freq < 1908){
        wrap = 65535; // 65535 is the maximum wrap value for 16-bit PWM
    }
    return (uint16_t)wrap;
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
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: watchdog timeout error, please restart Linuxcnc\n", d->index);
            d->checksum_index_in = 1;  // Reset checksum index
            d->checksum_index = 1;     // Reset checksum index
        }
        d->watchdog_expired = 1; 
    } else {
        d->watchdog_expired = 0; 
    }
}

uint16_t nearest(uint16_t period){
    int min_diff = 655535;
    int value = (uint16_t)period / 8;
    int calc = 0;
    uint8_t index = 0;
    if (value < pio_settings[0].high_cycles){
        return 0;
    }
    if (value > pio_settings[298].high_cycles){
        return 298;
    }
    for (int i = 0; i < 299; i++){
        calc = abs(pio_settings[i].high_cycles - value);
        if (calc < min_diff){
            min_diff = calc;
            index = i;
        }
    }   
    return index;
}

int _receive(void *arg){
    // full duplex transmission not need to receive
    return sizeof(transmission_pico_pc_t);
}

void printbuf(uint8_t *buf, size_t len){
    size_t i;
    for (i=0;i<len;i++){
        printf("%02x", buf[i]);
    }
    printf("\n");
}

int _send(void *arg){
    #if raspberry_pi_spi == 0
        module_data_t *d = arg;
        return sendto(d->sockfd, tx_buffer, tx_size, MSG_DONTROUTE | MSG_DONTWAIT, &d->remote_addr, sizeof(d->remote_addr));
    #else
        // working full duplex
        static size_t ssize = sizeof(transmission_pc_pico_t);
        char *readbuff = malloc(ssize);
        memset(readbuff, 0, ssize);
        bcm2835_spi_transfernb((char *)tx_buffer, readbuff, ssize);
        memcpy(rx_buffer, readbuff + 3, sizeof(transmission_pico_pc_t));
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
        int len = recvfrom(d->sockfd, rx_buffer, rx_size, MSG_DONTWAIT, &d->remote_addr, &addrlen);
    #endif
    if (len == rx_size) {
        if (!tx_checksum_ok(rx_buffer)) {
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
        *d->spindle_index = rx_buffer->interrupt_data && 1;
        // user code start (process received data) rx_buffer[*]
        for (uint8_t i = 0; i < encoders; i++) {
            #if debug == 1
                if (*d->enc_reset[i] == 1)
                {
                    d->enc_offset[i] = rx_buffer->encoder_counter[i];
                    *d->enc_reset[i] = 0; // reset the encoder
                }
                *d->raw_count[i] = rx_buffer->encoder_counter[i] - d->enc_offset[i]; // raw encoder count
                *d->enc_value[i] = (float)(*d->raw_count[i] * *d->enc_scale[i]);
                *d->scaled_count[i] = (int32_t)(*d->raw_count[i] * *d->enc_scale[i]);
            #else
                *d->raw_count[i] = rx_buffer->encoder_counter[i] - d->enc_offset[i]; // raw encoder count
                *d->enc_value[i] = (float)(rx_buffer->encoder_counter[i] * *d->enc_scale[i]);
                *d->scaled_count[i] = (int32_t)(rx_buffer->encoder_counter[i] * *d->enc_scale[i]);
            #endif

        }
        // get the inputs defined in the transmission.c
        for (uint8_t i = 0; i < in_pins_no; i++) {
            *d->input[i] = (rx_buffer->inputs[0] >> (input_pins[i] & 31)) & 1;
            *d->input_not[i] = !(*d->input[i]); // Inverted inputs
        }

        #if raspberry_pi_spi == 1
            for (int i=0; i<rpi_inputs_no;i++){
                *d->rpi_input[i]=bcm2835_gpio_lev(i);
            }
        #endif

        #if breakout_board > 0
            for (uint8_t i = 0; i < 16; i++) {
                *d->input[i + sizeof(input_pins)] = (rx_buffer->inputs[1] >> i) & 1; // MCP23017 inputs
                *d->input_not[i + sizeof(input_pins)] = !(*d->input[i]); // Inverted inputs
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
    double f_steps[stepgens] = {0,};
    uint8_t sign = 0;
    total_cycles = (uint32_t)(*d->period * 1000) / 1000;
    uint32_t max_f = (uint32_t)(1.0 / ((*d->pulse_width * 2) * 1e-9));
    #if debug == 1
    *d->debug_freq = (float)max_f / 1000.0;
    #endif
    memset(tx_buffer, 0, tx_size);
    if (old_pulse_width != *d->pulse_width) {
        uint32_t step_counter;
        uint32_t pio_cmd;
        total_cycles = (uint32_t)((period * 125000UL) / 1000000UL); // pico = 125MHz
        uint16_t pio_index = nearest(*d->pulse_width);
        rtapi_print_msg(RTAPI_MSG_INFO, "Max frequency: %.4f KHz\n", max_f / 1000.0);
        rtapi_print_msg(RTAPI_MSG_INFO, "max pulse_width: %dnS\n", pio_settings[298].high_cycles*8);
        rtapi_print_msg(RTAPI_MSG_INFO, "min pulse_width: %dnS\n", pio_settings[0].high_cycles*8);
        rtapi_print_msg(RTAPI_MSG_INFO, "total_cycles: %d\n", total_cycles);
        rtapi_print_msg(RTAPI_MSG_INFO, "high_cycles: %d\n", pio_settings[pio_index].high_cycles);
        rtapi_print_msg(RTAPI_MSG_INFO, "pio_index: %d\n", pio_index);
        memset(timing, 0, sizeof(timing));
        for (uint16_t i=1; i < 1024; i++){
            step_counter = (uint32_t)((float)((total_cycles ) / i) - pio_settings[pio_index].high_cycles) - dormant_cycles;
            pio_cmd = (uint32_t)(step_counter << 10 | (i - 1));
            timing[i] = pio_cmd;
        }
        old_pulse_width = *d->pulse_width;
    }

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

    if (d->watchdog_running == 1) {
        int32_t cmd[stepgens] = {0,};
        for (int i = 0; i < stepgens; i++) {
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

                #if debug == 1
                *d->debug_steps[i] -= steps ;
                if (*d->debug_steps_reset == 1) {
                    *d->debug_steps[i] = 0;
                    if (i == stepgens - 1){
                        *d->debug_steps_reset = 0;
                    }
                }
                #endif
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

                #if debug == 1
                *d->debug_steps[i] += (uint16_t)steps_per_cycle;
                if (*d->debug_steps_reset == 1) {
                    *d->debug_steps[i] = 0;
                    *d->debug_steps_reset = 0;
                }
                #endif

                if (steps_per_cycle > 0) {
                    cmd[i] = timing[steps_per_cycle] | (sign << 31) ;
                } else {
                    cmd[i] = 0;
                }
            }
            *d->feedback[i] = *d->command[i];
        }

        d->first_data = false;

        for (uint8_t i = 0; i < stepgens; i++) {
            tx_buffer->stepgen_command[i] = cmd[i];
        }

        tx_buffer->pio_timing = nearest(*d->pulse_width);

    if (out_pins_no > 0){
        uint32_t outs=0;
        for (uint8_t i = 0; i < out_pins_no; i++) {
            outs |= *d->output[i] == 1 ? 1 << i : 0;
        }
        tx_buffer->outputs = outs;
    }

    #if raspberry_pi_spi == 1
        for (int i=0; i<rpi_outputs_no;i++){
            if (*d->rpi_output[i]){
                bcm2835_gpio_set(i);
            } else{
                bcm2835_gpio_clr(i);
            }
        }
    #endif

    #if breakout_board > 0
        uint8_t outs=0;
        for (uint8_t i = 0; i < 8; i++) {
            // Copy the encoder values to the tx_buffer
            outs |= *d->output[i] == 1 ? 1 << i : 0; // Set the bit if output is high
        }
    #endif

    #if use_pwm == 1
    for (int i=0;i<pwm_count;i++){
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
    #endif

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

int rtapi_app_main(void) {
    int r;

    rtapi_set_msg_level(RTAPI_MSG_INFO);

    module_init();

    #if raspberry_pi_spi == 0
        IpPort results[MAX_CHAN];
        // parse the IP address and port from the modparam
        instances = parse_ip_port((char *)ip_address[0], results, 8);

        // print parsed IP addresses and ports
        for (int i = 0; i < instances; i++) {
            rtapi_print_msg(RTAPI_MSG_INFO, "Parsed IP: %s, Port: %d\n", results[i].ip, results[i].port);
        }

        // Check if instances is greater than MAX_CHAN
        if (instances > MAX_CHAN) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": Too many channels, max %d allowed\n", MAX_CHAN);
            return -1;
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

        #if raspberry_pi_spi == 0
            hal_data[j].ip_address = &results[j];
            rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: init_socket\n", j);
            init_socket(&hal_data[j]);
            rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: init_socket ready..\n", j);
        #else
            rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: init_spi\n", j);
            init_spi();
            rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: init_spi ready..\n", j);
            instances = 1;
        #endif

        uint32_t nsize = sizeof(name);
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.connected", j);

        r = hal_pin_bit_newf(HAL_IN, &hal_data[j].connected, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.stepgen.pulse-width", j);
        r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pulse_width, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }
        *hal_data[j].pulse_width = default_pulse_width; // Default pulse width in nanoseconds

        #if debug == 1
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.stepgen.max-freq-khz", j);
        r = hal_pin_float_newf(HAL_OUT, &hal_data[j].debug_freq, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.jitter", j);
        r = hal_pin_u32_newf(HAL_OUT, &hal_data[j].jitter, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }
        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.stepgen.debug-steps-reset", j);
        r = hal_pin_bit_newf(HAL_IN, &hal_data[j].debug_steps_reset, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }
        #endif

        #if raspberry_pi_spi == 1
            for (int i = 0; i< rpi_inputs_no; i++){
                bcm2835_gpio_fsel(rpi_inputs[i], BCM2835_GPIO_FSEL_INPT);
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.rpi-input.gp%d", j, rpi_inputs[i]);
                r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].rpi_input[i], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.rpi-input.gp%d-not", j, rpi_inputs[i]);
                r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].rpi_input_not[i], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
            }
            for (int i = 0; i< rpi_outputs_no; i++){
                bcm2835_gpio_fsel(rpi_outputs[i], BCM2835_GPIO_FSEL_OUTP);
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.rpi-output.gp%d", j, rpi_outputs[i]);
                r = hal_pin_bit_newf(HAL_IN, &hal_data[j].rpi_output[i], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
                *hal_data[j].rpi_output[i] = 0; // Initialize output pins to 0
            }
        #endif

        for (int i = 0; i< in_pins_no; i++){
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%d.input.gp%d", j, input_pins[i]);
            r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].input[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%d.input.gp%d-not", j, input_pins[i]);
            r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].input_not[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
        }

        #if spindle_encoder_index_GPIO > 0
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%d.spindle.index-enable", j);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].spindle_index, comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
        #endif

        if (out_pins_no > 0){
            for (int i = 0; i< out_pins_no; i++){
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.output.gp%d", j, output_pins[i]);
                r = hal_pin_bit_newf(HAL_IN, &hal_data[j].output[i], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
                *hal_data[j].output[i] = 0; // Initialize output pins to 0
            }
        }

        #if breakout_board > 0
            for (int i = 0; i < 8; i++){
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.output.%d", j, i);
                r = hal_pin_bit_newf(HAL_IN, &hal_data[j].output[i], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
                *hal_data[j].output[i] = 0; // Initialize output pins to 0
            }
            for (int i = 0; i < 16; i++){
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.input.%d", j, input_pins[i]);
                r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].input[i + sizeof(input_pins)], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.input.%d-not", j, input_pins[i]);
                r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].input_not[i + sizeof(input_pins)], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
            }
        #endif

        #if use_pwm == 1
            for(int i = 0; i < pwm_count; ++i){
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.pwm.%d.enable", j, i);
                r = hal_pin_bit_newf(HAL_IN, &hal_data[j].pwm_enable[i], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
                *hal_data[j].pwm_enable[i] = 0;
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.pwm.%d.duty", j, i);
                r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_output[i], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.pwm.%d.frequency", j, i);
                r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_frequency[i], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
                *hal_data[j].pwm_frequency[i] = default_pwm_frequency; // Default PWM frequency in Hz

                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.pwm.%d.min-limit", j, i);
                r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_min_limit[i], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
                *hal_data[j].pwm_min_limit[i] = 0; // Default minimum limit for PWM output

                memset(name, 0, nsize);
                snprintf(name, nsize, module_name ".%d.pwm.%d.max-scale", j, i);
                r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_maxscale[i], comp_id, name, j);
                if (r < 0) {
                    rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                    hal_exit(comp_id);
                    return r;
                }
                *hal_data[j].pwm_maxscale[i] = default_pwm_maxscale; // default max scale
            }
        #endif

        for (int i = 0; i<stepgens; i++){
            #if debug == 1
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%d.stepgen.%d.debug-steps", j, i);
            r = hal_pin_s32_newf(HAL_OUT, &hal_data[j].debug_steps[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            *hal_data[j].debug_steps[i] = 0;
            #endif

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%d.stepgen.%d.command", j, i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].command[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%d.stepgen.%d.step-scale", j, i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].scale[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            *hal_data[j].scale[i] = default_step_scale; // Default scale factor

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%d.stepgen.%d.feedback", j, i);
            r = hal_pin_float_newf(HAL_OUT, &hal_data[j].feedback[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%d.stepgen.%d.mode", j, i);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].mode[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            *hal_data[j].mode[i] = 0; //always start with position mode

            memset(name, 0, nsize);
            snprintf(name, nsize, module_name ".%d.stepgen.%d.enable", j, i);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].enable[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            *hal_data[j].enable[i] = 0;
        }

        for (int i = 0; i<encoders; i++)
        {
            #if use_stepcounter == 1
                #define e_name module_name ".%d.stepcounter"
            #else
                #define e_name module_name ".%d.encoder"
            #endif
            hal_data[j].enc_offset[i] = 0; // Initialize encoder offset to 0
            memset(name, 0, nsize);
            snprintf(name, nsize, e_name ".%d.raw-count", j, i);
            r = hal_pin_s32_newf(HAL_IN, &hal_data[j].raw_count[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, nsize);
            snprintf(name, nsize, e_name ".%d.scaled-count", j, i);
            r = hal_pin_s32_newf(HAL_OUT, &hal_data[j].scaled_count[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, nsize);
            snprintf(name, nsize, e_name ".%d.scale", j, i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].enc_scale[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            *hal_data[j].scale[i] = 1;
            memset(name, 0, nsize);
            snprintf(name, nsize, e_name ".%d.scaled-value", j, i);
            r = hal_pin_float_newf(HAL_OUT, &hal_data[j].enc_value[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            #if debug == 1
            memset(name, 0, nsize);
            snprintf(name, nsize, e_name ".%d.debug-reset", j, i);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].enc_reset[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            *hal_data[j].enc_reset[i] = 0; // Initialize reset pin to 0
            #endif
        }

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.period", j);

        r = hal_pin_u32_newf(HAL_IN, &hal_data[j].period, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.io-ready-in", j);

            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].io_ready_in, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": ERROR: pin connected export failed with err=%i\n", r);
            hal_exit(comp_id);
            return r;
        }

        memset(name, 0, nsize);
        snprintf(name, nsize, module_name ".%d.io-ready-out", j);

        r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].io_ready_out, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": ERROR: pin connected export failed with err=%i\n", r);
            hal_exit(comp_id);
            return r;
        }
        #pragma message "Adding export functions. (watchdog)"
        char watchdog_name[48] = {0};
        snprintf(watchdog_name, sizeof(watchdog_name),module_name ".%d.watchdog-process", j);
        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: hal_export_funct for watchdog-process: %d init...\n", j, r);
        r = hal_export_funct(watchdog_name, watchdog_process, &hal_data[j], 1, 1, comp_id);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_export_funct failed for watchdog-process: %d\n", r);
            hal_exit(comp_id);
            return r;
        }
        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: hal_export_funct for watchdog-process: %d\n", j, r);

        #pragma message "Adding export functions. (process-send)"
        char process_send[48] = {0};
        snprintf(process_send, sizeof(process_send), module_name ".%d.process-send", j);
        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: hal_export_funct for process-send %d init...\n", j, r);
        r = hal_export_funct(process_send, udp_io_process_send, &hal_data[j], 1, 1, comp_id);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_export_funct failed: %d\n", r);
            hal_exit(comp_id);
            return r;
        }
        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: hal_export_funct for process_send: %d\n", j, r);

        #pragma message "Adding export functions. (process-recv)"
        char process_recv[48] = {0};
        snprintf(process_recv, sizeof(process_recv), module_name ".%d.process-recv", j);
        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: hal_export_funct for process-recv: %d init...\n", j, r);
        r = hal_export_funct(process_recv, udp_io_process_recv, &hal_data[j], 1, 1, comp_id);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_export_funct failed: %d\n", r);
            hal_exit(comp_id);
            return r;
        }
        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: hal_export_funct for process_recv: %d\n", j, r);
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
