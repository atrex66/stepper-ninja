# stepper-ninja

An open-source, free, high performance step/generator, quadrature encoder counter, digital input/output, pwm interface for LinuxCNC using W5100S-evb-pico or pico + W5500 module.

You definietly not need this to work with the ninja, only a W5100S-evb-pico or a normal pico + W5500 module and a cheap printerport breakout board!
![official breakout board](docs/images/sm_black_top.png)

## Features

- **communication**:

  - W5100S-evb-pico UDP Ethernet. <https://docs.wiznet.io/Product/iEthernet/W5100S/w5100s-evb-pico>
  - pico + W5500 module (need same wiring as W5100S-evb-pico)
  - planned support communication over spi with raspberry-pi4

- **step-generator**: multiple channels 1Mhz per channel. pulse width set from hal pin (96nS - 6300 nS).

- **quadrature-encoder**: multiple channels 12.5MHz count rate per channel (theoretical).

- **Software**:
  - !UPDATE! the current configuration 4 step generator 2 encoder 3 output 1 pwm 4 input. but you can easily modifi to your own machine.
  - LinuxCNC HAL driver supporting multiple instances (max 4), with safety functions (timeout, data checks). (tested with raspberry-pi4)

- **Open-Source**: code and docs under MIT License, quadrature-encoder PIO program uses BSD-3 license.

- **ready-to-ride**: w5100s-evb-pico and w5500 + pico version prebuilt uf2 in the binary directory. To install the hal driver you need to run the install.sh in the hal-driver directory

## Contributors

- **code**: atrex66

- **testing**: Jimfong1

## Contact

- **Discord**:

  - (stepper-ninja Discord)<https://discord.gg/bM2mQNCt>

## License

The quadrature encoder PIO program uses BSD-3 license [LICENSE.TXT](LICENSE.TXT)
The `ioLibrary_Driver` is licensed under the MIT License by Wiznet.
