# Stepper-Ninja के लिए इंस्टॉलेशन और बिल्ड निर्देश

यह गाइड बताती है कि **Stepper-Ninja** को कैसे बनाएं — यह LinuxCNC के लिए Raspberry Pi Pico पर चलने वाला HAL ड्राइवर है। बिल्ड के लिए **CMake** और Unix Makefiles (`make`) का उपयोग किया जाता है। यह प्रोजेक्ट W5100S-EVB-Pico और सामान्य Pico के साथ W5500 Ethernet मॉड्यूल को सपोर्ट करता है, जिसमें 255 kHz स्टेप जनरेशन और 12.5 MHz एनकोडर काउंटिंग हासिल होती है।

Pico SDK 2.1.1, CMake 3.20.6 और GNU ARM Embedded Toolchain के साथ परीक्षण किया गया। सामान्य समस्याओं के लिए [Troubleshooting](#troubleshooting) देखें।

---

## आवश्यक शर्तें

बिल्ड करने से पहले निम्नलिखित डिपेंडेंसी इंस्टॉल करें:

1. **CMake** (संस्करण 3.15 या उससे ऊपर):

   ```bash
   sudo apt install cmake  # Debian/Ubuntu
   ```

2. **GNU ARM Embedded Toolchain**:

   ```bash
   sudo apt update
   sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi unzip
   ```

3. **Pico SDK (2.1.1)**:

   ```bash
   git clone https://github.com/raspberrypi/pico-sdk
   cd pico-sdk
   git submodule update --init
   export PICO_SDK_PATH=/path/to/pico-sdk
   ```

   `PICO_SDK_PATH` को अपनी शेल प्रोफ़ाइल (जैसे `~/.bashrc`) में जोड़ें।

4. **बिल्ड टूल्स**:

   - Linux: सुनिश्चित करें कि `make` इंस्टॉल है (`sudo apt install build-essential`)।

5. **PICOTOOL इंस्टॉल करना (वैकल्पिक)**:

   ```bash
   cd ~
   git clone https://raspberrypi/picotool
   cd picotool
   mkdir build && cd build
   cmake ..
   make
   sudo make install
   ```

---

## रिपॉजिटरी क्लोन करें

Stepper-Ninja रिपॉजिटरी क्लोन करें:

```bash
git clone https://github.com/atrex66/stepper-ninja
cd stepper-ninja
```

---

## CMake और Make से बिल्ड करना

CMake और Unix Makefiles से Stepper-Ninja बनाने के लिए ये चरण फॉलो करें।

### 1. बिल्ड डायरेक्टरी बनाएं

```bash
cd firmware/
mkdir build && cd build
```

### 2. CMake कॉन्फ़िगर करें

CMake चलाएं और WIZnet चिप टाइप (`W5100S` या `W5500`) निर्दिष्ट करें। निर्दिष्ट न करने पर डिफ़ॉल्ट `W5100S` होता है।

- **W5100S-EVB-Pico** के लिए (डिफ़ॉल्ट):

  ```bash
  cmake ..
  ```

- **W5100S-EVB-Pico2** के लिए:

  ```bash
  cmake -DBOARD=pico2 ..
  ```

- **W5500 + pico** के लिए:

  ```bash
  cmake -DWIZCHIP_TYPE=W5500 ..
  ```

- **W5500 + pico2** के लिए:

  ```bash
  cmake -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 ..
  ```

- यदि आप बूट के बाद फर्मवेयर को RAM से चलाना चाहते हैं, तो यह जोड़ें:

   ```bash
   -DSTEPPER_NINJA_RUN_FROM_RAM=ON
   ```

- यदि आप पुराना encoder PIO संस्करण बनाना चाहते हैं, तो यह compiler define जोड़ें:

   ```bash
   CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY' cmake -DWIZCHIP_TYPE=W5500 ..
   ```

- डिफ़ॉल्ट encoder mode `ENCODER_PIO_SUBSTEP` है। वर्तमान builds में encoder velocity estimation दोनों modes के लिए HAL driver में की जाती है।

- `config.h` के सभी विकल्पों की व्याख्या के लिए [CONFIG.hi.md](CONFIG.hi.md) देखें।

### 3. प्रोजेक्ट बिल्ड करें

`make` से कंपाइल करें:

```bash
make
```

यह `stepper-ninja-picoX-W5XXX.uf2` फ़ाइल बनाता है (Pico फ्लैश करने के लिए, X cmake defs पर निर्भर है)।

### 4. Pico फ्लैश करें

- Pico को BOOTSEL मोड में कनेक्ट करें (BOOTSEL दबाकर USB लगाएं)।
- अगर Pico पर पहले से अन्य फर्मवेयर है, तो flash_nuke से पुराना डेटा मिटाएं।
- `stepper-ninja-picoX-W5XXX.uf2` को Pico के mass storage डिवाइस पर कॉपी करें।
- Pico रिबूट होता है और फर्मवेयर चलने लगता है।

---

## W5500 सपोर्ट

सामान्य Pico के साथ W5500 मॉड्यूल के लिए सुनिश्चित करें:

- W5500 सही तरीके से वायर्ड हो (SPI पिन, 3.3V पावर)।
- CMake स्टेप में `-DWIZCHIP_TYPE=W5500` उपयोग करें।
- RAM से चलाने के लिए `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` जोड़ सकते हैं।
- पुराना encoder PIO चुनने के लिए `cmake` से पहले `CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY'` सेट करें।

## Pico2 सपोर्ट

W5500 मॉड्यूल के साथ Pico2 के लिए सुनिश्चित करें:

- W5500 सही तरीके से वायर्ड हो (SPI पिन, 3.3V पावर)।
- CMake स्टेप में `-DBOARD=pico2 -DWIZCHIP_TYPE=W5500` उपयोग करें।
- RAM से चलाने के लिए `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` जोड़ सकते हैं।
- पुराना encoder PIO चुनने के लिए `cmake` से पहले `CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY'` सेट करें।

---

## LinuxCNC HAL ड्राइवर इंस्टॉल करना

Stepper-Ninja को LinuxCNC के साथ जोड़ने के लिए:

1. **HAL ड्राइवर इंस्टॉल करें**:

   ```bash
   cd hal-driver
   ./install.sh
   ```

2. **HAL फ़ाइल बनाएं** (जैसे `stepper-ninja.hal`):

   ```hal
   loadrt stepgen-ninja ip_address="192.168.0.177:8888"

   addf stepgen-ninja.0.watchdog-process servo-thread
   addf stepgen-ninja.0.process-send servo-thread
   addf stepgen-ninja.0.process-recv servo-thread

   net x-pos-cmd joint.0.motor-pos-cmd => stepgen-ninja.0.stepgen.0.command
   net x-pos-fb stepgen-ninja.0.stepgen.0.feedback => joint.0.motor-pos-fb
   net x-enable axis.0.amp-enable-out => stepgen-ninja.0.stepgen.0.enable
   ```

3. **INI फ़ाइल अपडेट करें** (जैसे `your_config.ini`):

   ```ini
   [HAL]
   HALFILE = stepper-ninja.hal

   [EMC]
   SERVO_PERIOD = 1000000
   ```

4. **LinuxCNC चलाएं**:

   ```bash
   linuxcnc your_config.ini
   ```

---

## Troubleshooting

- **CMake Error: PICO_SDK_PATH not found**:
  सुनिश्चित करें कि `PICO_SDK_PATH` सेट है और Pico SDK डायरेक्टरी की ओर इशारा करता है।

  ```bash
  export PICO_SDK_PATH=/path/to/pico-sdk
  ```

- **Missing pioasm/elf2uf2**:
  Pico SDK में ये टूल बनाएं:

  ```bash
  cd pico-sdk/tools/pioasm
  mkdir build && cd build
  cmake .. && make
  ```

- **UTF-8 BOM Errors** (जैसे `∩╗┐` लिंकर आउटपुट में):
  CMake 3.20.6 उपयोग करें या `-DCMAKE_UTF8_BOM=OFF` जोड़ें:

  ```bash
  cmake -DCMAKE_UTF8_BOM=OFF ..
  ```

- **HAL Driver Errors**:
  UDP कनेक्शन समस्याओं के लिए `dmesg` जाँचें:

  ```bash
  dmesg | grep stepgen-ninja
  ```

अधिक मदद के लिए, [GitHub Issues page](https://github.com/atrex66/stepper-ninja/issues) या [Reddit thread](https://www.reddit.com/r/hobbycnc/comments/1koomzu/) पर एरर लॉग साझा करें।

---

## कम्युनिटी नोट्स

r/hobbycnc कम्युनिटी (4.7k व्यूज़!) को परीक्षण के लिए धन्यवाद, विशेषकर उस उपयोगकर्ता को जिसने CMake और Pico+W5500 मॉड्यूल से कंपाइल किया! Stepper-Ninja v1.0 अब एक स्थिर रिलीज के रूप में टैग है: <https://github.com/atrex66/stepper-ninja/releases/tag/v1.0>

Ninja बिल्ड या अन्य सेटअप के लिए, [README](README.md) देखें।
