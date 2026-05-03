# stepper-ninja

Nyílt forráskódú, ingyenes, nagy teljesítményű step-generátor, kvadratúrás enkóderszámláló, digitális I/O és PWM interfész LinuxCNC-hez.

Az encoder modul lehetővé teszi a főorsó-szinkron mozgások használatát is, például a menetvágást és más főorsóhoz szinkronizált mozgásokat.

A stepper-ninja használatához nincs szükség a hivatalos breakout boardra. Egy olcsó printer-port breakout board is elég lehet, és más konfigurációk is használhatók.

![official breakout board](docs/20250812_165926.jpg)

## Dokumentáció

- [Telepítési útmutató](docs/INSTALL.hu.md)
- [Konfigurációs útmutató](docs/CONFIG.hu.md)
- [IP konfiguráció](docs/IPCONFIG.hu.md)
- [Saját breakout board készítése](docs/MAKE-YOUR-OWN-BREAKOUTBOARD.hu.md)

## Nyelvek

- [English](README.md)
- [Deutsch](README.de.md)
- [हिन्दी](README.hi.md)
- [Magyar](README.hu.md)
- [Português (Brasil)](README.pt-BR.md)

## Főbb jellemzők

- **Támogatott konfigurációk**:

  - W5100S-evb-pico UDP Ethernet
  - W5500-evb-pico
  - W5100S-evb-pico2
  - W5500-evb-pico2
  - pico + W5500 modul
  - pico2 + W5500 modul
  - pico + Raspberry Pi4 SPI kapcsolaton
  - pico2 + Raspberry Pi4 SPI kapcsolaton
  - pico + PI ZERO2W SPI kapcsolaton
  - pico2 + PI ZERO2W SPI kapcsolaton
  - hivatalos Stepper-Ninja breakout board

- **Breakout-board v1.0 - digitális verzió**: 16 optocsatolt bemenet, 8 optocsatolt kimenet, 4 step-generátor, 2 nagysebességű enkóderbemenet, 2 unipoláris 12 bites DAC kimenet.

- **step-generator**: maximum 8 Pico 1 esetén, maximum 12 Pico2 esetén. Csatornánként akár 1 MHz. Az impulzusszélesség HAL pinről állítható.

- **quadrature-encoder**: maximum 8 Pico1 esetén, maximum 12 Pico2 esetén. Nagy sebesség, indexkezelés, sebességbecslés és főorsó-szinkron mozgások támogatása.

- **digitális I/O**: a Pico szabad lábai bemenetként és kimenetként konfigurálhatók.

- **PWM**: legfeljebb 16 GPIO használható PWM kimenetként.

- **Szoftver**: LinuxCNC HAL driver több példánnyal és biztonsági funkciókkal.

- **Open source**: a kód és a dokumentáció MIT licenc alatt érhető el.

## PIO-konfiguráció és erőforrás-korlátok

A Raspberry Pi Pico és Pico2 **PIO-blokkokat (Programmable I/O)** használ a step-generátorok és encoder-számlálók hardveres megvalósításához. Mivel a PIO-utasításmemória rendkívül korlátozott, **a step-generátorok és az encoderek nem használhatók egyszerre** — az egyik engedélyezése letiltja a másikat.

### RP2040 (Pico)

- 2 PIO-blokk (PIO0 és PIO1), mindkettőben **4 állapotgép** és **32 utasításhely**
- Step-generátor PIO-program: ~20 utasítás állapotgépenként
- Encoder PIO-program: ~15–18 utasítás állapotgépenként
- **4 step-generátor** teljesen kitölt egy PIO-blokkot — nem marad hely encoder számára ugyanabban a blokkban
- **4 encoder** teljesen kitölt egy PIO-blokkot — nem marad hely step-generátor számára
- Maximum: **8 step-generátor** VAGY **8 encoder** (mindkét PIO-blokk felhasználásával), soha nem vegyítve

### RP2350 (Pico2)

- 2 PIO-blokk, mindkettőben **4 állapotgép** és **32 utasításhely** (azonos struktúra, mint az RP2040)
- Az RP2350 rendelkezik egy további **PIO2** blokkal, amely 4 további állapotgépet tartalmaz
- Maximum: **12 step-generátor** VAGY **12 encoder** (mindhárom PIO-blokk felhasználásával)
- A step-generátorok és encoderek keveredése továbbra sem lehetséges — minden funkció PIO-programja teljesen kitölti a blokkot

### Módok közötti váltás

Szerkeszd a `firmware/inc/config.h` fájlt a kívánt mód beállításához:

```c
#define stepgens 4   // step-generátorok száma (0 = csak encoder mód)
#define encoders 0   // encoderek száma (0 = csak step-generátor mód)
```

A beállítás módosítása után build-eld újra a firmware-t, és flasheld a Picóra.

### Encoder PIO-változatok

Két encoder-implementáció áll rendelkezésre:

- **`ENCODER_PIO_SUBSTEP`** (alapértelmezett): sub-step interpoláció a sima sebességbecsléshez (~18 utasítás)
- **`ENCODER_PIO_LEGACY`**: egyszerű kvadratúra-számlálás, kissé kisebb (~15 utasítás)

Legacy encoderrel való build:
```bash
CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY' cmake ..
```

---

## Licenc

- A kvadratúrás encoder PIO program a Raspberry Pi (Trading) Ltd. BSD-3 licencét használja.
- Az `ioLibrary_Driver` a Wiznet MIT licencével érhető el.