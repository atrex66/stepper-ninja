# stepper-ninja

An open-source, free, high performance step/generator, quadrature encoder counter, digital input/output, pwm interface for LinuxCNC using W5100S-evb-pico or pico + W5500 module.

You definietly not need this to work with the ninja, only a W5100S-evb-pico or a normal pico + W5500 module and a cheap printerport breakout board!

![official breakout board](docs/images/sm_black_top.png)

## Features

- **communication**:

  - W5100S-evb-pico UDP Ethernet. <https://docs.wiznet.io/Product/iEthernet/W5100S/w5100s-evb-pico>
  - pico + W5500 module (need same wiring as W5100S-evb-pico)
  - poco2 + W5500 module (if you use pico2 board with 48 GPIO you can use the extra GPIO now)
  - **alpha test of the rpi4 - stepper-ninja setup.**

- **step-generator**: (max 8 with pico 1) 1Mhz per channel. pulse width set from hal pin (96nS - 6300 nS).

- **quadrature-encoder**: (max 4 with pico1) high speed, zero pulse(index) handling.

- **digital IO**: you can configure the free pins of the pico to inputs and outputs.

- **pwm**: you can configure 1 GPIO for the pwm signal (1900Hz(16bit resolution) to 1Mhz(7bit resolution)) and can configure active low or active high

- **Software**:
  - LinuxCNC HAL driver supporting multiple instances (max 4), with safety functions (timeout, data checks).

- **Open-Source**: code and docs under MIT License.

- **ready-to-ride**: w5100s-evb-pico and w5500 + pico version prebuilt uf2 in the binary directory. To install the hal driver you need to run the install.sh in the hal-driver directory.

## Contributors

- **code**: atrex66

- **testing**: Jimfong1, Galetops

## Contact

- **Discord**:

  - [stepper-ninja Discord](https://discord.gg/4MuegvNU)

## License

- The quadrature encoder PIO program uses BSD-3 license by Raspberry Pi (Trading) Ltd.
- The `ioLibrary_Driver` is licensed under the MIT License by Wiznet.