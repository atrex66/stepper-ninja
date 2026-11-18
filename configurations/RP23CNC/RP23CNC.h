#ifndef CONFIG_H
#define CONFIG_H
#include "internals.h"
#define SN_CONFIG_RP23CNC 1

// Phil Barrett RP23CNC V1.0 / V2 LinuxCNC preset, optional WIZ850io (W5500).
// Source: https://github.com/phil-barrett/RP23CNC
// Use as firmware/inc/config.h and rebuild matching firmware and HAL driver.
// Build with BOARD=rp23cnc, WIZCHIP_TYPE=W5500, STEPPER_NINJA_PROFILE=linuxcnc.
// Pre-V1.0 prototypes have different limit/aux wiring and are not covered.
#if defined(PICO_RP2040) || (defined(PICO_RP2350A) && PICO_RP2350A)
#error "RP23CNC requires the 48-GPIO RP2350B board: build with BOARD=rp23cnc"
#endif
#if defined(SN_HOST_MACH3) && SN_HOST_MACH3
#error "RP23CNC preset is for LinuxCNC; the fixed Mach3 pinout is different"
#endif

#define DEFAULT_MAC {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}
#define DEFAULT_IP {192, 168, 0, 177}
#define DEFAULT_PORT 8888
#define DEFAULT_GATEWAY {192, 168, 0, 1}
#define DEFAULT_SUBNET {255, 255, 255, 0}
#define DEFAULT_TIMEOUT 1000000

#define breakout_board 0
#define io_expanders 0
#define raspberry_pi_spi 0

// Board-only overrides: no edits to internals.h or other presets are required.
#undef SPI_PORT
#define SPI_PORT spi1
#undef GPIO_MISO
#define GPIO_MISO GP44
#undef GPIO_CS
#define GPIO_CS GP41
#undef GPIO_SCK
#define GPIO_SCK GP46
#undef GPIO_MOSI
#define GPIO_MOSI GP43
#undef GPIO_INT
#define GPIO_INT GP42
#undef GPIO_RESET
#define GPIO_RESET GP_NULL // WIZ RST is pulled high on the PCB, not MCU-controlled.
#undef LED_GPIO
#define LED_GPIO GP_NULL // No dedicated GPIO LED; GP25 is A enable.

// X, Y, Z, A, B. Enable outputs are separate digital outputs below.
#define stepgens 5
#define stepgen_steps {17, 18, 19, 20, 21}
#define stepgen_dirs {12, 13, 14, 15, 16}
#define step_invert {0, 0, 0, 0, 0}
#define encoders 0
#define enc_pins {}
#define enc_index_pins {}
#define enc_index_active_level {}

// X/Y/Z/A/B limits; probe, door/alarm, cycle start, feed hold, E-stop;
// D_IN0, D_IN1, D_IN2, D_IN3. Set the D_IN3 jumper to digital input, not UART1.
#define in_pins {6, 5, 4, 3, 2, 7, 8, 9, 10, 11, 29, 28, 47, 27}
#define in_pullup {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}

// X/Y/Z/A/B EN; spindle EN, spindle DIR; Aux0/1/2; flood, mist.
// EN polarity is selected in the machine HAL wiring for the actual drivers.
// Aux2 must select the digital/UART1-TX GPIO38 path, not spindle PWM GPIO35.
#define out_pins {22, 23, 24, 25, 26, 33, 34, 36, 37, 38, 39, 40}
#define use_pwm 1
#define pwm_count 1
#define pwm_pin {35}
#define pwm_invert {0}
#define default_pwm_frequency 10000
#define default_pwm_maxscale 4096
#define default_pwm_min_limit 0
#define default_pulse_width 2000
#define default_step_scale 1000
#ifndef use_timer_interrupt
#define use_timer_interrupt 0
#endif
#define encoder_pio_version ENCODER_PIO_SUBSTEP
#include "footer.h"
// RP2350's standard rated clock; this override affects this preset only.
#undef pico_clock
#define pico_clock 150000000
#endif
