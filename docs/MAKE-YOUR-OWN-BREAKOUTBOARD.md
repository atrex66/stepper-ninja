# Make Your Own Breakout Board for stepper-ninja

This guide explains how to design and integrate a custom breakout board with stepper-ninja, from electrical planning to firmware/HAL integration and LinuxCNC bring-up.

## 1. What You Are Building

A custom board in this project has two software sides:

- Firmware side on Pico/Pico2:
  - File pattern: firmware/modules/breakoutboard_<ID>.c
  - Handles physical hardware (GPIO, I2C expanders, DACs, safety state).
  - Maps raw board state into packet fields.

- LinuxCNC HAL side:
  - File pattern: hal-driver/modules/breakoutboard_hal_<ID>.c
  - Exports HAL pins and maps packet fields to those pins.
  - Packs HAL outputs back into packet fields sent to firmware.

Both sides are selected using `breakout_board` in firmware/inc/config.h and hal-driver/config.h.

## 2. Prerequisites

- Working stepper-ninja build environment.
- Pico SDK installed (or already usable by firmware/CMakeLists.txt).
- LinuxCNC dev headers/tools installed for HAL module build.
- A known-good existing board to copy as a reference:
  - firmware/modules/breakoutboard_1.c
  - hal-driver/modules/breakoutboard_hal_1.c

## 3. Hardware Planning Checklist

Before coding, lock down these items:

- Power rails and logic levels:
  - 3.3V logic compatibility with Pico GPIO.
  - Proper level shifting/opto isolation where needed.

- Safety behavior:
  - Define exactly what outputs must do on communication timeout/disconnect.
  - Ensure enable lines fail safe (motor enable off, spindle disable, analog outputs zero).

- I/O budget:
  - Digital inputs/outputs count.
  - Analog outputs count and DAC type.
  - Encoder channels and index behavior.

- Bus/peripheral allocation:
  - I2C bus/pins for expanders/DACs.
  - SPI/WIZnet wiring and reset/interrupt lines.

- Signal integrity:
  - Keep step/dir traces clean and short.
  - Add pullups/pulldowns where boot-time state matters.

## 4. Choose a New Board ID

Pick an unused integer ID, for example `42`.

You will use this same ID in:

- firmware/modules/breakoutboard_42.c
- hal-driver/modules/breakoutboard_hal_42.c
- `#define breakout_board 42` in config headers
- selection branches in firmware and HAL build paths

## 5. Firmware Side Integration

### 5.1 Create the firmware module

1. Copy template:

```bash
cp firmware/modules/breakoutboard_user.c firmware/modules/breakoutboard_42.c
```

2. In firmware/modules/breakoutboard_42.c change:

```c
#if breakout_board == 255
```

to:

```c
#if breakout_board == 42
```

3. Implement the required callbacks:

- `breakout_board_setup()`:
  - Initialize GPIO/I2C/SPI devices on your board.
  - Probe and configure expanders and DACs.

- `breakout_board_disconnected_update()`:
  - Force all outputs to safe state.
  - Set DAC outputs to zero or safe bias.

- `breakout_board_connected_update()`:
  - Apply outputs from received packet to hardware.
  - Refresh input snapshots from hardware.

- `breakout_board_handle_data()`:
  - Fill outbound packet fields (`tx_buffer->inputs[]`, etc.).
  - Consume inbound fields (`rx_buffer->outputs[]`, analog payloads).

### 5.2 Add your module to firmware build

Edit firmware/CMakeLists.txt and add your new source file in `add_executable(...)`:

```cmake
modules/breakoutboard_42.c
```

### 5.3 Define board macros in firmware config footer

Edit firmware/inc/footer.h and add a new block:

```c
#if breakout_board == 42
    // your pin mapping and board-specific macros
    // examples:
    // #define I2C_SDA 26
    // #define I2C_SCK 27
    // #define I2C_PORT i2c1
    // #define MCP23017_ADDR 0x20
    // #define ANALOG_CH 2
    // #define MCP4725_BASE 0x60

    // override stepgen/encoder/in/out/pwm macros as needed
#endif
```

Use existing blocks for IDs 1, 2, 3, 100 as style references.

### 5.4 Select your board in firmware config

Edit firmware/inc/config.h:

```c
#define breakout_board 42
```

## 6. HAL Driver Side Integration

### 6.1 Create HAL board module

1. Copy template:

```bash
cp hal-driver/modules/breakoutboard_hal_user.c hal-driver/modules/breakoutboard_hal_42.c
```

2. In hal-driver/modules/breakoutboard_hal_42.c change:

```c
#if breakout_board == 255
```

to:

```c
#if breakout_board == 42
```

3. Implement:

- `bb_hal_setup_pins(...)`:
  - Export all required pins (`hal_pin_bit_newf`, `hal_pin_float_newf`, etc.).
  - Initialize default values.

- `bb_hal_process_recv(...)`:
  - Unpack `rx_buffer->inputs[...]` and other fields into HAL output pins.

- `bb_hal_process_send(...)`:
  - Pack HAL command pins into `tx_buffer->outputs[...]` and analog fields.

### 6.2 Register your HAL board include

Edit hal-driver/stepper-ninja.c in the board-selection include section and add:

```c
#elif breakout_board == 42
#include "modules/breakoutboard_hal_42.c"
```

### 6.3 Select board ID in HAL config

Edit hal-driver/config.h:

```c
#define breakout_board 42
```

Keep firmware and HAL `breakout_board` IDs identical.

## 7. Packet Mapping Rules (Important)

Use a stable contract between both sides.

- Firmware side writes machine-to-host data:
  - `tx_buffer->inputs[0..3]`
  - encoder feedback/jitter fields

- HAL side reads these into HAL pins in `bb_hal_process_recv()`.

- HAL side writes host-to-firmware commands:
  - `tx_buffer->outputs[0..1]`
  - analog and optional control fields

- Firmware side reads those commands via `rx_buffer` and applies them in `breakout_board_connected_update()`.

Any mismatch here causes swapped pins or dead I/O. Keep a single mapping table in comments while developing.

## 8. Build and Flash Firmware

From repository root:

```bash
cd firmware
cmake -S . -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500
cmake --build build -j"$(nproc)"
ls build/*.uf2
```

If you use Pico v1, set `-DBOARD=pico`.

Flash:

1. Hold BOOTSEL while plugging the Pico.
2. Copy generated `.uf2` to the mounted RPI-RP2 drive.
3. Reboot and open serial output to verify init logs.

## 9. Build and Install LinuxCNC HAL Module

From repository root:

```bash
cd hal-driver
cmake -S . -B build-cmake
cmake --build build-cmake --target stepper-ninja
sudo cmake --install build-cmake --component stepper-ninja
```

This installs `stepper-ninja.so` into your LinuxCNC module directory.

## 10. LinuxCNC HAL Bring-Up (Minimal)

Example skeleton in your HAL file:

```hal
loadrt stepper-ninja ip_address="192.168.0.177:8888"
addf stepper-ninja.0.watchdog-process servo-thread
addf stepper-ninja.0.process-send     servo-thread
addf stepper-ninja.0.process-recv     servo-thread

# Example board I/O nets (names depend on your bb_hal_setup_pins implementation)
# net estop-in      stepper-ninja.0.inputs.0     => some-signal
# net coolant-out   some-command                 => stepper-ninja.0.outputs.0
```

Then verify with `halshow`/`halcmd show pin stepper-ninja.0.*`.

## 11. Commissioning Procedure (Recommended)

Run this exact order:

1. Power-only test:
  - Validate rails, no overheating, no excessive idle current.

2. Bus discovery test:
  - Confirm expected I2C addresses are detected by firmware prints.

3. Disconnect safety test:
  - Stop LinuxCNC communication.
  - Confirm outputs and analog channels go to safe state.

4. Input polarity test:
  - Toggle each physical input and confirm matching HAL pin state.
  - Validate `-not` complements if exported.

5. Output test:
  - Toggle each HAL output pin and verify physical output.

6. Encoder test:
  - Rotate slowly and quickly, verify count direction and index handling.

7. Motion test with motor disconnected:
  - Validate step/dir polarity and pulse timing first.

8. Motion test connected:
  - Start with conservative velocity/acceleration.

## 12. Troubleshooting

- No I/O response:
  - Check that `breakout_board` ID matches in both firmware and HAL configs.
  - Check your new files are included in build paths.

- Build succeeds but board callbacks not active:
  - Confirm `#if breakout_board == 42` guard is correct.
  - Confirm footer/config branch exists and compiles.

- Inputs shifted/wrong bit order:
  - Recheck bit packing/unpacking between:
    - firmware/modules/breakoutboard_42.c
    - hal-driver/modules/breakoutboard_hal_42.c

- Random disconnect behavior:
  - Verify timeout-safe code in `breakout_board_disconnected_update()`.
  - Check network stability and packet watchdog wiring.

- Analog output clipping/overflow:
  - Clamp HAL values before packing DAC words.
  - Confirm scaling against DAC resolution.

## 13. Version Control Recommendations

For each custom board change set, commit in this order:

1. Firmware module + footer/config updates.
2. HAL module + include/config updates.
3. LinuxCNC HAL config updates.
4. Documentation for wiring and pin map.

Keep a board-specific markdown file with:

- Schematic link
- Pin map table
- I2C address map
- Safe-state policy
- HAL pin naming contract

## 14. Quick File Checklist

You should touch all of the following for a new board ID:

- firmware/modules/breakoutboard_42.c
- firmware/CMakeLists.txt
- firmware/inc/footer.h
- firmware/inc/config.h
- hal-driver/modules/breakoutboard_hal_42.c
- hal-driver/stepper-ninja.c
- hal-driver/config.h
- (optional) docs/your-board-name.md

If these are complete and IDs are consistent, your custom breakout board integration is usually straightforward.
