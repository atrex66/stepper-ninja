# Kommunikációs benchmark

Fordítás a célgépen (a szkript bármely könyvtárból indítható):

```sh
/home/ninja/stepper-ninja-test/utility/build.sh
```

UDP módban az IP-cím és a port indításkor adható meg, nem kell őket
belefordítani. Külön indítószkript nem szükséges:

```sh
./benchmark_udp 192.168.0.177 8888
./benchmark_udp 192.168.0.177 8888 10000
./benchmark_udp --help
```

Formátum: `benchmark_udp IP [PORT [CIKLUSOK]]`. A port alapértéke a
`config.h` fájlban megadott `DEFAULT_PORT` (jelenleg 8888), a ciklusszámé
100000. Az IP-cím kötelező, numerikus IPv4-cím.

A program ugyanarra a helyi UDP-portra köt, mint a célport, az eredeti
kommunikáció szerint. Mérés előtt állítsd le az ezt a portot használó
LinuxCNC drivert vagy más benchmarkot. A csomagok nullázott gépi
parancsokat tartalmaznak; a mérést nyugalomban lévő gépen végezd.
Válasz nélkül 2 másodperc után hibával kilép. Ellenőrzi a válasz méretét,
protokollazonosítóját és ellenőrzőösszegét; hibás válaszból nem számol
sikeres ciklust. Az időmérés monoton órával, nanoszekundumos felbontással
történik, az eredmény milliszekundumban jelenik meg.

A `config.h` szerinti SPI-fordítás megmarad; abban a formátum
`benchmark_udp [CIKLUSOK]`, hálózati cím nélkül. A binárist mindig a
célgép architektúrájára kell fordítani.
