# Konfigurálható I²C DAC-kimenetek — prototípus

A [test ágon](https://github.com/atrex66/stepper-ninja/tree/test) elérhető funkció
analóg feszültségkimeneti parancsokat ad egyedi LinuxCNC konfigurációkhoz.
A fix breakout boardok eddigi DAC-kezelése változatlan. Mach3-támogatás és
**fizikai DAC-/kimeneti próba még nincs**.

## Támogatott típusok és árak

| Gyártó | Típus | Felbontás | Kimenet/IC |
|---|---|---|---|
| Microchip | MCP4725 | 12 bit | 1 |
| Texas Instruments | DAC5571 / DAC6571 / DAC7571 | 8 / 10 / 12 bit | 1 |
| Analog Devices | AD5337 / AD5338 / AD5339 | 8 / 10 / 12 bit | 2 |

2026-11-20-án ellenőrzött árpéldák: az [Adafruit MCP4725 modul](https://www.adafruit.com/category/42)
listázott ára 4,95 USD; az [LCSC DAC7571](https://www.lcsc.com/product-detail/C118112.html)
1 darabos IC-ára 3,3691 USD. Az ADI gyártói, **1000 darabos listaára**
[AD5337-nél](https://www.analog.com/en/products/ad5337.html) 2,31 USD-től,
[AD5338-nál](https://www.analog.com/en/products/ad5338.html) 3,24 USD-től,
[AD5339-nél](https://www.analog.com/en/products/ad5339.html) 6,67 USD-től indul.
Ezek eltérő mennyiségek és IC/modul ajánlatok, nem szállított végösszegek vagy
készletgaranciák. Nyolc/tíz bithez a kétcsatornás AD5337/38 is érdekes lehet.

A Microchip az [MCP4725-öt](https://www.microchip.com/en-us/product/mcp4725) új
tervekhez már nem ajánlja, helyette MCP4726-ot javasol. **MCP4726-meghajtó ebben
a prototípusban nincs.** A meglévő MCP4725 modulok támogatása ettől hasznos.
Az olcsó modulokról ismert [NXP PCF8591](https://www.nxp.com/products/PCF8591)
gyártói státusza End of Life; itt csak összehasonlításként szerepel, meghajtót
vagy ADC-támogatást nem kapott.

## Konfiguráció

A [teljes példában](../configurations/I2C-DAC/config.h) négy DAC-csatorna van
három IC-n, mellettük két digitális bővítő és natív GPIO, közös buszon.
Motorok, enkóderek és billentyűmátrix nincsenek engedélyezve. A GPIO0/1 itt
szabad példalábak; egy meglévő gépkonfiguráció kiosztásához hozzá kell igazítani.

A beállítások a `#include "footer.h"` elé kerüljenek:

```c
#define I2C_IO_BUS_COUNT 1
#define I2C_IO_BUSES {{0, GP00, GP01, 400000, 1000}}
#define I2C_DAC_ENABLE 1
#define I2C_DAC_CHANNEL_COUNT 4
#define I2C_DAC_CHANNELS { \
    {SN_DAC_MCP4725, 0, 0x60, 0, 0, 10}, \
    {SN_DAC_DAC7571, 0, 0x4c, 0, 0, 10}, \
    {SN_DAC_AD5339, 0, 0x0c, 0, 0, 10}, \
    {SN_DAC_AD5339, 0, 0x0c, 1, 1024, 10} \
}
```

Az `I2C_IO_BUS_COUNT` és `I2C_IO_BUSES` elnevezés kompatibilitási okból marad;
ez közös busztábla. A digitális bővítőket az `I2C_IO_ENABLE` kapcsolja,
DAC-only használathoz ez nulla maradhat. A busz mezői: vezérlő (0/1), SDA,
SCL, sebesség Hz-ben (legfeljebb 400000), SDK-műveletenkénti timeout µs-ban
(1–10000). Legfeljebb két hardveres busz használható.

A DAC-leíró mezői: típus, busztábla-index, 7 bites cím, fizikai csatorna,
indulási/letiltási/kapcsolatvesztési nyers kód, frissítés ms-ban (1–60000).
Típusnevek: `SN_DAC_MCP4725`, `SN_DAC_DAC5571`, `SN_DAC_DAC6571`,
`SN_DAC_DAC7571`, `SN_DAC_AD5337`, `SN_DAC_AD5338`, `SN_DAC_AD5339`.

Legfeljebb **négy csatornaleíró** lehet. Az egykimenetes IC-k csatornája nulla,
AD533x esetén 0=A, 1=B. Ugyanazon AD533x két kimenetéhez két azonos
busz/cím/típus bejegyzés kell, eltérő csatornával. A nem konfigurált másik
kimenetet a driver nem vezérli. Ismételt csatorna, ütköző eszköz és hibás cím
elutasításra kerül. Címek: MCP4725 0x60–0x67 a tényleges változat szerint;
TI 0x4c/0x4d; AD533x 0x0c/0x0d, AD5338-1 esetén 0x22/0x23 is használható.

A footer alapértékei: `I2C_DAC_ENABLE=0`, `I2C_DAC_CHANNEL_COUNT=0`,
`I2C_DAC_CHANNELS={}`. A régi konfigurációkat nem kell átírni. Bekapcsoláskor
az `ANALOG_CH` a csatornaszámot követi; ha külön megadod, egyeznie kell vele.
A fix boardok meglévő `ANALOG_CH`-kezelése és csomagkódolása nem változik.
DAC-only esetben nem foglaljuk le a második natív digitális kimeneti szót;
digitális bővítőkkel együtt azok korábbi, 32 natív kimenetes korlátja érvényes.

## HAL pinek és skálázás

| Pin, például a nulladik csatornán | Irány | Jelentés |
|---|---|---|
| `stepgen-ninja.0.dac.0.enable` | HAL_IN | Engedélyezés; induláskor hamis |
| `stepgen-ninja.0.dac.0.value` | HAL_IN float | Kért érték; alapérték 0 |
| `stepgen-ninja.0.dac.0.scale` | HAL_IN float | Pozitív teljes skála; alapérték 1 |
| `stepgen-ninja.0.dac.0.raw-command` | HAL_OUT u32 | Kiküldésre kért, kvantált kód; nem feszültségmérés |
| `stepgen-ninja.0.dac.0.online` | HAL_OUT | Az utolsó teljes írási ciklus ACK-val zárult |
| `stepgen-ninja.0.dac.0.valid` | HAL_OUT | Ebben a prototípusban az írási ciklus érvényessége |

Engedélyezve a véges érték a `0..scale` tartományra korlátozódik, majd a
`0..(2^bit-1)` kódtartományra képeződik, legközelebbi kódra kerekítve.
Letiltva, nem véges bemenetnél vagy nem pozitív/nem véges skálánál a konfigurált
indulási kód kerül a csomagba. `scale=1` normalizált parancsot jelent;
`scale=10` tízes teljes skálát, **nem automatikusan fizikai 0–10 V-ot**.
Ahhoz külön analóg illesztés kell. A táp/referencia, terhelés és analóg fokozat
határozza meg a tényleges kimenetet; a Pico GPIO nem 5 V-tűrő.

## Működés és ellenőrzés

A közös buszt egyszer inicializáljuk, a GPIO-foglalásokat ellenőrizzük, majd az
1-es mag főciklusa kezeli a digitális és DAC-eszközöket. A csomagkezelés csak
röviden zárolt pillanatképet másol; I²C-műveletet nem végez. A timeout minden
SDK-átvitelre külön érvényes, a frissítési periódus nem garantált válaszidő.

Induláskor és hibából visszatérve először a konfigurált kezdőkód íródik ki,
utána a host parancsa. Hibás/rövid írás törli az érvényességet; a következő
ciklus újrapróbálja. A többi eszköz kiszolgálása folytatódik. Kapcsolatvesztéskor
a modul a periódus kivárása nélkül megpróbálja kiírni a leállási kódokat.
Hibás buszon ez nem garantálható, a DAC a korábbi kimenetét tarthatja.
A kezdőkód lehet nem nulla is: a példa negyedik csatornáján 1024.

EEPROM-írás nincs, ezért MCP4725-nél a tárolt bekapcsolási érték a firmware
első sikeres írásáig megjelenhet. AD533x-nél nincs globális törlés; a megadott
csatornát azonnali frissítéssel írjuk. A két kimenet egymás után frissül,
nem szinkronban. ADC, feszültség-visszamérés, külső LDAC/reset, bipoláris kód
és tartós EEPROM-beállítás nincs implementálva.

Egyedi DAC-konfigurációban az `analog_out[n]` csatornánként egy nyers kód.
A meglévő konfigurációfüggő `ANALOG_CH` mező csatornánként négy bájttal növeli
a kéréscsomagot. **Azonos konfigurációval fordított firmware és HAL kell.**
A válasz mérete nem változik: `inputs[3]` online bitek 16–19, valid bitek 24–27;
a digitális bővítők 0/1 és 8/9 bitje megmarad. A host watchdog törli a HAL
állapotpineket. Az ACK nem igazolja a tényleges analóg feszültséget.

A tesztek a hét bájtkódolást, a csatornaválasztást, skálázást, letiltást,
buszhibát, rövid írást, helyreállást, digitális bővítőkkel közös használatot,
a régi konfigurációkat és a HAL-pinek létrehozási hibáit ellenőrzik.
Futtatásuk és az elsődleges adatlapok az [angol útmutatóban](I2C-DAC.en.md)
találhatók. A fordítás és a szoftveres teszt nem helyettesíti a fizikai próbát.
