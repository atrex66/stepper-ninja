# stepper-ninja
An open-source, free step/generator and quadrature encoder interface for LinuxCNC using W5100S-evb-pico.

## Features
- **communication**: 
  - W5100S-evb-pico UDP (data integrity check) Ethernet. https://docs.wiznet.io/Product/iEthernet/W5100S/w5100s-evb-pico
  - planned support communication over spi with raspberry-pi4
- **step-generator**: multiple channels fixed (1.28uS) pulse width maximum 255KHz per channel (15m/min with 0.001mm) uses PIO state machines 
- **quadrature-encoder**: multiple channels 12.5MHz count rate per channel (theoretical) uses PIO state machines.
- **Software**:
  - 1-8 PIO step generators and or quadrature encoders (current config 4 stepgen + 4 encoder)
  - LinuxCNC HAL driver supporting multiple instances (max 4), with safety functions (timeout, data checks). (tested with raspberry-pi4)
- **Open-Source**: code and docs under MIT License, quadrature-encoder PIO program uses BSD-3 license.

## License
This project is licensed under the MIT License. See [LICENSE](LICENSE).
The `ioLibrary_Driver` in `firmware/pico/ioLibrary_Driver.zip` is licensed under the MIT License by Wiznet. See [firmware/pico/ioLibrary_Driver.zip/LICENSE.txt](firmware/pico/ioLibrary_Driver.zip/LICENSE.txt).
