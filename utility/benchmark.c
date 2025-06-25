#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>

#include "transmission.h"
#include "transmission.c"

#define NUM_CYCLES 100000
#define TIMEOUT_SEC 2  // Increased timeout

static transmission_pc_pico_t *tx_buffer;
static transmission_pico_pc_t *rx_buffer;

#if raspberry_pi_spi == 0
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include "jump_table.h"
    #include <arpa/inet.h>

    static int sockfd;
    static struct sockaddr_in local_addr, remote_addr;
    static uint8_t jump_table_index = 1;
    char *ip_addr = "192.168.0.177";
    uint16_t port = 8888;
#else
    #include "bcm2835.h"
    #include "bcm2835.c"
    #define SPI_SPEED BCM2835_SPI_CLOCK_DIVIDER_128
    const uint8_t rpi_inputs[] = raspi_inputs;
    const uint8_t rpi_outputs[] = raspi_outputs;
    const uint8_t rpi_input_pullup[] = raspi_input_pullups;
    const uint8_t rpi_inputs_no = sizeof(rpi_inputs);
    const uint8_t rpi_outputs_no = sizeof(rpi_outputs);
#endif 
uint8_t counter = 0;

long long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000LL + tv.tv_usec / 1000LL;
}

static void init_socket(void) {
    #if raspberry_pi_spi == 0
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(port);
        local_addr.sin_addr.s_addr = INADDR_ANY;
        bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr));

        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        remote_addr.sin_family = AF_INET;
        remote_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip_addr, &remote_addr.sin_addr);
    #else
        if (!bcm2835_init()) {
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
    #endif
}

int main() {
    long long start_time, end_time, elapsed_time;
    int cycles_completed = 0;
    long long total_latency = 0;
    size_t ssize = sizeof(transmission_pc_pico_t);
    char *readbuff = malloc(ssize);
    uint8_t tx_size = sizeof(transmission_pc_pico_t);
    uint8_t rx_size = sizeof(transmission_pico_pc_t);

    rx_buffer = (transmission_pico_pc_t *)malloc(rx_size);
    if (rx_buffer == NULL) {
        printf("rx_buffer allocation failed\n");
        return -1;
    }
    tx_buffer = (transmission_pc_pico_t *)malloc(tx_size);
    if (tx_buffer == NULL) {
        printf("tx_buffer allocation failed\n");
        return -1;
    }

    #if raspberry_pi_spi == 0
        // Create UDP socket
        if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }
    #endif

    init_socket();

    #if raspberry_pi_spi == 0
        printf("UDP socket created (%s:%d), connected to target.\n", ip_addr, port);
    #endif

    // Benchmark loop
    start_time = get_time_ms();
    printf("Benchmark started. Running %d cycles...\n", NUM_CYCLES);
    long long cycle_start = get_time_ms();

    for (int i = 0; i < NUM_CYCLES; i++) {
        long long cycle_start = get_time_ms();
        memset(tx_buffer, 0, tx_size);
        // Send packet
        tx_buffer->packet_id = counter++;
        tx_buffer->checksum = calculate_checksum(tx_buffer, tx_size - 1); // Exclude checksum byte itself

        #if raspberry_pi_spi == 0
            if (sendto(sockfd, tx_buffer, tx_size, 0, (struct sockaddr*)&remote_addr, sizeof(remote_addr)) < 0) {
                perror("Send failed");
                break;
            }
        #else
            // working full duplex
            bcm2835_gpio_clr(raspi_int_out);
            memset(readbuff, 0, ssize);
            bcm2835_spi_transfernb((char *)tx_buffer, readbuff, ssize);
            memcpy(rx_buffer, readbuff , sizeof(transmission_pico_pc_t));
            bcm2835_gpio_set(raspi_int_out);
        #endif
        // counter++;

        #if raspberry_pi_spi == 0
            // Receive response
            socklen_t addr_len = sizeof(ip_addr);
            int recv_len = -1;
            while (recv_len < 0) {recv_len = recvfrom(sockfd, rx_buffer, rx_size, 0, NULL, NULL);}
        #endif
        long long cycle_end = get_time_ms();
        total_latency += (cycle_end - cycle_start);
        cycles_completed++;
    }

    // Results
    end_time = get_time_ms();
    elapsed_time = end_time - start_time;

    printf("\nBenchmark results:\n");
    printf("Cycles attempted: %d\n", NUM_CYCLES);
    printf("Cycles completed: %d\n", cycles_completed);
    printf("Total time: %lld ms\n", elapsed_time);
    printf("Average cycle time (write + read): %.4f ms\n", (double)total_latency / cycles_completed);
    printf("Throughput: %d cycles/s\n", (uint32_t)(cycles_completed / (elapsed_time / 1000.0)));

    #if raspberry_pi_spi == 0
        close(sockfd);
        printf("Socket closed. Exiting...\n");
    #endif
    free(readbuff);
    free(rx_buffer);
    free(tx_buffer);
    return 0;
}
