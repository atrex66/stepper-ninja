# Stepper-Ninja कॉन्फ़िगरेशन गाइड

नए बोर्ड कॉन्फ़िगरेशन और I²C I/O प्रोटोटाइप [test branch](https://github.com/atrex66/stepper-ninja/tree/test) पर उपलब्ध हैं। वास्तविक हार्डवेयर पर जाँच अभी बाकी है।

यह गाइड `config.h` में उपयोग होने वाले मुख्य `#define` विकल्पों और उनके उपयोग की व्याख्या करती है।

Stepper-Ninja में मिलते-जुलते configuration headers यहां रखे गए हैं:

- `firmware/inc/config.h`
- `hal-driver/config.h`

सामान्य उपयोग में `hal-driver/config.h`, `firmware/inc/config.h` का symlink होना चाहिए, अलग से रखी गई कॉपी नहीं।

अगर symlinks गायब हों, तो `hal-driver` डायरेक्टरी में यह चलाएँ:

```bash
./make_symlinks.sh
```

यह script HAL driver द्वारा उपयोग किए जाने वाले shared symlinks फिर से बनाती है।

---

## कौन-सी फ़ाइल संपादित करें?

- Pico firmware behavior बदलने के लिए `firmware/inc/config.h` संपादित करें।
- `hal-driver/config.h` को उसी फ़ाइल का symlink होना चाहिए।
- यदि `hal-driver/config.h` symlink की जगह सामान्य फ़ाइल है, तो `hal-driver/make_symlinks.sh` चलाएँ।

ध्यान दें: यदि `breakout_board` का मान `0` नहीं है, तो कुछ मान `footer.h` में अपने-आप override हो जाते हैं।

---

## नेटवर्क डिफ़ॉल्ट

ये values startup network configuration तय करती हैं, जो flash में तब तक रहती हैं जब तक serial terminal से नहीं बदली जातीं।

| Define | अर्थ |
|---|---|
| `DEFAULT_MAC` | डिफ़ॉल्ट Ethernet MAC address |
| `DEFAULT_IP` | डिफ़ॉल्ट static IP address |
| `DEFAULT_PORT` | HAL driver द्वारा उपयोग किया जाने वाला UDP port |
| `DEFAULT_GATEWAY` | डिफ़ॉल्ट gateway |
| `DEFAULT_SUBNET` | डिफ़ॉल्ट subnet mask |
| `DEFAULT_TIMEOUT` | communication timeout, microseconds में |

यदि आप बाद में serial terminal के `ipconfig` commands का उपयोग करते हैं, तो runtime values flash में stored defaults को override कर सकती हैं।

---

## बोर्ड चयन

`breakout_board` hardware layout चुनता है।

| Value | अर्थ |
|---|---|
| `0` | `config.h` से custom pin mapping |
| `1` | Stepper-Ninja breakout board |
| `2` | IO-Ninja breakout board |
| `3` | Analog-Ninja breakout board |
| `100` | BreakoutBoard100 |

जब `breakout_board` का मान `0` से बड़ा होता है, तो `footer.h` कई custom pin defines को board-specific values से बदल देता है।

### अतिरिक्त बोर्ड कॉन्फ़िगरेशन

- [Expatria PicoBOB Deluxe A3](../configurations/PicoBOB-DLX/picoBOB-DLX.h): W5500 RESET/INT, LED और PWM पिन का संशोधित विन्यास।
- [Phil Barrett RP23CNC V1.0/V2](../configurations/RP23CNC/README.md) (अंग्रेज़ी): पाँच अक्ष, 14 इनपुट, 12 आउटपुट, एक स्पिंडल PWM और वैकल्पिक WIZ850io/W5500। `-DBOARD=rp23cnc -DWIZCHIP_TYPE=W5500` उपयोग करें; `BOARD=pico2` अलग 30-GPIO MCU चुनता है।

ये टेम्पलेट `breakout_board=0` उपयोग करते हैं; नए संख्यात्मक बोर्ड ID नहीं हैं। चुना टेम्पलेट `firmware/inc/config.h` में कॉपी करें और समान कॉन्फ़िगरेशन से firmware तथा HAL दोबारा बनाएँ। पिन और वैकल्पिक `GP_NULL` LED/reset सेटिंग बोर्ड-विशिष्ट हैं।

---

## Motion channels

ये defines बताती हैं कि कितने channels हैं और वे कौन-से pins उपयोग करते हैं।

| Define | अर्थ |
|---|---|
| `stepgens` | step-generator channels की संख्या |
| `stepgen_steps` | step output pins |
| `stepgen_dirs` | direction output pins |
| `step_invert` | हर channel के लिए step inversion |
| `encoders` | encoder channels की संख्या |
| `enc_pins` | हर quadrature encoder pair का पहला pin |
| `enc_index_pins` | encoder index pins |
| `enc_index_active_level` | हर index input का active level |

हर encoder के लिए `enc_pins` दो GPIO इस्तेमाल करता है: configured pin पर `A` और अगले pin पर `B`।

---

## Encoder mode

Firmware दो encoder PIO implementations सपोर्ट करती है:

| Define | अर्थ |
|---|---|
| `ENCODER_PIO_LEGACY` | पुराना quadrature counter PIO |
| `ENCODER_PIO_SUBSTEP` | नया substep-aware PIO |
| `encoder_pio_version` | कौन-सा encoder PIO compile होगा |

सामान्य सेटिंग:

```c
#define encoder_pio_version ENCODER_PIO_SUBSTEP
```

पुराने encoder PIO पर वापस जाने के लिए:

```c
#define encoder_pio_version ENCODER_PIO_LEGACY
```

वर्तमान behavior:

- Encoder velocity estimation HAL driver में होती है।
- Legacy mode में firmware हर cycle का encoder count delta भेजता है।
- Substep mode में firmware raw encoder count data भेजता है, लेकिन velocity output HAL side पर बनती है।

आप build command से mode override भी कर सकते हैं:

```bash
CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY' cmake -S firmware -B build -DWIZCHIP_TYPE=W5500
```

---

## Digital inputs और outputs

| Define | अर्थ |
|---|---|
| `in_pins` | input GPIO list |
| `in_pullup` | प्रति input pull-up enable |
| `out_pins` | output GPIO list |

ये values केवल custom mode में उपयोग होती हैं, या तब जब चुना गया breakout board इन्हें override नहीं करता।

---

## कॉन्फ़िगर करने योग्य डिजिटल I²C I/O (प्रोटोटाइप)

कस्टम **LinuxCNC** कॉन्फ़िगरेशन (`breakout_board=0`) के लिए वैकल्पिक सामान्य डिजिटल I/O। प्रकार: `SN_IO_MCP23008`, `SN_IO_MCP23017`, `SN_IO_TCA9535`, `SN_IO_TCA9555`। स्थिर breakout boards का मौजूदा कोड और `io_expanders` का अर्थ नहीं बदलता। इस प्रोटोटाइप में Mach3 समर्थित नहीं है।

| Define | अर्थ |
|---|---|
| `I2C_IO_ENABLE` | मॉड्यूल चालू करें; डिफ़ॉल्ट `0` |
| `I2C_IO_BUS_COUNT` | चालू होने पर 1–2 बस; डिफ़ॉल्ट `0` |
| `I2C_IO_BUSES` | बस विवरणों की सूची; डिफ़ॉल्ट `{}` |
| `I2C_IO_DEVICE_COUNT` | कुल 1–2 डिवाइस; डिफ़ॉल्ट `0` |
| `I2C_IO_DEVICES` | डिवाइस विवरणों की सूची; डिफ़ॉल्ट `{}` |

सेटिंग `#include "footer.h"` से **पहले** रखें। Footer `#ifndef` से अनुपस्थित डिफ़ॉल्ट भरता है; पुराने कॉन्फ़िगरेशन में नया खंड आवश्यक नहीं और अतिरिक्त बस चालू नहीं होती। Firmware और HAL समान सेटिंग से दोबारा बनाएँ।

उदाहरण: एक TCA9555, बिट 0–7 इनपुट, 8–15 आउटपुट, हर 10 ms पर polling:

```c
#define I2C_IO_ENABLE 1
#define I2C_IO_BUS_COUNT 1
#define I2C_IO_BUSES {{0, GP00, GP01, 400000, 1000}}
#define I2C_IO_DEVICE_COUNT 1
#define I2C_IO_DEVICES { \
    {SN_IO_TCA9555, 0, 0x20, 0x00ff, 0, 0, 0, 0, 10} \
}
```

बस फ़ील्ड का क्रम: controller (0/1), SDA GPIO, SCL GPIO, आवृत्ति Hz में (अधिकतम 400000), प्रत्येक SDK transfer का timeout µs में (1–10000)। डिवाइस फ़ील्ड: प्रकार, बस-सूची index, 7-bit पता (0x20–0x27), direction mask (`1=input`), pull-up mask, input inversion, output inversion, startup/disconnect के तार्किक output स्तर, polling अवधि ms में (1–60000)।

उदाहरण के GPIO0/1 खाली होने चाहिए: STEP/DIR, encoder/index, native I/O, PWM, keyboard, LED या communication से टकराव नहीं होना चाहिए। [पूरा I/O उदाहरण](../configurations/I2C-IO/config.h) अपने बोर्ड के अनुसार बदलें।

- अधिकतम दो डिवाइस और 32 native digital outputs। MCP23008 सहित प्रत्येक डिवाइस 16-bit स्थान लेता है। दोनों native GPIO input banks और packet format बने रहते हैं।
- MCP23008 के mask 8-bit, अन्य 16-bit हैं। नए पथ में MCP23017 बिट 7/15 इनपुट नहीं हो सकते (वर्तमान datasheet के अनुसार GPA7/GPB7 केवल आउटपुट हैं); पुराने बोर्डों का व्यवहार नहीं बदलता।
- TCA pull-up mask शून्य रखें। TCA9535 में internal I/O pull-ups नहीं हैं; TCA9555 के स्थिर input pull-ups इस फ़ील्ड से बंद नहीं होते।
- HAL नाम: `stepgen-ninja.0.expander.0.input.0`, `input.0-not`, `output.8`, `online`, `valid`। केवल निर्धारित दिशा export होती है: भौतिक input/status HAL_OUT और output command HAL_IN।
- बस transfer step/encoder IRQ के बाहर चलते हैं। विफलता पर `online`/`valid` साफ होते हैं; input में आखिरी सफल sample बना रहता है। खराब बस पर disconnect output स्तर लिखे जाने की गारंटी नहीं है।

[विवरण, सीमाएँ और परीक्षण](I2C-IO.en.md) (अंग्रेज़ी)। वास्तविक I²C हार्डवेयर परीक्षण अभी बाकी है।

---

## PWM options

| Define | अर्थ |
|---|---|
| `use_pwm` | PWM support enable करता है |
| `pwm_count` | PWM outputs की संख्या |
| `pwm_pin` | PWM output pins |
| `pwm_invert` | प्रति channel PWM invert |
| `default_pwm_frequency` | डिफ़ॉल्ट PWM frequency |
| `default_pwm_maxscale` | HAL scaling limit |
| `default_pwm_min_limit` | minimum PWM output limit |

`use_pwm` को `1` केवल तब सेट करें जब आपकी hardware के लिए PWM count और pins सही हों।

---

## Raspberry Pi SPI mode

| Define | अर्थ |
|---|---|
| `raspberry_pi_spi` | Wiznet Ethernet के बजाय Raspberry Pi से SPI link |
| `RASPI_INT_OUT_GPIO` | Raspberry Pi के लिए interrupt/status pin |
| `RASPI_SPI_CS_GPIO` | Raspberry Pi के लिए SPI chip-select GPIO |
| `raspi_inputs` | Raspberry Pi को दिखने वाले inputs |
| `raspi_input_pullups` | उन inputs के pull-up settings |
| `raspi_outputs` | Raspberry Pi द्वारा नियंत्रित outputs |

जब `raspberry_pi_spi` का मान `0` होता है, firmware Wiznet Ethernet path का उपयोग करती है।

---

## Timing defaults

| Define | अर्थ |
|---|---|
| `default_pulse_width` | default step pulse width, nanoseconds में |
| `default_step_scale` | default steps per unit |
| `use_timer_interrupt` | step command ring buffer और timer-driven step output enable करता है |

`use_timer_interrupt` step commands को buffer करके PC-to-Pico jitter कम कर सकता है।

---

## footer.h में नियंत्रित defines

कुछ महत्वपूर्ण defines `footer.h` में रहती हैं क्योंकि वे board या platform पर निर्भर हैं।

| Define | अर्थ |
|---|---|
| `use_stepcounter` | quadrature encoder path के बजाय step counter उपयोग करता है |
| `debug_mode` | Raspberry Pi communication के लिए extra debug behavior |
| `max_statemachines` | कुल PIO state machines की derived संख्या |
| `pico_clock` | firmware द्वारा उपयोग की जाने वाली Pico system clock |

इनको बिना timing और PIO allocation के प्रभाव समझे न बदलें।

---

## `config.h` के बाहर के build options

कुछ उपयोगी options CMake में चुनी जाती हैं, `config.h` में नहीं।

| Option | अर्थ |
|---|---|
| `-DWIZCHIP_TYPE=W5100S` | W5100S के लिए build |
| `-DWIZCHIP_TYPE=W5500` | W5500 के लिए build |
| `-DBOARD=pico` | Pico के लिए build |
| `-DBOARD=pico2` | Pico2 के लिए build |
| `-DBOARD=rp23cnc` | 48-GPIO RP2350B वाला RP23CNC (संबंधित टेम्पलेट और W5500 आवश्यक) |
| `-DSTEPPER_NINJA_PROFILE=linuxcnc` / `mach3-3axis` | Host profile: `linuxcnc` (डिफ़ॉल्ट) या `mach3-3axis` (स्थिर पिन) |
| `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` | execution से पहले firmware को SRAM में copy करता है |

उदाहरण:

```bash
cmake -S firmware -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_RUN_FROM_RAM=ON
```

Mach3 प्रोटोटाइप अपना स्थिर तीन-अक्ष profile उपयोग करता है। [Mach3 विवरण](MACH3.en.md) और [build निर्देश](MACH3-BUILD.en.md) देखें (अंग्रेज़ी)। Pico firmware Windows पर भी बनाया जा सकता है।

---

## Recommended workflow

1. `breakout_board` से board चुनें।
2. Custom mode में `stepgens`, `encoders` और pin lists सेट करें।
3. `encoder_pio_version` चुनें।
4. PWM केवल तभी enable करें जब hardware को आवश्यकता हो।
5. Firmware और HAL driver config files को aligned रखें।
6. हर महत्वपूर्ण config change के बाद firmware और HAL driver को rebuild करें।

Runtime network settings के लिए [IPCONFIG.hi.md](IPCONFIG.hi.md) भी देखें।
