# Stepper-Ninja konfigurációs útmutató

Az új kártyakonfigurációk és az I²C-I/O prototípus a [test ágon](https://github.com/atrex66/stepper-ninja/tree/test) érhetők el. Fizikai hardveres ellenőrzésük még hátravan.

Ez az útmutató a `config.h` fájl fő `#define` opcióit és azok használatát magyarázza el.

A Stepper-Ninja két egymáshoz illeszkedő konfigurációs fejlécet használ:

- `firmware/inc/config.h`
- `hal-driver/config.h`

Általános használatban a `hal-driver/config.h` fájlnak a `firmware/inc/config.h` symlinkjének kell lennie, nem pedig külön karbantartott másolatnak.

Ha a symlinkek hiányoznak, a `hal-driver` könyvtárból ezzel hozhatod létre őket:

```bash
./make_symlinks.sh
```

Ez a script újra létrehozza a HAL driver által használt közös symlinkeket.

---

## Melyik fájlt módosítsd?

- A Pico firmware viselkedésének módosításához a `firmware/inc/config.h` fájlt szerkeszd.
- A `hal-driver/config.h` ennek a fájlnak symlinkje kell legyen.
- Ha a `hal-driver/config.h` nem symlink, hanem normál fájl, futtasd a `hal-driver/make_symlinks.sh` scriptet.

Megjegyzés: egyes értékeket a `footer.h` automatikusan felülír, ha a `breakout_board` nem `0`.

---

## Alapértelmezett hálózati értékek

Ezek az értékek határozzák meg az induló hálózati konfigurációt, amely a flash memóriában marad, amíg a soros terminálon át meg nem változtatod.

| Define | Jelentés |
|---|---|
| `DEFAULT_MAC` | Alapértelmezett Ethernet MAC-cím |
| `DEFAULT_IP` | Alapértelmezett statikus IP-cím |
| `DEFAULT_PORT` | A HAL driver által használt UDP port |
| `DEFAULT_GATEWAY` | Alapértelmezett átjáró |
| `DEFAULT_SUBNET` | Alapértelmezett alhálózati maszk |
| `DEFAULT_TIMEOUT` | Kommunikációs időtúllépés mikroszekundumban |

Ha később a soros terminál `ipconfig` parancsait használod, a futásidejű értékek felülírhatják a flash-ben tárolt alapbeállításokat.

---

## Board kiválasztása

A `breakout_board` a hardverelrendezést választja ki.

| Érték | Jelentés |
|---|---|
| `0` | Egyedi pin-kiosztás a `config.h` alapján |
| `1` | Stepper-Ninja breakout board |
| `2` | IO-Ninja breakout board |
| `3` | Analog-Ninja breakout board |
| `100` | BreakoutBoard100 |

Ha a `breakout_board` értéke nagyobb mint `0`, a `footer.h` több egyedi pin-definíciót boardspecifikus értékekre cserél.

### További kártyakonfigurációk

- [Expatria PicoBOB Deluxe A3 konfiguráció](../configurations/PicoBOB-DLX/picoBOB-DLX.h): javított W5500 RESET/INT-, LED- és PWM-kiosztás.
- [Phil Barrett RP23CNC V1.0/V2](../configurations/RP23CNC/README.hu.md): öt tengely, 14 bemenet, 12 kimenet, egy főorsó-PWM, opcionális WIZ850io/W5500. Fordítása: `-DBOARD=rp23cnc -DWIZCHIP_TYPE=W5500`; a `BOARD=pico2` másik, 30 GPIO-s MCU-változatot választ.

Ezek egyedi konfigurációk (`breakout_board=0`), nem új számozott breakout-board típusok. A kiválasztott mintát másold a `firmware/inc/config.h` helyére, majd azonos konfigurációval fordítsd újra a firmware-t és a HAL-t. A lábkiosztás és az opcionális `GP_NULL` LED/reset-kezelés kártyafüggő; ne másold át más kártyákra.

---

## Mozgáscsatornák

Ezek a definíciók adják meg, hány csatorna van és mely pineket használják.

| Define | Jelentés |
|---|---|
| `stepgens` | Step-generátor csatornák száma |
| `stepgen_steps` | Step kimeneti pinek |
| `stepgen_dirs` | Irányjelek pinek |
| `step_invert` | Step jel invertálása csatornánként |
| `encoders` | Enkóder csatornák száma |
| `enc_pins` | Minden kvadratúrás enkóderpár első pinje |
| `enc_index_pins` | Enkóder index pinek |
| `enc_index_active_level` | Az index bemenetek aktív szintje |

Minden enkódernél az `enc_pins` két GPIO-t használ: az `A` a megadott pinen van, a `B` pedig a következőn.

---

## Encoder mód

A firmware két encoder PIO megvalósítást támogat:

| Define | Jelentés |
|---|---|
| `ENCODER_PIO_LEGACY` | Az eredeti kvadratúrás számláló PIO |
| `ENCODER_PIO_SUBSTEP` | Az újabb, substep-aware PIO |
| `encoder_pio_version` | Kiválasztja, melyik encoder PIO forduljon |

Jellemző beállítás:

```c
#define encoder_pio_version ENCODER_PIO_SUBSTEP
```

Ha vissza akarsz térni a régi encoder PIO-ra:

```c
#define encoder_pio_version ENCODER_PIO_LEGACY
```

Jelenlegi működés:

- Az enkóder sebességbecslése a HAL driverben történik.
- Legacy módban a firmware ciklusonkénti enkóder számlálódeltát küld.
- Substep módban a firmware továbbra is nyers számlálóadatot küld, de a sebességkimenetet a HAL oldal állítja elő.

A mód build közben is felülírható:

```bash
CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY' cmake -S firmware -B build -DWIZCHIP_TYPE=W5500
```

---

## Digitális bemenetek és kimenetek

| Define | Jelentés |
|---|---|
| `in_pins` | Bemeneti GPIO-k listája |
| `in_pullup` | Pull-up engedélyezése bemenetenként |
| `out_pins` | Kimeneti GPIO-k listája |

Ezek az értékek csak custom módban használatosak, vagy ha a kiválasztott breakout board nem írja felül őket.

---

## Konfigurálható I²C digitális I/O (prototípus)

Opcionális, egyszerű digitális I/O egyedi **LinuxCNC** konfigurációkhoz (`breakout_board=0`). Támogatott típusok: `SN_IO_MCP23008`, `SN_IO_MCP23017`, `SN_IO_TCA9535`, `SN_IO_TCA9555`. A fix breakout boardok külön úton maradnak, a régi `io_expanders` jelentése változatlan. A prototípus Mach3-hoz nem használható.

| Define | Jelentés |
|---|---|
| `I2C_IO_ENABLE` | Az új modul engedélyezése; alapérték `0` |
| `I2C_IO_BUS_COUNT` | Buszok száma, bekapcsolva 1–2; alapérték `0` |
| `I2C_IO_BUSES` | Buszleírók listája; alapérték `{}` |
| `I2C_IO_DEVICE_COUNT` | Bővítők száma összesen, bekapcsolva 1–2; alapérték `0` |
| `I2C_IO_DEVICES` | Eszközleírók listája; alapérték `{}` |

A beállításokat a `#include "footer.h"` **elé** tedd. A footer `#ifndef` védelemmel ad alapértékeket: a meglévő konfigurációkhoz nem kell új szakasz, további buszt sem inicializálnak. A firmware és a HAL azonos beállításokkal fordítandó.

Példa: egy TCA9555, 0–7 bit bemenet, 8–15 bit kimenet, 10 ms-os lekérdezés:

```c
#define I2C_IO_ENABLE 1
#define I2C_IO_BUS_COUNT 1
#define I2C_IO_BUSES {{0, GP00, GP01, 400000, 1000}}
#define I2C_IO_DEVICE_COUNT 1
#define I2C_IO_DEVICES { \
    {SN_IO_TCA9555, 0, 0x20, 0x00ff, 0, 0, 0, 0, 10} \
}
```

A busz mezői sorrendben: vezérlő (0/1), SDA GPIO, SCL GPIO, sebesség Hz-ben (legfeljebb 400000), időkorlát SDK-műveletenként µs-ban (1–10000). Az eszköz mezői: típus, busztábla-index, 7 bites cím (0x20–0x27), iránymaszk (`1=bemenet`), felhúzómaszk, bemeneti invertálás, kimeneti invertálás, logikai indulási/kapcsolatvesztési kimeneti állapot, lekérdezési periódus ms-ban (1–60000).

A példában a GPIO0/1 legyen szabad: nem ütközhet STEP/DIR-, enkóder/index-, natív I/O-, PWM-, billentyűmátrix-, LED- vagy kommunikációs lábbal. A [teljes, csak I/O-t használó példa](../configurations/I2C-IO/config.h) kiindulási minta, nem bármely kártyára ráilleszthető kiegészítés.

- Legfeljebb két bővítő és 32 natív digitális kimenet. Bővítőnként 16 bites hely foglalt, MCP23008 esetén is. Mindkét natív GPIO-bemeneti bank megmarad. A csomagformátum változatlan.
- MCP23008 esetén 8, a többinél 16 bites maszkok használhatók. Az új útvonal MCP23017-en elutasítja a 7/15 bit bemeneti használatát (GPA7/GPB7, az aktuális adatlap szerint csak kimenet). A régi boardkezelés nem változik.
- TCA-nál a felhúzómaszk nulla. TCA9535-ben nincs belső I/O-felhúzó; a TCA9555 fix bemeneti felhúzását ez a mező nem kapcsolja ki.
- HAL-példák: `stepgen-ninja.0.expander.0.input.0`, `input.0-not`, `output.8`, `online`, `valid`. Csak a beállított irány jön létre. A fizikai bemenet/állapot HAL_OUT, a fizikai kimenet parancsa HAL_IN.
- A buszműveletek a step/enkóder IRQ-kon kívül futnak. Hibakor az `online`/`valid` hamis, a bemeneti adat az utolsó sikeres minta marad. Hibás buszon a kapcsolatvesztési kimeneti állapot kiírása nem garantálható.

Részletek, korlátok és tesztelés: [I²C-I/O útmutató](I2C-IO.hu.md). A fizikai I²C-próba még hátravan.

---

## PWM opciók

| Define | Jelentés |
|---|---|
| `use_pwm` | PWM támogatás engedélyezése |
| `pwm_count` | PWM kimenetek száma |
| `pwm_pin` | PWM kimeneti pinek |
| `pwm_invert` | PWM invertálása csatornánként |
| `default_pwm_frequency` | Alapértelmezett PWM frekvencia |
| `default_pwm_maxscale` | HAL skálázási határ |
| `default_pwm_min_limit` | Minimális PWM kimenet |

Csak akkor állítsd a `use_pwm` értékét `1`-re, ha a hardveredhez tartozó PWM csatornaszám és pinek érvényesek.

---

## Raspberry Pi SPI mód

| Define | Jelentés |
|---|---|
| `raspberry_pi_spi` | Wiznet Ethernet helyett SPI kapcsolat Raspberry Pi felé |
| `RASPI_INT_OUT_GPIO` | Interrupt/státusz pin a Raspberry Pi felé |
| `RASPI_SPI_CS_GPIO` | SPI chip-select GPIO a Raspberry Pi felé |
| `raspi_inputs` | A Raspberry Pi által látható bemenetek |
| `raspi_input_pullups` | Ezek pull-up beállításai |
| `raspi_outputs` | A Raspberry Pi által vezérelt kimenetek |

Ha a `raspberry_pi_spi` értéke `0`, a firmware a Wiznet Ethernet útvonalat használja.

---

## Időzítési alapértékek

| Define | Jelentés |
|---|---|
| `default_pulse_width` | Alapértelmezett step impulzusszélesség nanoszekundumban |
| `default_step_scale` | Alapértelmezett steps per unit érték |
| `use_timer_interrupt` | Engedélyezi a step parancs ring buffert és az időzítővezérelt step kimenetet |

A `use_timer_interrupt` csökkentheti a PC és a Pico közötti látható jittert a step parancsok pufferelésével.

---

## A `footer.h` által vezérelt definíciók

Néhány fontos define a `footer.h` fájlban található, mert board- vagy platformfüggő.

| Define | Jelentés |
|---|---|
| `use_stepcounter` | Kvadratúrás enkóder helyett step számlálót használ |
| `debug_mode` | További debug viselkedés Raspberry Pi kommunikációnál |
| `max_statemachines` | A PIO state machine-ek teljes, származtatott száma |
| `pico_clock` | A firmware által használt Pico rendszeróra |

Ezeket ne módosítsd könnyelműen, ha nem ismered az időzítésre és a PIO kiosztásra gyakorolt hatásukat.

---

## Build opciók a `config.h`-n kívül

Néhány hasznos opció CMake-ben választható ki, nem a `config.h`-ban.

| Opció | Jelentés |
|---|---|
| `-DWIZCHIP_TYPE=W5100S` | Build W5100S-hez |
| `-DWIZCHIP_TYPE=W5500` | Build W5500-hoz |
| `-DBOARD=pico` | Build Picóhoz |
| `-DBOARD=pico2` | Build Pico2-höz |
| `-DBOARD=rp23cnc` | RP23CNC fordítása (48 GPIO-s RP2350B; megfelelő minta és W5500 kell) |
| `-DSTEPPER_NINJA_PROFILE=linuxcnc` / `mach3-3axis` | Host profil: `linuxcnc` (alapérték) vagy `mach3-3axis` (fix kiosztás) |
| `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` | Futás előtt SRAM-ba másolja a firmware-t |

Példa:

```bash
cmake -S firmware -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_RUN_FROM_RAM=ON
```

A Mach3 prototípus saját fix háromtengelyes profilt használ, nem a LinuxCNC rugalmas GPIO-kiosztását. Lásd a [Mach3 leírást](MACH3.hu.md) és a [fordítási útmutatót](MACH3-BUILD.hu.md). A Pico firmware Windows alatt is fordítható.

---

## Javasolt munkafolyamat

1. Válaszd ki a boardot a `breakout_board` segítségével.
2. Állítsd be a `stepgens`, `encoders` értékeket és a pin-listákat custom módban.
3. Válaszd ki az `encoder_pio_version` értékét.
4. Csak akkor engedélyezd a PWM-et, ha a hardvered igényli.
5. Tartsd összhangban a firmware és a HAL driver konfigurációját.
6. Minden érdemi konfigurációváltozás után fordítsd újra a firmware-t és a HAL drivert.

Futásidejű hálózati beállításokhoz lásd még: [IPCONFIG.hu.md](IPCONFIG.hu.md).
