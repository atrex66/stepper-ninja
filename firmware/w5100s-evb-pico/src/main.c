#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/multicore.h"
#include "pico/divider.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/regs/pll.h"
#include "hardware/structs/pll.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "serial_terminal.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "jump_table.h"
#include "transmission.h"
#include "flash_config.h"
#include "pio_settings.h"
#include "freq_generator.pio.h"
#include "quadrature_encoder.pio.h"

#if brakeout_board > 0
#include "hardware/i2c.h"
#endif 

#include "main.h"


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
extern const uint8_t input_pins[4];
#if use_outputs == 1
extern const uint8_t output_pins[3];
#endif
extern const uint8_t pwm_pin;
wiz_NetInfo net_info;

transmission_pc_pico_t *rx_buffer;
transmission_pico_pc_t *tx_buffer;

#if breakout_board > 0
    uint32_t input_buffer;
    uint32_t output_buffer;
#endif

uint32_t pwm_freq_buffer;

uint8_t first_send = 1;
volatile bool first_data = true;

#ifdef USE_SPI_DMA
static uint dma_tx;
static uint dma_rx;
static dma_channel_config dma_channel_config_tx;
static dma_channel_config dma_channel_config_rx;
#endif

uint32_t step_pin[stepgens] = {0, 2, 4, 6};
uint8_t encoder_base[4] = {8, 10, 12, 14};

int32_t encoder[encoders] = {0,};

uint8_t buffer_index = 0;

int32_t *command;
uint8_t *src_ip;
uint16_t src_port;
uint8_t rx_counter=0;
uint32_t total_steps[stepgens] = {0,};

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
uint32_t old_pwm_frequency = 0;

void __time_critical_func(stepgen_update_handler)() {
    // uint32_t irq = save_and_disable_interrupts();
    static int32_t cmd[stepgens] = {0, };
    static PIO pio;
    int j=0;

    if (checksum_error){
        return;
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

    // put non zero commands to PIO fifo
    for (int i = 0; i < stepgens; i++) {
        if (command[i] != 0){
            pio_sm_put_blocking(pio0, i, command[i] & 0x7fffffff);
            total_steps[i] += (command[i] & 0x1ff) + 1;
            }
    }
    // restore_interrupts(irq);

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

    #if brakeout_board > 0

        i2c_setup(); // Set I2C frequency to 100kHz
        #ifdef MCP23008_ADDR
        printf("Detecting MCP23008 (Outputs) on %#x address\n", MCP23008_ADDR);
        if (i2c_check_address(i2c1, MCP23008_ADDR)) {
            printf("MCP23008 (Outputs) Init\n");
            mcp_write_register(MCP23008_ADDR, 0x00, 0x00);
        }
        else {
            printf("No MCP23008 (Outputs) found on %#x address.\n", MCP23008_ADDR);
        }
        #endif

        #ifdef MCP23017_ADDR
            printf("Detecting MCP23017 (Inputs) on %#x address\n", MCP23017_ADDR);
            if (i2c_check_address(i2c1, MCP23017_ADDR)) {
                printf("MCP23017 (Inputs) Init\n");
                mcp_write_register(MCP23017_ADDR, 0x00, 0xff);
                mcp_write_register(MCP23017_ADDR, 0x01, 0xff);
            }
            else {
                printf("No MCP23017 (Inputs) found on %#x address.\n", MCP23017_ADDR);
            }
        #endif
    #endif

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
                rx_counter = 0;
                timeout_error = 1;
                checksum_index = 1;
                checksum_index_in = 1;
                checksum_error = 0;
                first_data = true;
                first_send = 1;
                #if use_outputs == 1
                for (int i = 0; i < sizeof(output_pins); i++) {
                    gpio_put(output_pins[i], 0); // reset outputs
                }
                #endif
            }
        }
        else {
            timeout_error = 0;
        }

        #if brakeout_board > 0
            #ifdef MCP23008_ADDR
                if (checksum_error == 0 && timeout_error == 0) {
                    mcp_write_register(MCP23008_ADDR, 0x09, output_buffer & 0xFF); // Set outputs
                }
                else
                {
                    mcp_write_register(MCP23008_ADDR, 0x09, 0x00);
                }
            #endif
            #ifdef MCP23017_ADDR
                input_buffer  = mcp_read_register(MCP23017_ADDR, 0x13) << 8;
                input_buffer |= mcp_read_register(MCP23017_ADDR, 0x12);
            #endif
        #endif

        #if use_pwm == 1
        if (old_pwm_frequency != pwm_freq_buffer){
            old_pwm_frequency = pwm_freq_buffer;
            uint16_t wrap = pwm_calculate_wrap(rx_buffer->pwm_frequency);
            if (wrap > 0) {
                uint slice = pwm_gpio_to_slice_num(pwm_pin);
                pwm_set_enabled(pwm_gpio_to_slice_num(pwm_pin), false); // Disable PWM output before changing wrap
                pwm_set_wrap(slice, wrap); // Set the PWM wrap value
                pwm_set_enabled(slice, true); // Enable PWM output
                printf("PWM frequency: %d Hz wrap:%d\n", rx_buffer->pwm_frequency, wrap);
            }
        }
        #endif

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

    for (int i = 0; i < sizeof(input_pins); i++) {
        gpio_init(input_pins[i]);
        gpio_set_dir(input_pins[i], GPIO_IN);
        gpio_pull_up(input_pins[i]); // Pull-up ellenállás, ha szükséges
    }

    #if use_pwm == 1
    gpio_set_function(pwm_pin, GPIO_FUNC_PWM); // Set PWM pin function
    // Figure out which slice we just connected to the LED pin
    uint slice_num = pwm_gpio_to_slice_num(pwm_pin);

    // Get some sensible defaults for the slice configuration. By default, the
    // counter is allowed to wrap over its maximum range (0 to 2**16-1)
    pwm_config config = pwm_get_default_config();
    // Set divider, reduces counter clock to sysclock/this value
    pwm_config_set_clkdiv(&config, 1.0f);
    // Set the PWM frequency to 1kHz
    pwm_config_set_wrap(&config, 2048);
    pwm_init(slice_num, &config, false); // Initialize the PWM slice with the config
    //pwm_set_gpio_level(pwm_pin, 5000); // Set initial level to low
    #endif

    #if use_outputs == 1
    for (int i = 0; i < sizeof(output_pins); i++) {
        gpio_init(output_pins[i]);
        gpio_set_dir(output_pins[i], GPIO_OUT);
        gpio_put(output_pins[i], 0); // Set output pins to low
    }
    #endif

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

#if brakeout_board > 0
void i2c_setup(void) {
    i2c_init(i2c1, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCK, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCK);
}

uint8_t mcp_read_register(uint8_t i2c_addr, uint8_t reg) {
    uint8_t data;
    int ret;
    ret = i2c_write_blocking(i2c1, i2c_addr, &reg, 1, true);
    if (ret < 0) return 0xFF;
    ret = i2c_read_blocking(i2c1, i2c_addr, &data, 1, false);
    if (ret < 0) return 0xFF;
    return data;
}

void mcp_write_register(uint8_t i2c_addr, uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    int ret = i2c_write_blocking(i2c1, i2c_addr, data, 2, false);
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
#endif // brakeout_board > 0


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
    //time_diff = 0xFFFFFFFF;
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
                //printf("%d Received bytes: %d\n", rx_counter, len);
                last_packet_time = get_absolute_time();
                if (rx_buffer->packet_id != rx_counter) {
                    printf("packet loss: %d != %d\n", rx_buffer->packet_id, rx_counter);
                    rx_counter = rx_buffer->packet_id;
                }
                if (!rx_checksum_ok(rx_buffer)) {
                    printf("Checksum error: %d != %d\n", rx_buffer->checksum, checksum_index_in);
                    checksum_error = 1;
                }
                if (!checksum_error) {
                
                // update stepgens
                stepgen_update_handler();

                #if breakout_board > 0
                    // update output buffer
                    output_buffer = rx_buffer->outputs;
                #endif

                #if use_pwm == 1
                    // update pwm
                    pwm_freq_buffer = rx_buffer->pwm_frequency;
                    pwm_set_gpio_level(pwm_pin, (uint16_t)rx_buffer->pwm_duty ); // Set PWM value
                    }
                #else
                    }
                #endif
                
                // update encoders
                for (int i = 0; i < encoders; i++) {
                    encoder[i] = quadrature_encoder_get_count(pio1, i);
                    tx_buffer->encoder_counter[i] = encoder[i];
                }
                
                #if use_outputs == 1
                //set output pins
                for (uint8_t i = 0; i < sizeof(output_pins); i++) {
                    gpio_put(output_pins[i], (rx_buffer->outputs >> i) & 1);
                    }
                #endif
                
                tx_buffer->inputs[0] = gpio_get_all() & 0xFFFFFFFF; // Read all GPIO inputs
                #if brakeout_board > 0
                    tx_buffer->inputs[1] = input_buffer; // Read MCP23017 inputs
                #endif
                tx_buffer->packet_id = rx_counter;
                tx_buffer->checksum = calculate_checksum(tx_buffer, tx_size - 1);
                if (!checksum_error){
                _sendto(0, (uint8_t *)tx_buffer, tx_size, src_ip, src_port);
                rx_counter += 1;
                }
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
