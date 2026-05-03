# Stepper-Ninja Configuration Guide

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
| `raspi_int_out` | Interrupt/status pin toward Raspberry Pi |
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
| `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` | Copy firmware to SRAM before execution |

Example:

```bash
cmake -S firmware -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_RUN_FROM_RAM=ON
```

---

## Recommended Workflow

1. Select the board with `breakout_board`.
2. Set `stepgens`, `encoders`, and pin lists if using custom mode.
3. Choose `encoder_pio_version`.
4. Enable PWM only if your hardware uses it.
5. Keep firmware and HAL driver config files aligned.
6. Build firmware and HAL driver after every meaningful config change.

For network settings changed at runtime, also see [IPCONFIG.MD](IPCONFIG.MD).