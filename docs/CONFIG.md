# Stepper-Ninja Configuration Guide

The new board presets and I²C I/O prototype are available on the [test branch](https://github.com/atrex66/stepper-ninja/tree/test). Physical hardware validation is still pending.

This guide explains the main `#define` options in `config.h` and where they are used.

Stepper-Ninja keeps matching configuration headers in:

- `firmware/inc/config.h`
- `hal-driver/config.h`

In normal use, `hal-driver/config.h` should be a symlink to `firmware/inc/config.h`, not a separately maintained copy.

If the symlinks are missing, create them from the `hal-driver` directory with:

```bash
./make_symlinks.sh
```

This script recreates the shared symlinks used by the HAL driver.

---

## Which File Should You Edit?

- Edit `firmware/inc/config.h` when you change Pico firmware behavior.
- `hal-driver/config.h` should point to that file as a symlink.
- If `hal-driver/config.h` is a regular file instead of a symlink, run `hal-driver/make_symlinks.sh`.

Note: some values are overridden automatically in `footer.h` when `breakout_board` is not `0`.

---

## Network Defaults

These values define the startup network configuration stored in flash until changed by the serial terminal.

| Define | Meaning |
|---|---|
| `DEFAULT_MAC` | Default Ethernet MAC address |
| `DEFAULT_IP` | Default static IP address |
| `DEFAULT_PORT` | UDP port used by the HAL driver |
| `DEFAULT_GATEWAY` | Default gateway |
| `DEFAULT_SUBNET` | Default subnet mask |
| `DEFAULT_TIMEOUT` | Communication timeout in microseconds |

If you later use the serial terminal `ipconfig` commands, those runtime values can override the defaults stored in flash.

---

## Board Selection

`breakout_board` selects the hardware layout.

| Value | Meaning |
|---|---|
| `0` | Custom pin mapping from `config.h` |
| `1` | Stepper-Ninja breakout board |
| `2` | IO-Ninja breakout board |
| `3` | Analog-Ninja breakout board |
| `100` | BreakoutBoard100 |

When `breakout_board` is greater than `0`, `footer.h` replaces several custom pin definitions with board-specific values.

### Additional board presets

- [Expatria PicoBOB Deluxe A3 configuration](../configurations/PicoBOB-DLX/picoBOB-DLX.h): corrected W5500 RESET/INT, LED and PWM assignments.
- [Phil Barrett RP23CNC V1.0/V2](../configurations/RP23CNC/README.md): five axes, 14 inputs, 12 outputs, one spindle PWM and optional WIZ850io/W5500. Use `-DBOARD=rp23cnc -DWIZCHIP_TYPE=W5500`; `BOARD=pico2` selects a different, 30-GPIO MCU variant.

These presets use custom mode (`breakout_board=0`), not new numbered breakout-board IDs. Copy the selected preset to `firmware/inc/config.h` and rebuild matching firmware and HAL. Pin assignments and optional `GP_NULL` LED/reset handling are board-specific; do not copy them into unrelated configurations.

---

## Motion Channels

These defines set how many channels exist and which pins they use.

| Define | Meaning |
|---|---|
| `stepgens` | Number of step-generator channels |
| `stepgen_steps` | Step output pins |
| `stepgen_dirs` | Direction output pins |
| `step_invert` | Invert step outputs per channel |
| `encoders` | Number of encoder channels |
| `enc_pins` | First pin of each quadrature encoder pair |
| `enc_index_pins` | Index pins for encoders |
| `enc_index_active_level` | Active level of each index input |

For each encoder, `enc_pins` uses two GPIOs: `A` on the configured pin and `B` on the next pin.

---

## Encoder Mode

The encoder firmware supports two PIO implementations:

| Define | Meaning |
|---|---|
| `ENCODER_PIO_LEGACY` | Original quadrature counter PIO |
| `ENCODER_PIO_SUBSTEP` | Newer substep-aware PIO |
| `encoder_pio_version` | Selects which encoder PIO is compiled |

Typical setting:

```c
#define encoder_pio_version ENCODER_PIO_SUBSTEP
```

To switch back to the original encoder PIO, set:

```c
#define encoder_pio_version ENCODER_PIO_LEGACY
```

Current behavior:

- Encoder velocity estimation is done in the HAL driver.
- In legacy mode, firmware sends per-cycle encoder count deltas.
- In substep mode, firmware still sends raw encoder count data, but velocity output is produced on the HAL side.

You can also override the mode from the build command line:

```bash
CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY' cmake -S firmware -B build -DWIZCHIP_TYPE=W5500
```

---

## Digital Inputs and Outputs

| Define | Meaning |
|---|---|
| `in_pins` | Input GPIO list |
| `in_pullup` | Enable pull-up per input |
| `out_pins` | Output GPIO list |

These are used only in custom mode or where the selected breakout board does not override them.

---

## Configurable I²C digital I/O (prototype)

Optional ordinary digital I/O for custom **LinuxCNC** configurations (`breakout_board=0`). Supported types: `SN_IO_MCP23008`, `SN_IO_MCP23017`, `SN_IO_TCA9535`, `SN_IO_TCA9555`. The fixed breakout-board implementations remain separate, and their existing `io_expanders` setting is unchanged. Mach3 does not support this prototype.

| Define | Meaning |
|---|---|
| `I2C_IO_ENABLE` | Enable the new module; default `0` |
| `I2C_IO_BUS_COUNT` | Number of buses, 1–2 when enabled; default `0` |
| `I2C_IO_BUSES` | Bus descriptor list; default `{}` |
| `I2C_IO_DEVICE_COUNT` | Total device count, 1–2 when enabled; default `0` |
| `I2C_IO_DEVICES` | Device descriptor list; default `{}` |

Put the settings **before** `#include "footer.h"`. The footer provides missing defaults with `#ifndef`, so existing configurations require no new section and initialize no additional bus. Rebuild firmware and HAL with identical settings.

Example: one TCA9555, bits 0–7 inputs and 8–15 outputs, polling every 10 ms:

```c
#define I2C_IO_ENABLE 1
#define I2C_IO_BUS_COUNT 1
#define I2C_IO_BUSES {{0, GP00, GP01, 400000, 1000}}
#define I2C_IO_DEVICE_COUNT 1
#define I2C_IO_DEVICES { \
    {SN_IO_TCA9555, 0, 0x20, 0x00ff, 0, 0, 0, 0, 10} \
}
```

Bus fields, in order: controller (0/1), SDA GPIO, SCL GPIO, frequency in Hz (up to 400000), timeout per SDK transfer in µs (1–10000). Device fields: type, bus-list index, 7-bit address (0x20–0x27), direction mask (`1=input`), pull-up mask, input inversion, output inversion, logical startup/disconnect output levels, poll interval in ms (1–60000).

GPIO0/1 in the example must be free: do not overlap STEP/DIR, encoder/index, native I/O, PWM, keyboard, LED or transport pins. Use the [complete I/O-only example](../configurations/I2C-IO/config.h) as a starting point, not as a drop-in overlay for every board.

- At most two devices in total and 32 native digital outputs. Each expander reserves a 16-bit slot, including MCP23008. Native GPIO inputs retain both banks. The packet layout is unchanged.
- MCP23008 masks are 8-bit; other masks are 16-bit. The new path rejects MCP23017 input bits 7/15 (GPA7/GPB7, output-only in the current datasheet). This does not change legacy board handling.
- TCA pull-up masks must be zero. TCA9535 has no internal I/O pull-ups; TCA9555 has fixed input pull-ups which this field cannot disable.
- HAL names include `stepgen-ninja.0.expander.0.input.0`, `input.0-not`, `output.8`, `online` and `valid`. Only the configured direction is exported. Physical inputs/status are HAL_OUT; physical output commands are HAL_IN.
- Bus transfers run outside step/encoder IRQs. Failed transfers clear `online`/`valid`; input values retain the last successful sample. A failed bus cannot guarantee delivery of disconnected output levels.

See [I²C I/O details, limits and testing](I2C-IO.en.md) for register initialization, packet mapping and failure behavior. Physical I²C testing is still pending.

---

## PWM Options

| Define | Meaning |
|---|---|
| `use_pwm` | Enables PWM support |
| `pwm_count` | Number of PWM outputs |
| `pwm_pin` | PWM output pins |
| `pwm_invert` | Invert PWM per channel |
| `default_pwm_frequency` | Default PWM frequency |
| `default_pwm_maxscale` | HAL scaling limit |
| `default_pwm_min_limit` | Minimum PWM output limit |

Set `use_pwm` to `1` only when the PWM channel count and pins are valid for your board.

---

## Raspberry Pi SPI Mode

| Define | Meaning |
|---|---|
| `raspberry_pi_spi` | Use SPI link to a Raspberry Pi instead of Wiznet Ethernet |
| `RASPI_INT_OUT_GPIO` | Interrupt/status pin toward Raspberry Pi |
| `RASPI_SPI_CS_GPIO` | SPI chip-select GPIO toward Raspberry Pi |
| `raspi_inputs` | Raspberry Pi visible inputs |
| `raspi_input_pullups` | Pull-up setting for those inputs |
| `raspi_outputs` | Raspberry Pi controlled outputs |

When `raspberry_pi_spi` is `0`, the firmware uses the Wiznet Ethernet path.

---

## Timing Defaults

| Define | Meaning |
|---|---|
| `default_pulse_width` | Default step pulse width in nanoseconds |
| `default_step_scale` | Default steps per unit |
| `use_timer_interrupt` | Enables the step command ring buffer and timer-driven step output |

`use_timer_interrupt` can reduce visible PC-to-Pico jitter by buffering step commands.

---

## Footer-Controlled Defines

Some important defines live in `footer.h` because they depend on the selected board or platform.

| Define | Meaning |
|---|---|
| `use_stepcounter` | Uses step counter instead of the quadrature encoder path |
| `debug_mode` | Extra debug behavior for Raspberry Pi communication |
| `max_statemachines` | Derived total PIO state machine count |
| `pico_clock` | Pico system clock used by the firmware |

Do not change these casually unless you understand the impact on timing and PIO allocation.

---

## Build-Time Options Outside config.h

Some useful options are selected in CMake instead of `config.h`.

| Option | Meaning |
|---|---|
| `-DWIZCHIP_TYPE=W5100S` | Build for W5100S |
| `-DWIZCHIP_TYPE=W5500` | Build for W5500 |
| `-DBOARD=pico` | Build for Pico |
| `-DBOARD=pico2` | Build for Pico2 |
| `-DBOARD=rp23cnc` | Build for RP23CNC (48-GPIO RP2350B; matching preset and W5500 required) |
| `-DSTEPPER_NINJA_PROFILE=linuxcnc` / `mach3-3axis` | Host profile: `linuxcnc` (default) or `mach3-3axis` (fixed pinout) |
| `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` | Copy firmware to SRAM before execution |

Example:

```bash
cmake -S firmware -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_RUN_FROM_RAM=ON
```

The Mach3 prototype uses its own fixed three-axis profile, not the flexible LinuxCNC GPIO mapping. See [Mach3 configuration](MACH3.en.md) and [build instructions](MACH3-BUILD.en.md). The Pico firmware can also be built on Windows.

---

## Recommended Workflow

1. Select the board with `breakout_board`.
2. Set `stepgens`, `encoders`, and pin lists if using custom mode.
3. Choose `encoder_pio_version`.
4. Enable PWM only if your hardware uses it.
5. Keep firmware and HAL driver config files aligned.
6. Build firmware and HAL driver after every meaningful config change.

For network settings changed at runtime, also see [IPCONFIG.MD](IPCONFIG.MD).
