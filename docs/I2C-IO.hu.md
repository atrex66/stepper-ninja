# Konfigurálható I²C digitális I/O — prototípus

Az új, opcionális modul egyedi LinuxCNC konfigurációhoz (`breakout_board=0`)
ad egyszerű digitális bemeneteket és kimeneteket. A fix breakout boardok továbbra
is a saját, eddigi kódjukon működnek. Ez a prototípus Mach3-hoz még nem használható.

Támogatott típusok: **MCP23008, MCP23017, TCA9535 és TCA9555**. Legfeljebb két
hardveres I²C-busz és összesen két bővítő konfigurálható. Több eszköz használhat
közös buszt, különböző címekkel. Minden bővítő 16 bites csomaghelyet foglal,
az MCP23008 is. Mellette legfeljebb 32 közvetlen digitális kimenet használható;
a Pico mindkét natív GPIO-bemeneti bankja megmarad.

## Visszafelé kompatibilitás

A `footer.h` `#ifndef` védelemmel kikapcsolt/üres alapértékeket ad. A meglévő
konfigurációkat nem kell módosítani; az új modul kikapcsolva nem inicializál
buszt, nem foglal GPIO-t és nem hoz létre új HAL pineket. A régi `io_expanders`
beállítást nem értelmezi át. Az adatcsomag mérete és formátuma változatlan.

A bekapcsolás fix breakout boarddal vagy Mach3 profillal fordítási hiba.
A hibás eszközszámot és a kimeneti csomaghely túllépését is ellenőrizzük.
Hibás busz-/eszközleírás vagy GPIO-ütközés esetén a firmware nem inicializálja
az új buszokat; az állapotjelzések hamisak maradnak, soros diagnosztika készül.

## Beállítás

A teljes, kétbuszos [példakonfiguráció](../configurations/I2C-IO/config.h)
nem engedélyez motorokat vagy enkódereket. Natív bemenetei GPIO22/23, kimenete
GPIO24, billentyűmátrix nélkül. Ez kiindulási minta, nem bármely kártyára
változtatás nélkül ráilleszthető konfiguráció.

Az `I2C_IO_ENABLE`, `I2C_IO_BUS_COUNT`, `I2C_IO_BUSES`, `I2C_IO_DEVICE_COUNT`
és `I2C_IO_DEVICES` beállítások a `footer.h` beillesztése elé kerüljenek.
A firmware-t és a HAL drivert azonos konfigurációval kell újrafordítani.

A busz mezői sorrendben: vezérlő (0/1), SDA GPIO, SCL GPIO, sebesség Hz-ben
(maximum 400000), egy SDK-művelet időkorlátja µs-ban (1–10000).
A GPIO-k legyenek a vezérlőhöz érvényes, elérhető, szabad I²C-lábak.
A modul ellenőrzi az ütközést a natív I/O-val, STEP/DIR-rel, enkóderrel,
indexszel, PWM-mel, Pico-billentyűmátrixszal, LED-del és a kommunikációs lábakkal.
Az SDA/SCL belső felhúzását engedélyezi; a tényleges buszhoz megfelelő külső
felhúzók is szükségesek. A példa a Pico buszán 3,3 V-os logikát feltételez.

Az eszköz mezői sorrendben:

| Mező | Jelentés |
|---|---|
| Típus | `SN_IO_MCP23008`, `SN_IO_MCP23017`, `SN_IO_TCA9535`, `SN_IO_TCA9555` |
| Busz | A busztábla indexe, nullától |
| Cím | 7 bites I²C-cím, 0x20–0x27 |
| Irány | 1 = bemenet, 0 = kimenet |
| Felhúzás | MCP bemeneti felhúzómaszk; TCA-nál nulla |
| Bemeneti invertálás | A bemeneti bitekre alkalmazott szoftveres invertálás |
| Kimeneti invertálás | A kimeneti bitekre alkalmazott szoftveres invertálás |
| Indulási/kapcsolatvesztési állapot | Logikai kimeneti érték, invertálás előtt |
| Frissítés | A lekérdezések kezdetei közti minimális idő ms-ban, 1–60000 |

MCP23008 esetén a maszkok 8 bitesek, a többinél 16 bitesek. Az alsó bájt
GPA/P0, a felső GPB/P1. Az aktuális Microchip adatlap szerint MCP23017-en
GPA7/GPB7 csak kimenetként használható: az új modul ezeken a biteken elutasítja
a bemeneti irányt. A régi breakout boardok kezelését ez nem változtatja meg.
TCA9535-ben nincs belső I/O-felhúzó, TCA9555-ben pedig a bemeneti felhúzás fix,
nem az MCP-knél megszokott módon kapcsolható. A nulla felhúzómaszk TCA9555-nél
nem kapcsolja ki a belső felhúzást.

## HAL és hibakezelés

Például a nulladik komponens nulladik bővítőjén:

- `stepgen-ninja.0.expander.0.input.0`, illetve `input.0-not`: fizikai bemenet, HAL_OUT.
- `stepgen-ninja.0.expander.0.output.4`: fizikai kimenet parancsa, HAL_IN.
- `stepgen-ninja.0.expander.0.online`, `.valid`: HAL_OUT állapotjelzés.

Csak a láb beállított irányának megfelelő pin jön létre. A bővítő sorszáma az
eszköztábla sorrendje, a lábszám a fizikai port bitje (a felső porton 8–15).
A natív GPIO-k HAL-nevei megmaradnak. A HAL-kimenetek a konfigurált logikai
indulási állapotot kapják.

Az `online` és `valid` teljes, sikeres I/O-ciklus után igaz. Hibás vagy rövid
átvitelkor mindkettő hamis lesz; a következő lekérdezés teljes újrainicializálást
próbál. A bemenet ilyenkor az utolsó sikeres mintát tartja: az érvényességjelzést
is figyelni kell. A host watchdog visszajelzés hiányában törli az állapotpineket.
A firmware kapcsolat nélkül is mintavételez és újrapróbálkozik.

Az I²C-műveletek az 1-es mag főciklusában futnak, a lépés-/enkódermegszakításokon
kívül. A csomagkezelés csak rövid zárolással pillanatképet másol, buszműveletet
nem végez. Az egyes átvitelek időkorlátosak, de egy ciklus több műveletből áll;
a frissítési periódus nem garantált válaszidő. Lassú/hibás busz az 1-es magot
késleltetheti. Ez általános digitális I/O, nem garantált időzítésű jelkezelés.

Induláskor és újrainicializáláskor előbb a kimeneti tároló kapja meg a kívánt
állapotot, utána kapcsoljuk be a kimeneti irányt. A modul először bemeneti
irányra engedi el a portokat. MCP-knél bekapcsolási BANK=0 regiszterkiosztást
feltételez; más firmware után reset/tápújraindítás szükséges. RESET- és
megszakításláb-kezelés nincs, a mintavétel periodikus lekérdezéssel történik.
Host-kapcsolatvesztéskor a modul a normál periódus kivárása nélkül megpróbálja
kiírni a konfigurált kimeneti állapotot. **Hibás buszon a kiírás nem garantálható**;
a chip addig a saját hardveres állapotát tartja.

A csomag leképezése és a részletes mezőleírás az [angol dokumentumban](I2C-IO.en.md)
található. Röviden: a natív kimenetek az `outputs[0]`, a bővítők az `outputs[1]`
szóban vannak. A natív GPIO-bemenetek `inputs[0..1]`, a bővítők `inputs[2]`,
az állapotok `inputs[3]` (online: 0/1 bit, valid: 8/9 bit).

## Ellenőrzés

A LinuxCNC CTest-csomag része a regiszterkezelési és HAL-teszt. Külön is futtathatók:

```sh
python3 common/tests/i2c_io_contract.py cc
python3 common/tests/i2c_io_hal_contract.py --include /usr/include/linuxcnc
```

A tesztek a valódi meghajtókódot futtatják szimulált I²C-eszközökkel, illetve
valódi LinuxCNC fejlécekkel és teszt által biztosított HAL-tárolóval. Lefedik a
négy típust, az inicializálási sorrendet, invertálást, mintavételt, buszhibát,
rövid olvasást, helyreállást, kapcsolatvesztést, GPIO-ütközést, régi konfigurációk
kikapcsolt alapértékeit és a HAL-pinek bitkiosztását/létrehozási hibáit.
**Fizikai I²C- és kártyateszt még nem történt.**

Források: [Microchip MCP23017 adatlap](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf),
[TI TCA9535](https://www.ti.com/product/TCA9535),
[TI TCA9555 adatlap](https://www.ti.com/lit/ds/symlink/tca9555.pdf).
