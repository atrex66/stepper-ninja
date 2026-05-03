# Stepper-Ninja – Working Principles

This document explains the internal architecture, working principles, and purpose of every module and file in the stepper-ninja project. It is intended for developers who want to understand how the system operates, debug existing behaviour, or add new features.

---

## 1. System Overview

Stepper-Ninja is a motion-control offload bridge between **LinuxCNC** running on a PC or Raspberry Pi and a **Raspberry Pi Pico / Pico2** microcontroller. The Pico handles all real-time, cycle-critical hardware tasks — step/direction pulse generation, quadrature encoder counting, PWM output, and I/O — while LinuxCNC drives it via a low-latency network protocol.

```
┌─────────────────────────────────┐       UDP / SPI       ┌────────────────────────────────┐
│          HOST (PC / RPi)        │ ◄──────────────────── │     Pico / Pico2 Firmware      │
│                                 │ ──────────────────────►│                                │
│  LinuxCNC  ◄──►  stepper-ninja  │       ~1 ms period     │  PIO stepgen  Encoder  I/O    │
│           HAL module (.so)      │                        │  Breakout board peripherals    │
└─────────────────────────────────┘                        └────────────────────────────────┘
```

The two sides share a compact binary packet structure (`transmission_pc_pico_t` / `transmission_pico_pc_t`) with a rolling checksum. Everything timing-critical runs on the Pico; LinuxCNC only needs to compute how many steps fall inside each servo period.

---

## 2. Dual-Core Architecture (Firmware)

The Pico firmware runs on both cores simultaneously:

| Core | Responsibility |
|------|---------------|
| **Core 0** | Network (UDP/SPI), packet reception, timer interrupt, PIO step-command dispatch via `stepgen_update_handler()` |
| **Core 1** | LED heartbeat, timeout watchdog, breakout board polling (`breakout_board_connected_update`), encoder index interrupt management, PWM frequency updates, serial terminal (while disconnected) |

This split avoids interference between network DMA latency (Core 0) and peripheral polling (Core 1). The two cores communicate through shared volatile globals (`timeout_error`, `connected`, `rx_buffer`, `tx_buffer`, `checksum_error`) and a multicore lockout mechanism used during flash writes.

---

## 3. Communication Protocol

### 3.1 Transport Layer

- **Ethernet UDP** (primary): Uses the WIZnet W5100S-EVB-Pico or an external W5500 module. SPI DMA is used for burst transfers to the WIZnet chip, minimising CPU stall time.
- **SPI slave** (alternate): Used when the Pico acts as a co-processor directly attached to a Raspberry Pi GPIO header. Configured with `raspberry_pi_spi = 1` in `config.h`.

### 3.2 Packet Structures (`firmware/modules/inc/transmission.h`)

Two complementary packed structs form the entire communication contract:

**Host → Pico (`transmission_pc_pico_t`):**

| Field | Type | Purpose |
|-------|------|---------|
| `stepgen_command[N]` | `uint32_t[]` | Steps + direction for each axis. Bit 31 = direction, bits 0–30 = step count for this servo period. |
| `outputs[2]` | `uint32_t[2]` | Bitmask of digital output states (up to 64 outputs). |
| `pwm_duty[N]` | `uint32_t[]` | PWM duty cycle values. |
| `pwm_frequency[N]` | `uint32_t[]` | PWM frequency (Hz) — applied only when changed. |
| `pio_timing` | `uint16_t` | Index into the `pio_settings` table; controls step pulse width. |
| `enc_control` | `uint8_t` | Bitmask enabling/disabling encoder index interrupts per channel. |
| `analog_out[N]` | `uint32_t[]` | Target DAC values for analog outputs (12-bit, e.g. MCP4725). |
| `packet_id` | `uint8_t` | Rolling counter for packet sequencing. |
| `checksum` | `uint8_t` | Integrity byte (jump-table hash over all prior fields). |

**Pico → Host (`transmission_pico_pc_t`):**

| Field | Type | Purpose |
|-------|------|---------|
| `encoder_counter[N]` | `int32_t[]` | Signed quadrature encoder counts. |
| `encoder_velocity[N]` | `int32_t[]` | Velocity estimate (sub-step interpolated). |
| `encoder_timestamp[N]` | `uint32_t[]` | Timestamp of last encoder event (for velocity calculation). |
| `interrupt_data` | `uint8_t` | Encoder index pulse flags. |
| `inputs[4]` | `uint32_t[4]` | Bitmask of digital input states (up to 128 inputs). |
| `jitter` | `uint32_t` | Network round-trip jitter measurement (µs). |
| `packet_id` | `uint8_t` | Mirrors the received `packet_id` for echo checking. |
| `checksum` | `uint8_t` | Integrity byte. |

Both structs are declared `#pragma pack(push, 1)` — no padding, every byte is significant.

### 3.3 Checksum Algorithm (`firmware/modules/transmission.c`)

A non-cryptographic rolling hash using a precomputed 256-entry `jump_table` (`firmware/modules/inc/jump_table.h`). The checksum is computed over all fields except the final `checksum` byte. On the receive side, a mismatch sets `checksum_error = 1`, which immediately suppresses all step generation until the next valid packet.

---

## 4. Step Generation

### 4.1 PIO Program (`firmware/pio/freq_generator.pio`)

The step generator is implemented entirely in PIO (Programmable I/O) assembly. One PIO state machine runs per axis. The program:

1. Pulls a 32-bit word from the TX FIFO (blocking wait).
2. Extracts the **step count** from bits 9–0 (up to 1023 steps per servo period).
3. Loops: asserts the step pin HIGH for a configurable high time, then LOW for a configurable low time.
4. Repeats until the step count is exhausted, then waits for the next command.

The high time is set by a `set y, <sety>` instruction, and the low time by a `nop [<delay>]`. Both values come from the `pio_settings` table indexed by `rx_buffer->pio_timing`. This allows the pulse width to be tuned at runtime from a LinuxCNC HAL pin.

Maximum step rate: **255 kHz** per axis (limited by the servo period, not the PIO clock). Maximum pulse frequency achievable: **up to 1 MHz** with a 1 ms servo thread (1023 steps per period).

### 4.2 Direction Signal

The direction GPIO is driven by Core 0 immediately before the step count is pushed to the PIO FIFO. Bit 31 of `stepgen_command[i]` is decoded as the direction bit.

### 4.3 Runtime Pulse Width Change (`main.c` — Core 1 loop)

When `rx_buffer->pio_timing` changes, Core 1 patches the PIO instruction memory directly:

```c
stepgen_pio[i].pio->instr_mem[4] = pio_encode_set(pio_y, sety);
stepgen_pio[i].pio->instr_mem[5] = pio_encode_nop() | pio_encode_delay(nop);
```

This hot-patches the running program without stopping the state machine, allowing seamless pulse-width switching.

### 4.4 PIO Resource Manager (`firmware/modules/pio_utils.c`)

`get_next_pio(length)` allocates PIO state machines from the available PIO blocks (2 blocks on RP2040, 3 on RP2350, 4 state machines each). It ensures that all state machines loading the same program share the same instruction memory slot, avoiding duplication. Returns a `PIO_def_t` struct with the assigned PIO block, program offset, and state machine number.

---

## 5. Quadrature Encoder Counting

### 5.1 PIO Program (`firmware/pio/quadrature_encoder.pio`)

Implements a 16-state quadrature decoder entirely in PIO. It must be loaded at address 0 (`.origin 0`) because it uses computed jumps to a 4×4 state transition table. The Y register accumulates the signed count in hardware, achieving:

- **Maximum count rate**: system clock / 10 = **12.5 Msteps/sec at 125 MHz**.
- State transitions: increment, decrement, or no-change based on current and previous A/B pin states.
- The current count is pushed to the RX FIFO continuously (non-blocking); the firmware drains the FIFO and takes the latest value.

### 5.2 Substep Interpolation (`quadrature_encoder_substep/`)

An optional sub-step velocity estimator that uses timestamps between encoder events to interpolate position between count increments. This improves velocity feedback smoothness at low speeds. The state for each encoder channel is kept in a `substep_state_t` struct.

### 5.3 Index Handling

Encoder index pins are managed via GPIO interrupts, enabled/disabled per channel by the `enc_control` byte received from the host. When an index pulse fires, the interrupt callback sets a flag in `interrupt_data`, which is reported back to the HAL driver in the next packet.

---

## 6. PWM Output (`firmware/modules/pwm.c`)

Uses the RP2040/RP2350 hardware PWM slices directly.

- `ninja_pwm_init(pin)` — configures GPIO for PWM function and initialises the slice in disabled state.
- `ninja_pwm_set_frequency(pin, freq)` — computes the wrap value as `sys_clock / freq` with a fixed 1.0 clock divider. For frequencies below 1908 Hz, the wrap is clamped to 65535 for maximum resolution (16-bit). Changes are applied with a brief disable/re-enable to avoid glitches.
- `ninja_pwm_set_duty(pin, duty)` — sets the compare level.

Frequency range: **1907 Hz – 1 MHz**. Resolution: 16-bit at low frequencies, reduces toward 7-bit at 1 MHz.

---

## 7. Timeout and Connection State Machine

The connection lifecycle is tracked through two global flags: `timeout_error` and `connected`.

```
timeout_error = 1 (Disconnected)
        │
        │  first valid packet received within TIMEOUT_US
        ▼
timeout_error = 0 (Connected)
        │
        │  no packet received for > TIMEOUT_US microseconds
        ▼
timeout_error = 1 (Disconnected)
  → breakout_board_disconnected_update() called
  → all GPIO outputs zeroed
  → output_buffer = 0
  → PIO encoder counts reset to 0
  → checksum state reset
  → serial terminal re-enabled
```

The default timeout is **1,000,000 µs (1 second)**, configurable via the serial terminal (`timeout <value>`) and stored in flash.

---

## 8. Configuration and Flash Storage (`firmware/modules/flash_config.c`)

### Goal

Network parameters (IP, MAC, port, subnet, gateway, timeout) are stored in the Pico's own flash memory at a **1 MB offset** from the start of flash. This survives power cycles and firmware reflashes (as long as flash_nuke is not used).

### Mechanism

- On startup, `load_configuration()` reads the `configuration_t` struct from the target flash page and validates its checksum. If invalid, `restore_default_config()` writes the compile-time defaults.
- `save_config_to_flash()` must run on Core 0 because the Pico SDK's `flash_safe_execute` requires only one core to be active during flash erases/writes. Core 1 is locked out via `multicore_lockout_start_blocking()` during the operation.
- `request_save_config_to_flash()` / `consume_save_config_request()` provide a deferred save mechanism: the serial terminal sets a request flag; the main loop checks it on Core 0 and performs the actual write safely.

---

## 9. Serial Terminal (`firmware/modules/serial_terminal.c`)

The serial terminal is active **only when LinuxCNC is disconnected** (`timeout_error == 1`). It is polled from Core 1. When a connection is established, it is silenced to avoid interfering with real-time operation.

- Input: non-blocking `getchar_timeout_us`, buffered up to 63 characters.
- Commands are parsed by `process_command()` using `strcmp`/`strncmp`/`sscanf`.
- Modifiable parameters: IP address, MAC address, port, timeout, time constant.
- `save` command persists current settings to flash via the deferred save mechanism.
- `defaults` calls `restore_default_config()`.
- `reset`/`reboot` triggers a watchdog-induced reboot.

See [IPCONFIG.md](IPCONFIG.md) for full command reference.

---

## 10. Breakout Board Module System

### 10.1 Concept

The breakout board ID system allows the same firmware binary tree to support multiple physical board layouts without duplicate code. A single `#define breakout_board N` in `firmware/inc/config.h` and `hal-driver/config.h` switches both the firmware peripheral init code and the HAL pin exports at compile time.

### 10.2 Board ID Registry

| ID | Name | Digital I/O | Analog | Encoders | Stepgens |
|----|------|-------------|--------|----------|----------|
| 0 | None (custom GPIO config) | Direct GPIO | — | Custom | Custom |
| 1 | Stepper-Ninja v1.0 | 16 in (MCP23017), 8 out (MCP23008) | 2× MCP4725 | 2 | 4 |
| 2 | IO-Ninja | 96 in (6× MCP23017), 32 out (2× MCP23017) | — | 0 | 0 |
| 3 | Analog-Ninja | Minimal | 6× MCP4725 + high-speed encoders | 6 | — |
| 100 | Breakoutboard-100 | 32 in (MCP23017), 16 out | — | 2 | 4 |
| 255 | User template | (user-defined) | (user-defined) | (user-defined) | (user-defined) |

### 10.3 Firmware Side (`firmware/modules/breakoutboard_<ID>.c`)

Each board module is wrapped in `#if breakout_board == N` ... `#endif` and must implement four callbacks:

| Callback | Called by | Purpose |
|----------|-----------|---------|
| `breakout_board_setup()` | Core 1 startup | I2C bus init, MCP chip detection and register init, DAC zeroing |
| `breakout_board_disconnected_update()` | Core 1 timeout path | Force all outputs safe (DAC = 0, output registers cleared) |
| `breakout_board_connected_update()` | Core 1 main loop | Read MCP input registers → `input_buffer[]`, write `output_buffer` → MCP output registers |
| `breakout_board_handle_data()` | IRQ / packet handler | Unpack `rx_buffer` fields into hardware state; pack hardware inputs into `tx_buffer->inputs[]` |

### 10.4 Common I2C Infrastructure (`firmware/modules/breakoutboard_common.c`)

Shared utilities used by all board modules:

- `i2c_setup()` — initialises the I2C bus at 400 kHz and configures SDA/SCL pins.
- `mcp4725_port_setup()` — initialises a separate I2C port for DAC chips (board 1 uses `i2c0` for DAC, `i2c1` for expanders).
- `mcp_read_register(addr, reg)` — single-byte register read from any MCP chip.
- `mcp_write_register(addr, reg, value)` — single-byte register write with error reporting.
- `i2c_check_address(i2c, addr)` — probes an I2C address with a 100 µs timeout; returns true if acknowledged. Used during startup to detect which chips are physically present.

### 10.5 HAL Side (`hal-driver/modules/breakoutboard_hal_<ID>.c`)

Mirrors the firmware board selection. Each HAL board module implements:

| Function | Purpose |
|----------|---------|
| `bb_hal_setup_pins(...)` | `hal_pin_*_newf()` calls to export all board-specific HAL pins |
| `bb_hal_process_recv(...)` | Unpack `rx_buffer->inputs[0..3]` into HAL output pins (bit extraction) |
| `bb_hal_process_send(...)` | Pack HAL command pins into `tx_buffer->outputs[0..1]` and analog fields |

The HAL board module is `#include`-d directly into `hal-driver/stepper-ninja.c` via a compile-time `#elif breakout_board == N` chain.

---

## 11. HAL Driver (`hal-driver/stepper-ninja.c`)

### 11.1 Purpose

This is the LinuxCNC realtime kernel module. It is loaded by the LinuxCNC HAL layer and runs as a realtime task. Its job is to:

1. Translate LinuxCNC joint position commands into `stepgen_command[]` step counts.
2. Send the UDP packet to the Pico.
3. Receive the Pico's response packet.
4. Update encoder feedback and digital I/O HAL pins.

### 11.2 HAL Pin Exports

The driver exports an extensive set of HAL pins in the `data_t` struct:

- **Stepgen**: `command[N]`, `feedback[N]`, `scale[N]`, `enable[N]`, `mode[N]`, `pulse_width` (shared)
- **Encoders**: `raw_count[N]`, `enc_position[N]`, `enc_velocity[N]`, `enc_scale[N]`, `enc_reset[N]`, `enc_index[N]`, `enc_rpm[N]`
- **PWM**: `pwm_enable[N]`, `pwm_output[N]`, `pwm_frequency[N]`, `pwm_maxscale[N]`, `pwm_min_limit[N]`
- **Analog**: `analog_value[N]`, `analog_min[N]`, `analog_max[N]`, `analog_enable[N]`, `analog_offset[N]`
- **I/O**: `input[96]`, `input_not[96]`, `output[64]`
- **Status**: `connected`, `jitter`, `io_ready_in`, `io_ready_out`, `period`
- **Debug**: `debug_freq`, `debug_steps[N]`, `debug_steps_reset`

### 11.3 Realtime Loop (servo-thread functions)

Three functions are added to the servo thread:

| Function | Order | Action |
|----------|-------|--------|
| `watchdog-process` | 1st | Safety watchdog; clears outputs if no packet acknowledged |
| `process-send` | 2nd | Build `transmission_pc_pico_t`, compute checksum, send UDP packet |
| `process-recv` | 3rd | Receive UDP reply, validate checksum, unpack into HAL pins |

The step count for each axis is computed as the difference between the current commanded position and the last sent position, scaled by `scale[i]`. A +10,000 mm offset is added internally to avoid integer zero-crossing artefacts in simulation runs.

### 11.4 Transport Selection

Compiled with `raspberry_pi_spi = 0` (default): uses BSD sockets over UDP. Compiled with `raspberry_pi_spi = 1`: uses `bcm2835` SPI library for direct RPi GPIO SPI communication with no network stack overhead.

---

## 12. Configuration Headers

### `firmware/inc/config.h`

The single top-level configuration file for the firmware. Defines:

- Default network parameters (IP, MAC, port, gateway, subnet, timeout).
- `breakout_board` ID.
- When `breakout_board == 0`: all GPIO pin assignments for stepgens, encoders, digital I/O, and PWM.
- Raspberry Pi SPI mode flag.
- Default pulse width and step scale.
- Includes `footer.h` and `kbmatrix.h`.

### `firmware/inc/footer.h`

Processed **after** `config.h`. Contains `#if breakout_board == N` blocks that:

- Override the pin definitions set in `config.h` with the board-specific values.
- Define I2C bus pins, expander addresses, DAC addresses, and expander counts.
- Undef and redefine stepgen/encoder/pwm macros appropriate for that board.

This two-pass approach (`config.h` → `footer.h`) means user-facing changes stay in `config.h` while board-specific overrides are isolated in `footer.h`.

### `firmware/inc/internals.h`

Low-level constants and pin alias macros:

- `GP00`–`GP47`: GPIO number aliases for RP2040/RP2350.
- `PIN_1`–`PIN_n`: Physical header pin number to GPIO number mappings.
- `GP_03`–`GP_22`: Raspberry Pi GPIO header number to BCM number mappings.
- `high`/`low`, `GP_NULL`/`PIN_NULL`: Sentinel values.
- `version`: Firmware version string.

### `firmware/inc/main.h`

Aggregate include that pulls in all SDK headers, module headers, and config headers. Every `.c` file in the firmware includes this single header.

### `hal-driver/config.h`

Mirrors `firmware/inc/config.h` for the HAL driver. Must have an identical `breakout_board` value. Also includes `pio_settings.h` (the pulse-width lookup table used by both sides).

---

## 13. File Map Summary

### Firmware (`firmware/`)

| File | Goal |
|------|------|
| `src/main.c` | Entry point, Core 0/1 split, packet loop, PIO control, init sequence |
| `inc/config.h` | User configuration: pins, board ID, network defaults |
| `inc/footer.h` | Board-specific override macros (processed after config.h) |
| `inc/internals.h` | GPIO aliases, pin-number mappings, version constant |
| `inc/main.h` | Master include for all firmware source files |
| `modules/breakoutboard_1.c` | Board 1: MCP23017 inputs, MCP23008 outputs, MCP4725 DAC |
| `modules/breakoutboard_2.c` | Board 2: IO-Ninja, 96-input / 32-output with 8× MCP23017 |
| `modules/breakoutboard_3.c` | Board 3: Analog-Ninja with 6× MCP4725 |
| `modules/breakoutboard_100.c` | Board 100: Mixed I/O with MCP23017 |
| `modules/breakoutboard_common.c` | Shared I2C utilities used by all board modules |
| `modules/breakoutboard_user.c` | User template (ID 255) — copy to create a new board |
| `modules/transmission.c` | Packet checksum functions |
| `modules/flash_config.c` | Flash read/write for network configuration |
| `modules/pio_utils.c` | PIO state machine allocator |
| `modules/pwm.c` | Hardware PWM init, frequency, and duty control |
| `modules/serial_terminal.c` | USB serial command interface for IP configuration |
| `modules/mcp4725.c` | 12-bit I2C DAC driver (MCP4725) |
| `modules/sh1106.c` | Optional SH1106 OLED display driver |
| `modules/inc/transmission.h` | Packet struct definitions (shared with HAL driver) |
| `modules/inc/flash_config.h` | Flash config struct and function declarations |
| `modules/inc/pio_utils.h` | PIO allocator types and declarations |
| `modules/inc/breakoutboard.h` | Board callback declarations |
| `modules/inc/jump_table.h` | 256-entry checksum lookup table |
| `modules/inc/pio_settings.h` | Step pulse width timing table |
| `pio/freq_generator.pio` | Step pulse generator PIO program |
| `pio/quadrature_encoder.pio` | Quadrature decoder PIO program (RP2040) |
| `pio/quadrature_encoder pico2.pio` | Quadrature decoder PIO program (RP2350) |
| `pio/quadrature_encoder_substep.pio` | Sub-step velocity interpolator PIO program |
| `pio/step_counter.pio` | Alternative step counter PIO program |
| `CMakeLists.txt` | CMake build script; controls which board modules are compiled |

### HAL Driver (`hal-driver/`)

| File | Goal |
|------|------|
| `stepper-ninja.c` | LinuxCNC realtime HAL module; UDP send/recv loop, HAL pin exports |
| `config.h` | Mirror of firmware config.h — must match `breakout_board` ID |
| `transmission.h` | Same packet struct header shared with firmware |
| `transmission.c` | Same checksum implementation shared with firmware |
| `pio_settings.h` | Pulse-width table used to decode the `pio_timing` index |
| `hal_pin_macros.h` | Convenience macros for HAL pin declaration |
| `hal_util.c/.h` | Utility helpers for the HAL module |
| `modules/breakoutboard_hal_1.c` | HAL pin exports and packet mapping for Board 1 |
| `modules/breakoutboard_hal_2.c` | HAL pin exports and packet mapping for Board 2 |
| `modules/breakoutboard_hal_3.c` | HAL pin exports and packet mapping for Board 3 |
| `modules/breakoutboard_hal_100.c` | HAL pin exports and packet mapping for Board 100 |
| `modules/breakoutboard_hal_0.c` | HAL pin exports for the no-breakout-board mode |
| `modules/breakoutboard_hal_user.c` | User template for new boards (ID 255) |
| `modules/breakoutboard_common.c` | Shared utilities (I2C helpers, address probe) |
| `install.sh` | Builds and installs `stepper-ninja.so` into LinuxCNC module path |
| `CMakeLists.txt` | CMake build script for the HAL shared object |

---

## 14. Data Flow Diagram

```
LinuxCNC servo-thread (every 1 ms)
│
├─ process-send:
│   ├─ compute step counts per axis (position delta × scale)
│   ├─ pack outputs, PWM, analog into transmission_pc_pico_t
│   ├─ calculate rolling checksum
│   └─ send UDP packet to Pico (192.168.0.177:8888 default)
│
└─ process-recv:
    ├─ receive UDP reply (transmission_pico_pc_t)
    ├─ validate checksum
    ├─ update encoder_position[N], encoder_velocity[N]
    ├─ update input[0..127] HAL pins
    └─ update jitter, connected flags

Pico Core 0 (interrupt-driven):
├─ receive UDP packet
├─ validate checksum → set checksum_error if bad
├─ push stepgen_command[] to PIO FIFOs via stepgen_update_handler()
├─ collect encoder counts from PIO RX FIFOs
├─ update inputs from GPIO / breakout board input_buffer[]
├─ build and send reply packet (transmission_pico_pc_t)
└─ reset packet timer (last_packet_time)

Pico PIO (hardware, always running):
├─ freq_generator.pio: generates step pulses per axis at configured timing
└─ quadrature_encoder.pio: counts A/B transitions at up to 12.5 Msteps/sec

Pico Core 1 (continuous loop):
├─ connected: call breakout_board_connected_update() → poll MCP I2C chips
├─ connected: update PWM frequency if changed
├─ timeoutled: call breakout_board_disconnected_update() → safe state
└─ disconnected: poll serial terminal for IP configuration
```
