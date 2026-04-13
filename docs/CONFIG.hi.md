# Stepper-Ninja कॉन्फ़िगरेशन गाइड

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
| `raspi_int_out` | Raspberry Pi के लिए interrupt/status pin |
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
| `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` | execution से पहले firmware को SRAM में copy करता है |

उदाहरण:

```bash
cmake -S firmware -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_RUN_FROM_RAM=ON
```

---

## Recommended workflow

1. `breakout_board` से board चुनें।
2. Custom mode में `stepgens`, `encoders` और pin lists सेट करें।
3. `encoder_pio_version` चुनें।
4. PWM केवल तभी enable करें जब hardware को आवश्यकता हो।
5. Firmware और HAL driver config files को aligned रखें।
6. हर महत्वपूर्ण config change के बाद firmware और HAL driver को rebuild करें।

Runtime network settings के लिए [IPCONFIG.hi.md](IPCONFIG.hi.md) भी देखें।