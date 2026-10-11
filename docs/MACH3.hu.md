# Mach3 támogatás

[English](MACH3.en.md)

A Mach3 fejlesztési implementáció külön `mach3-driver/` könyvtárban és külön
firmware-profilban van. A közös protokoll és a firmware állapotgépe a `common/`
alatt található. A LinuxCNC marad az alapértelmezett, rugalmas konfigurációjú
profil. Mach3-hoz: `STEPPER_NINJA_PROFILE=mach3-3axis`.

A fix kiosztás három STEP/DIR tengely (GPIO 0..5), főorsó PWM (GPIO 6),
hat kimenet (GPIO 7..12) és hét bemenet (GPIO 13,14,15,22,26,27,28).
Az UDP-kezelés külön `firmware/modules/udp_transport.c` fájlba került.

Megvalósítva: session és ACK; pufferelt, 1 ms-os XYZ lépésrekordok;
fizikai STEP számlálás szerinti végrehajtási report; helyi gyorsulásos jog;
élszenzoros homing; koordinált G31 érintési pozíciórögzítéssel és fékezéssel;
a Mach3 fékezési pályáját továbbító Feed Hold; dwell; IO és PWM; watchdog;
Mach3 konfigurációs ablak és SDK adapterforrás.

A Windows DLL fordítása/betöltése és a hardveres működés még ellenőrizendő.
A G31 SDK reportjait és a valódi Mach3 Feed Hold / Cycle Start útvonalat
Windows alatt kell igazolni. [Működés és példák](MACH3-FEEDHOLD-G31.hu.md).
USB lehetséges bővítés,
a jelenlegi implementáció UDP-t használ.

A pontos adatmezők, állapotátmenetek, Mach3 reportok, I/O szerepek és ismert
korlátok: [Mach3 végrehajtás és protokoll](MACH3-PROTOCOL.hu.md).
Fordítás: [fordítási útmutató](MACH3-BUILD.hu.md) és
[driver README](../mach3-driver/README.md).
