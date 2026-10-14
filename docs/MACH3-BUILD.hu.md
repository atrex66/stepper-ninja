# A StepperNinja Mach3 prototípus fordítása

[English](MACH3-BUILD.en.md)

A Mach3 driver **prototípus**. A hordozható magot és a Pico/W5500 firmware-t
Linuxon lefordítottuk és teszteltük; a Windows DLL, a valódi Mach3 integráció
és a fizikai hardver működése még ellenőrizendő.

**A Pico firmware natívan Windows alatt is fordítható. A firmware és a Mach3
DLL elkészítéséhez nem szükséges Linux gép.** Az alábbi Windows parancsok ezt
az utat írják le; ebben a projektben még nem futtattuk őket Windows gépen.

Amint a valódi Mach3- és hardvertesztek igazolták a megbízható működést,
tervezett release készül **előre fordított Pico firmware-rel és Mach3 driverrel**.
Ez az igazolt release még nem érhető el. A 3-as mozgásprotokoll-verzióhoz
egyező firmware és driver build szükséges.

## 1. Fordítási célok és követelmények

A parancsokat a repó gyökeréből futtasd, olyan checkoutban, amely tartalmazza
a `mach3-driver/` és `common/` könyvtárat. Minden célnak külön buildkönyvtára van.

| Cél | Gazdagép | Követelmények | Kimenet |
|---|---|---|---|
| Hordozható mag és tesztek | Linux | CMake ≥3.16, C/C++ fordító C++11 támogatással, make vagy Ninja, Python 3 | Tesztprogramok és statikus könyvtár |
| Pico firmware | Linux vagy Windows | Pico SDK submodule-okkal, Arm GNU toolchain, CMake, Python 3, natív buildeszközök, WIZnet források | `.uf2` és `.elf` |
| Mach3 plugin | Windows | MSVC, x86 MFC, Windows SDK, CMake, Mach3 SDK 2.62 | 32 bites `StepperNinja.dll` |
| Opcionális LinuxCNC regresszió | Linux | LinuxCNC fejlesztői fájlok, pkg-config, libgpiod fejlesztői fájlok statikus könyvtárral | HAL tesztek és buildkimenetek |

A firmware Arm keresztfordítót használ. A Mach3 DLL MSVC-t és MFC-t használ;
ez két külön toolchain. A Linux tesztek nem fordítják a Windows adaptert.
Az alábbi buildparancsok nem telepítenek modult, nem másolják a DLL-t a Mach3-ba
és nem flashelik a Picót.

## 2. Hordozható mag és tesztek Linuxon

Debian/Ubuntu alatt a host buildhez `build-essential`, `cmake` és `python3` kell.
A firmware-hez még `gcc-arm-none-eabi`, `binutils-arm-none-eabi`,
`libnewlib-arm-none-eabi`, `libstdc++-arm-none-eabi-newlib`, `git` és `unzip`
szükséges. A csomagtelepítés külön környezet-előkészítési lépés.

```sh
cmake -S mach3-driver -B build/mach3-host -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build/mach3-host --parallel
ctest --test-dir build/mach3-host --output-on-failure
```

Linuxon jelenleg nyolc Mach3 teszt fut, köztük UDP loopback, probe mozgás,
firmware-profil ellenőrzés, WIZ transzportmodell és PIO utasításmodell.
A loopback tesztekhez helyi socketelérés kell. Ezek nem igazolják a Windows
ABI kompatibilitását vagy a fizikai gép működését.

## 3. Pico firmware Linuxon

Készítsd elő a [hivatalos Pico SDK-t](https://github.com/raspberrypi/pico-sdk),
a submodule-okkal együtt. A `/path/to/pico-sdk` helyére a tényleges elérési út
kerüljön. A meglévő Linux alapérték `$HOME/pico-sdk`; az explicit
`-DPICO_SDK_PATH` elsőbbséget élvez. A CMake build bash/unzip segítségével
ki tudja bontani a mellékelt WIZnet ZIP-et, ha hiányzik a
`firmware/ioLibrary_Driver/README.md`.

```sh
cmake -S firmware -B build/mach3-firmware -DBOARD=pico -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_PROFILE=mach3-3axis -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build/mach3-firmware --parallel
```

Ezekkel az opciókkal a kimenetek:

- `build/mach3-firmware/stepper-ninja-pico-W5500-mach3-3axis.uf2`
- `build/mach3-firmware/stepper-ninja-pico-W5500-mach3-3axis.elf`

A kiválasztott hardver Pico/RP2040 és W5500. A CMake elfogadja a `BOARD=pico2`
és `WIZCHIP_TYPE=W5100S` értékeket is; ezek Mach3 hardveres működése még nincs
igazolva. A tényleges hardvert válaszd, és hardverváltáskor használj külön
buildkönyvtárat. A Mach3 profil fix lábkiosztású, nem a LinuxCNC rugalmas
kiosztását használja.

## 4. Pico firmware Windowson, Linux és WSL nélkül

Állítsd be a natív Pico C/C++ fejlesztői környezetet. A
[hivatalos Raspberry Pi Pico VS Code bővítmény](https://github.com/raspberrypi/pico-vscode)
Windows fejlesztést is támogat. Használd a beállított terminálját/eszközútvonalait,
vagy készítsd elő ugyanezeket kézzel: Pico SDK submodule-okkal, Arm GNU GCC,
Python 3, CMake, Ninja és natív fordító az SDK segédprogramjaihoz.
Használd a környezet által biztosított SDK/toolchain verziókat.
A firmware fordításához nem kell Mach3 SDK vagy MFC.

PowerShellben konfigurálás előtt bontsd ki a mellékelt WIZnet forrásokat.
Így nincs szükség a Linux bash kicsomagolási tartalékágra; ha a források már
meg vannak, a kicsomagolás kimarad:

```powershell
if (-not (Test-Path firmware/ioLibrary_Driver/README.md)) {
    Expand-Archive -Path firmware/ioLibrary_Driver.zip -DestinationPath firmware -Force
}
```

Az SDK útvonala a tényleges SDK könyvtár legyen; a `C:/Pico/pico-sdk` itt példa.
A szükséges eszközök ugyanabban a terminálban legyenek elérhetők:

```powershell
cmake -S firmware -B build/mach3-firmware-windows -G Ninja -DBOARD=pico -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_PROFILE=mach3-3axis -DPICO_SDK_PATH=C:/Pico/pico-sdk
cmake --build build/mach3-firmware-windows --parallel
```

Kimenet: `build/mach3-firmware-windows/stepper-ninja-pico-W5500-mach3-3axis.uf2`
és a hozzá tartozó `.elf`. Ehhez a keresztfordított firmware buildhez Ninja kell,
nem a DLL-hez használt Visual Studio generátor. Ha az SDK nem találja az Arm
fordítót vagy egy natív segédprogramot, a fejlesztői környezet/eszközútvonalak
javítása után konfigurálj újra.

## 5. Mach3 DLL Windowson

Telepítsd a Visual C++ asztali buildeszközöket **x86 MFC**-vel és Windows SDK-val.
Használj Visual Studio fejlesztői terminált. A Windows gazdagép lehet 64 bites,
de a Mach3 **32 bites DLL-t** igényel, ezért `-A Win32` kell.
A platformválasztást a [CMake Visual Studio generátor leírása](https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2017%202022.html)
is ismerteti.

Töltsd le a [Mach3 SDK 2.62-t a hivatalos letöltési oldalról](https://www.machsupport.com/downloads-updates/mach3-downloads/).
Tartsd a repón kívül, és a `MACH3_SDK_DIR` a kicsomagolt `SDK` könyvtárra
mutasson, amelyben ezek megtalálhatók:

```text
SDK/
  BlankMovement/MachDevice.cpp
  BlankMovement/XMLProfile.cpp
  MachIncludes/Engine.h
```

Visual Studio 2022 esetén CMake ≥3.21 kell; futtasd:

```powershell
cmake -S mach3-driver -B build/mach3-windows -G "Visual Studio 17 2022" -A Win32 -DSN_BUILD_MACH3_PLUGIN=ON -DBUILD_TESTING=ON -DMACH3_SDK_DIR=C:/Mach3Development/SDK
cmake --build build/mach3-windows --config Release --parallel
ctest --test-dir build/mach3-windows -C Release --output-on-failure
```

Várt DLL: `build/mach3-windows/Release/StepperNinja.dll`. Windowson jelenleg
négy hordozható teszt van; a hét Linux-specifikus teszt ott nem regisztrálódik.
A build a hivatalos SDK wrapperét és XML kódját a buildkönyvtárba másolja,
és a saját adapterrel, illetve konfigurációs ablakkal együtt fordítja.

**Ez a DLL build még nincs igazolva.** A régi SDK örökölt ActiveX/Flash
fejléceket tartalmaz; a választott MSVC/MFC/Windows SDK-val való kompatibilitást
ellenőrizni kell. A Linux magtesztek ezeket a kompatibilitási problémákat nem
oldják meg. Sikeres fordítás után külön Mach3 tesztprofilban kell ellenőrizni
a DLL betöltését és a Config Plugins működését.
Lásd az [ellenőrzési sorrendet](MACH3-FEEDHOLD-G31.hu.md).

WIZnet hardver nélküli Pico esetén az USB-only build használható (azonos fix lábkiosztás).
Windows Pico fejlesztői terminálban is működik; az ottani SDK útvonalat add meg.

```sh
cmake -S firmware -B build/mach3-usb-firmware -DBOARD=pico -DSTEPPER_NINJA_PROFILE=mach3-3axis -DSTEPPER_NINJA_MACH3_USB_ONLY=ON -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build/mach3-usb-firmware --parallel
```

Kimenet: `build/mach3-usb-firmware/stepper-ninja-pico-usb-mach3-3axis.uf2`.
A Config Plugins ablakban válaszd a **USB connection** mezőt és a COM portot.
Előtte zárd be a terminálprogramot. Lásd a [konzolátadást és watchdogot](MACH3-PROTOCOL.hu.md#9-usb-kapcsolat-és-a-konzol-átadása).

## 6. Opcionális LinuxCNC regressziós ellenőrzések

A LinuxCNC marad az alapértelmezett firmware-profil. Ezekhez az ellenőrzésekhez
Linuxos LinuxCNC fejlesztői környezet és libgpiod fejlesztői/statikus fájlok kellenek:

```sh
cmake -S firmware -B build/linuxcnc-firmware -DBOARD=pico -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_PROFILE=linuxcnc -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build/linuxcnc-firmware --parallel

cmake -S hal-driver -B build/hal-regression -DBUILD_TESTING=ON
cmake --build build/hal-regression --parallel
ctest --test-dir build/hal-regression --output-on-failure
```

Ezek a Mach3 fordításához opcionálisak; a Windows firmware/DLL fejlesztéshez
nem szükséges miattuk Linux gép.

## 7. Hibaelhárítás és ellenőrzési státusz

- Hiányzó Pico SDK: a tényleges gyökerét add meg `-DPICO_SDK_PATH` alatt,
  és inicializáld a submodule-okat. Ez nem a Mach3 SDK útvonala.
- Hiányzó WIZnet források Windowson: konfigurálás előtt bontsd ki a
  `firmware/ioLibrary_Driver.zip` fájlt; a tartalék script bash/unzip eszközöket igényel.
- Hiányzó `arm-none-eabi-gcc`, Ninja, Python vagy SDK segédeszköz:
  javítsd a Pico fejlesztői terminál eszközútvonalait.
- A DLL konfigurálása elutasítva: Windows, MSVC, x86 MFC és `-A Win32` kell;
  a MinGW vagy egy Linux keresztfordító nem biztosítja a szükséges MFC környezetet.
- Hiányzó Mach3 SDK fájlok: a `MACH3_SDK_DIR` az `SDK` könyvtárra mutasson,
  ne annak szülőkönyvtárára.
- Generátor/platform eltérés: más generátorhoz vagy architektúrához új
  buildkönyvtárat használj.

Rögzített Linux eredmények, 2026-10-14: 11/11 Mach3 teszt és 32/32 LinuxCNC HAL
regressziós teszt sikeres; mindkét Pico/W5500 firmware-profil lefordult.
A Windows firmware/DLL parancsok és a valódi hardver működése még ellenőrizendő.
A tervezett bináris release a valódi Mach3- és hardvertesztek után következik.
