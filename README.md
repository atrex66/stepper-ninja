# stepper-ninja

An open-source, free, high-performance step generator, quadrature encoder counter, digital I/O, and PWM interface for LinuxCNC.

The encoder module also enables spindle-synchronized motion use cases, for example threading and other spindle-synchronous moves.

You definitely do not need the official breakout board to use stepper-ninja. A cheap printer-port breakout board is enough, and other configurations are also possible.

![official breakout board](docs/20250812_165926.jpg)

## Documentation

- [Installation Guide](docs/INSTALL.md)
- [Configuration Guide](docs/CONFIG.md)
- [IP Configuration Guide](docs/IPCONFIG.MD)
- [Make Your Own Breakout Board](docs/MAKE-YOUR-OWN-BREAKOUTBOARD.md)

## Languages

- [English](README.md)
- [Deutsch](README.de.md)
- [हिन्दी](README.hi.md)
- [Magyar](README.hu.md)
- [Português (Brasil)](README.pt-BR.md)

## Features

- **Using with single board compiters (SBC) and SPI**:
  - the stepper-ninja project now uses the kernel spi drivers and libgpiod v2 for communication, a littlebit slower
  - than the bmc2835 driver but with this change in theory no more barrier to use another SBC (single board computer) 

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

- **Breakout-board v1.0 - Digital version**: 16 optically isolated inputs, 8 optically isolated outputs, 4 step generators (differential output), 2 high-speed encoder inputs, 2 12-bit DAC outputs (unipolar).

- **step-generator**: max 8 with pico 1, max 12 with pico2. 1 MHz per channel. Pulse width is set from a HAL pin (96 ns - 6300 ns with a 125 MHz pico, 60 ns - 4000 ns with a 200 MHz pico).

- **quadrature-encoder**: max 8 with pico1, max 12 with pico2. High speed, zero-pulse handling, velocity estimation for low-resolution encoders, and spindle-synchronized motion support.

- **digital I/O**: you can configure the free pins of the pico as inputs and outputs.

- **PWM**: you can configure up to 16 GPIOs for PWM output (1900 Hz with 16-bit resolution up to 1 MHz with 7-bit resolution), and you can choose active-low or active-high behavior.

- **Software**:
  - LinuxCNC HAL driver supporting multiple instances (max 4), with safety functions (timeout, data checks).

- **Open-Source**: code and docs under MIT License.

- **ready-to-run**: prebuilt breakout board UF2 images and the HAL driver are in the binary directory. To install the HAL driver, copy stepgen-ninja.so to your local hal-driver directory.

- **supporters**: all active sponsors at or above $15 get access to the private breakout board repository for personal use, including production files such as Gerbers, BOM, and component positions.

- **extra I/O**: the I/O expander for the breakout board supports up to 64 inputs and 32 outputs total (with 3 expanders, 24 V optically isolated).

- **new breakout boards**: type 2 has 96 optically isolated inputs and 32 outputs. Type 3 has 4 high-speed encoders and 4 bipolar analog outputs.

- **user breakout board code**: breakoutboard_user.c and breakoutboard_hal_user.c were added to the modules directory for makers to make integration easier.


## Contributors

- **code**: atrex66, pippin88

- **testing**: Jimfong1, Griletos

- **supporters**: Griletos, Cofhal, Sunhapas

## Discord

- Request a fresh Discord invite in Discussions.

## License

- The quadrature encoder PIO program uses BSD-3 license by Raspberry Pi (Trading) Ltd.
- The `ioLibrary_Driver` is licensed under the MIT License by Wiznet.