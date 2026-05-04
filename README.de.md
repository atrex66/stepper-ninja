# stepper-ninja

Eine freie, quelloffene und leistungsstarke Step-Generator-, Quadratur-Encoder-, Digital-I/O- und PWM-Schnittstelle für LinuxCNC.

Das Encoder-Modul ermöglicht außerdem hauptsynchronisierte Bewegungen, zum Beispiel Gewindeschneiden und andere spindelsynchrone Abläufe.

Du brauchst das offizielle Breakout-Board nicht, um stepper-ninja zu verwenden. Ein günstiges Parallelport-Breakout-Board reicht aus, und andere Konfigurationen sind ebenfalls möglich.

![official breakout board](docs/20250812_165926.jpg)

## Dokumentation

- [Installationsanleitung](docs/INSTALL.de.md)
- [Konfigurationsleitfaden](docs/CONFIG.de.md)
- [IP-Konfiguration](docs/IPCONFIG.de.md)
- [Eigenes Breakout-Board bauen](docs/MAKE-YOUR-OWN-BREAKOUTBOARD.de.md)

## Sprachen

- [English](README.md)
- [Deutsch](README.de.md)
- [हिन्दी](README.hi.md)
- [Magyar](README.hu.md)
- [Português (Brasil)](README.pt-BR.md)

## Funktionen

- **Unterstützte Konfigurationen**:

  - W5100S-evb-pico UDP Ethernet
  - W5500-evb-pico
  - W5100S-evb-pico2
  - W5500-evb-pico2
  - pico + W5500 Modul
  - pico2 + W5500 Modul
  - pico + Raspberry Pi4 über SPI
  - pico2 + Raspberry Pi4 über SPI
  - pico + PI ZERO2W über SPI
  - pico2 + PI ZERO2W über SPI
  - offizielles Stepper-Ninja Breakout-Board

- **Breakout-Board v1.0 - Digitalversion**: 16 optisch getrennte Eingänge, 8 optisch getrennte Ausgänge, 4 Step-Generatoren, 2 schnelle Encoder-Eingänge, 2 unipolare 12-Bit-DAC-Ausgänge.

- **Step-Generator**: maximal 8 mit Pico 1, maximal 12 mit Pico2. Bis zu 1 MHz pro Kanal. Die Pulsbreite wird über einen HAL-Pin eingestellt.

- **Quadratur-Encoder**: maximal 8 mit Pico1, maximal 12 mit Pico2. Hohe Geschwindigkeit, Index-/Nullimpulsbehandlung, Geschwindigkeits-Schätzung und Unterstützung für spindelsynchrone Bewegungen.

- **Digitale I/O**: freie Pico-Pins können als Eingänge und Ausgänge konfiguriert werden.

- **PWM**: bis zu 16 GPIOs können als PWM-Ausgänge konfiguriert werden.

- **Software**: LinuxCNC-HAL-Treiber mit Mehrfachinstanzen und Sicherheitsfunktionen.

- **Open Source**: Code und Dokumentation unter MIT-Lizenz.

- **Beispiel-KiCad-Breakout-Board**: Ein einfaches, günstiges DIY-Breakout-Board-KiCad-Projekt ist unter `configurations/RaspberryPi` verfügbar. Es verbindet einen Pico/Pico2 über SPI mit dem Raspberry Pi und benötigt lediglich einen Pico oder Pico2, einen Standard-40-Pin-GPIO-Stecker und 2,54-mm-Leiterplatten-Schraubklemmen.

## PIO-Konfiguration und Ressourcenbeschränkungen

Die Raspberry Pi Pico und Pico2 verwenden **PIO-Blöcke (Programmierbare I/O)**, um Step-Generatoren und Encoder-Zähler direkt in Hardware zu realisieren. Da der PIO-Instruktionsspeicher sehr begrenzt ist, **können Step-Generatoren und Encoder nicht gleichzeitig verwendet werden** — das Aktivieren einer Funktion deaktiviert die andere.

### RP2040 (Pico)

- 2 PIO-Blöcke (PIO0 und PIO1), jeweils mit **4 Zustandsmaschinen** und **32 Instruktionsplätzen**
- Step-Generator-PIO-Programm: ~20 Instruktionen pro Zustandsmaschine
- Encoder-PIO-Programm: ~15–18 Instruktionen pro Zustandsmaschine
- **4 Step-Generatoren** füllen einen PIO-Block vollständig — kein Platz mehr für Encoder im selben Block
- **4 Encoder** füllen einen PIO-Block vollständig — kein Platz mehr für Step-Generatoren im selben Block
- Maximum: **8 Step-Generatoren** ODER **8 Encoder** (über beide PIO-Blöcke), niemals gemischt

### RP2350 (Pico2)

- 2 PIO-Blöcke, jeweils mit **4 Zustandsmaschinen** und **32 Instruktionsplätzen** (gleiche Struktur wie RP2040)
- Zusätzlich verfügt der RP2350 über **PIO2** mit 4 weiteren Zustandsmaschinen
- Maximum: **12 Step-Generatoren** ODER **12 Encoder** über alle drei PIO-Blöcke
- Step-Generatoren und Encoder können weiterhin nicht gemischt werden — das PIO-Programm jeder Funktion füllt den Block vollständig

### Zwischen den Modi wechseln

Bearbeite `firmware/inc/config.h`, um den Modus zu wählen:

```c
#define stepgens 4   // Anzahl der Step-Generatoren (0 = nur Encoder-Modus)
#define encoders 0   // Anzahl der Encoder (0 = nur Step-Generator-Modus)
```

Firmware nach der Änderung neu bauen und auf den Pico flashen.

### Encoder-PIO-Varianten

Es stehen zwei Encoder-Implementierungen zur Verfügung:

- **`ENCODER_PIO_SUBSTEP`** (Standard): Sub-Schritt-Interpolation für sanfte Geschwindigkeitsschätzung (~18 Instruktionen)
- **`ENCODER_PIO_LEGACY`**: einfache Quadratur-Zählung, etwas kleiner (~15 Instruktionen)

Mit Legacy-Encoder bauen:
```bash
CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY' cmake ..
```

---

## Lizenz

- Das Quadratur-Encoder-PIO-Programm verwendet die BSD-3-Lizenz von Raspberry Pi (Trading) Ltd.
- Der `ioLibrary_Driver` steht unter der MIT-Lizenz von Wiznet.