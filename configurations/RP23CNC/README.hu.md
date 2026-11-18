# Phil Barrett RP23CNC — StepperNinja LinuxCNC konfiguráció

[English, teljes lábkiosztás és buildparancsok](README.md)

Külön mintakonfiguráció a sorozatgyártott V1.0 és V2 kiosztásához:
RP2350B / RP2354B, 48 GPIO, 2 MiB flash. A korábbi prototípusok eltérő
limit/aux kiosztása nem támogatott. A forrás- és fordítási ellenőrzés nem
helyettesíti a fizikai kártyán végzett próbát.

Az `RP23CNC.h` öt STEP/DIR tengelyt, 14 bemenetet, 12 digitális kimenetet
és egy főorsó PWM-et ad. A tengelyenkénti enable külön digitális kimenet.
Encoder, UART, I2C, SD-kártya, billentyűmátrix és RGB LED nincs engedélyezve.
A meglévő aktív konfiguráció és más breakout boardok lábkiosztása nem változik.

| Funkció | GPIO-k |
|---|---|
| STEP X/Y/Z/A/B | 17, 18, 19, 20, 21 |
| DIR X/Y/Z/A/B | 12, 13, 14, 15, 16 |
| EN X/Y/Z/A/B | 22, 23, 24, 25, 26 |
| Limit X/Y/Z/A/B | 6, 5, 4, 3, 2 |
| Probe / Door-Alarm | 7 / 8 |
| Cycle Start / Feed Hold / E-stop | 9 / 10 / 11 |
| D_IN0 / D_IN1 / D_IN2 / D_IN3 | 29 / 28 / 47 / 27 |
| Főorsó enable / irány / PWM | 33 / 34 / 35 |
| Aux0 / Aux1 / Aux2 | 36 / 37 / 38 |
| Flood / Mist | 39 / 40 |
| W5500 SPI1 RX / TX / SCK / CS / INT | 44 / 43 / 46 / 41 / 42 |

Minden szám MCU GPIO-szám, nem csatlakozóláb-szám.

## Használat

- Az Ethernethez opcionális **WIZ850io, W5500 chippel** szükséges.
  A WIZ820io/WIZ550io hasonló neve nem jelent azonos, kompatibilis modult.
- A D_IN3 jumper digitális bemenetre, az Aux2 digitális kimenetre legyen állítva,
  ne UART1-re, illetve spindle PWM-re. A funkciófeliratokat ellenőrizd a kártyádon.
- A servo alarm PCB-n kiválasztott vezérlőbemenetre csatlakozik; nincs hozzá
  külön, tengelyenkénti GPIO. A főorsó 0–10 V / 0–5 V / PWM választását a
  hardveren kell a hajtáshoz igazítani.
- A GPIO25 itt A tengely enable, **nem státusz-LED**. Külön GPIO-s státusz-LED
  nincs; a WIZ resetjét pedig felhúzó tartja magas szinten, nem az MCU vezérli.
- A bemenetek fizikai GPIO-névvel jelennek meg, például
  `stepgen-ninja.0.input.gp47`; a kimenetek például
  `stepgen-ninja.0.output.gp33`. A szerepeket és a megfelelő polaritást a gép
  HAL-fájljában kell bekötni. Az E-stop, probe, limit és feed hold ebben a
  LinuxCNC profilban nem kap automatikus firmware-funkciót. Az enable sem
  kapcsolódik automatikusan a tengelyhez.

## Fordítás

Az [angol leírás](README.md) pontos parancsai külön másolatban fordítanak,
így az aktív `firmware/inc/config.h` érintetlen marad.
A build kötelező opciói:

```text
-DBOARD=rp23cnc
-DWIZCHIP_TYPE=W5500
-DSTEPPER_NINJA_PROFILE=linuxcnc
```

A sima `BOARD=pico2` nem jó: az SDK abban 30 GPIO-s RP2350A-t választ.
Az új `rp23cnc` SDK board header 48 GPIO-t és 2 MiB flash-t ad meg.
A konfiguráció 150 MHz rendszerórát használ.

A firmware és a HAL-driver azonos `RP23CNC.h` konfigurációval fordítandó.
Ebben a checkoutban a HAL `config.h` a firmware fejlécére mutató symlink;
külön másolatban is a másolat firmware-fejlécére kell mutatnia. Más kártyára
fordított HAL bináris nem használható automatikusan ezzel a firmware-rel.

A várt firmware neve: `stepper-ninja-rp23cnc-W5500.uf2`.
A parancsok nem telepítenek modult és nem flashelnek firmware-t.

## A közös kód módosításai

Az SPI és DMA a konfigurált `SPI_PORT`-ot használja. A GPIO32 feletti
kimenetekhez a csomag bitjeit a kimeneti lista indexe szerint kell olvasni,
nem a fizikai GPIO bankja szerint; az alacsony GPIO-s kiosztás változatlan.
A `GP_NULL` értékű opcionális LED/reset kimarad, valódi GPIO-val a régi ág fut.
A `pico` és `pico2` SDK-definíciókat nem módosítottuk.

A hardveres működés, Ethernet, impulzusidőzítés, polaritások és főorsófeszültség
ellenőrzése még hátravan. A rögzített gyártói források az [angol leírásban](README.md)
találhatók; a konfiguráció nem valósít meg grblHAL protokollt.

Ellenőrzés, 2026-11-18: RP23CNC firmware és HAL-forrás fordítása sikeres;
Pico/W5100S, Pico 2/W5500, PicoBOB-DLX és Mach3/Pico firmware fordítása sikeres;
33/33 LinuxCNC/GPIO és 13/13 Mach3 hordozható regressziós teszt sikeres.
A Mach3 elavult tesztje most a konfigurált pozitív/negatív határ elfogadását
és az azon túli értékek elutasítását ellenőrzi. Csak a teszt változott,
a Mach3 pályakezelése/protokollja nem.
