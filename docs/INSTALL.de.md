# Stepper-Ninja – Installations- und Build-Anleitung

Diese Anleitung beschreibt, wie **Stepper-Ninja** — ein LinuxCNC-HAL-Treiber für den Raspberry Pi Pico — mit **CMake** und Unix Makefiles (`make`) gebaut wird. Das Projekt unterstützt das W5100S-EVB-Pico und ein einfaches Pico + W5500-Ethernet-Modul mit 255-kHz-Schrittgenerierung und 12,5-MHz-Encoder-Zählung.

Getestet mit Pico SDK 2.1.1, CMake 3.20.6 und GNU ARM Embedded Toolchain. Häufige Probleme sind im Abschnitt [Fehlerbehebung](#fehlerbehebung) beschrieben.

---

## Voraussetzungen

Installiere vor dem Build die folgenden Abhängigkeiten:

1. **CMake** (Version 3.15 oder höher):

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
   export PICO_SDK_PATH=/pfad/zum/pico-sdk
   ```

   Füge `PICO_SDK_PATH` zu deinem Shell-Profil hinzu (z. B. `~/.bashrc`).

4. **Build-Werkzeuge**:

   - Linux: Stelle sicher, dass `make` installiert ist (`sudo apt install build-essential`).

5. **PICOTOOL installieren (optional)**:

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

## Repository klonen

Klone das Stepper-Ninja-Repository:

```bash
git clone https://github.com/atrex66/stepper-ninja
cd stepper-ninja
```

---

## Build mit CMake und Make

Führe die folgenden Schritte aus, um Stepper-Ninja mit CMake + Unix Makefiles zu bauen.

### 1. Build-Verzeichnis anlegen

```bash
cd firmware/
mkdir build && cd build
```

### 2. CMake konfigurieren

Führe CMake aus und gib den WIZnet-Chip-Typ an (`W5100S` oder `W5500`). Ohne Angabe ist `W5100S` der Standardwert.

- **W5100S-EVB-Pico** (Standard):

  ```bash
  cmake ..
  ```

- **W5100S-EVB-Pico2**:

  ```bash
  cmake -DBOARD=pico2 ..
  ```

- **W5500 + Pico**:

  ```bash
  cmake -DWIZCHIP_TYPE=W5500 ..
  ```

- **W5500 + Pico2**:

  ```bash
  cmake -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 ..
  ```

- Um die Firmware nach dem Start aus dem RAM auszuführen, ergänze:

   ```bash
   -DSTEPPER_NINJA_RUN_FROM_RAM=ON
   ```

- Um die alte Encoder-PIO-Implementierung zu bauen, füge eine Compiler-Definition hinzu:

   ```bash
   CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY' cmake -DWIZCHIP_TYPE=W5500 ..
   ```

- Der Standard ist `ENCODER_PIO_SUBSTEP`. Die Geschwindigkeits-Schätzung des Encoders läuft in den aktuellen Builds im HAL-Treiber, unabhängig vom Encoder-Modus.

- Eine vollständige Erklärung der `config.h`-Optionen findest du in [CONFIG.de.md](CONFIG.de.md).

### 3. Projekt kompilieren

Kompiliere mit `make`:

```bash
make
```

Dies erzeugt die Datei `stepper-ninja-picoX-W5XXX.uf2` (für das Flashen des Pico; X hängt von den CMake-Parametern ab).

### 4. Pico flashen

- Verbinde den Pico im BOOTSEL-Modus (halte die BOOTSEL-Taste gedrückt, dann per USB verbinden).
- Falls der Pico bereits Firmware enthält, verwende flash_nuke, um den Flash-Speicher zu löschen.
- Kopiere `stepper-ninja-picoX-W5XXX.uf2` auf das Massenspeichergerät des Pico.
- Der Pico startet neu und führt die Firmware aus.

---

## W5500-Unterstützung

Bei einem einfachen Pico + W5500-Modul stelle sicher, dass:

- Der W5500 korrekt verdrahtet ist (SPI-Pins, 3,3-V-Versorgung).
- Der Parameter `-DWIZCHIP_TYPE=W5500` im CMake-Schritt verwendet wird.
- Optional `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` ergänzen, wenn die Firmware nach dem Booten aus dem RAM laufen soll.
- `CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY'` vor `cmake` setzen, wenn du die alte Encoder-PIO verwenden willst.

## Pico2-Unterstützung

Bei einem Pico2 + W5500-Modul stelle sicher, dass:

- Der W5500 korrekt verdrahtet ist (SPI-Pins, 3,3-V-Versorgung).
- Die Parameter `-DBOARD=pico2 -DWIZCHIP_TYPE=W5500` im CMake-Schritt verwendet werden.
- Optional `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` ergänzen, wenn die Firmware nach dem Booten aus dem RAM laufen soll.
- `CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY'` vor `cmake` setzen, wenn du die alte Encoder-PIO verwenden willst.

---

## LinuxCNC-HAL-Treiber installieren

Zur Integration von Stepper-Ninja mit LinuxCNC:

1. **HAL-Treiber installieren**:

   ```bash
   cd hal-driver
   ./install.sh
   ```

2. **HAL-Datei erstellen** (z. B. `stepper-ninja.hal`):

   ```hal
   loadrt stepgen-ninja ip_address="192.168.0.177:8888"

   addf stepgen-ninja.0.watchdog-process servo-thread
   addf stepgen-ninja.0.process-send servo-thread
   addf stepgen-ninja.0.process-recv servo-thread

   net x-pos-cmd joint.0.motor-pos-cmd => stepgen-ninja.0.stepgen.0.command
   net x-pos-fb stepgen-ninja.0.stepgen.0.feedback => joint.0.motor-pos-fb
   net x-enable axis.0.amp-enable-out => stepgen-ninja.0.stepgen.0.enable
   ```

3. **INI-Datei aktualisieren** (z. B. `your_config.ini`):

   ```ini
   [HAL]
   HALFILE = stepper-ninja.hal

   [EMC]
   SERVO_PERIOD = 1000000
   ```

4. **LinuxCNC starten**:

   ```bash
   linuxcnc your_config.ini
   ```

---

## Fehlerbehebung

- **CMake-Fehler: PICO_SDK_PATH nicht gefunden**:
  Stelle sicher, dass `PICO_SDK_PATH` gesetzt ist und auf das Pico-SDK-Verzeichnis zeigt.

  ```bash
  export PICO_SDK_PATH=/pfad/zum/pico-sdk
  ```

- **Fehlende pioasm/elf2uf2**:
  Kompiliere diese Werkzeuge im Pico SDK:

  ```bash
  cd pico-sdk/tools/pioasm
  mkdir build && cd build
  cmake .. && make
  ```

- **UTF-8-BOM-Fehler** (z. B. `∩╗┐` in der Linker-Ausgabe):
  Verwende CMake 3.20.6 oder füge `-DCMAKE_UTF8_BOM=OFF` hinzu:

  ```bash
  cmake -DCMAKE_UTF8_BOM=OFF ..
  ```

- **HAL-Treiber-Fehler**:
  Überprüfe `dmesg` bei UDP-Verbindungsproblemen:

  ```bash
  dmesg | grep stepgen-ninja
  ```

Für weitere Hilfe teile die Fehlerlogs auf [GitHub Issues](https://github.com/atrex66/stepper-ninja/issues) oder im [Reddit-Thread](https://www.reddit.com/r/hobbycnc/comments/1koomzu/).

---

## Hinweise der Community

Dank an die r/hobbycnc-Community (4700+ Aufrufe!) für das Testen, insbesondere an den Nutzer, der mit CMake und einem Pico+W5500-Modul gebaut hat! Stepper-Ninja v1.0 ist als stabiles Release verfügbar: <https://github.com/atrex66/stepper-ninja/releases/tag/v1.0>

Für Ninja-Builds oder andere Konfigurationen siehe die [README](README.md).
