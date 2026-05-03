# Installation and Build Instructions for Stepper-Ninja (Raspberry Pi SPI)

This guide explains how to build **Stepper-Ninja** and the LinuxCNC HAL driver for Raspberry Pi Pico/Pico2 connected via **SPI** to a Raspberry Pi 4/5.

This version uses the **kernel SPI driver** (`spidev`) and **libgpiod v2** for communication — no bcm2835 library required.

Tested with Pico SDK 2.1.1, CMake 3.20.6, and GNU ARM Embedded Toolchain. See [Troubleshooting](#troubleshooting) for common issues.

---

## Prerequisites

Before building, install the following dependencies:

```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi binutils-arm-none-eabi \
                 build-essential unzip libgpiod-dev
```

**Pico SDK (2.1.1)**:

```bash
git clone https://github.com/raspberrypi/pico-sdk
cd pico-sdk
git submodule update --init
export PICO_SDK_PATH=/path/to/pico-sdk
```

Add `PICO_SDK_PATH` to your shell profile (e.g., `~/.bashrc`).

---

## Cloning the Repository

```bash
git clone https://github.com/atrex66/stepper-ninja
cd stepper-ninja
```

---

## Raspberry Pi SPI Setup

Enable SPI in `/boot/firmware/config.txt`:

```
dtparam=spi=on
```

> **Note:** The previous bcm2835-based version required `spi=off`. This version uses the kernel `spidev` driver — SPI **must be enabled**.

---

## Building with CMake and Make

### 1. Copy the Raspberry Pi configuration

```bash
cd ~/stepper-ninja
cp configurations/RaspberryPI/config.h firmware/inc/
```

### 2. Create a Build Directory

```bash
cd firmware/
mkdir build && cd build
```

### 3. Configure CMake

- For **Pico**:

  ```bash
  cmake ..
  ```

- For **Pico2**:

  ```bash
  cmake -DBOARD=pico2 ..
  ```

### 4. Build the Project

```bash
make
```

This generates `stepper-ninja-picoX.uf2`.

### 5. Flash the Pico

- Connect the Pico in BOOTSEL mode (hold BOOTSEL, plug in USB).
- If you are using the pico with other firmware use flash_nuke to erase previous data from the flash memory.
- Copy the `.uf2` file to the Pico's mass storage device.
- The Pico reboots and runs the firmware.

---

### 6. Build the HAL driver

 - go to __~/stepper-ninja/hal-driver__
 - run __./install.sh__ or __./rip_install.sh__ depending on your LinuxCNC installation

## Pico2 Support

For a Pico2, ensure:

- The Raspberry Pi is properly wired (SPI pins, INT pin, power).
- Use `-DBOARD=pico2` in the CMake step.


## Installing the LinuxCNC HAL Driver

To integrate Stepper-Ninja with LinuxCNC:

1. **Install the HAL Driver**:

   ```bash
   cd hal-driver
   ./install.sh
   ```

2. **Create a HAL File** (e.g., `stepper-ninja.hal`):

   ```hal
   loadrt stepgen-ninja

   addf stepgen-ninja.0.watchdog-process servo-thread
   addf stepgen-ninja.0.process-send servo-thread
   addf stepgen-ninja.0.process-recv servo-thread

   net x-pos-cmd joint.0.motor-pos-cmd => stepgen-ninja.0.stepgen.0.command
   net x-pos-fb stepgen-ninja.0.stepgen.0.feedback => joint.0.motor-pos-fb
   net x-enable axis.0.amp-enable-out => stepgen-ninja.0.stepgen.0.enable
   ```

3. **Update the INI File** (e.g., `your_config.ini`):

   ```ini
   [HAL]
   HALFILE = stepper-ninja.hal

   [EMC]
   SERVO_PERIOD = 1000000
   ```

4. **Run LinuxCNC**:

   ```bash
   linuxcnc your_config.ini
   ```

---

## Troubleshooting

- **CMake Error: PICO_SDK_PATH not found**:
  Ensure `PICO_SDK_PATH` is set and points to the Pico SDK directory.
  
  ```bash
  export PICO_SDK_PATH=/path/to/pico-sdk
  ```

- **Missing pioasm/elf2uf2**:
  Build these tools in the Pico SDK:
  
  ```bash
  cd pico-sdk/tools/pioasm
  mkdir build && cd build
  cmake .. && make
  ```

- **UTF-8 BOM Errors** (e.g., `∩╗┐` in linker output):
  Use CMake 3.20.6 or add `-DCMAKE_UTF8_BOM=OFF`:
  
  ```bash
  cmake -DCMAKE_UTF8_BOM=OFF ..
  ```

- **HAL Driver Errors**:
  Check `dmesg` for UDP connection issues:
  
  ```bash
  dmesg | grep stepgen-ninja
  ```

For more help, share error logs on the [GitHub Issues page](https://github.com/atrex66/stepper-ninja/issues) or the [Reddit thread](https://www.reddit.com/r/hobbycnc/comments/1koomzu/).

---

## Community Notes

Thanks to the r/hobbycnc community for testing! Stepper-Ninja v1.0 is tagged as a stable release: <https://github.com/atrex66/stepper-ninja/releases/tag/v1.0>

This version switches from bcm2835 to kernel SPI (`spidev`) and libgpiod v2, making it compatible with a wider range of SBCs in the future.

For more information, see the main [README](../../README.md).
