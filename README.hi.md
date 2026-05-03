# stepper-ninja

LinuxCNC के लिए एक open-source, free और high-performance step generator, quadrature encoder counter, digital I/O और PWM interface.

Encoder module मुख्य spindle-synchronized motions को भी संभव बनाता है, जैसे threading और अन्य spindle-synchronous moves.

Stepper-ninja उपयोग करने के लिए आपको official breakout board की आवश्यकता नहीं है। एक सस्ता printer-port breakout board भी पर्याप्त हो सकता है, और अन्य configurations भी संभव हैं।

![official breakout board](docs/20250812_165926.jpg)

## Documentation

- [Installation Guide](docs/INSTALL.hi.md)
- [Configuration Guide](docs/CONFIG.hi.md)
- [IP Configuration Guide](docs/IPCONFIG.hi.md)
- [Make Your Own Breakout Board](docs/MAKE-YOUR-OWN-BREAKOUTBOARD.hi.md)

## Languages

- [English](README.md)
- [Deutsch](README.de.md)
- [हिन्दी](README.hi.md)
- [Magyar](README.hu.md)
- [Português (Brasil)](README.pt-BR.md)

## Features

- **Supported configurations**:

  - W5100S-evb-pico UDP Ethernet
  - W5500-evb-pico
  - W5100S-evb-pico2
  - W5500-evb-pico2
  - pico + W5500 module
  - pico2 + W5500 module
  - pico + Raspberry Pi4 via SPI
  - pico2 + Raspberry Pi4 via SPI
  - pico + PI ZERO2W via SPI
  - pico2 + PI ZERO2W via SPI
  - official Stepper-Ninja breakout board

- **Breakout-board v1.0 - Digital version**: 16 optically isolated inputs, 8 optically isolated outputs, 4 step generators, 2 high-speed encoder inputs, 2 unipolar 12-bit DAC outputs.

- **step-generator**: Pico 1 पर अधिकतम 8, Pico2 पर अधिकतम 12. प्रति channel 1 MHz तक. Pulse width HAL pin से सेट की जा सकती है।

- **quadrature-encoder**: Pico1 पर अधिकतम 8, Pico2 पर अधिकतम 12. High speed counting, index handling, velocity estimation, और spindle-synchronized motion support.

- **digital I/O**: Pico के free pins को inputs और outputs के रूप में configure किया जा सकता है।

- **PWM**: 16 GPIO तक PWM output के लिए configure किए जा सकते हैं।

- **Software**: LinuxCNC HAL driver multiple instances और safety functions के साथ।

- **Open-Source**: code और docs MIT License के अंतर्गत।

## License

- Quadrature encoder PIO program Raspberry Pi (Trading) Ltd. की BSD-3 license का उपयोग करता है।
- `ioLibrary_Driver` Wiznet की MIT License के अंतर्गत है।