# Stepper-Ninja konfigurációs útmutató

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
| `raspi_int_out` | Interrupt/státusz pin a Raspberry Pi felé |
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
| `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` | Futás előtt SRAM-ba másolja a firmware-t |

Példa:

```bash
cmake -S firmware -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_RUN_FROM_RAM=ON
```

---

## Javasolt munkafolyamat

1. Válaszd ki a boardot a `breakout_board` segítségével.
2. Állítsd be a `stepgens`, `encoders` értékeket és a pin-listákat custom módban.
3. Válaszd ki az `encoder_pio_version` értékét.
4. Csak akkor engedélyezd a PWM-et, ha a hardvered igényli.
5. Tartsd összhangban a firmware és a HAL driver konfigurációját.
6. Minden érdemi konfigurációváltozás után fordítsd újra a firmware-t és a HAL drivert.

Futásidejű hálózati beállításokhoz lásd még: [IPCONFIG.hu.md](IPCONFIG.hu.md).