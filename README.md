# stepper-ninja

An open-source, free, high-performance step generator, digital I/O, and PWM interface for LinuxCNC and Mach3.

**NEW — first version of the 3-axis Mach3 driver (USB):**
[StepperNinja Mach3 USB package](configurations/StepperNinja-Mach3-USB).
Includes Pico/Pico 2 firmware, the Windows plugin, machine profiles, and
[setup instructions](configurations/StepperNinja-Mach3-USB/README.en.md).

A separate **prototype Mach3 driver** is available for a fixed three-axis USB/UDP
firmware profile. LinuxCNC remains the default profile with flexible configuration.

You definitely do not need the official breakout board to use stepper-ninja. A cheap printer-port breakout board is enough, and other configurations are also possible.

![official breakout board](docs/20250812_165926.jpg)

## Documentation

- [Installation Guide](docs/INSTALL.md)
- [Configuration Guide](docs/CONFIG.md)
- [IP Configuration Guide](docs/IPCONFIG.MD)
- [Make Your Own Breakout Board](docs/MAKE-YOUR-OWN-BREAKOUTBOARD.md)
- [Mach3 prototype overview](docs/MACH3.en.md) ([Magyar](docs/MACH3.hu.md))
- [Mach3 build guide](docs/MACH3-BUILD.en.md) ([Magyar](docs/MACH3-BUILD.hu.md))
- [Mach3 protocol and execution reports](docs/MACH3-PROTOCOL.en.md)
- [Mach3 Feed Hold and G31](docs/MACH3-FEEDHOLD-G31.en.md)

## Languages

- [English](README.md)
- [Deutsch](README.de.md)
- [हिन्दी](README.hi.md)
- [Magyar](README.hu.md)
- [Português (Brasil)](README.pt-BR.md)

## LinuxCNC features

LinuxCNC supports quadrature encoders and spindle-synchronized motion, including threading. Encoder support is not implemented in the Mach3 driver.

- **Supported configurations**:

  - W5100S-evb-pico UDP Ethernet. <https://docs.wiznet.io/Product/iEthernet/W5100S/w5100s-evb-pico>
  - W5500-evb-pico (same as above)
  - W5100S-evb-pico2 (same as above)
  - W5500-evb-pico2 (same as above)
  - pico + W5500 module (needs the same wiring as the W5100S-evb-pico)
  - pico2 + W5500 module (if you use a pico2 board with 48 GPIO, you can use the extra GPIO now)
  - pico + Raspberry Pi4 (uses direct SPI connection, tested with the official LinuxCNC ISO, utilizes the extra GPIO from the Pi 4)
  - pico2 + Raspberry Pi4 (same as above)
  - pico + PI ZERO2W (uses SPI connection, LinuxCNC runs with linuxcncrsh, tested on Bookworm with patched kernel 6.13.2.5-rt5-v7+)
  - pico2 + PI ZERO2W (same as above, able to use the extra GPIO from the ZERO2W)
  - Stepper Ninja official breakout board (needs a pico and a W5500 module)

- **Using with single board compiters (SBC) and SPI**:
  - the stepper-ninja project now uses the kernel spi drivers and libgpiod v2 for communication, a littlebit slower
  - than the bmc2835 driver but with this change in theory no more barrier to use another SBC (single board computer) 

- **Breakout-board v1.0 - Digital version**: 16 optically isolated inputs, 8 optically isolated outputs, 4 step generators (differential output), 2 high-speed encoder inputs, 2 12-bit DAC outputs (unipolar).

- **step-generator**: max 8 with pico 1, max 12 with pico2. 1 MHz per channel. Pulse width is set from a HAL pin (96 ns - 6300 ns with a 125 MHz pico, 60 ns - 4000 ns with a 200 MHz pico).

- **quadrature-encoder**: max 8 with pico1, max 12 with pico2. High speed, zero-pulse handling, velocity estimation for low-resolution encoders, and spindle-synchronized motion support.

- **digital I/O**: you can configure the free pins of the pico as inputs and outputs.

- **PWM**: you can configure up to 16 GPIOs for PWM output (1900 Hz with 16-bit resolution up to 1 MHz with 7-bit resolution), and you can choose active-low or active-high behavior.

- **Software**:
  - LinuxCNC HAL driver supporting multiple instances (max 4), with safety functions (timeout, data checks).

- **extra I/O**: the I/O expander for the breakout board supports up to 64 inputs and 32 outputs total (with 3 expanders, 24 V optically isolated).

- **user breakout board code**: breakoutboard_user.c and breakoutboard_hal_user.c were added to the modules directory for makers to make integration easier.

- **sample KiCad breakout board**: a simple, low-cost DIY breakout board KiCad project is available in `configurations/RaspberryPi`. It connects a Pico/Pico2 to a Raspberry Pi via SPI and requires only a Pico or Pico2, a standard 40-pin GPIO header, and 2.54 mm PCB screw terminals.

- **supporters**: all active sponsors at or above $15 get access to the private breakout board repository for personal use, including production files such as Gerbers, BOM, and component positions.

- **Open-Source**: code and docs under MIT License.


## Mach3 prototype driver

The `mach3-driver/` directory contains a Mach3 SDK adapter and a portable
controller/client core. Select `STEPPER_NINJA_PROFILE=mach3-3axis` when building
the matching firmware. This profile has a fixed pinout: three STEP/DIR axes,
seven digital inputs, six digital outputs, and one spindle PWM output.

Implemented in the prototype:

- Buffered 1 ms XYZ motion and execution reports from physical STEP counts.
- Local acceleration-limited jog, homing, and dwell.
- Digital I/O, spindle RUN/DIR/PWM, and coolant outputs.
- Sessions, ACKs, duplicate-request suppression, watchdog, E-stop and limit handling.
- A Mach3 configuration dialog for IP/port, timing, polarity, and homing settings.
- Native Mach3 Feed Hold forwarding the planned braking trajectory; Cycle Start
  forwards Mach3's new startup trajectory.
- Coordinated G31 with separate contact and stopping positions, missed-contact
  detection, and controlled cancellation.

**Prototype status:** portable Linux tests cover motion, reports, and UDP behavior;
these use a substitute Mach3 planner. I have verified on Windows that USB STEP outputs and inputs work from Mach3 with the prebuilt driver and firmware.
Full Mach3 Feed Hold/G31 integration and hardware timing still need validation.
USB CDC motion transport is implemented with automatic console handover; full
Windows and physical machine validation remain pending. The Mach3 profile has no encoder; the LinuxCNC
performance and configuration features listed above do not describe its limits.
Motion protocol version 3 requires matching Mach3 driver and firmware builds.
The Pico firmware can also be built natively on Windows; no Linux machine is
required to build the firmware and Mach3 DLL. Once real Mach3 and hardware
testing has confirmed reliable operation, a release is planned with prebuilt
firmware and driver binaries. That validated release is not available yet.

[Mach3 USB configuration directory — prebuilt driver, firmware, and machine profiles](configurations/StepperNinja-Mach3-USB).

See the [English build guide](docs/MACH3-BUILD.en.md) or
[Hungarian build guide](docs/MACH3-BUILD.hu.md). The separate Mach3 profile
preserves LinuxCNC's existing flexible configuration and protocol.

## Contributors

The LinuxCNC 2.9/2.10 HAL API compatibility patch is based on code by [Frederic Müller (FredericM88)](https://github.com/FredericM88) in [linuxcnc-sim](https://github.com/FredericM88/linuxcnc-sim/tree/42ef3723da83b8ed88c123138c60437093255a22/compat/stepper-ninja), adapted for this driver under the MIT license. See the [compatibility notes](hal-driver/hal-api-compatibility.md) and [original license](hal-driver/LICENSE.hal-compat.txt).

- **code**: atrex66, pippin88

- **testing**: Jimfong1, Griletos

- **supporters**: Griletos, Cofhal, Sunhapas, milhead2, vanOverload

- **known-ninjas**: Griletos, Cofhal, Sunhapas, milhead2, vanOverload, Player

## Discord

- Request a fresh Discord invite in Discussions.

## License

- The quadrature encoder PIO program uses BSD-3 license by Raspberry Pi (Trading) Ltd.
- The `ioLibrary_Driver` is licensed under the MIT License by Wiznet.
