# Saját Breakout Board készítése a stepper-ninjához

Ez az útmutató elmagyarázza, hogyan tervezz és integráld a saját egyedi breakout boardodat a stepper-ninjába — az elektromos tervezéstől a firmware/HAL integráción át a LinuxCNC üzembe helyezéséig.

## 1. Mit fogsz építeni

Az egyedi board ebben a projektben két szoftver oldalból áll:

- Firmware oldal Pico/Pico2-n:
  - Fájlminta: `firmware/modules/breakoutboard_<ID>.c`
  - Kezeli a fizikai hardvert (GPIO, I2C bővítők, DAC-ok, biztonsági állapot).
  - A nyers boardállapotot csomagmezőkbe képezi le.

- LinuxCNC HAL oldal:
  - Fájlminta: `hal-driver/modules/breakoutboard_hal_<ID>.c`
  - HAL pineket exportál, és a csomagmezőket azokhoz rendeli.
  - A HAL kimeneteket visszacsomagolja a firmware-nek küldött csomagmezőkbe.

Mindkét oldalt a `firmware/inc/config.h` és `hal-driver/config.h` fájlokban lévő `breakout_board` értéke választja ki.

## 2. Előfeltételek

- Működő stepper-ninja fordítási környezet.
- Telepített Pico SDK (vagy a `firmware/CMakeLists.txt` által már használható).
- LinuxCNC fejlesztői fejlécek/eszközök telepítve a HAL modul fordításához.
- Egy jól működő meglévő board referenciaként:
  - `firmware/modules/breakoutboard_1.c`
  - `hal-driver/modules/breakoutboard_hal_1.c`

## 3. Hardvertervezési ellenőrzőlista

Kódolás előtt döntsd el az alábbiak mindegyikét:

- Tápfeszültség sínek és logikai szintek:
  - 3,3 V-os logikai kompatibilitás a Pico GPIO-val.
  - Megfelelő szintillesztés/optoszeparátor ahol szükséges.

- Biztonsági viselkedés:
  - Pontosan definiáld, mit kell a kimeneteknek tenniük kommunikációs timeout/lekapcsolás esetén.
  - Gondoskodj arról, hogy az enable vonalak biztonságosan meghibásodjanak (motor enable ki, orsó letiltva, analóg kimenetek nullán).

- I/O költségvetés:
  - Digitális bemenetek/kimenetek száma.
  - Analóg kimenetek száma és DAC típusa.
  - Enkóder csatornák és index viselkedés.

- Busz/periféria kiosztás:
  - I2C busz/lábak bővítőkhöz/DAC-okhoz.
  - SPI/WIZnet bekötés és reset/interrupt vonalak.

- Jelintegritás:
  - Tartsd a step/dir jelvezetékeket rendezetten és rövidre.
  - Adj hozzá felhúzó/lehúzó ellenállásokat ahol a betöltési állapot fontos.

## 4. Új Board ID választása

Válassz egy még nem használt egész szám ID-t, például `42`.

Ezt az azonos ID-t fogod használni a következőkben:

- `firmware/modules/breakoutboard_42.c`
- `hal-driver/modules/breakoutboard_hal_42.c`
- `#define breakout_board 42` a konfigurációs fejlécekben
- Kiválasztási ágak a firmware és HAL fordítási útvonalakban

## 5. Firmware oldal integrációja

### 5.1 A firmware modul létrehozása

1. Másold a sablont:

```bash
cp firmware/modules/breakoutboard_user.c firmware/modules/breakoutboard_42.c
```

2. A `firmware/modules/breakoutboard_42.c` fájlban módosítsd:

```c
#if breakout_board == 255
```

erre:

```c
#if breakout_board == 42
```

3. Implementáld a szükséges callback-eket:

- `breakout_board_setup()`:
  - Inicializálja a GPIO/I2C/SPI eszközöket a boardon.
  - Bővítőket és DAC-okat keres és konfigurál.

- `breakout_board_disconnected_update()`:
  - Minden kimenetet biztonságos állapotba kényszerít.
  - A DAC kimeneteket nullára vagy biztonságos torzítóra állítja.

- `breakout_board_connected_update()`:
  - A fogadott csomagból alkalmazza a kimeneteket a hardverre.
  - Frissíti a hardverből vett bemeneti pillanatképeket.

- `breakout_board_handle_data()`:
  - Kitölti a kimenő csomagmezőket (`tx_buffer->inputs[]`, stb.).
  - Feldolgozza a bejövő mezőket (`rx_buffer->outputs[]`, analóg adatok).

### 5.2 A modul hozzáadása a firmware fordításhoz

Szerkeszd a `firmware/CMakeLists.txt` fájlt, és add hozzá az új forrásfájlt az `add_executable(...)` részhez:

```cmake
modules/breakoutboard_42.c
```

### 5.3 Board makrók definiálása a firmware konfigurációs láblécében

Szerkeszd a `firmware/inc/footer.h` fájlt, és adj hozzá egy új blokkot:

```c
#if breakout_board == 42
    // saját lábkiosztásod és board-specifikus makróid
    // példák:
    // #define I2C_SDA 26
    // #define I2C_SCK 27
    // #define I2C_PORT i2c1
    // #define MCP23017_ADDR 0x20
    // #define ANALOG_CH 2
    // #define MCP4725_BASE 0x60

    // felülírja a stepgen/encoder/in/out/pwm makrókat szükség szerint
#endif
```

Stílus referenciáként használd az 1, 2, 3, 100 azonosítókhoz tartozó meglévő blokkokat.

### 5.4 A board kiválasztása a firmware konfigurációban

Szerkeszd a `firmware/inc/config.h` fájlt:

```c
#define breakout_board 42
```

## 6. HAL driver oldal integrációja

### 6.1 HAL board modul létrehozása

1. Másold a sablont:

```bash
cp hal-driver/modules/breakoutboard_hal_user.c hal-driver/modules/breakoutboard_hal_42.c
```

2. A `hal-driver/modules/breakoutboard_hal_42.c` fájlban módosítsd:

```c
#if breakout_board == 255
```

erre:

```c
#if breakout_board == 42
```

3. Implementáld:

- `bb_hal_setup_pins(...)`:
  - Exportálja az összes szükséges pint (`hal_pin_bit_newf`, `hal_pin_float_newf`, stb.).
  - Inicializálja az alapértelmezett értékeket.

- `bb_hal_process_recv(...)`:
  - Kicsomagolja az `rx_buffer->inputs[...]` és egyéb mezőket HAL kimeneti pinekbe.

- `bb_hal_process_send(...)`:
  - HAL parancs pineket becsomagolja a `tx_buffer->outputs[...]` és analóg mezőkbe.

### 6.2 A HAL board include regisztrálása

Szerkeszd a `hal-driver/stepper-ninja.c` fájlt a board-kiválasztási include részben, és add hozzá:

```c
#elif breakout_board == 42
#include "modules/breakoutboard_hal_42.c"
```

### 6.3 Board ID kiválasztása a HAL konfigurációban

Szerkeszd a `hal-driver/config.h` fájlt:

```c
#define breakout_board 42
```

A firmware és HAL `breakout_board` ID-knek azonosnak kell lenniük.

## 7. Csomagkiosztási szabályok (Fontos)

Használj stabil szerződést mindkét oldal között.

- Firmware oldal géptől-hoszt adatot ír:
  - `tx_buffer->inputs[0..3]`
  - Enkóder visszajelzés/jitter mezők

- HAL oldal ezeket HAL pinekbe olvassa a `bb_hal_process_recv()` függvényben.

- HAL oldal hoszt-firmware parancsokat ír:
  - `tx_buffer->outputs[0..1]`
  - Analóg és opcionális vezérlő mezők

- Firmware oldal ezeket a parancsokat az `rx_buffer`-en keresztül olvassa, és a `breakout_board_connected_update()` függvényben alkalmazza.

Bármilyen eltérés itt felcserélt pineket vagy holt I/O-t okoz. A fejlesztés során tartsd fenn a kiosztási táblázatot megjegyzésekként.

## 8. Firmware fordítása és flashelése

A tároló gyökeréből:

```bash
cd firmware
cmake -S . -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500
cmake --build build -j"$(nproc)"
ls build/*.uf2
```

Ha Pico v1-et használsz, állítsd `-DBOARD=pico`-ra.

Flashelés:

1. Tartsd lenyomva a BOOTSEL gombot, miközben csatlakoztatod a Picót.
2. Másold a generált `.uf2` fájlt a felcsatolt RPI-RP2 meghajtóra.
3. Indítsd újra, és nyisd meg a soros kimenetet az inicializálási naplók ellenőrzéséhez.

## 9. A LinuxCNC HAL modul fordítása és telepítése

A tároló gyökeréből:

```bash
cd hal-driver
cmake -S . -B build-cmake
cmake --build build-cmake --target stepper-ninja
sudo cmake --install build-cmake --component stepper-ninja
```

Ez telepíti a `stepper-ninja.so` fájlt a LinuxCNC modul könyvtárba.

## 10. LinuxCNC HAL üzembe helyezés (minimális)

Példa váz a HAL fájlodban:

```hal
loadrt stepper-ninja ip_address="192.168.0.177:8888"
addf stepgen-ninja.0.watchdog-process servo-thread
addf stepgen-ninja.0.process-send     servo-thread
addf stepgen-ninja.0.process-recv     servo-thread

# Példa board I/O net-ek (a nevek a bb_hal_setup_pins implementációdtól függnek)
# net estop-in      stepgen-ninja.0.inputs.0     => some-signal
# net coolant-out   some-command                 => stepgen-ninja.0.outputs.0
```

Ezután ellenőrizd a `halshow`/`halcmd show pin stepgen-ninja.0.*` parancsokkal.

## 11. Üzembe helyezési eljárás (Ajánlott)

Kövesd pontosan ezt a sorrendet:

1. Csak tápellátás teszt:
  - Ellenőrizd a feszültségszinteket, nincs túlmelegedés, nincs túlzott üresjárati áram.

2. Busz felderítési teszt:
  - Erősítsd meg, hogy az elvárt I2C címeket észleli a firmware.

3. Lekapcsolási biztonsági teszt:
  - Állítsd le a LinuxCNC kommunikációt.
  - Erősítsd meg, hogy a kimenetek és analóg csatornák biztonságos állapotba kerülnek.

4. Bemeneti polaritás teszt:
  - Kapcsolj minden fizikai bemenetet, és ellenőrizd a megfelelő HAL pin állapotát.
  - Ellenőrizd a `-not` komplementereket ha exportálva vannak.

5. Kimenet teszt:
  - Kapcsolj minden HAL kimeneti pint, és ellenőrizd a fizikai kimenetet.

6. Enkóder teszt:
  - Forgass lassan és gyorsan, ellenőrizd a számlálási irányt és az index kezelést.

7. Mozgás teszt lekapcsolt motorral:
  - Először ellenőrizd a step/dir polaritást és az impulzus időzítést.

8. Mozgás teszt csatlakoztatva:
  - Kezdj konzervatív sebességgel/gyorsulással.

## 12. Hibaelhárítás

- Nincs I/O válasz:
  - Ellenőrizd, hogy a `breakout_board` ID egyezik-e a firmware és HAL konfigurációkban.
  - Ellenőrizd, hogy az új fájljaid szerepelnek-e a fordítási útvonalakon.

- Fordítás sikeres, de a board callback-ek nem aktívak:
  - Ellenőrizd, hogy a `#if breakout_board == 42` feltétel helyes.
  - Ellenőrizd, hogy a lábléc/konfiguráció ág létezik és lefordul.

- Eltolt/helytelen bitrendű bemenetek:
  - Ellenőrizd a bit csomagolást/kicsomagolást a következők között:
    - `firmware/modules/breakoutboard_42.c`
    - `hal-driver/modules/breakoutboard_hal_42.c`

- Véletlenszerű lekapcsolási viselkedés:
  - Ellenőrizd a timeout-biztos kódot a `breakout_board_disconnected_update()` függvényben.
  - Ellenőrizd a hálózati stabilitást és a csomag watchdog bekötését.

- Analóg kimenet vágás/túlcsordulás:
  - Korlátozd a HAL értékeket a DAC szavak csomagolása előtt.
  - Ellenőrizd a skálázást a DAC felbontásával szemben.

## 13. Verziókövetési ajánlások

Minden egyedi board változáskészlethez, kövesd be ebben a sorrendben:

1. Firmware modul + lábléc/konfiguráció frissítések.
2. HAL modul + include/konfiguráció frissítések.
3. LinuxCNC HAL konfiguráció frissítések.
4. Dokumentáció a bekötéshez és pin kiosztáshoz.

Tarts fenn egy board-specifikus markdown fájlt a következőkkel:

- Kapcsolási rajz hivatkozás
- Pin kiosztás táblázat
- I2C cím térkép
- Biztonságos állapot szabályzat
- HAL pin elnevezési szerződés

## 14. Gyors fájl-ellenőrzőlista

Az alábbi fájlokat kell módosítanod az új board ID-hez:

- `firmware/modules/breakoutboard_42.c`
- `firmware/CMakeLists.txt`
- `firmware/inc/footer.h`
- `firmware/inc/config.h`
- `hal-driver/modules/breakoutboard_hal_42.c`
- `hal-driver/stepper-ninja.c`
- `hal-driver/config.h`
- (opcionális) `docs/a-te-board-neved.md`

Ha ezek teljesek és az ID-k konzisztensek, az egyedi breakout board integráció általában könnyen megvalósítható.
