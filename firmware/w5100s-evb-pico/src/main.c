#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/regs/pll.h"
#include "hardware/structs/pll.h"
#include "hardware/pio.h"
#include "serial_terminal.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "jump_table.h"
#include "transmission.h"
#include "main.h"
#include "config.h"
#include "pio_settings.h"
#include "freq_generator.pio.h"
#include "quadrature_encoder.pio.h"


// Author:Viola Zsolt (atrex66@gmail.com)
// Date: 2025
// Description: udp-warrior driver for W5100S-EVB-PICO
// License: MIT
// Description: This code is a driver for the W5100S-EVB-PICO board.
// It also includes a serial terminal interface for configuration and debugging.
// The code is designed to run on the Raspberry Pi Pico and uses the Pico SDK for hardware access.
// The code is structured to run on two cores, with core 0 handling network communication and core 1 handling quadrature encoders (theoretical 12.5Mhz count).
// The code is using DMA for SPI (burst) communication with the W5100S chip, which allows for high-speed data transfer.
// The code uses the Wiznet W5100S library for network communication.
// The code includes functions for initializing the hardware, handling network communication, and processing commands from the serial terminal.
// The code is designed to be modular and extensible, allowing for easy addition of new features and functionality.
// The code is also designed to be efficient and responsive, with low latency and high throughput.
// Note: checksum algorithm is used to ensure data integrity, and the code includes error handling for network communication. (timeout + jumpcode checksum)
// Note: The code is disables the terminal when the HAL driver connect to the W5100S-EVB-PICO board, and enables when not running.

// -------------------------------------------
// Network Configuration
// -------------------------------------------
extern wiz_NetInfo default_net_info;
extern uint16_t port;
extern configuration_t *flash_config;
wiz_NetInfo net_info;

transmission_pc_pico_t *rx_buffer;
transmission_pico_pc_t *tx_buffer;

uint8_t first_send = 1;
volatile bool first_data = true;

#ifdef USE_SPI_DMA
static uint dma_tx;
static uint dma_rx;
static dma_channel_config dma_channel_config_tx;
static dma_channel_config dma_channel_config_rx;
#endif

uint32_t step_pin[stepgens] = {0, 2, 4, 6};
uint8_t encoder_base[encoders] = {8, 10, 12, 14};

int32_t encoder[encoders] = {0,};

int32_t *command;
uint8_t *src_ip;
uint16_t src_port;
uint32_t rx_counter=0;

uint8_t rx_size = sizeof(transmission_pc_pico_t);
uint8_t tx_size = sizeof(transmission_pico_pc_t);

uint8_t sety = 0;
uint8_t nop = 0;
uint8_t old_sety = 0;
uint8_t old_nop = 0;

uint8_t checksum_error = 0;
uint8_t timeout_error = 1;
uint32_t last_time = 0;
static absolute_time_t last_packet_time;
uint32_t TIMEOUT_US = 100000;
uint32_t time_diff;
uint8_t checksum_index = 1;
uint8_t checksum_index_in = 1;

bool __time_critical_func(stepgen_update_handler)() {
    uint32_t irq = save_and_disable_interrupts();
    static int32_t cmd[stepgens] = {0, };
    static PIO pio;
    int zeros = 0;
    int j=0;

    if (checksum_error){
        return false;
    }
    // get command from rx_buffer and set direction signals
    for (int i = 0; i < stepgens; i++) {
        pio = i < 4 ? pio0 : pio1;
        j = i < 4 ? i : i - 4;
        command[i] = rx_buffer->stepgen_command[i];
        // ha van sebesseg parancs
        if (command[i] != 0){
            if ((command[i] >> 31) & 1) {
                gpio_put(step_pin[i] + 1, 1);
            }
            else {
                gpio_put(step_pin[i] + 1, 0);
            }
        }
        else {
            zeros++;
        }
    }

    sety = pio_settings[rx_buffer->pio_timing].sety & 31;
    nop = pio_settings[rx_buffer->pio_timing].nop & 31;

    if (old_sety != sety || old_nop != nop) {
        for (int i=0; i<stepgens; i++){
            // jump to the pull block
            pio_sm_exec(pio0, i, pio_encode_jmp(1));
        }
        pio0->instr_mem[4] = pio_encode_set(pio_y, sety);
        pio0->instr_mem[5] = pio_encode_nop() | pio_encode_delay(nop);
        printf("New pulse width set: %d\n", pio_settings[rx_buffer->pio_timing].high_cycles * 8);
        old_sety = sety;
        old_nop = nop;
    }

    // pio0->ctrl = 0;
    // put non zero commands to PIO fifo
    for (int i = 0; i < stepgens; i++) {
        if (command[i] != 0){
            pio_sm_put_blocking(pio0, i, command[i] & 0x7fffffff);
            }
    }
    // pio0->ctrl = 1 << 0 | 1 << 1 | 1 << 2 | 1 << 3;

    restore_interrupts(irq);
    return true;
}

// -------------------------------------------
// Core 1 Entry Point
// -------------------------------------------
void core1_entry() {

    memset(src_ip, 0, 4);
    sleep_ms(500);
    //init_timer_interrupt();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    while(1){

        gpio_put(LED_PIN, !timeout_error);
        if (time_diff > TIMEOUT_US) {
            if (timeout_error == 0){
                printf("Timeout error.\n");
                // reset encoder PIO-s when timeout
                for (int i = 0; i < encoders; i++) {
                    pio_sm_set_enabled(pio1, i, false);
                    pio_sm_restart(pio1, i);
                    pio_sm_exec(pio1, i, pio_encode_set(pio_y, 0));
                    pio_sm_set_enabled(pio1, i, true);
                    printf("Encoder %d reset\n", i);
                }
            }
            timeout_error = 1;
            checksum_index = 1;
            checksum_index_in = 1;
            checksum_error = 0;
            first_data = true;
            first_send = 1;
            //reset_with_watchdog();
        }
        else {
            timeout_error = 0;
        }

        handle_serial_input();
    }
}

// -------------------------------------------
// (Core 0) UDP communication (DMA, SPI)
// -------------------------------------------
int main() {

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

    stdio_init_all();
    stdio_usb_init();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    sleep_ms(2000);

    printf("\033[2J");
    printf("\033[H");
    
    printf("\n\n--- stepper-ninja V1.0 ---\n");
    printf("Viola Zsolt 2025\n");
    printf("E-mail:atrex66@gmail.com\n");
    printf("\n");

    printf("rx_buffer size: %d\n", rx_size);
    printf("tx_buffer size: %d\n", tx_size);

    // igy jon ki pontosra a 1mS PIO
    set_sys_clock_khz(125000, true);
    
    clock_configure(clk_peri,
                    0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                    clock_get_hz(clk_sys),
                    clock_get_hz(clk_sys));

    spi_init(spi0, 40000000);
    hw_write_masked(&spi_get_hw(spi0)->cr0, (0) << SPI_SSPCR0_SCR_LSB, SPI_SSPCR0_SCR_BITS); // SCR = 0
    hw_write_masked(&spi_get_hw(spi0)->cpsr, 4, SPI_SSPCPSR_CPSDVSR_BITS); // CPSDVSR = 4
    
    w5100s_init();
    w5100s_interrupt_init();

    load_configuration();
    network_init();
    multicore_launch_core1(core1_entry);

    PIO pio = pio0;
    int p = 0;

    gpio_init(15);
    gpio_set_dir(15, GPIO_OUT);

    uint32_t offset[2] = {0, };

    offset[0] = pio_add_program_at_offset(pio0, &freq_generator_program, 0);
    //offset[1] = pio_add_program_at_offset(pio1, &freq_generator_program, 0);
    
    uint32_t sm = 0;
    uint8_t o = 0;
    for (uint32_t i = 0; i < stepgens; i++){
        pio = i < 4 ? pio0 : pio1;
        sm = i < 4 ? i : i - 4;
        p = i < 4 ? 0 : 1;
        // step pin
        pio_gpio_init(pio, step_pin[i]);
        // dir pin
        gpio_init(step_pin[i]+1);
        gpio_set_dir(step_pin[i]+1, GPIO_OUT);
        pio_sm_set_consecutive_pindirs(pio, sm, step_pin[i], 1, true);
        pio_sm_config c = freq_generator_program_get_default_config(offset[o]);
        // sm_config_set_clkdiv(&c, clock_get_hz(5) / 10000000);
        sm_config_set_set_pins(&c, step_pin[i], 1);
        pio_sm_init(pio, sm, offset[o], &c);
        // pio_sm_put_blocking(pio, sm, (uint32_t)(10000 << 8) + 5);
        pio_sm_set_enabled(pio, sm, true);
        printf("stepgen%d init done...\n", i);
        //printf("Pio tx_fifo_is %d\n", pio_sm_is_tx_fifo_empty(pio, sm));
    }

    // encoder init
    for (int i = 0; i < encoders; i++) {
        gpio_init(encoder_base[i]);
        gpio_init(encoder_base[i]+1);
        gpio_set_dir(encoder_base[i], GPIO_IN);
        gpio_set_dir(encoder_base[i]+1, GPIO_IN);
        gpio_pull_up(encoder_base[i]); // Pull-up ellenállás, ha szükséges
        gpio_pull_up(encoder_base[i]+1); // Pull-up ellenállás, ha szükséges
    }

    pio_add_program(pio1, &quadrature_encoder_program);
    for (int i = 0; i < encoders; i++) {
        quadrature_encoder_program_init(pio1, i, encoder_base[i], 0);
        printf("encoder %d init done...\n", i);
    }

    printf("Pio init done....\n");

    handle_udp();
}

void reset_with_watchdog() {
    watchdog_enable(1, 1);
    while(1);
}

void __time_critical_func(cs_select)() {
    gpio_put(PIN_CS, 0);
    asm volatile("nop \n nop \n nop");
}

void __time_critical_func(cs_deselect)() {
    gpio_put(PIN_CS, 1);
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

void __not_in_flash_func(core0_wait)(void) {
    while (!multicore_fifo_wready()) {
        tight_loop_contents();
    }
    multicore_fifo_push_blocking(0xFEEDFACE);
    printf("Core0 is ready to write...\n");
    uint32_t signal = multicore_fifo_pop_blocking();
    if (signal == 0xDEADBEEF) {
        watchdog_reboot(0, 0, 0);
    }
}

int32_t __time_critical_func(_recvfrom)(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t *port) {
    uint8_t head[8];
    uint16_t pack_len = 0;

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

// -------------------------------------------
// UDP handler
// -------------------------------------------
void __not_in_flash_func(handle_udp)() {
    gpio_pull_up(IRQ_PIN);

    setIMR(0x01);
    #if _WIZCHIP_ == W5100S
        setIMR2(0x00);
    #endif

    last_packet_time = get_absolute_time();
    time_diff = 0xFFFFFFFF;
    while (1){
    // todo: W5100S interrupt setup is different than W5500 so to work with W5500 and int's need to implement new interrupt inicialization
        #if _WIZCHIP_ == W5100S
            while(gpio_get(IRQ_PIN) == 1)
            {
                time_diff = (uint32_t)absolute_time_diff_us(last_packet_time, get_absolute_time());
                if (multicore_fifo_rvalid()) {
                    break;
                }
            }
        #else
        #warning "W5500 interrupt setup is not implemented, polling mode"
                        time_diff = (uint32_t)absolute_time_diff_us(last_packet_time, get_absolute_time());
        #endif
        setSn_IR(0, Sn_IR_RECV);  // clear interrupt
        if(getSn_RX_RSR(0) != 0) {
            int len = _recvfrom(0, (uint8_t *)rx_buffer, rx_size, src_ip, &src_port);
            if (len == rx_size) {
                rx_counter ++;
                //printf("%d Received bytes: %d\n", rx_counter, len);
                last_packet_time = get_absolute_time();
                //jump_table_checksum();
                // update stepgens
                stepgen_update_handler();
                // update encoders
                for (int i = 0; i < encoders; i++) {
                    encoder[i] = quadrature_encoder_get_count(pio1, i);
                    tx_buffer->encoder_counter[i] = encoder[i];
                }
                //jump_table_checksum_in();
                _sendto(0, (uint8_t *)tx_buffer, tx_size, src_ip, src_port);
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

void w5100s_interrupt_init() {
    gpio_init(INT_PIN);
    gpio_set_dir(INT_PIN, GPIO_IN);
    gpio_pull_up(INT_PIN);
    
    uint8_t imr = IMR_RECV;        
    uint8_t sn_imr = Sn_IMR_RECV;  
    
    setIMR(imr);
    setSn_IMR(SOCKET_DHCP, sn_imr);
}

// -------------------------------------------
// W5100S Init
// -------------------------------------------
void w5100s_init() {

    // GPIO init
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    cs_deselect();

    gpio_init(PIN_RESET);
    gpio_set_dir(PIN_RESET, GPIO_OUT);
    gpio_put(PIN_RESET, 1);

    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
   
    reg_wizchip_cs_cbfunc(cs_select, cs_deselect);
    reg_wizchip_spi_cbfunc(spi_read, spi_write);
    
    #if USE_SPI_DMA
        reg_wizchip_spiburst_cbfunc(wizchip_read_burst, wizchip_write_burst);
    #endif

    gpio_put(PIN_RESET, 0);
    sleep_ms(100);
    gpio_put(PIN_RESET, 1);
    sleep_ms(500);

#ifdef USE_SPI_DMA
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
#endif

}

// -------------------------------------------
// Network Init
// -------------------------------------------
void network_init() {
    wiz_PhyConf phyconf;

    uint8_t tx[_WIZCHIP_SOCK_NUM_] = {0,};
    uint8_t rx[_WIZCHIP_SOCK_NUM_] = {0,};
    memset(tx, 2, _WIZCHIP_SOCK_NUM_);
    memset(rx, 2, _WIZCHIP_SOCK_NUM_);
    wizchip_init(tx, rx);

    // wizchip_init(0, 0);
    wizchip_setnetinfo(&net_info);

    setSn_CR(0, Sn_CR_CLOSE);
    setSn_CR(0, Sn_CR_OPEN);
    uint8_t sock_num = 0;
    socket(sock_num, Sn_MR_UDP, port, 0);

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
    printf("PHY Duplex: %s\n", phyconf.duplex == PHY_DUPLEX_FULL ? "Full" : "Half");
    printf("PHY Speed: %s\n", phyconf.speed == PHY_SPEED_100 ? "100Mbps" : "10Mbps");
    printf("*******************************************\n");
    }

#ifdef USE_SPI_DMA
static void wizchip_read_burst(uint8_t *pBuf, uint16_t len)
{
    uint8_t dummy_data = 0xFF;

    channel_config_set_read_increment(&dma_channel_config_tx, false);
    channel_config_set_write_increment(&dma_channel_config_tx, false);
    dma_channel_configure(dma_tx, &dma_channel_config_tx,
                          &spi_get_hw(SPI_PORT)->dr,
                          &dummy_data,              
                          len,                      
                          false);                   

    channel_config_set_read_increment(&dma_channel_config_rx, false);
    channel_config_set_write_increment(&dma_channel_config_rx, true);
    dma_channel_configure(dma_rx, &dma_channel_config_rx,
                          pBuf,                     
                          &spi_get_hw(SPI_PORT)->dr,
                          len,                      
                          false);                   

    dma_start_channel_mask((1u << dma_tx) | (1u << dma_rx));
    dma_channel_wait_for_finish_blocking(dma_rx);
}

static void wizchip_write_burst(uint8_t *pBuf, uint16_t len)
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
#endif
