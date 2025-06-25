# Installation and Build Instructions for Stepper-Ninja

This guide explains how to build **Stepper-Ninja**, and the LinuxCNC HAL driver for Raspberry Pi Pico/Pico2 on Raspberry PI4/5

Tested with Pico SDK 2.1.1, CMake 3.20.6, and GNU ARM Embedded Toolchain. See [Troubleshooting](#troubleshooting) for common issues.

---

## Prerequisites

Before building, install the following dependencies:

1. **CMake** (version 3.15 or higher):

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

   Add `PICO_SDK_PATH` to your shell profile (e.g., `~/.bashrc`).

4. **Build Tools**:

   - Linux: Ensure `make` is installed (`sudo apt install build-essential`).

5. **UNZIP**:

   ```bash
   sudo apt install unzip
   ```

---

## Cloning the Repository

Clone the Stepper-Ninja repository (test branch):

```bash
git clone -b test https://github.com/atrex66/stepper-ninja
cd stepper-ninja
```

---

## Building with CMake and Make

Follow these steps to build Stepper-Ninja using CMake with Unix Makefiles.

### 1. Raspberry PI4 configuration copy

```bash
cd ~/stepper-ninja
cp configurations/RaspberryPI4/config.h firmware/inc/
```

### 2. Create a Build Directory

```bash
cd firmware/
mkdir build && cd build
```

### 3. Configure CMake

Run CMake to generate Makefiles, do not specify the WIZnet chip type.

- For **Pico** (default):
  
  ```bash
  cmake ..
  ```

- For **Pico2** (default):
  
  ```bash
  cmake -DBOARD=pico2 ..
  ```

### 4. Build the Project

Compile the project using `make`:

```bash
make
```

This generates `stepper-ninja-picoX-W5100s.uf2` (for flashing the Pico, X-s is depend of cmake defs).

### 5. Flash the Pico

- Connect the Pico in BOOTSEL mode (hold BOOTSEL, plug in USB).
- If you are using the pico with other firmware use flash_nuke to erase previous data from the flash memory
- Copy `stepper-ninja-picoX-W5100s.uf2` to the Pico’s mass storage device.
- The Pico reboots and runs the firmware.

---

### 5. Build the hal driver

 - go to __~/stepper-ninja/hal-driver__
 - run the __./install.sh__ or the __./rip_install.sh__ depends on your linuxcnc installation

### 6. Before try to run LinuxCNC

 - edit your __/boot/firmware/config.txt__
 - make sure to __dtparam=spi=off__ turn off the SPI, because the bcm2835 library conflicts with the kernel driver

## pico2 Support

For a Pico2, ensure:

- The raspberry pi4 properly wired (SPI pins, INT pin, power).
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

Thanks to the r/hobbycnc community (4.7k views!) for testing, especially the user who compiled with CMake and a Pico+W5500 module! Stepper-Ninja v1.0 is now tagged as a stable release: <https://github.com/atrex66/stepper-ninja/releases/tag/v1.0>

For Ninja builds or other setups, see the [README](README.md).
