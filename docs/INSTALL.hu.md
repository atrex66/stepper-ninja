# A Stepper-Ninja telepítési és fordítási útmutatója

Ez az útmutató bemutatja, hogyan fordítsd le a **Stepper-Ninja** szoftvert — egy LinuxCNC HAL-illesztőprogramot Raspberry Pi Picóhoz — **CMake** és Unix Makefiles (`make`) segítségével. A projekt támogatja a W5100S-EVB-Pico és a sima Pico + W5500 Ethernet modult, 255 kHz-es lépésgenerálással és 12,5 MHz-es enkóder számlálással.

Tesztelve: Pico SDK 2.1.1, CMake 3.20.6 és GNU ARM Embedded Toolchain. Gyakori problémákért lásd a [Hibaelhárítás](#hibaelhárítás) részt.

---

## Előfeltételek

A fordítás megkezdése előtt telepítsd a következő függőségeket:

1. **CMake** (3.15-ös vagy újabb verzió):

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
   export PICO_SDK_PATH=/pico-sdk/elérési/útja
   ```

   Add hozzá a `PICO_SDK_PATH` változót a shell profilodhoz (pl. `~/.bashrc`).

4. **Fordítóeszközök**:

   - Linux: ellenőrizd, hogy a `make` telepítve van-e (`sudo apt install build-essential`).

5. **PICOTOOL telepítése (opcionális)**:

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

## A tároló klónozása

Klónozd a Stepper-Ninja tárolót:

```bash
git clone https://github.com/atrex66/stepper-ninja
cd stepper-ninja
```

---

## Fordítás CMake és Make segítségével

Kövesd az alábbi lépéseket a Stepper-Ninja CMake + Unix Makefiles alapú fordításához.

### 1. Hozd létre a build könyvtárat

```bash
cd firmware/
mkdir build && cd build
```

### 2. Konfiguráld a CMake-et

Futtasd a CMake-et, és add meg a WIZnet chip típusát (`W5100S` vagy `W5500`). Ha nem adod meg, az alapértelmezett `W5100S`.

- **W5100S-EVB-Pico** esetén (alapértelmezett):

  ```bash
  cmake ..
  ```

- **W5100S-EVB-Pico2** esetén:

  ```bash
  cmake -DBOARD=pico2 ..
  ```

- **W5500 + pico** esetén:

  ```bash
  cmake -DWIZCHIP_TYPE=W5500 ..
  ```

- **W5500 + pico2** esetén:

  ```bash
  cmake -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 ..
  ```

### 3. Fordítsd le a projektet

Fordítás `make`-kel:

```bash
make
```

Ez létrehozza a `stepper-ninja-picoX-W5XXX.uf2` fájlt (a Pico flasheléséhez; az X a cmake paraméterektől függ).

### 4. Flasheld a Picót

- Csatlakoztasd a Picót BOOTSEL módban (tartsd lenyomva a BOOTSEL gombot, majd csatlakoztasd USB-n).
- Ha a Picón már van firmware, használj flash_nuke-ot a flash előző tartalmának törléséhez.
- Másold a `stepper-ninja-picoX-W5XXX.uf2` fájlt a Pico tömegtároló eszközére.
- A Pico újraindul és elindítja a firmware-t.

---

## W5500 támogatás

Sima Pico + W5500 modul esetén győződj meg róla, hogy:

- A W5500 megfelelően van bekötve (SPI lábak, 3,3 V-os tápellátás).
- Használd a `-DWIZCHIP_TYPE=W5500` paramétert a CMake lépésnél.

## Pico2 támogatás

Pico2 + W5500 modul esetén győződj meg róla, hogy:

- A W5500 megfelelően van bekötve (SPI lábak, 3,3 V-os tápellátás).
- Használd a `-DBOARD=pico2 -DWIZCHIP_TYPE=W5500` paramétert a CMake lépésnél.

---

## A LinuxCNC HAL-illesztőprogram telepítése

A Stepper-Ninja LinuxCNC-vel való integrálásához:

1. **Telepítsd a HAL-illesztőprogramot**:

   ```bash
   cd hal-driver
   ./install.sh
   ```

2. **Hozz létre egy HAL fájlt** (pl. `stepper-ninja.hal`):

   ```hal
   loadrt stepgen-ninja ip_address="192.168.0.177:8888"

   addf stepgen-ninja.0.watchdog-process servo-thread
   addf stepgen-ninja.0.process-send servo-thread
   addf stepgen-ninja.0.process-recv servo-thread

   net x-pos-cmd joint.0.motor-pos-cmd => stepgen-ninja.0.stepgen.0.command
   net x-pos-fb stepgen-ninja.0.stepgen.0.feedback => joint.0.motor-pos-fb
   net x-enable axis.0.amp-enable-out => stepgen-ninja.0.stepgen.0.enable
   ```

3. **Frissítsd az INI fájlt** (pl. `your_config.ini`):

   ```ini
   [HAL]
   HALFILE = stepper-ninja.hal

   [EMC]
   SERVO_PERIOD = 1000000
   ```

4. **Indítsd el a LinuxCNC-t**:

   ```bash
   linuxcnc your_config.ini
   ```

---

## Hibaelhárítás

- **CMake hiba: PICO_SDK_PATH nem található**:
  Ellenőrizd, hogy a `PICO_SDK_PATH` be van-e állítva, és a Pico SDK könyvtárára mutat-e.

  ```bash
  export PICO_SDK_PATH=/pico-sdk/elérési/útja
  ```

- **Hiányzó pioasm/elf2uf2**:
  Fordítsd le ezeket az eszközöket a Pico SDK-ban:

  ```bash
  cd pico-sdk/tools/pioasm
  mkdir build && cd build
  cmake .. && make
  ```

- **UTF-8 BOM hibák** (pl. `∩╗┐` a linker kimenetében):
  Használj CMake 3.20.6-ot, vagy add hozzá a `-DCMAKE_UTF8_BOM=OFF` paramétert:

  ```bash
  cmake -DCMAKE_UTF8_BOM=OFF ..
  ```

- **HAL-illesztőprogram hibák**:
  Ellenőrizd a `dmesg`-et UDP kapcsolati problémák esetén:

  ```bash
  dmesg | grep stepgen-ninja
  ```

További segítségért oszd meg a hibanaplókat a [GitHub Issues oldalon](https://github.com/atrex66/stepper-ninja/issues) vagy a [Reddit szálon](https://www.reddit.com/r/hobbycnc/comments/1koomzu/).

---

## Közösségi megjegyzések

Köszönet az r/hobbycnc közösségnek (4700+ megtekintés!) a tesztelésért, különösen annak a felhasználónak, aki CMake-kel és Pico+W5500 modullal fordította le! A Stepper-Ninja v1.0 stabil kiadásként van megjelölve: <https://github.com/atrex66/stepper-ninja/releases/tag/v1.0>

Ninja buildekhez vagy egyéb beállításokhoz lásd a [README](README.md) fájlt.
