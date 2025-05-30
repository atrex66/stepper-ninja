#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "lwipopts.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/tcp.h"
#include "webpage.h"

#if ENCODER_COUNTER == 0
// PIO program for step/dir counter
#include "step_counter.pio.h"
#else
#include "encoder_counter.pio.h"
#endif

char ssid[] = WIFI_SSID; // Your WiFi SSID
char pass[] = WIFI_PASSWORD; // Your WiFi password

// Web server port
#define TCP_PORT 80

#define stepcounters 4 // Number of step/dir counters

// Base GPIO pins for step/dir counters
const uint stepcounter_base[8] = {0, 2, 4, 6, 8, 10, 12, 14};

// Global counter variable
volatile int32_t counter_value[stepcounters] = {0,};

// Initialize the PIO step/dir counter (same as before)
void init_pio_counter() {

    for (int i = 0; i < stepcounters; i++) {
            gpio_init(stepcounter_base[i]);
            gpio_init(stepcounter_base[i]+1);
            gpio_set_dir(stepcounter_base[i], GPIO_IN);
            gpio_set_dir(stepcounter_base[i] + 1, GPIO_IN);
            gpio_pull_up(stepcounter_base[i]); // Pull-up ellenállás, ha szükséges
            gpio_pull_up(stepcounter_base[i]+1); // Pull-up ellenállás, ha szükséges
        }
        #if ENCODER_COUNTER == 0
        pio_add_program(pio0, &step_counter_program);
        pio_add_program(pio1, &step_counter_program);
        #else
        pio_add_program(pio0, &encoder_counter_program);
        pio_add_program(pio1, &encoder_counter_program);
        #endif
        for (int i = 0; i < stepcounters; i++) {
            PIO pio = i < 4 ? pio0 : pio1;
            int sm = i < 4 ? i : i - 4; // Use 4 state machines per PIO
            #if ENCODER_COUNTER == 0
            step_counter_program_init(pio, sm, stepcounter_base[i], 0);
            printf("stepcounter %d init done...\n", i);
             #else
            encoder_counter_program_init(pio, sm, stepcounter_base[i], 0);
            printf("encoder counter %d init done...\n", i);
            #endif
            // pio_sm_set_enabled(pio, sm, true);
        }
}


void resetCounter(int index) {
    printf("Resetting counter %d\n", index);
    if (index < 0 || index >= stepcounters) return;
    PIO pio = index < 4 ? pio0 : pio1;
    int sm = index < 4 ? index : index - 4; // Use 4 state machines per PIO
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_restart(pio, sm);
    pio_sm_set_enabled(pio, sm, true);
    pio_sm_exec(pio, sm, pio_encode_set(pio_y, 0));
}


 err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    // printf("Received HTTP request\n");
    if (p == NULL) {
    //    printf("Connection closed by client\n");
        tcp_close(tpcb);
        return ERR_OK;
    }
    // printf("Request payload: %s\n", (char*)p->payload);

    for (int i = 0; i < stepcounters; i++) {
        // Read the current counter value from PIO
        PIO pio = i < 4 ? pio0 : pio1;
        int sm = i < 4 ? i : i - 4; // Use 4 state machines per PIO
        #if ENCODER_COUNTER == 0
        counter_value[i] = -step_counter_get_count(pio, sm);
        #else
        counter_value[i] = -encoder_counter_get_count(pio, sm);
        #endif
    }

    // Handle GET request for JSON data
    if (strstr((char*)p->payload, "GET /counter.json") != NULL) {
        tcp_recved(tpcb, p->tot_len);
        
        char response[256];
        int len = snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Connection: close\r\n\r\n"
            "{\"counters\":[%d,%d,%d,%d]}",
            counter_value[0], counter_value[1], counter_value[2], counter_value[3]);

        tcp_write(tpcb, response, len, TCP_WRITE_FLAG_COPY);
        tcp_close(tpcb);
        pbuf_free(p);
        return ERR_OK;
    }

    // Handle POST request to reset a single counter
    if (strstr((char*)p->payload, "POST /reset") != NULL) {
        tcp_recved(tpcb, p->tot_len);
        
        // Parse JSON payload to get counter index
        char *json_start = strstr((char*)p->payload, "{\"counter\":");
        if (json_start) {
            int index;
            if (sscanf(json_start, "{\"counter\":%d}", &index) == 1 && index >= 0 && index < stepcounters) {
                resetCounter(index);
            }
        }

        char response[] = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{}";
        tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
        tcp_close(tpcb);
        pbuf_free(p);
        return ERR_OK;
    }

    // Handle POST request to reset all counters
    if (strstr((char*)p->payload, "POST /reset_all") != NULL) {
        tcp_recved(tpcb, p->tot_len);
        
        // Reset all counters
        for (int i = 0; i < stepcounters; i++) {
            resetCounter(i);
        }

        char response[] = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{}";
        tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
        tcp_close(tpcb);
        pbuf_free(p);
        return ERR_OK;
    }

    // Serve the HTML page for other GET requests
    tcp_recved(tpcb, p->tot_len);
    
    tcp_write(tpcb, html_response, strlen(html_response), TCP_WRITE_FLAG_COPY);
    tcp_close(tpcb);
    pbuf_free(p);
    return ERR_OK;
}


// TCP accept callback - renamed to avoid conflict
err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

// Initialize the web server
void init_web_server() {
    //cyw43_arch_lwip_begin();
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, tcp_accept_callback);
    //cyw43_arch_lwip_end();
}

// Main function
int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("\n");
    #if ENCODER_COUNTER == 0
    printf("Pico-W Step/Dir Counter Web Server\n");
    #else
    printf("Pico-W Encoder Counter Web Server\n");
    #endif

    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Connecting to WiFi SSID: %s\n", ssid);

    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("failed to connect\n");
        return 1;
    }

    printf("connected\n");
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    init_pio_counter();
    init_web_server();
    
    printf("\nReady, running web server at %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
    printf("Web server initialized on port %d\n", TCP_PORT);
    
    // Main loop
    while (true) {
        cyw43_arch_poll();
        sleep_ms(100);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN));
    }
    
    cyw43_arch_deinit();
    return 0;
}