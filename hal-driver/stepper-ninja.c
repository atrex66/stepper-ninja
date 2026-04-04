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
    #define module_name "stepper-ninja"
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

#include "hal_pin_macros.h"

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
#define ENCODER_VEL_FP_SCALE 10000.0f

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
#endif

typedef struct {
    char ip[16]; // Holds IPv4 address
    int port;
} IpPort;

typedef struct {
    float y;         // Szűrt érték
    float alpha;     // Előre kiszámolt súlyzó (T / (tau + T))
} LowPassFilter;

typedef struct {
    #if stepgens > 0
    hal_float_t *command[stepgens];
    hal_float_t *feedback[stepgens];
    hal_float_t *scale[stepgens];
    hal_bit_t *mode[stepgens];
    hal_bit_t *enable[stepgens];
    hal_u32_t *pulse_width;
    #endif
    #if encoders > 0
    // encoder pins
    hal_s32_t *raw_count[encoders];
    //hal_s32_t *count_latched[encoders];
    hal_float_t *enc_scale[encoders];
    hal_float_t *enc_position[encoders];
    //hal_float_t *enc_position_latched[encoders];
    hal_float_t *enc_velocity[encoders];
    hal_bit_t *enc_index[encoders];
    hal_bit_t *enc_reset[encoders];
    hal_float_t *enc_rpm[encoders];
    #endif
    #if use_pwm == 1
    // pwm output
    hal_bit_t *pwm_enable[pwm_count];
    hal_u32_t *pwm_output[pwm_count];
    hal_u32_t *pwm_frequency[pwm_count];
    hal_u32_t *pwm_maxscale[pwm_count];
    hal_u32_t *pwm_min_limit[pwm_count];
    #endif 
    
    #if ANALOG_CH > 0
    hal_float_t *analog_value[ANALOG_CH];
    hal_float_t *analog_min[ANALOG_CH];
    hal_float_t *analog_max[ANALOG_CH];
    hal_bit_t *analog_enable[ANALOG_CH];
    hal_s32_t *analog_offset[ANALOG_CH];
    #endif

    hal_s32_t *jitter;
    // inputs
    hal_bit_t *input[96];
    hal_bit_t *input_not[96];
    hal_bit_t *rpi_input[32];
    hal_bit_t *rpi_input_not[32];

    // outputs
    hal_bit_t *output[64];
    hal_bit_t *rpi_output[32];

#if toolchanger_encoder == 1
    hal_bit_t *toolchanger_bit0;  // 4 bit data from the toolchanger, latched on the rising edge of the strobe signal
    hal_bit_t *toolchanger_bit1;  // 4 bit data from the toolchanger, latched on the rising edge of the strobe signal
    hal_bit_t *toolchanger_bit2;  // 4 bit data from the toolchanger, latched on the rising edge of the strobe signal
    hal_bit_t *toolchanger_bit3;  // 4 bit data from the toolchanger, latched on the rising edge of the strobe signal
    hal_bit_t *toolchanger_strobe; // active high strobe signal from the toolchanger, data is latched on the rising edge
    hal_bit_t *toolchanger_parity; // parity bit from the toolchanger, latched on the rising edge of the strobe signal
    hal_bit_t *toolchanger_even_or_odd_parity;  // 1 if odd parity, 0 if even parity
    hal_u32_t *toolchanger_position; // the current position of the toolchanger, updated on each strobe
    hal_bit_t *toolchanger_error; // indicates a parity error in the received toolchanger data
#endif

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
    float enc_prev_pos[encoders];
    uint32_t enc_timestamp[encoders];
    int32_t enc_offset[encoders];
    uint32_t delta_time[encoders];
    int64_t prev_pos[6];
    int64_t curr_pos[6];
    bool watchdog_running;
    bool error_triggered;
    bool first_data;
    float delta_pos[encoders];
    int32_t delta_count[encoders];
    int32_t delta_count_accum[encoders];
    uint8_t tx_counter;
} module_data_t;

static int instances = 1; // Példányok száma
static int comp_id = -1; // HAL komponens azonosító
static module_data_t *hal_data; // Pointer a megosztott memóriában lévő adatra

#if stepgens > 0
static uint32_t timing[1024] = {0, };
static uint32_t old_pulse_width = 0;

#endif

static uint32_t counter=0;

float cycle_time_ns = 1.0f / pico_clock * 1000000000.0f; // Ciklusidő nanoszekundumban
transmission_pc_pico_t *tx_buffer;
transmission_pico_pc_t *rx_buffer;

#if encoders > 0
LowPassFilter filter[encoders];
float error_estimate = 0.1;
#endif

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
    #if encoders > 0
        for (int i = 0; i<encoders; i++){
        lpf_init(&filter[i], 0.008f, 0.0001f);
        }
    #endif
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

// ==================== BREAKOUT BOARD: HAL-side module includes ====================
// Each board module implements bb_hal_setup_pins(), bb_hal_process_recv(),
// and bb_hal_process_send() with common names — only one is ever compiled in.
#if breakout_board == 1
#include "modules/breakoutboard_hal_1.c"
#elif breakout_board == 2
#include "modules/breakoutboard_hal_2.c"
#elif breakout_board == 3
#include "modules/breakoutboard_hal_3.c"
#elif breakout_board == 100
#include "modules/breakoutboard_hal_100.c"
#else
#include "modules/breakoutboard_hal_0.c"
#endif
// ====================================================================================

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

uint32_t test[3] = {1, 0, 0};  // bit 0 aktív

static inline void roll_left_96(uint32_t buf[3])
{
    uint32_t carry0 = (buf[0] >> 31) & 1;  // ami átmegy buf[1]-be
    uint32_t carry1 = (buf[1] >> 31) & 1;  // ami átmegy buf[2]-be
    uint32_t carry2 = (buf[2] >> 31) & 1;  // ami visszamegy buf[0]-ba

    buf[0] = (buf[0] << 1) | carry2;
    buf[1] = (buf[1] << 1) | carry0;
    buf[2] = (buf[2] << 1) | carry1;
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
        *d->jitter = 1000 - rx_buffer->jitter; // Set jitter value from received data
        #if encoders > 0
            for (uint8_t i = 0; i < encoders; i++) {
                #if debug == 1
                    if (*d->enc_reset[i] == 1)
                    {
                        d->enc_offset[i] = rx_buffer->encoder_counter[i];
                        *d->enc_reset[i] = 0; // reset the encoder
                    }
                #endif
                uint32_t encoder_ts = rx_buffer->encoder_timestamp[i];
                int32_t encoder_count = rx_buffer->encoder_counter[i];
                uint8_t index_reset_event = (rx_buffer->interrupt_data >> i) & 0x01u;

                //*d->count_latched[i] = encoder_count - rx_buffer->encoder_latched[i];
                *d->enc_position[i] = (float)encoder_count / *d->enc_scale[i];
                //*d->enc_position_latched[i] = (float)((encoder_count - rx_buffer->encoder_latched[i]) / *d->enc_scale[i]);

                if (d->enc_timestamp[i] == 0) {
                    *d->raw_count[i] = encoder_count;
                    d->enc_timestamp[i] = encoder_ts;
                    d->delta_count[i] = 0;
                    d->delta_count_accum[i] = 0;
                    d->delta_time[i] = 0;
                    d->delta_pos[i] = 0.0f;
                    d->enc_prev_pos[i] = *d->enc_position[i];
                    continue;
                }

                if (index_reset_event) {
                    // Rebase encoder timing/count state on index-reset event so
                    // one-shot index-enable reset does not inject a velocity spike.
                    *d->raw_count[i] = encoder_count;
                    d->enc_timestamp[i] = encoder_ts;
                    d->delta_count[i] = 0;
                    d->delta_count_accum[i] = 0;
                    d->delta_time[i] = 0;
                    d->delta_pos[i] = 0.0f;
                    d->enc_prev_pos[i] = *d->enc_position[i];
                    *d->enc_index[i] = 0;
                    continue;
                }

                if (*d->enc_index[i] == 1) {
                    d->delta_count[i] = encoder_count - *d->raw_count[i];
                    if (d->delta_count[i] < -(*d->enc_scale[i] / 2)) {
                        d->delta_count[i] += (int32_t)*d->enc_scale[i];
                    }
                    else if (d->delta_count[i] > (*d->enc_scale[i] / 2)) {
                        d->delta_count[i] -= (int32_t)*d->enc_scale[i];
                    }
                } else {
                    d->delta_count[i] = (int32_t)((uint32_t)encoder_count - (uint32_t)*d->raw_count[i]);
                }

                *d->raw_count[i] = encoder_count; // raw encoder count
                d->delta_time[i] += encoder_ts - d->enc_timestamp[i];
                d->delta_count_accum[i] += d->delta_count[i];

                #if use_stepcounter == 0
                    d->delta_pos[i] = (float)d->delta_count[i] / *d->enc_scale[i];
                    *d->enc_velocity[i] = lpf_update(
                        &filter[i],
                        (((float)rx_buffer->encoder_velocity[i]) / ENCODER_VEL_FP_SCALE) / *d->enc_scale[i]
                    );
                    //*d->enc_velocity[i] = (((float)rx_buffer->encoder_velocity[i]) / ENCODER_VEL_FP_SCALE) / *d->enc_scale[i];
                    d->delta_time[i] = 0;
                    d->delta_count_accum[i] = 0;
                    *d->enc_rpm[i] = (*d->enc_velocity[i]) * 60.0f; // Convert velocity to RPM
                #else
                    if (d->delta_time[i] > 2500000) {
                        *d->enc_velocity[i] = 0;
                        d->delta_time[i] = 0;
                        d->delta_count_accum[i] = 0;
                    } else if (d->delta_count_accum[i] != 0 && d->delta_time[i] > 0) {
                        d->delta_pos[i] = (float)d->delta_count_accum[i] / *d->enc_scale[i];
                        *d->enc_velocity[i] = lpf_update(&filter[i], d->delta_pos[i] * (1000000.0f / (float)d->delta_time[i]));
                        //*d->enc_velocity[i] = kalman_filter(d->delta_pos[i] * (1000000.0f / (float)d->delta_time[i]), *d->enc_velocity[i], &error_estimate);
                        d->delta_time[i] = 0;
                        d->delta_count_accum[i] = 0;
                    } else {
                        d->delta_pos[i] = 0.0f;
                    }
                #endif

                d->enc_timestamp[i] = encoder_ts;
                d->enc_prev_pos[i] = *d->enc_position[i];
            }
        #endif
        #if raspberry_pi_spi == 1
            for (int i=0; i<rpi_inputs_no;i++){
                *d->rpi_input[i]=bcm2835_gpio_lev(rpi_inputs[i]);
            }
        #endif

        bb_hal_process_recv(d);

    }
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
    #if encoders > 0
    tx_buffer->enc_control = 0;
    for (int i=0;i<encoders;i++){
        tx_buffer->enc_control |= (uint8_t)(1 * *d->enc_index[i])  << (CTRL_SPINDEX + i);
    }
    #endif

    if (d->watchdog_running == 1) {
        #if stepgens > 0
        double f_steps[stepgens] = {0,};
        uint32_t max_f = (uint32_t)(1.0 / ((*d->pulse_width * 2) * 1e-9));
        #if debug == 1
        *d->debug_freq = (float)max_f / 1000.0;
        #endif
        if (old_pulse_width != *d->pulse_width) {
            old_pulse_width = *d->pulse_width;
            uint32_t step_counter;
            uint32_t pio_cmd;
            total_cycles = (uint32_t)((period * (pico_clock / 1000)) / 1000000UL); // pico = 125MHz
            uint16_t pio_index = nearest(*d->pulse_width);
            rtapi_print_msg(RTAPI_MSG_INFO, "Max frequency: %.4f KHz\n", max_f / 1000.0);
            rtapi_print_msg(RTAPI_MSG_INFO, "max pulse_width: %dnS\n", pio_settings[298].high_cycles*(int)cycle_time_ns);
            rtapi_print_msg(RTAPI_MSG_INFO, "min pulse_width: %dnS\n", pio_settings[0].high_cycles*(int)cycle_time_ns);
            memset(timing, 0, sizeof(timing));
            for (uint16_t i=1; i < 1024; i++){
                step_counter = (uint32_t)((float)((total_cycles ) / i) - pio_settings[pio_index].high_cycles) - dormant_cycles;
                pio_cmd = (uint32_t)(step_counter << 10 | (i - 1));
                timing[i] = pio_cmd;
            }
        }

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
        #endif

    #if raspberry_pi_spi == 1
        for (int i=0; i<rpi_outputs_no;i++){
            if (*d->rpi_output[i]){
                bcm2835_gpio_set(rpi_outputs[i]);
            } else{
                bcm2835_gpio_clr(rpi_outputs[i]);
            }
        }
    #endif

    bb_hal_process_send(d);

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

    tx_buffer->packet_id = d->tx_counter;
    tx_buffer->checksum = calculate_checksum(tx_buffer, tx_size - 1); // Calculate checksum excluding the checksum byte itself
    _send(d);
    d->tx_counter++;
    
    }
    else{
        //if the watchdog is not running, we should not send data (stepper-ninja side is going to timeout error and turn off outputs)
        if (!d->error_triggered){
            d->error_triggered = true; // Set the error triggered flag to prevent multiple messages
            *d->io_ready_out = 0;      // set the io-ready-out pin to 0 to break the estop-loop
            rtapi_print_msg(RTAPI_MSG_ERR ,module_name ".%d: watchdog not running\n", d->index);
            return;  // No data to send (generate stepper-ninja side timeout error)
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
        PIN_BIT(&hal_data[j].connected, HAL_IN, module_name ".%d.connected", j);

        #if stepgens > 0
        PIN_U32_INIT(&hal_data[j].pulse_width, HAL_IN, default_pulse_width, module_name ".%d.stepgen.pulse-width", j);

            #if debug == 1
            PIN_FLOAT(&hal_data[j].debug_freq, HAL_OUT, module_name ".%d.stepgen.max-freq-khz", j);
            PIN_BIT(&hal_data[j].debug_steps_reset, HAL_IN, module_name ".%d.stepgen.debug-steps-reset", j);
            #endif
        #endif

        PIN_S32(&hal_data[j].jitter, HAL_OUT, module_name ".%d.jitter", j);

        #if raspberry_pi_spi == 1
            for (int i = 0; i< rpi_inputs_no; i++){

                bcm2835_gpio_fsel(rpi_inputs[i], BCM2835_GPIO_FSEL_INPT);
                // enable gpio pullup if need
                if (rpi_input_pullup[i]){
                    bcm2835_gpio_set_pud(rpi_inputs[i], BCM2835_GPIO_PUD_UP);
                }
                else{
                    bcm2835_gpio_set_pud(rpi_inputs[i],BCM2835_GPIO_PUD_DOWN);
                }
                PIN_BIT(&hal_data[j].rpi_input[i], HAL_OUT, module_name ".%d.rpi-input.gp%d", j, rpi_inputs[i]);
                PIN_BIT(&hal_data[j].rpi_input_not[i], HAL_OUT, module_name ".%d.rpi-input.gp%d-not", j, rpi_inputs[i]);
            }
            for (int i = 0; i< rpi_outputs_no; i++){
                bcm2835_gpio_fsel(rpi_outputs[i], BCM2835_GPIO_FSEL_OUTP);
                PIN_BIT_INIT(&hal_data[j].rpi_output[i], HAL_IN, 0, module_name ".%d.rpi-output.gp%d", j, rpi_outputs[i]);
            }
        #endif

        r = bb_hal_setup_pins(&hal_data[j], j, comp_id, name, nsize);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: board pin setup failed err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }
        // Analog channel pins for custom configurations. Breakout boards that
        // provide analog channels register them in their board-specific helpers.
        #if ANALOG_CH > 0 && breakout_board < 1
            for (int i=0;i < ANALOG_CH; i++){
                PIN_BIT_INIT(&hal_data[j].analog_enable[i], HAL_IN, 0, module_name ".%d.analog.%d.enable", j, i);
                PIN_FLOAT_INIT(&hal_data[j].analog_min[i], HAL_IN, 0.0, module_name ".%d.analog.%d.minimum", j, i);
                PIN_FLOAT_INIT(&hal_data[j].analog_max[i], HAL_IN, 0.0, module_name ".%d.analog.%d.maximum", j, i);
                PIN_FLOAT_INIT(&hal_data[j].analog_value[i], HAL_IN, 0.0, module_name ".%d.analog.%d.value", j, i);
            }
        #endif

        #if use_pwm == 1
            for(int i = 0; i < pwm_count; ++i){
                PIN_BIT_INIT(&hal_data[j].pwm_enable[i], HAL_IN, 0, module_name ".%d.pwm.%d.enable", j, i);
                PIN_U32(&hal_data[j].pwm_output[i], HAL_IN, module_name ".%d.pwm.%d.duty", j, i);
                PIN_U32_INIT(&hal_data[j].pwm_frequency[i], HAL_IN, default_pwm_frequency, module_name ".%d.pwm.%d.frequency", j, i);
                PIN_U32_INIT(&hal_data[j].pwm_min_limit[i], HAL_IN, 0, module_name ".%d.pwm.%d.min-limit", j, i);
                PIN_U32_INIT(&hal_data[j].pwm_maxscale[i], HAL_IN, default_pwm_maxscale, module_name ".%d.pwm.%d.max-scale", j, i);
            }
        #endif

        #if stepgens > 0
        for (int i = 0; i<stepgens; i++){
            #if debug == 1
            PIN_S32_INIT(&hal_data[j].debug_steps[i], HAL_OUT, 0, module_name ".%d.stepgen.%d.debug-steps", j, i);
            #endif

            PIN_FLOAT(&hal_data[j].command[i], HAL_IN, module_name ".%d.stepgen.%d.command", j, i);
            PIN_FLOAT_INIT(&hal_data[j].scale[i], HAL_IN, default_step_scale, module_name ".%d.stepgen.%d.step-scale", j, i);
            PIN_FLOAT(&hal_data[j].feedback[i], HAL_OUT, module_name ".%d.stepgen.%d.feedback", j, i);
            PIN_BIT_INIT(&hal_data[j].mode[i], HAL_IN, 0, module_name ".%d.stepgen.%d.mode", j, i);
            PIN_BIT_INIT(&hal_data[j].enable[i], HAL_IN, 0, module_name ".%d.stepgen.%d.enable", j, i);
        }
        #endif
        #if encoders > 0
        for (int i = 0; i<encoders; i++)
        {
            hal_data[j].delta_time[i] = 0;
            hal_data[j].delta_count_accum[i] = 0;
            hal_data[j].enc_timestamp[i] = 0;
            #if use_stepcounter == 1
                #define e_name module_name ".%d.stepcounter"
            #else
                #define e_name module_name ".%d.encoder"
            #endif
            hal_data[j].enc_offset[i] = 0; // Initialize encoder offset to 0
            PIN_S32(&hal_data[j].raw_count[i], HAL_OUT, e_name ".%d.raw-count", j, i);
            PIN_FLOAT(&hal_data[j].enc_position[i], HAL_OUT, e_name ".%d.position", j, i);
            PIN_FLOAT_INIT(&hal_data[j].enc_scale[i], HAL_IN, 1, e_name ".%d.scale", j, i);
            PIN_FLOAT(&hal_data[j].enc_velocity[i], HAL_OUT, e_name ".%d.velocity-rps", j, i);
            PIN_BIT(&hal_data[j].enc_index[i], HAL_IN, e_name ".%d.index-enable", j, i);
            PIN_FLOAT(&hal_data[j].enc_rpm[i], HAL_OUT, e_name ".%d.velocity-rpm", j, i);
            #if debug == 1
            PIN_BIT_INIT(&hal_data[j].enc_reset[i], HAL_IN, 0, e_name ".%d.debug-reset", j, i);
            hal_data[j].enc_prev_pos[i] = 0;
            #endif
        }
        #endif

        PIN_U32(&hal_data[j].period, HAL_IN, module_name ".%d.period", j);
        PIN_BIT(&hal_data[j].io_ready_in, HAL_IN, module_name ".%d.io-ready-in", j);
        PIN_BIT(&hal_data[j].io_ready_out, HAL_OUT, module_name ".%d.io-ready-out", j);
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
