# StepperNinja – Mach3 USB csomag

Raspberry Pi Pico / RP2040 és Pico 2 / RP2350 USB firmware. Készült: 2026-10-14. Mozgásprotokoll: 3.
Fejlesztési változat; fizikai mozgásra még nem hitelesített.

## Tartalom és beállítások

- `firmware/`: USB-only UF2 firmware, WIZnet nélkül, Pico/RP2040 és Pico 2/RP2350 panelekhez.
- `driver/StepperNinja.dll`: 32 bites Windows Mach3 plugin.
- `driver/sn-mach3-usb-check.exe`: mozgás nélküli kapcsolatpróba.
- `profiles/StepperNinjaMill.xml`: maró, X/Y/Z, 1024.set képernyő.
- `profiles/StepperNinjaTurn.xml`: eszterga, X/Z, sugármód, 1024.lset képernyő.
- `macros/`: a két profil saját alapmakrói; M6End nem végez automatikus mozgást.
- `docs/Pico-bekotes-pinout.pdf`: kétnyelvű, vizuális bekötési rajz.
- `docs/Pico-wiring-pinout.en.pdf`: külön, teljesen angol nyelvű bekötési rajz.
- `pin-map.csv`, `package-info.json`, `SHA256SUMS.txt`: lábtábla, buildadatok, SHA256 összegek.

Mindkét profil: **100 steps/mm**, **10 000 mm/perc**, **250 mm/s²**, natív mm,
USB **COM12**. Egy STEP 0,01 mm. Kezdő jog: 10%, legfeljebb 1000 mm/perc.
Az eszterga X sugármódú (UseDiam=0); a kikapcsolt Y-hoz is érvényes tuningadatok
vannak, mert a driver mindhárom tengelyt ellenőrzi. Mindkét LPT port kikapcsolva.

## 1. Firmware feltöltése

1. Zárd be a Mach3-at és a soros terminált.
2. BOOTSEL nyomva tartása mellett csatlakoztasd USB-n a Picót.
3. Az RPI-RP2 meghajtóra másold a `firmware` alatti `.uf2` fájlt.
4. A Pico újraindul, a bootmeghajtó eltűnik. Az Eszközkezelőben nézd meg a COM-portot.

A COM-port panelenként és gépenként eltérhet; a Pico 2 ezen a gépen COM13. A `pico` UF2 RP2040-hez, a `pico2` UF2 RP2350-hez készült.
A csomag firmware-jét és DLL-jét együtt használd: a protokollverziónak egyeznie kell.

## 2. A Mach3 fájljainak telepítése

Telepített Mach3 és az alap 1024.set / 1024.lset képernyők szükségesek.
Zárt Mach3 mellett készíts mentést a felülírandó fájlokról, majd másold:

| A csomagból | Telepítési hely |
|---|---|
| driver/StepperNinja.dll | C:\Mach3\PlugIns\StepperNinja.dll |
| profiles/StepperNinjaMill.xml | C:\Mach3\StepperNinjaMill.xml |
| profiles/StepperNinjaTurn.xml | C:\Mach3\StepperNinjaTurn.xml |
| macros/StepperNinjaMill teljes mappa | C:\Mach3\macros\StepperNinjaMill |
| macros/StepperNinjaTurn teljes mappa | C:\Mach3\macros\StepperNinjaTurn |

Más telepítési útvonal esetén a tényleges Mach3 könyvtárat használd.
A DLL x86, 64 bites Windowson is. Másik gépen szükség lehet a Microsoft
[Visual C++ v14 x86 futtatókörnyezetére](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).
Az eredeti Mach3Mill/Mach3Turn profilokat nem kell felülírni. Ez a gép már be van állítva.

## 3. Használat és kapcsolatpróba

A `launchers/Start-Mill.cmd` a marót, a `Start-Turn.cmd` az esztergát indítja
az alapértelmezett C:\Mach3 útvonalról. Ugyanez parancssorból:

```text
C:\Mach3\Mach3.exe /p StepperNinjaMill
C:\Mach3\Mach3.exe /p StepperNinjaTurn
```

Egyszerre egy profil használja a Picót. A plugin azonosítója
`StepperNinja_USB_UDP_v0_3_3axis`; a profilokban engedélyezett és kiválasztott
vezérlő. Ha a Mach3 mégis választást kér, a StepperNinját válaszd. Szükség esetén:
Function Cfg's → Reset Device Sel, újraindítás, majd StepperNinja választás.

Config → Config Plugins → StepperNinja CONFIG: **USB connection**, a tényleges
COM-port, **Test connection**. Beállítani Reset állapotban lehet. A külön EXE
csak zárt Mach3 mellett fusson, a csomag gyökeréből:

```powershell
.\driver\sn-mach3-usb-check.exe COM12
```

Csak CLAIM / POLL / RELEASE parancsot küld; nem konfigurál, nem élesít és nem
mozgat. Foglalt portnál zárd be a másik soros programot. A PDF a panel felső
nézetét mutatja, USB-vel felül. A fizikai lábszám nem azonos a GPIO-számmal.
Esztergán X=GP0/GP1, Z=GP4/GP5; a Y GP2/GP3 pár nincs használva.

## Bemenetenkénti invertálás

A Config → Config Plugins → StepperNinja CONFIG ablak **Inputs - polarity and live state**
részében mind a hét DI mellett külön **Invert** jelölőnégyzet van, a jel
nevével és GPIO-számával. Bejelölve aktív alacsony, üresen aktív magas.
A **Save** a jelenlegi profilba ment, és az adott bemenetre rendelt összes
Mach3 jel **Active Low** értékét is összehangolja (például X++, X-- és X Home).
A **Cancel** nem ment. A két gépprofil polaritása külön állítható.
A régi profilok InputInvert értékei automatikusan betöltődnek a hét kapcsolóba.

## 200 kHz-es Mach3 mód

A firmware és a DLL együtt frissítendő. A felső határ 200 lépés/ms/tengely,
200 000 lépés/s. A magas és alacsony STEP-időt egyaránt ellenőrzi.
1000 steps/mm és 10 000 mm/perc kb. 166,7 kHz: 2500 ns STEP-idővel használható.
A teljes 200 kHz-hez legfeljebb 2000 ns STEP-idő kell (DIR setup 5000 ns).
Hosszabb impulzust is lehet állítani, de kisebb sebesség szükséges hozzá.
A külön **StepperNinjaMill1000** és **StepperNinjaTurn1000** mintaprofil
1000 steps/mm, 10 000 mm/perc és 250 mm/s² értékeket tartalmaz.
Csak az 1000 steps/mm-nek megfelelő mechanikai/mikrolépéses beállításhoz használd;
a steps/mm puszta átírása nem növeli a fizikai felbontást.
A profil XML-eket C:\Mach3 alá, a hozzájuk tartozó makrókat azonos nevű
C:\Mach3\macros alkönyvtárba másold. Az új 1000-es indítók ezeket nyitják.
A meglévő profilok kalibrációja megmarad. A PIO rövid impulzusvonatokat ad
1 ms-os frame-ekben; az impulzusvonaton belüli frekvencia magasabb lehet az
átlagos frame-frekvenciánál. A fizikai 200 kHz oszcilloszkópos mérése még nincs meg.

## RP2350 / Pico 2 firmware

Fájl: `firmware/stepper-ninja-pico2-usb-mach3-3axis.uf2`.
Cél: szabványos Raspberry Pi Pico 2, RP2350, ARM Cortex-M33 (rp2350-arm-s).
USB-only Mach3 profil, ugyanaz a 3 tengely / 7 DI / 6 DO / PWM GPIO-kiosztás.
Ugyanaz a Mach3 DLL, maró- és esztergaprofil használható hozzá.
BOOTSEL módban másold a `pico2` UF2-t a Pico 2 bootmeghajtójára.
Az RP2350 változat lefordult; fizikai RP2350 panelen még nem tesztelt.
Más RP2350 panelnél ellenőrizd a panel GPIO-kivezetéseit.

## Mach3 kompatibilitási javítás

Ez a DLL a hivatalos Plugin Wizard rögzített adatstruktúráit használja.
A korábbi modern MFC-elrendezés hibás motoradatokat olvasott, emiatt a normál
bemenetfrissítés nem indult el, miközben a plugin monitora még működött.
Részletek: docs/MACH3-ABI.en.md. A konfiguráció megmutatja az utolsó driverhibát;
a COM-beállítási hibák pontos Windows-hibakódot is tartalmaznak.

## Élő bemenetdiagnosztika

A futó StepperNinja driver konfigurációjában a bemenetkijelzés automatikusan
elindul, a driver meglévő kapcsolatán. Nem nyit másik USB-kapcsolatot, és
nem adja át a vezérlő tulajdonjogát. A kijelzés körülbelül 50 ms-onként frissül.
**H/L** = fizikai GPIO-szint, **ON/off** = az Invert szerinti aktív/inaktív állapot.
Az invertálás előnézete azonnali. A **Save** a firmware és a Mach3 bemeneti
polaritását is frissíti, a **Cancel** megtartja a korábbi beállításokat.
Az ablak bezárása után a driver ugyanazon a kapcsolaton folytatja a lekérdezést.
A kapcsolat mezőinek szerkesztése csak Save után érvényesül.
Kapcsolathibánál **-- / --** látszik. A monitor nem élesíti a vezérlőt.
Ha a plugin nincs mozgásvezérlőnek kiválasztva, a **Start input monitor**
gombbal indítható az önálló ellenőrzés.
A korábbi csomag frissített UF2-je továbbra is megfelelő.
Valódi USB-s Picóval ellenőrizve; a fizikai kapcsolók kapcsolgatása nincs tesztelve.

## Gépfüggő értékek és korlátok

A GPIO 3,3 V-os logika; 5/24 V-os jelekhez megfelelő illesztő kell.
Alap bemenetpolaritás: aktív alacsony, InputInvert=127. A Mach3 port 1/pin 1..7
a DI1..DI7 sorszámát jelenti. STEP magas idő 2,5 µs; DIR setup 5 µs;
watchdog és válaszidő 500 ms. A kapcsolók polaritását, mozgásirányokat, méretpontosságot
és a tényleges géputakat a gépen ellenőrizni kell. Home alapérték: negatív irány,
1 mm visszaállás, 1000 mm keresési keret; ez nem mért gépút.

Az orsó típusa, fordulatszám-tartománya és illesztése nincs megadva, ezért külön
orsókalibrálás nem készült; a mintaprofilok orsóbeállításai megmaradtak.
**Nincs orsójeladó-kezelés és szinkron menetvágás.** A Turn profil X/Z alapkonfiguráció.
STEP korlát: tengelyenként 200 kHz, fizikailag még nem mérve. A megadott sebességhez
kb. 16,7 kHz kell. Ellenőrizve: 10/10 Windows teszt, x86 DLL-betöltés és indítási export,
USB kapcsolat a valódi RP2040-en, a két profil XML-je, maróprofil megnyitása Mach3-ban.
Fizikai mozgás és a teljes esztergás működés még nincs igazolva.

Mach3 telepítő, licenc és SDK nincs a csomagban. Licenc: LICENSE.txt.
Források: docs/pinout-sources.txt; a PDF ábrája saját vektoros rajz,
a fizikai lábak alapja a Raspberry Pi hivatalos Pico pinoutja.
