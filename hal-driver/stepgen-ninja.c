#include "rtapi.h"              /* RTAPI realtime OS API */
#include "rtapi_app.h"          /* RTAPI realtime module decls */
#include "rtapi_errno.h"        /* EINVAL etc */
#include "hal.h"                /* HAL public API decls */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

// name of the module
#define module_name "stepgen-ninja"
#define debug 1

/* module information */
MODULE_AUTHOR("Viola Zsolt");
MODULE_DESCRIPTION(module_name " driver");
MODULE_LICENSE("MIT");

// to parse the modparam
char *ip_address[128] = {0,};
RTAPI_MP_ARRAY_STRING(ip_address, 128, "Ip address");

// receive buffer size 
// #define tx_size 25
uint8_t tx_size = 25; // transmit buffer size

// transmit buffer size
// #define rx_size 25
uint8_t rx_size = 25; // receive buffer size

// maximum number of channels
#define MAX_CHAN 4

uint32_t total_cycles;

#define dormant_cycles 5

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
    // encoder pins
    hal_s32_t *raw_count[encoders];
    hal_s32_t *scaled_count[encoders];
    hal_float_t *enc_value[encoders];
    hal_float_t *enc_scale[encoders];
    // pwm output
    hal_bit_t *pwm_enable;
    hal_u32_t *pwm_output;
    hal_u32_t *pwm_frequency;
    hal_u32_t *pwm_maxscale;
    hal_u32_t *pwm_min_limit;
    // inputs
    hal_bit_t *input[32];
    #if use_outputs == 1
    // outputs
    hal_bit_t *output[32];
    #endif

#if debug == 1
    hal_float_t *debug_freq;
    hal_u32_t *debug_steps[stepgens];
#endif
    // hal driver inside working
    hal_u32_t *period;
    hal_bit_t *connected;  
    hal_bit_t *io_ready_in;  
    hal_bit_t *io_ready_out;
    IpPort *ip_address;
    int sockfd;
    struct sockaddr_in local_addr, remote_addr;
    long long last_received_time;
    long long watchdog_timeout;
    int watchdog_expired; 
    long long current_time;
    int index;
    uint8_t checksum_index;
    uint8_t checksum_index_in;
    uint8_t checksum_error;
    int32_t prev_pos[6];
    int32_t curr_pos[6];
    bool watchdog_running;
    bool error_triggered;
    bool first_data;
} module_data_t;

static int instances = 0; // Példányok száma
static int comp_id = -1; // HAL komponens azonosító
static module_data_t *hal_data; // Pointer a megosztott memóriában lévő adatra
static int checksum_error = 0;
static int bufsize = 65536;
static uint32_t timing[1024] = {0, };
static bool first_send = true;
static uint32_t old_pulse_width = 0;
static uint8_t tx_counter = 0;
transmission_pc_pico_t *tx_buffer;
transmission_pico_pc_t *rx_buffer;

uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
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

// Watchdog process
void watchdog_process(void *arg, long period) {
    module_data_t *d = arg;
    static uint64_t prev_time_ns = 0;
    uint64_t now = get_time_ns();
    if (prev_time_ns != 0) {
        uint64_t diff = now - prev_time_ns;
        //*d->period = (uint32_t)diff;
    }
    prev_time_ns = now;

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

uint8_t nearest(uint16_t period){
    int min_diff = 655535;
    int value = (uint16_t)period / 8;
    int calc = 0;
    uint8_t index = 0;
    if (value < pio_settings[0].high_cycles){
        return 0;
    }
    if (value > pio_settings[224].high_cycles){
        return 224;
    }
    for (int i = 0; i < 225; i++){
        calc = abs(pio_settings[i].high_cycles - value);
        if (calc < min_diff){
            min_diff = calc;
            index = i;
        }
    }   
    return index;
}

// parse inputs
void udp_io_process_recv(void *arg, long period) {
    module_data_t *d = arg;
    static socklen_t addrlen = sizeof(d->remote_addr);
    uint8_t calcChecksum = 0;
    if (d->watchdog_expired) {
        *d->io_ready_out = 0;
        return;
    }
    int len = recvfrom(d->sockfd, rx_buffer, rx_size, MSG_WAITALL, &d->remote_addr, &addrlen);
    //int len = recvfrom(d->sockfd, d->rx_buffer, rx_size, 0, &d->remote_addr, &addrlen);
    if (len == rx_size) {
        if (!tx_checksum_ok(rx_buffer)) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: checksum error: %d != %d\n", 
                           d->index, rx_buffer->checksum, calculate_checksum(&rx_buffer, rx_size - 1));
            d->checksum_error = 1;
            d->connected = 0;
            *d->io_ready_out = 0; // set io-ready-out to 0 to break the estop-loop
            return;
        }
        *d->connected = 1;
        d->last_received_time = d->current_time;
        // user code start (process received data) rx_buffer[*]
        for (uint8_t i = 0; i < encoders; i++) {
            *d->raw_count[i] = rx_buffer->encoder_counter[i];
            *d->enc_value[i] = (float)(rx_buffer->encoder_counter[i] * *d->enc_scale[i]);
            *d->scaled_count[i] = (int32_t)(rx_buffer->encoder_counter[i] * *d->enc_scale[i]);
        }
        // get the inputs defined in the transmission.c
        for (uint8_t i = 0; i < sizeof(input_pins); i++) {
            *d->input[i] = (rx_buffer->inputs[0] >> (input_pins[i] & 31)) & 1; 
        }
        // user code end
    }
    // no data received len = -1
}

void print_binary_to_array(uint32_t num) {
    char binary[40]; // 32 bit + 4 szóköz + 1 lezáró null
    int index = 0;

    for (int i = 31; i >= 0; i--) {
        binary[index++] = ((num >> i) & 1) ? '1' : '0'; // Bit kiolvasása és tárolása
        if (i % 8 == 0 && i != 0) {
            binary[index++] = ' '; // 8 bitenként szóköz
        }
    }
    binary[index] = '\0'; // Tömb lezárása

    // Végső kiírás
    rtapi_print_msg(RTAPI_MSG_INFO,"%s\n", binary);
} 
  
// parse outputs
static void udp_io_process_send(void *arg, long period) {
    module_data_t *d = arg;
    uint8_t enc_index = 0;
    int16_t steps;
    uint8_t sign = 0;
    total_cycles = (uint32_t)(*d->period * 1000) / 1000;
    bool reset_active = false;
    uint32_t max_f = (uint32_t)(1.0 / ((*d->pulse_width * 2) * 1e-9));
    #if debug == 1
    *d->debug_freq = (float)max_f / 1000.0;
    #endif
    memset(tx_buffer, 0, tx_size);
    if (old_pulse_width != *d->pulse_width) {
        uint32_t step_counter;
        uint32_t pio_cmd;
        total_cycles = (uint32_t)((period * 125000UL) / 1000000UL); // pico = 125MHz
        uint8_t pio_index = nearest(*d->pulse_width);
        rtapi_print_msg(RTAPI_MSG_INFO, "Max frequency: %.4f KHz\n", max_f / 1000.0);
        rtapi_print_msg(RTAPI_MSG_INFO, "max pulse_width: %dnS\n", pio_settings[264].high_cycles*8);
        rtapi_print_msg(RTAPI_MSG_INFO, "total_cycles: %d\n", total_cycles);
        rtapi_print_msg(RTAPI_MSG_INFO, "high_cycles: %d\n", pio_settings[pio_index].high_cycles);
        rtapi_print_msg(RTAPI_MSG_INFO, "pio_index: %d\n", pio_index);
        memset(timing, 0, sizeof(timing));
        for (uint16_t i=1; i < 512; i++){
            #if debug == 0
            step_counter = (uint32_t)((float)(total_cycles  / i) - pio_settings[pio_index].high_cycles) - dormant_cycles;
            #else
            step_counter = (((total_cycles ) / i) - pio_settings[pio_index].high_cycles) - dormant_cycles;
            #endif
            pio_cmd = (uint32_t)(step_counter << 9 | (i - 1));
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
        // fill tx_buffer with zeros
        int32_t cmd[stepgens] = {0,};
        for (int i = 0; i < stepgens; i++) {
            // position mode
            if (d->first_data) {
                d->prev_pos[i] = (int32_t)(*d->command[i] * *d->scale[i]);
            }
            if (*d->enable[i] == 0) {
                cmd[i] = 0; // disable stepgen
                continue;
            }
            if (*d->mode[i] == 0) {
                d->curr_pos[i] = (int32_t)(*d->command[i] * *d->scale[i]);
                steps = (int16_t)(d->prev_pos[i] - d->curr_pos[i]);
                if (steps < 0){
                    steps = steps * -1;
                }
                #if debug == 1
                *d->debug_steps[i] += (uint16_t)steps ;
                #endif
                sign = 0;
                if (d->prev_pos[i] < d->curr_pos[i]){
                    sign = 1;
                }
                d->prev_pos[i] = d->curr_pos[i];
                if (steps > 0){
                    cmd[i] = timing[steps] | (sign << 31);
                }
                else{
                    cmd[i] = 0;
                }
            }
            else{
                // velocity mode
                // Sebesség mód
                float velocity = *d->command[i]; // Sebesség RPS-ben
                float steps_per_sec = velocity * *d->scale[i]; // Lépések/másodperc
                uint8_t sign = (velocity >= 0) ? 1 : 0; // Irány meghatározása
                steps_per_sec = fabs(steps_per_sec); // Abszolút érték a frekvenciához

                if (steps_per_sec > max_f) {
                    steps_per_sec = max_f; // Korlátozás a periodusidobol származó maximális frekvenciára
                    // rtapi_print_msg(RTAPI_MSG_WARN, module_name ".%d: Velocity capped at 255 kHz\n", d->index);
                }

                // Számoljuk ki a lépések számát 1 ms alatt (1 kHz szervo ciklus)
                uint32_t steps_per_cycle = (uint32_t)(steps_per_sec * (period / 1000000000.0)); // Lépések/ciklus
                if (steps_per_cycle > 511) {
                    steps_per_cycle = 511;
                }

                #if debug == 1
                *d->debug_steps[i] += (uint16_t)steps_per_cycle;
                #endif

                // PIO parancs: a timing táblázatból vesszük a megfelelő értéket
                if (steps_per_cycle > 0) {
                    cmd[i] = timing[steps_per_cycle] | (sign << 31) ;
                } else {
                    cmd[i] = 0; // Ha a sebesség 0, nincs lépés
                }
            }
            *d->feedback[i] = *d->command[i];
        }

        d->first_data = false;

        for (uint8_t i = 0; i < stepgens; i++) {
            // Copy the command to the tx_buffer
            tx_buffer->stepgen_command[i] = cmd[i];
        }

        tx_buffer->pio_timing = nearest(*d->pulse_width);

    #if use_outputs == 1
    uint32_t outs=0;
    for (uint8_t i = 0; i < sizeof(output_pins); i++) {
        // Copy the encoder values to the tx_buffer
        outs |= 1 << *d->output[i];
    }
    tx_buffer->outputs = outs; // Clear outputs
    #endif

    #if use_pwm == 1
    // Set the PWM output
   if (*d->pwm_enable) {
        if (*d->pwm_frequency > 0) {
            if (*d->pwm_frequency > 1000000) {
                *d->pwm_frequency = 1000000; // Cap the frequency to 1MHz
            }
            if (*d->pwm_frequency < 1907) {
                *d->pwm_frequency = 1907;
            }
            if (*d->pwm_output < *d->pwm_min_limit) {
                *d->pwm_output = *d->pwm_min_limit; // Ensure PWM output is above the minimum limit
            }
            uint16_t wrap = pwm_calculate_wrap(*d->pwm_frequency);
            // scale the duty cycle to the wrap value
            uint16_t duty_cycle = (uint16_t)(round(((float)*d->pwm_output / *d->pwm_maxscale) * wrap));
            tx_buffer->pwm_frequency = *d->pwm_frequency; // Set the PWM frequency
            tx_buffer->pwm_duty = duty_cycle;
        } else {
            tx_buffer->pwm_duty = 0; // Disable PWM output
        }
    }
    #endif

    tx_buffer->packet_id = tx_counter;
    tx_buffer->checksum = calculate_checksum(tx_buffer, tx_size - 1); // Calculate checksum excluding the checksum byte itself
    sendto(d->sockfd, tx_buffer, tx_size, MSG_DONTROUTE | MSG_DONTWAIT, &d->remote_addr, sizeof(d->remote_addr));
    tx_counter++;
    //rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: sent data to %s:%d\n", d->index, d->ip_address->ip, d->ip_address->port);

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

/*
 * parse_ip_port - Parses a string containing IP:port pairs separated by semicolons.
 *
 * @input: A null-terminated string containing IP:port pairs (e.g., "192.168.1.1:8080;10.0.0.1:80").
 * @output: An array of IpPort structures to store the parsed IP addresses and ports.
 * @max_count: The maximum number of entries that can be stored in the output array.
 *
 * Returns:
 *   - The number of valid IP:port pairs successfully parsed and stored in output.
 *   - -1 if input is NULL, output is NULL, max_count <= 0, or memory allocation fails.
 *
 * Notes:
 *   - Each IP:port pair must be in the format "IP:port" (e.g., "192.168.1.1:8080").
 *   - Invalid entries (missing colon, invalid port, etc.) are logged and skipped.
 *   - The function ensures the IP string is null-terminated and port is within valid range (0-65535).
 *   - The input string is duplicated to avoid modifying the original string.
 */
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
            continue; // Skip invalid port
        }

        // Copy IP into the struct, ensuring it is null-terminated
        snprintf(output[count].ip, sizeof(output[count].ip), "%s", ip);
        output[count].port = (int)port;

        count++;
    }

    free(input_copy);
    return count; // Return the number of valid entries parsed
}

int rtapi_app_main(void) {
    int r;

    rtapi_set_msg_level(RTAPI_MSG_INFO);

    module_init();

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

    char name[48] = {0};

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
        hal_data[j].ip_address = &results[j];
        hal_data[j].first_data = true;
        hal_data[j].error_triggered = false;

        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: init_socket\n", j);
        init_socket(&hal_data[j]);
        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: init_socket ready..\n", j);

        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), module_name ".%d.connected", j);

        r = hal_pin_bit_newf(HAL_IN, &hal_data[j].connected, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }

        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), module_name ".%d.stepgen.pulse-width", j);
        r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pulse_width, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }

        #if debug == 1
        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), module_name ".%d.stepgen.max-freq-khz", j);
        r = hal_pin_float_newf(HAL_OUT, &hal_data[j].debug_freq, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }

        #endif

        for (int i = 0; i<sizeof(input_pins); i++){
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.input.gp%d", j, input_pins[i]);
            r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].input[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
        }

        #if use_outputs == 1
        for (int i = 0; i<sizeof(output_pins); i++){
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.output.gp%d", j, output_pins[i]);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].output[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
        }
        #endif

        #if use_pwm == 1
        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), module_name ".%d.pwm.enable", j);
        r = hal_pin_bit_newf(HAL_IN, &hal_data[j].pwm_enable, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }
        *hal_data[j].pwm_enable = 0;
        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), module_name ".%d.pwm.duty", j);
        r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_output, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }
        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), module_name ".%d.pwm.frequency", j);
        r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_frequency, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }
        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), module_name ".%d.pwm.min-limit", j);
        r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_min_limit, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }
        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), module_name ".%d.pwm.max-scale", j);
        r = hal_pin_u32_newf(HAL_IN, &hal_data[j].pwm_maxscale, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }
        *hal_data[j].pwm_maxscale = 4096.0; // default max scale
        #endif

        for (int i = 0; i<stepgens; i++)
        {

            #if debug == 1
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.stepgen.%d.debug-steps", j, i);
            r = hal_pin_u32_newf(HAL_OUT, &hal_data[j].debug_steps[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            *hal_data[j].debug_steps[i] = 0;
            #endif

            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.stepgen.%d.command", j, i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].command[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.stepgen.%d.step-scale", j, i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].scale[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.stepgen.%d.feedback", j, i);
            r = hal_pin_float_newf(HAL_OUT, &hal_data[j].feedback[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.stepgen.%d.mode", j, i);
            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].mode[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            *hal_data[j].mode[i] = 0;
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.stepgen.%d.enable", j, i);
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

            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.encoder.%d.raw-count", j, i);
            r = hal_pin_s32_newf(HAL_IN, &hal_data[j].raw_count[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.encoder.%d.scaled-count", j, i);
            r = hal_pin_s32_newf(HAL_OUT, &hal_data[j].scaled_count[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.encoder.%d.scale", j, i);
            r = hal_pin_float_newf(HAL_IN, &hal_data[j].enc_scale[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
            *hal_data[j].scale[i] = 1;
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), module_name ".%d.encoder.%d.scaled-value", j, i);
            r = hal_pin_float_newf(HAL_OUT, &hal_data[j].enc_value[i], comp_id, name, j);
            if (r < 0) {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
                hal_exit(comp_id);
                return r;
            }
        }

        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), module_name ".%d.period", j);

        r = hal_pin_u32_newf(HAL_IN, &hal_data[j].period, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ".%d: ERROR: pin connected export failed with err=%i\n", j, r);
            hal_exit(comp_id);
            return r;
        }

        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), module_name ".%d.io-ready-in", j);

            r = hal_pin_bit_newf(HAL_IN, &hal_data[j].io_ready_in, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": ERROR: pin connected export failed with err=%i\n", r);
            hal_exit(comp_id);
            return r;
        }

        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), module_name ".%d.io-ready-out", j);

        r = hal_pin_bit_newf(HAL_OUT, &hal_data[j].io_ready_out, comp_id, name, j);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": ERROR: pin connected export failed with err=%i\n", r);
            hal_exit(comp_id);
            return r;
        }
        
        char watchdog_name[48] = {0};
        snprintf(watchdog_name, sizeof(watchdog_name),module_name ".%d.watchdog-process", j);
        r = hal_export_funct(watchdog_name, watchdog_process, &hal_data[j], 1, 1, comp_id);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_export_funct failed for watchdog-process: %d\n", r);
            hal_exit(comp_id);
            return r;
        }
        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: hal_export_funct for watchdog-process: %d\n", j, r);

        char process_send[48] = {0};
        snprintf(process_send, sizeof(process_send), module_name ".%d.process-send", j);
        r = hal_export_funct(process_send, udp_io_process_send, &hal_data[j], 1, 1, comp_id);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_export_funct failed: %d\n", r);
            hal_exit(comp_id);
            return r;
        }
        rtapi_print_msg(RTAPI_MSG_INFO, module_name ".%d: hal_export_funct for process_send: %d\n", j, r);

        char process_recv[48] = {0};
        snprintf(process_recv, sizeof(process_recv), module_name ".%d.process-recv", j);
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
        close(hal_data[i].sockfd);
    }
    hal_exit(comp_id);
}
