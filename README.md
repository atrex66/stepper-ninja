# stepper-ninja

An open-source, free, high performance step/generator and quadrature encoder interface for LinuxCNC using W5100S-evb-pico or pico + W5500 module.

![w5100s-evb-pico](docs/images/20250516_004009.jpg)

## Features

- **communication**:

  - W5100S-evb-pico UDP (data integrity check) Ethernet. <https://docs.wiznet.io/Product/iEthernet/W5100S/w5100s-evb-pico>
  - planned support communication over spi with raspberry-pi4
- **step-generator**: multiple channels maximum 255KHz per channel (15m/min with 0.001mm) uses PIO state machines (pulse width under development)
- **quadrature-encoder**: multiple channels 12.5MHz count rate per channel (theoretical) uses PIO state machines.
- **Software**:
  - 1-8 PIO step generators and or quadrature encoders (current config 4 stepgen + 4 encoder)
  - LinuxCNC HAL driver supporting multiple instances (max 4), with safety functions (timeout, data checks). (tested with raspberry-pi4)
- **Open-Source**: code and docs under MIT License, quadrature-encoder PIO program uses BSD-3 license.

## Contact

- **Discord**:

  - (stepper-ninja Discord)<https://discord.gg/bM2mQNCt>

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
This project is also uses code under BSD-3 license. See [LICENSE.TXT](LICENSE.TXT) /firmware/w5100s-evb-pico/src/quadrature_encoder.pio
The `ioLibrary_Driver` in `firmware/pico/ioLibrary_Driver.zip` is licensed under the MIT License by Wiznet. See [firmware/pico/ioLibrary_Driver.zip/LICENSE.txt](firmware/pico/ioLibrary_Driver.zip/LICENSE.txt).
