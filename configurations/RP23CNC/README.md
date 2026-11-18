# Phil Barrett RP23CNC — StepperNinja LinuxCNC preset

[Magyar](README.hu.md)

Separate, source/build-validated preset for the production V1.0 and V2 GPIO map
(RP2350B / RP2354B, 48 GPIO, 2 MiB flash). Physical operation is not validated.
Pre-V1.0 prototypes have different limit/aux wiring and are not covered.
The active configuration and all existing board pinouts remain unchanged.

## Pinout

All numbers below are **MCU GPIO numbers**, not connector pin numbers.

| Role | GPIO, in X/Y/Z/A/B order where applicable |
|---|---|
| STEP | 17, 18, 19, 20, 21 |
| DIR | 12, 13, 14, 15, 16 |
| Axis enable, digital outputs | 22, 23, 24, 25, 26 |
| Limits X/Y/Z/A/B | 6, 5, 4, 3, 2 |
| Probe / door-alarm | 7 / 8 |
| Cycle Start / Feed Hold / E-stop | 9 / 10 / 11 |
| Digital inputs D_IN0 / D_IN1 / D_IN2 / D_IN3 | 29 / 28 / 47 / 27 |
| Spindle enable / direction | 33 / 34 |
| Spindle PWM, 10 kHz default | 35 |
| Aux0 / Aux1 / Aux2 | 36 / 37 / 38 |
| Flood / mist | 39 / 40 |
| W5500 SPI | SPI1: RX 44, TX 43, SCK 46, CS 41, INT 42 |
| WIZ reset | Not MCU-controlled; PCB pull-up, `GP_NULL` |
| Status LED | No dedicated GPIO LED; `GP_NULL` |

The preset provides 5 step generators, 14 inputs, 12 digital outputs, and 1 PWM.
Encoders, UART, I2C, SD-card access, keypad scanning, and RGB LEDs are not enabled.
The fifth axis can also be wired as a LinuxCNC gantry motor; HAL/INI determines
which joints use the generators.

GPIO25 is **A enable**, not a Pico LED. Do not build this board as plain `pico2`:
that SDK definition uses the 30-GPIO RP2350A package and Pico-specific pins.
The dedicated `rp23cnc` SDK board header exposes all 48 GPIO and the 2 MiB flash.
The preset uses the standard 150 MHz system clock.

## Hardware selection and HAL wiring

- Fit the optional **WIZ850io with W5500**. The similar names WIZ820io and
  WIZ550io do not identify mechanically equivalent modules for this socket.
- Select D_IN3 as **digital input**, not UART1 RX, and Aux2 as **digital output**,
  not spindle PWM. Match the jumper function labels to your board revision.
- Servo alarm routing is selected on the PCB and shares the chosen control input;
  there are no extra per-axis alarm GPIO in this preset.
  Dust extraction follows the hardware spindle-enable/manual-switch circuit;
  it has no independent MCU GPIO.
- Use the board's 0–10 V / 0–5 V / PWM selection for your spindle interface.
  PWM is assigned exclusively to GPIO35.
- HAL exposes inputs and outputs by physical GPIO name, for example
  `stepgen-ninja.0.input.gp47`, `stepgen-ninja.0.output.gp33`.
  Input roles are labels in this preset: E-stop, probe, limits and feed hold
  must be connected to the corresponding LinuxCNC signals in your machine HAL.
- Check enable and input polarity against the actual drivers/switches using the
  HAL pin/inverted pin wiring. The firmware reports raw input levels and does
  not automatically assign safety functions to these inputs in the LinuxCNC profile.
  Enable outputs are independent ordinary outputs; there is no automatic
  per-axis enable mapping in this preset.

## Build without replacing the working configuration

The following Linux commands create a separate build copy. Run from the repository
root. They do not install a HAL module or flash a board. The copy is intentionally
not a symlink: it keeps the active configuration unchanged.

```sh
mkdir -p build/rp23cnc-source
cp -a firmware build/rp23cnc-source/firmware
cp -a common build/rp23cnc-source/common
cp -a hal-driver build/rp23cnc-source/hal-driver
cp configurations/RP23CNC/RP23CNC.h build/rp23cnc-source/firmware/inc/config.h

cmake -S build/rp23cnc-source/firmware -B build/rp23cnc-firmware -DBOARD=rp23cnc -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_PROFILE=linuxcnc -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build/rp23cnc-firmware --parallel

cmake -S build/rp23cnc-source/hal-driver -B build/rp23cnc-hal -DBUILD_TESTING=OFF
cmake --build build/rp23cnc-hal --target stepgen-ninja --parallel
```

Use a fresh `build/rp23cnc-source` directory. If a source directory already contains
its own build products, omit those products from the copy. The HAL header links
in the copy must point to that copy's firmware headers. If your checkout stores
regular HAL header files instead, replace the copied `hal-driver/config.h` with
the same preset too.

Firmware output:
`build/rp23cnc-firmware/stepper-ninja-rp23cnc-W5500.uf2` and matching `.elf`.
HAL output: `build/rp23cnc-hal/stepgen-ninja/stepgen-ninja.so`.
Firmware requires the Pico SDK and Arm GNU toolchain. The HAL module requires
LinuxCNC development files and libgpiod development/static files, as described
in the [installation guide](../../docs/INSTALL.md).
Build and deploy matching firmware/HAL configurations together; do not reuse a
HAL binary built for another channel count or GPIO list.

The generic HAL profile test expects the default configuration, so it is not
run against this preset. The RP23CNC firmware and HAL source are compiled
separately, and the normal configuration retains its regression suite.

## Shared code required by this board

- `SPI_PORT` is used for SPI initialization, registers and DMA requests.
- Custom GPIO output packets are packed by **output list index**, independent
  of the physical GPIO bank. This fixes outputs on GPIO33..40; low GPIO lists
  retain their existing mapping.
- `GP_NULL` skips optional GPIO LED and WIZ reset access. Existing boards with
  real GPIO assignments retain LED/reset handling.
- The `rp23cnc` CMake option selects the dedicated RP2350B SDK board header;
  default `pico` and `pico2` selections retain their existing definitions.

Recorded checks (2026-11-18): RP23CNC firmware and HAL source compile; default
Pico/W5100S, Pico 2/W5500, PicoBOB-DLX and Mach3/Pico firmware compile;
33/33 LinuxCNC/GPIO and 13/13 Mach3 portable regression tests pass.
The outdated Mach3 waypoint test was corrected to accept the configured positive
and negative limit and reject values beyond it. This changes the test only;
Mach3 trajectory/protocol behavior remains unchanged.

Validation is compilation and software testing only. No claim is made about
physical pulse timing, Ethernet stability, input polarity, spindle voltage,
or machine safety until checked on the actual board.

## Sources used for the mapping

- [Phil Barrett RP23CNC hardware](https://github.com/phil-barrett/RP23CNC/tree/23b20b9db1c1b12f40ed64462153dce8539cb6e8)
- [V1.0 schematic](https://github.com/phil-barrett/RP23CNC/blob/23b20b9db1c1b12f40ed64462153dce8539cb6e8/Schematic/V1.0%20schematic.pdf)
- [V2.02 schematic](https://github.com/phil-barrett/RP23CNC/blob/23b20b9db1c1b12f40ed64462153dce8539cb6e8/Schematic/V2.02%20schematic.pdf)
- [Manufacturer's user manual](https://github.com/phil-barrett/RP23CNC/blob/23b20b9db1c1b12f40ed64462153dce8539cb6e8/Documentation/user%20manual.pdf)
- [grblHAL RP2350B_5X map](https://github.com/grblHAL/RP2040/blob/2aee1379260043414b40874cd87ae890fb08af6c/boards/RP2350B_5X_map.h)

The grblHAL source is used as a pin reference; this preset does not implement
the grblHAL protocol or its plugins.
