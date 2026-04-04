# stepper-ninja के लिए अपना खुद का Breakout Board बनाएं

यह गाइड बताती है कि stepper-ninja के लिए एक कस्टम breakout board कैसे डिज़ाइन और इंटीग्रेट करें — बिजली की योजना बनाने से लेकर firmware/HAL इंटीग्रेशन और LinuxCNC ब्रिंग-अप तक।

## 1. आप क्या बना रहे हैं

इस प्रोजेक्ट में कस्टम बोर्ड के दो सॉफ़्टवेयर पक्ष होते हैं:

- **Pico/Pico2 पर Firmware साइड**:
  - फ़ाइल पैटर्न: `firmware/modules/breakoutboard_<ID>.c`
  - फिज़िकल हार्डवेयर (GPIO, I2C expanders, DAC, सेफ़्टी स्टेट) को संभालता है।
  - रॉ बोर्ड स्टेट को पैकेट फ़ील्ड में मैप करता है।

- **LinuxCNC HAL साइड**:
  - फ़ाइल पैटर्न: `hal-driver/modules/breakoutboard_hal_<ID>.c`
  - HAL पिन एक्सपोर्ट करता है और पैकेट फ़ील्ड को उन पिनों पर मैप करता है।
  - HAL आउटपुट को वापस firmware को भेजे जाने वाले पैकेट फ़ील्ड में पैक करता है।

दोनों पक्षों का चयन `breakout_board` से होता है — `firmware/inc/config.h` और `hal-driver/config.h` में।

## 2. आवश्यक शर्तें

- काम करने वाला stepper-ninja बिल्ड एनवायरनमेंट।
- Pico SDK इंस्टॉल्ड (या `firmware/CMakeLists.txt` से उपयोग के योग्य)।
- HAL मॉड्यूल बिल्ड के लिए LinuxCNC dev हेडर/टूल्स इंस्टॉल्ड।
- रेफरेंस के लिए एक मौजूदा बोर्ड की कॉपी:
  - `firmware/modules/breakoutboard_1.c`
  - `hal-driver/modules/breakoutboard_hal_1.c`

## 3. हार्डवेयर प्लानिंग चेकलिस्ट

कोडिंग शुरू करने से पहले इन बातों को तय करें:

- **पावर रेल और लॉजिक लेवल**:
  - Pico GPIO के साथ 3.3V लॉजिक की अनुकूलता।
  - जहाँ जरूरी हो लेवल शिफ्टिंग/ऑप्टो आइसोलेशन।

- **सेफ़्टी बिहेवियर**:
  - कम्युनिकेशन टाइमआउट/डिस्कनेक्ट पर आउटपुट का व्यवहार परिभाषित करें।
  - एनेबल लाइनें fail-safe हों (मोटर एनेबल ऑफ, स्पिंडल डिसेबल, एनालॉग आउटपुट ज़ीरो)।

- **I/O बजट**:
  - डिजिटल इनपुट/आउटपुट की संख्या।
  - एनालॉग आउटपुट और DAC का प्रकार।
  - एनकोडर चैनल और इंडेक्स बिहेवियर।

- **बस/पेरिफेरल आवंटन**:
  - expanders/DAC के लिए I2C बस/पिन।
  - SPI/WIZnet वायरिंग और reset/interrupt लाइनें।

- **सिग्नल इंटीग्रिटी**:
  - step/dir ट्रेस साफ़ और छोटे रखें।
  - जहाँ boot-time स्टेट मायने रखता हो, pullup/pulldown जोड़ें।

## 4. नया Board ID चुनें

कोई अप्रयुक्त इंटीजर ID चुनें, जैसे `42`।

यह ID इन सब जगह उपयोग होगी:

- `firmware/modules/breakoutboard_42.c`
- `hal-driver/modules/breakoutboard_hal_42.c`
- कॉन्फ़िग हेडर में `#define breakout_board 42`
- firmware और HAL बिल्ड पाथ में चयन ब्रांच

## 5. Firmware साइड इंटीग्रेशन

### 5.1 Firmware मॉड्यूल बनाएं

1. टेम्पलेट कॉपी करें:

```bash
cp firmware/modules/breakoutboard_user.c firmware/modules/breakoutboard_42.c
```

2. `firmware/modules/breakoutboard_42.c` में बदलें:

```c
#if breakout_board == 255
```

को:

```c
#if breakout_board == 42
```

3. आवश्यक callbacks लागू करें:

- `breakout_board_setup()`:
  - बोर्ड पर GPIO/I2C/SPI डिवाइस initialize करें।
  - expanders और DAC को probe और configure करें।

- `breakout_board_disconnected_update()`:
  - सभी आउटपुट को safe state में करें।
  - DAC आउटपुट ज़ीरो या safe bias पर सेट करें।

- `breakout_board_connected_update()`:
  - प्राप्त पैकेट से आउटपुट हार्डवेयर पर लागू करें।
  - हार्डवेयर से इनपुट स्नैपशॉट रीफ्रेश करें।

- `breakout_board_handle_data()`:
  - आउटबाउंड पैकेट फ़ील्ड भरें (`tx_buffer->inputs[]` आदि)।
  - इनबाउंड फ़ील्ड कंज्यूम करें (`rx_buffer->outputs[]`, analog payloads)।

### 5.2 Firmware बिल्ड में अपना मॉड्यूल जोड़ें

`firmware/CMakeLists.txt` एडिट करें और `add_executable(...)` में नई सोर्स फ़ाइल जोड़ें:

```cmake
modules/breakoutboard_42.c
```

### 5.3 Firmware config footer में बोर्ड मैक्रो परिभाषित करें

`firmware/inc/footer.h` एडिट करें और एक नया ब्लॉक जोड़ें:

```c
#if breakout_board == 42
    // आपकी पिन मैपिंग और बोर्ड स्पेसिफिक मैक्रो
    // उदाहरण:
    // #define I2C_SDA 26
    // #define I2C_SCK 27
    // #define I2C_PORT i2c1
    // #define MCP23017_ADDR 0x20
    // #define ANALOG_CH 2
    // #define MCP4725_BASE 0x60

    // stepgen/encoder/in/out/pwm मैक्रो को ज़रूरत के अनुसार override करें
#endif
```

ID 1, 2, 3, 100 के मौजूदा ब्लॉक शैली संदर्भ के रूप में उपयोग करें।

### 5.4 Firmware config में बोर्ड चुनें

`firmware/inc/config.h` एडिट करें:

```c
#define breakout_board 42
```

## 6. HAL ड्राइवर साइड इंटीग्रेशन

### 6.1 HAL बोर्ड मॉड्यूल बनाएं

1. टेम्पलेट कॉपी करें:

```bash
cp hal-driver/modules/breakoutboard_hal_user.c hal-driver/modules/breakoutboard_hal_42.c
```

2. `hal-driver/modules/breakoutboard_hal_42.c` में बदलें:

```c
#if breakout_board == 255
```

को:

```c
#if breakout_board == 42
```

3. लागू करें:

- `bb_hal_setup_pins(...)`:
  - सभी आवश्यक पिन एक्सपोर्ट करें (`hal_pin_bit_newf`, `hal_pin_float_newf` आदि)।
  - डिफ़ॉल्ट वैल्यू इनिशियलाइज़ करें।

- `bb_hal_process_recv(...)`:
  - `rx_buffer->inputs[...]` और अन्य फ़ील्ड को HAL आउटपुट पिनों में अनपैक करें।

- `bb_hal_process_send(...)`:
  - HAL कमांड पिनों को `tx_buffer->outputs[...]` और analog फ़ील्ड में पैक करें।

### 6.2 HAL बोर्ड include रजिस्टर करें

`hal-driver/stepper-ninja.c` में बोर्ड-सिलेक्शन include सेक्शन एडिट करें और जोड़ें:

```c
#elif breakout_board == 42
#include "modules/breakoutboard_hal_42.c"
```

### 6.3 HAL config में Board ID चुनें

`hal-driver/config.h` एडिट करें:

```c
#define breakout_board 42
```

Firmware और HAL का `breakout_board` ID एक समान रखें।

## 7. पैकेट मैपिंग नियम (महत्वपूर्ण)

दोनों पक्षों के बीच एक स्थिर कॉन्ट्रैक्ट उपयोग करें।

- **Firmware साइड** machine-to-host डेटा लिखता है:
  - `tx_buffer->inputs[0..3]`
  - encoder feedback/jitter फ़ील्ड

- **HAL साइड** इन्हें `bb_hal_process_recv()` में HAL पिनों में पढ़ता है।

- **HAL साइड** host-to-firmware कमांड लिखता है:
  - `tx_buffer->outputs[0..1]`
  - analog और वैकल्पिक कंट्रोल फ़ील्ड

- **Firmware साइड** `rx_buffer` से इन कमांड को पढ़कर `breakout_board_connected_update()` में लागू करता है।

यहाँ कोई भी मिसमैच swapped pins या dead I/O का कारण बनता है। डेवलपमेंट के समय comments में एक सिंगल मैपिंग टेबल रखें।

## 8. Firmware बिल्ड और फ्लैश करें

रिपॉजिटरी रूट से:

```bash
cd firmware
cmake -S . -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500
cmake --build build -j"$(nproc)"
ls build/*.uf2
```

Pico v1 के लिए `-DBOARD=pico` सेट करें।

फ्लैश करें:

1. Pico लगाते समय BOOTSEL दबाएं।
2. जनरेटेड `.uf2` को माउंट हुई RPI-RP2 ड्राइव पर कॉपी करें।
3. रिबूट करें और init लॉग verify करने के लिए सीरियल आउटपुट खोलें।

## 9. LinuxCNC HAL मॉड्यूल बिल्ड और इंस्टॉल करें

रिपॉजिटरी रूट से:

```bash
cd hal-driver
cmake -S . -B build-cmake
cmake --build build-cmake --target stepper-ninja
sudo cmake --install build-cmake --component stepper-ninja
```

यह `stepper-ninja.so` को आपकी LinuxCNC मॉड्यूल डायरेक्टरी में इंस्टॉल करता है।

## 10. LinuxCNC HAL ब्रिंग-अप (न्यूनतम)

आपकी HAL फ़ाइल में स्केलेटन उदाहरण:

```hal
loadrt stepper-ninja ip_address="192.168.0.177:8888"
addf stepper-ninja.0.watchdog-process servo-thread
addf stepper-ninja.0.process-send     servo-thread
addf stepper-ninja.0.process-recv     servo-thread

# उदाहरण बोर्ड I/O nets (नाम bb_hal_setup_pins के अनुसार बदलते हैं)
# net estop-in      stepper-ninja.0.inputs.0     => some-signal
# net coolant-out   some-command                 => stepper-ninja.0.outputs.0
```

फिर `halshow`/`halcmd show pin stepper-ninja.0.*` से verify करें।

## 11. कमीशनिंग प्रक्रिया (अनुशंसित)

इसी क्रम में चलाएं:

1. **केवल पावर टेस्ट**:
   - रेल validate करें, ओवरहीटिंग नहीं, अत्यधिक idle करंट नहीं।

2. **बस डिस्कवरी टेस्ट**:
   - firmware prints में अपेक्षित I2C एड्रेस की पुष्टि करें।

3. **डिस्कनेक्ट सेफ़्टी टेस्ट**:
   - LinuxCNC कम्युनिकेशन बंद करें।
   - आउटपुट और analog चैनल safe state में जाएं, यह confirm करें।

4. **इनपुट पोलैरिटी टेस्ट**:
   - प्रत्येक फिज़िकल इनपुट toggle करें और HAL पिन स्टेट match करें।
   - यदि एक्सपोर्ट किए हों तो `-not` complements validate करें।

5. **आउटपुट टेस्ट**:
   - प्रत्येक HAL आउटपुट पिन toggle करें और फिज़िकल आउटपुट verify करें।

6. **एनकोडर टेस्ट**:
   - धीरे और तेज़ घुमाएं, काउंट दिशा और index हैंडलिंग verify करें।

7. **मोटर डिस्कनेक्ट के साथ मोशन टेस्ट**:
   - पहले step/dir पोलैरिटी और पल्स टाइमिंग validate करें।

8. **मोटर कनेक्ट के साथ मोशन टेस्ट**:
   - कम वेलोसिटी/एक्सेलेरेशन से शुरू करें।

## 12. Troubleshooting

- **कोई I/O रिस्पॉन्स नहीं**:
  - जाँचें कि `breakout_board` ID firmware और HAL दोनों configs में मेल खाती है।
  - जाँचें कि नई फ़ाइलें बिल्ड पाथ में शामिल हैं।

- **बिल्ड सफल लेकिन बोर्ड callbacks सक्रिय नहीं**:
  - `#if breakout_board == 42` गार्ड सही है, यह confirm करें।
  - footer/config ब्रांच मौजूद है और compile होती है, यह confirm करें।

- **Inputs shifted/गलत bit order**:
  - इनके बीच bit packing/unpacking फिर से जाँचें:
    - `firmware/modules/breakoutboard_42.c`
    - `hal-driver/modules/breakoutboard_hal_42.c`

- **रेंडम डिस्कनेक्ट बिहेवियर**:
  - `breakout_board_disconnected_update()` में timeout-safe कोड verify करें।
  - नेटवर्क स्थिरता और पैकेट watchdog वायरिंग जाँचें।

- **Analog आउटपुट क्लिपिंग/ओवरफ्लो**:
  - DAC शब्द पैक करने से पहले HAL वैल्यू क्लैंप करें।
  - DAC रिज़ॉल्यूशन के विरुद्ध स्केलिंग confirm करें।

## 13. वर्शन कंट्रोल की अनुशंसाएं

प्रत्येक कस्टम बोर्ड change set के लिए इस क्रम में commit करें:

1. Firmware मॉड्यूल + footer/config अपडेट।
2. HAL मॉड्यूल + include/config अपडेट।
3. LinuxCNC HAL config अपडेट।
4. वायरिंग और पिन मैप के लिए दस्तावेज़।

बोर्ड स्पेसिफिक markdown फ़ाइल में रखें:

- Schematic लिंक
- पिन मैप टेबल
- I2C एड्रेस मैप
- Safe-state नीति
- HAL पिन नेमिंग कॉन्ट्रैक्ट

## 14. त्वरित फ़ाइल चेकलिस्ट

नए Board ID के लिए इन सभी फ़ाइलों को टच करें:

- `firmware/modules/breakoutboard_42.c`
- `firmware/CMakeLists.txt`
- `firmware/inc/footer.h`
- `firmware/inc/config.h`
- `hal-driver/modules/breakoutboard_hal_42.c`
- `hal-driver/stepper-ninja.c`
- `hal-driver/config.h`
- (वैकल्पिक) `docs/your-board-name.md`

यदि ये सब पूर्ण हैं और IDs सुसंगत हैं, तो आपका कस्टम breakout board इंटीग्रेशन आमतौर पर सरल होता है।
