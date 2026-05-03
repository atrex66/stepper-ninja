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

## PIO कॉन्फ़िगरेशन और संसाधन सीमाएँ

Raspberry Pi Pico और Pico2 **PIO (Programmable I/O) ब्लॉक** का उपयोग करके step generation और encoder counting को सीधे hardware में लागू करते हैं। PIO instruction memory बहुत सीमित होने के कारण, **step generators और encoders को एक साथ उपयोग नहीं किया जा सकता** — एक को सक्षम करने से दूसरा अक्षम हो जाता है।

### RP2040 (Pico)

- 2 PIO ब्लॉक (PIO0 और PIO1), प्रत्येक में **4 state machines** और **32 instruction slots**
- Step generator PIO program: प्रति state machine ~20 instructions
- Encoder PIO program: प्रति state machine ~15–18 instructions
- **4 step generators** एक PIO ब्लॉक को पूरी तरह भर देते हैं — उसी ब्लॉक में encoders के लिए कोई जगह नहीं बचती
- **4 encoders** एक PIO ब्लॉक को पूरी तरह भर देते हैं — step generators के लिए कोई जगह नहीं बचती
- अधिकतम: **8 step generators** अथवा **8 encoders** (दोनों PIO ब्लॉक उपयोग करके), कभी मिश्रित नहीं

### RP2350 (Pico2)

- 2 PIO ब्लॉक, प्रत्येक में **4 state machines** और **32 instruction slots** (RP2040 जैसी संरचना)
- RP2350 में अतिरिक्त **PIO2** भी है जिसमें 4 और state machines हैं
- अधिकतम: **12 step generators** अथवा **12 encoders** (तीनों PIO ब्लॉक उपयोग करके)
- Step generators और encoders को अभी भी मिश्रित नहीं किया जा सकता — प्रत्येक function का PIO program ब्लॉक को पूरी तरह भर देता है

### मोड बदलना

`firmware/inc/config.h` को संपादित करें:

```c
#define stepgens 4   // step generators की संख्या (0 = केवल encoder मोड)
#define encoders 0   // encoders की संख्या (0 = केवल step generator मोड)
```

यह सेटिंग बदलने के बाद firmware को rebuild करें और Pico पर flash करें।

### Encoder PIO Variants

दो encoder implementations उपलब्ध हैं:

- **`ENCODER_PIO_SUBSTEP`** (default): smooth velocity estimation के लिए sub-step interpolation (~18 instructions)
- **`ENCODER_PIO_LEGACY`**: सरल quadrature counting, थोड़ा छोटा (~15 instructions)

Legacy encoder के साथ build करें:
```bash
CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY' cmake ..
```

---

## License

- Quadrature encoder PIO program Raspberry Pi (Trading) Ltd. की BSD-3 license का उपयोग करता है।
- `ioLibrary_Driver` Wiznet की MIT License के अंतर्गत है।