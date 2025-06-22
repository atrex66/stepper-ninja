# Installation and Build Instructions for Stepper-Ninja

This guide explains how to build **Stepper-Ninja**, a LinuxCNC HAL driver for Raspberry Pi Pico, using **CMake** with the Unix Makefiles generator (`make`). The project supports W5100S-EVB-Pico and standard Pico with W5500 Ethernet modules, achieving 255 kHz step generation and 12.5 MHz encoder counting.

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

Clone the Stepper-Ninja repository:

```bash
git clone https://github.com/atrex66/stepper-ninja
cd stepper-ninja
```

---

## Building with CMake and Make

Follow these steps to build Stepper-Ninja using CMake with Unix Makefiles.

### 1. Create a Build Directory

```bash
cd firmware/
mkdir build && cd build
```

### 2. Configure CMake

Run CMake to generate Makefiles, specifying the WIZnet chip type (`W5100S` or `W5500`). The default is `W5100S` if not specified.

- For **W5100S-EVB-Pico** (default):
  
  ```bash
  cmake ..
  ```

- For **W5100S-EVB-Pico2** (default):
  
  ```bash
  cmake -DBOARD=pico2 ..
  ```

- For **W5500 + pico**
  
  ```bash
  cmake -DWIZCHIP_TYPE=W5500 ..
  ```

- For **W5500 + pico2**
  
  ```bash
  cmake -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 ..
  ```

### 3. Build the Project

Compile the project using `make`:

```bash
make
```

This generates `stepper-ninja-picoX-W5XXX.uf2` (for flashing the Pico, X-s is depend of cmake defs).

### 4. Flash the Pico

- Connect the Pico in BOOTSEL mode (hold BOOTSEL, plug in USB).
- If you are using the pico with other firmware use flash_nuke to erase previous data from the flash memory
- Copy `stepper-ninja-picoX-W5XXX.uf2` to the Pico’s mass storage device.
- The Pico reboots and runs the firmware.

---

## W5500 Support

For a standard Pico with a W5500 module, ensure:

- The W5500 is properly wired (SPI pins, 3.3V power).
- Use `-DWIZCHIP_TYPE=W5500` in the CMake step.

## pico2 Support

For a Pico2 with a W5500 module, ensure:

- The W5500 is properly wired (SPI pins, 3.3V power).
- Use `-DBOARD=pico2 -DWIZCHIP_TYPE=W5500` in the CMake step.


## Installing the LinuxCNC HAL Driver

To integrate Stepper-Ninja with LinuxCNC:

1. **Install the HAL Driver**:

   ```bash
   cd hal-driver
   ./install.sh
   ```

2. **Create a HAL File** (e.g., `stepper-ninja.hal`):

   ```hal
   loadrt stepgen-ninja ip_address="192.168.0.177:8888"

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
