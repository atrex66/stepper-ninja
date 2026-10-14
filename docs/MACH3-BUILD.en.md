# Building the StepperNinja Mach3 prototype

**Windows update, 2026-10-14:** the Win32 DLL and USB firmware now build.
The DLL loads in an x86 process and a physical RP2040 passes USB CLAIM/POLL/RELEASE.
Mach3 UI integration and physical motion still need validation.
See the [tested environment and commands (Hungarian)](MACH3-WINDOWS-VALIDATION.hu.md).


[Magyar](MACH3-BUILD.hu.md)

The Mach3 driver is a **prototype**. The portable core and Pico/W5500 firmware
have been built and tested on Linux. The Windows DLL and USB firmware now build; actual Mach3 integration and physical motion still need validation.

**The Pico firmware can also be built natively on Windows. No Linux machine is
required to build both the firmware and the Mach3 DLL.** The Windows commands
below describe that build path. See the validation note above for the tested VS2019 and USB-only commands.

Once real Mach3 and hardware tests confirm reliable operation, a release with
**prebuilt Pico firmware and Mach3 driver binaries** is planned. That validated
release is not available yet. Motion protocol version 3 requires matching
firmware and driver builds.

## 1. Build targets and prerequisites

Run the commands from the repository root. Use a checkout containing
`mach3-driver/` and `common/`. Build directories are separate for each target.

| Target | Host | Requirements | Output |
|---|---|---|---|
| Portable core and tests | Linux | CMake ≥3.16, C/C++ compiler with C++11 support, make or Ninja, Python 3 | Test executables and static core library |
| Pico firmware | Linux or Windows | Pico SDK with submodules, Arm GNU toolchain, CMake, Python 3, native build tools, WIZnet sources | `.uf2` and `.elf` |
| Mach3 plugin | Windows | MSVC, x86 MFC, Windows SDK, CMake, Mach3 SDK 2.62 | 32-bit `StepperNinja.dll` |
| Optional LinuxCNC regression | Linux | LinuxCNC development files, pkg-config, libgpiod development files including static library | HAL tests and build artifacts |

Firmware uses the Arm cross compiler. The Mach3 DLL uses MSVC and MFC; these
are different toolchains. Linux tests do not compile the Windows adapter.
Build commands below do not install modules, copy the DLL into Mach3, or flash
a Pico.

## 2. Portable core and tests on Linux

On Debian/Ubuntu, the host build needs `build-essential`, `cmake`, and `python3`.
For firmware, also install `gcc-arm-none-eabi`, `binutils-arm-none-eabi`,
`libnewlib-arm-none-eabi`, `libstdc++-arm-none-eabi-newlib`, `git`, and `unzip`.
Package installation is a separate environment setup step.

```sh
cmake -S mach3-driver -B build/mach3-host -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build/mach3-host --parallel
ctest --test-dir build/mach3-host --output-on-failure
```

Linux currently provides eight Mach3 tests, including UDP loopback, probe motion,
firmware profile checks, WIZ transport models, and PIO instruction models.
Loopback tests need local socket access. They do not establish Windows ABI
compatibility or physical machine operation.

## 3. Pico firmware on Linux

Prepare the [official Pico SDK](https://github.com/raspberrypi/pico-sdk), including
its submodules. Replace `/path/to/pico-sdk` with its actual location. The existing
Linux default is `$HOME/pico-sdk`; an explicit `-DPICO_SDK_PATH` takes precedence.
The CMake build can extract the bundled WIZnet ZIP using bash/unzip if
`firmware/ioLibrary_Driver/README.md` is missing.

```sh
cmake -S firmware -B build/mach3-firmware -DBOARD=pico -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_PROFILE=mach3-3axis -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build/mach3-firmware --parallel
```

Outputs for these options:

- `build/mach3-firmware/stepper-ninja-pico-W5500-mach3-3axis.uf2`
- `build/mach3-firmware/stepper-ninja-pico-W5500-mach3-3axis.elf`

The selected board is Pico/RP2040 with W5500. CMake also accepts `BOARD=pico2`
and `WIZCHIP_TYPE=W5100S`; their Mach3 hardware operation has not been validated.
Choose the actual hardware and use a separate build directory when changing it.
The Mach3 profile uses its fixed pinout, not the flexible LinuxCNC allocation.

## 4. Pico firmware on Windows, without Linux or WSL

Set up the native Pico C/C++ development environment. The
[official Raspberry Pi Pico VS Code extension](https://github.com/raspberrypi/pico-vscode)
provides Windows development support. Use its configured terminal/tool paths,
or prepare equivalent tools manually: Pico SDK with submodules, Arm GNU GCC,
Python 3, CMake, Ninja, and a native compiler for SDK helper tools. Use the
SDK/toolchain versions supplied by that environment. No Mach3 SDK or MFC is
needed for the firmware build.

In PowerShell, extract the bundled WIZnet sources before configuring. This avoids
the Linux bash extraction fallback; skip extraction if the sources already exist:

```powershell
if (-not (Test-Path firmware/ioLibrary_Driver/README.md)) {
    Expand-Archive -Path firmware/ioLibrary_Driver.zip -DestinationPath firmware -Force
}
```

Set the SDK path to your actual SDK directory; `C:/Pico/pico-sdk` below is an
example. Make the tools available in the same terminal before running:

```powershell
cmake -S firmware -B build/mach3-firmware-windows -G Ninja -DBOARD=pico -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_PROFILE=mach3-3axis -DPICO_SDK_PATH=C:/Pico/pico-sdk
cmake --build build/mach3-firmware-windows --parallel
```

Output: `build/mach3-firmware-windows/stepper-ninja-pico-W5500-mach3-3axis.uf2`
and the matching `.elf`. Use Ninja for this cross-compiled firmware build,
not the Visual Studio generator used for the DLL. If the SDK cannot find the
Arm compiler or a host helper tool, correct the development environment/tool
paths before reconfiguring.

## 5. Mach3 DLL on Windows

Install Visual C++ desktop build tools with **x86 MFC** and a Windows SDK.
Use a Visual Studio developer terminal. A 64-bit Windows host is fine, but
Mach3 requires a **32-bit DLL**, so select `-A Win32`.
The [CMake Visual Studio generator documentation](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2017%202022.html)
describes this platform selection.

Download [Mach3 SDK 2.62 from the official download page](https://www.machsupport.com/downloads-updates/mach3-downloads/).
Keep it outside this repository and point `MACH3_SDK_DIR` at the extracted
`SDK` directory containing:


The Win32 build also requires the official Mach3 Plugin Wizard 1.1a's
MachIncludes directory as `MACH3_ABI_DIR`. It supplies the fixed plain-data
Mach3 ABI; modern MFC CWnd/CView layouts must not be used for Mach3-owned
objects. Download it from the same official SDK page. See [ABI details](MACH3-ABI.en.md).

```text
SDK/
  BlankMovement/MachDevice.cpp
  BlankMovement/XMLProfile.cpp
  MachIncludes/Engine.h
```

For Visual Studio 2022, use CMake ≥3.21 and run:

```powershell
cmake -S mach3-driver -B build/mach3-windows -G "Visual Studio 17 2022" -A Win32 -DSN_BUILD_MACH3_PLUGIN=ON -DBUILD_TESTING=ON -DMACH3_SDK_DIR=C:/Mach3Development/SDK -DMACH3_ABI_DIR=C:/Mach3Development/MachPluginWizard/MachIncludes
cmake --build build/mach3-windows --config Release --parallel
ctest --test-dir build/mach3-windows -C Release --output-on-failure
```

Expected DLL: `build/mach3-windows/Release/StepperNinja.dll`. Windows currently
has eight portable/plugin tests; Linux-only tests are not registered there.
The build stages the official SDK wrapper and XML code in the build directory
and combines them with our adapter and configuration dialog.

**The DLL builds with VS2019 and loads in an x86 process.** The old SDK contains legacy
MFC class headers; the build uses the official Wizard plain-data structs to preserve the Mach3 ABI. Linux core tests alone cannot detect host ABI mismatches.
After a successful build, DLL loading and Config Plugins must be tested in a
separate Mach3 test profile. See the [validation sequence](MACH3-FEEDHOLD-G31.en.md).

For a Pico without WIZnet hardware, use this USB-only build (same fixed pinout).
It also works in a Windows Pico development terminal; use the SDK path there.

```sh
cmake -S firmware -B build/mach3-usb-firmware -DBOARD=pico -DSTEPPER_NINJA_PROFILE=mach3-3axis -DSTEPPER_NINJA_MACH3_USB_ONLY=ON -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build/mach3-usb-firmware --parallel
```

Output: `build/mach3-usb-firmware/stepper-ninja-pico-usb-mach3-3axis.uf2`.
Select **USB connection** and the COM port in Config Plugins. Close the terminal
application first. See [console handover and watchdog](MACH3-PROTOCOL.en.md#9-usb-connection-and-console-handover).

## 6. Optional LinuxCNC regression checks

LinuxCNC remains the default firmware profile. These checks require a Linux
LinuxCNC development environment and libgpiod development/static files:

```sh
cmake -S firmware -B build/linuxcnc-firmware -DBOARD=pico -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_PROFILE=linuxcnc -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build/linuxcnc-firmware --parallel

cmake -S hal-driver -B build/hal-regression -DBUILD_TESTING=ON
cmake --build build/hal-regression --parallel
ctest --test-dir build/hal-regression --output-on-failure
```

These checks are optional for building Mach3; they are not a reason to require
a Linux machine for Windows firmware/DLL development.

## 7. Troubleshooting and validation status

- Missing Pico SDK: supply its actual root with `-DPICO_SDK_PATH` and initialize
  SDK submodules. Do not point this option at the Mach3 SDK.
- Missing WIZnet sources on Windows: extract `firmware/ioLibrary_Driver.zip`
  before configuring; the fallback script requires bash/unzip.
- Missing `arm-none-eabi-gcc`, Ninja, Python, or SDK helper tools: fix the Pico
  development terminal's tool paths.
- DLL rejected at configure time: use Windows, MSVC, x86 MFC, and `-A Win32`;
  MinGW or a Linux cross compiler does not provide the required MFC environment.
- Missing Mach3 SDK files: point `MACH3_SDK_DIR` at `SDK`, not its parent directory.
- Generator/platform mismatch: use a new build directory for a different generator
  or architecture.

Recorded Linux results on 2026-10-14: 11/11 Mach3 tests and 32/32 LinuxCNC HAL
regression tests passed; both Pico/W5500 firmware profiles built successfully.
Windows DLL and USB firmware builds, x86 loading and a physical USB connection check passed; actual Mach3 integration and physical motion still need validation.
The planned binary release follows those real Mach3 and hardware tests.
