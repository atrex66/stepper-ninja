#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#include "transmission.h"
#include "transmission.c"

#define NUM_CYCLES 100000
#define TIMEOUT_MS 2000

#if raspberry_pi_spi == 0
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#else
#include "bcm2835.h"
#include "bcm2835.c"
#define SPI_SPEED BCM2835_SPI_CLOCK_DIVIDER_128
#endif

static long long get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void usage(const char *program) {
#if raspberry_pi_spi == 0
    printf("Usage: %s IP [PORT [CYCLES]]\n"
           "  IP: target IPv4 address (required)\n"
           "  PORT: target and local UDP port (default: %d)\n"
           "  CYCLES: number of exchanges (default: %d)\n"
           "  Response timeout: %d ms\n"
           "Example: %s 192.168.0.177 8888 10000\n",
           program, DEFAULT_PORT, NUM_CYCLES, TIMEOUT_MS, program);
#else
    printf("Usage: %s [CYCLES]\nDefault cycles: %d\n", program, NUM_CYCLES);
#endif
}

static int parse_positive(const char *text, long maximum, int *value) {
    char *end;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno || end == text || *end || parsed < 1 || parsed > maximum) {
        fprintf(stderr, "Invalid number: %s (expected 1..%ld)\n", text, maximum);
        return -1;
    }
    *value = (int)parsed;
    return 0;
}

int main(int argc, char **argv) {
    int cycles = NUM_CYCLES;
    int cycles_completed = 0;
    int failed = 0;
    long long total_latency = 0;
    transmission_pc_pico_t tx_buffer;
    transmission_pico_pc_t rx_buffer;

    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }

#if raspberry_pi_spi == 0
    int port = DEFAULT_PORT;
    struct sockaddr_in local_addr = {0}, remote_addr = {0};
    if (argc < 2 || argc > 4) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if ((argc >= 3 && parse_positive(argv[2], 65535, &port)) ||
        (argc == 4 && parse_positive(argv[3], INT_MAX, &cycles))) {
        return EXIT_FAILURE;
    }
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, argv[1], &remote_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("UDP bind failed (is LinuxCNC or another benchmark using this port?)");
        close(sockfd);
        return EXIT_FAILURE;
    }
    /* Connect UDP to accept responses only from the selected target. */
    if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        perror("UDP connect failed");
        close(sockfd);
        return EXIT_FAILURE;
    }
    printf("UDP target: %s:%d; local port: %d\n", argv[1], port, port);
#else
    if (argc > 2 || (argc == 2 && parse_positive(argv[1], INT_MAX, &cycles))) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (!bcm2835_init()) {
        fprintf(stderr, "bcm2835 init failed\n");
        return EXIT_FAILURE;
    }
    if (!bcm2835_spi_begin()) {
        fprintf(stderr, "bcm2835 SPI init failed\n");
        bcm2835_close();
        return EXIT_FAILURE;
    }
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE3);
    bcm2835_spi_setClockDivider(SPI_SPEED);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
    bcm2835_gpio_fsel(RASPI_INT_OUT_GPIO, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_set(RASPI_INT_OUT_GPIO);
#endif

    printf("Benchmark started. Running %d cycles...\n", cycles);
    fflush(stdout);
    long long start_time = get_time_ns();
    for (int i = 0; i < cycles; i++) {
        long long cycle_start = get_time_ns();
        memset(&tx_buffer, 0, sizeof(tx_buffer));
        tx_buffer.protocol_magic = SN_PROTOCOL_MAGIC;
        tx_buffer.packet_id = (uint8_t)i;
        tx_buffer.checksum = calculate_checksum(&tx_buffer, sizeof(tx_buffer) - 1);
#if raspberry_pi_spi == 0
        if (send(sockfd, &tx_buffer, sizeof(tx_buffer), 0) != sizeof(tx_buffer)) {
            perror("Send failed");
            failed = 1;
            break;
        }
        struct pollfd response = {.fd = sockfd, .events = POLLIN};
        int ready;
        do {
            long long remaining = TIMEOUT_MS * 1000000LL - (get_time_ns() - cycle_start);
            if (remaining <= 0) {
                ready = 0;
                break;
            }
            ready = poll(&response, 1, (int)((remaining + 999999) / 1000000));
        } while (ready < 0 && errno == EINTR);
        if (ready <= 0) {
            if (ready == 0) fprintf(stderr, "Response timeout (%d ms), cycle %d\n", TIMEOUT_MS, i + 1);
            else perror("Response wait failed");
            failed = 1;
            break;
        }
        ssize_t received = recv(sockfd, &rx_buffer, sizeof(rx_buffer), MSG_TRUNC | MSG_DONTWAIT);
        if (received < 0) {
            perror("Receive failed");
            failed = 1;
            break;
        }
        if (received != sizeof(rx_buffer)) {
            fprintf(stderr, "Unexpected response size: %zd (expected %zu)\n", received, sizeof(rx_buffer));
            failed = 1;
            break;
        }
#else
        char writebuff[SPI_TRANSFER_SIZE] = {0};
        char readbuff[SPI_TRANSFER_SIZE] = {0};
        memcpy(writebuff, &tx_buffer, sizeof(tx_buffer));
        bcm2835_gpio_clr(RASPI_INT_OUT_GPIO);
        bcm2835_spi_transfernb(writebuff, readbuff, SPI_TRANSFER_SIZE);
        bcm2835_gpio_set(RASPI_INT_OUT_GPIO);
        memcpy(&rx_buffer, readbuff, sizeof(rx_buffer));
#endif
        if (rx_buffer.protocol_magic != SN_PROTOCOL_MAGIC || !tx_checksum_ok(&rx_buffer)) {
            fprintf(stderr, "Invalid response protocol or checksum, cycle %d\n", i + 1);
            failed = 1;
            break;
        }
        total_latency += get_time_ns() - cycle_start;
        cycles_completed++;
    }
    long long elapsed = get_time_ns() - start_time;
    printf("\nBenchmark results:\n");
    printf("Cycles requested: %d\nCycles completed: %d\n", cycles, cycles_completed);
    printf("Total time: %.3f ms\n", elapsed / 1000000.0);
    if (cycles_completed > 0) {
        printf("Average cycle time (write + read): %.4f ms\n", total_latency / (1000000.0 * cycles_completed));
        printf("Throughput: %.2f cycles/s\n", cycles_completed * 1000000000.0 / elapsed);
    }
#if raspberry_pi_spi == 0
    close(sockfd);
#else
    bcm2835_spi_end();
    bcm2835_close();
#endif
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
