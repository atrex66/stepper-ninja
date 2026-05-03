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

## Lizenz

- Das Quadratur-Encoder-PIO-Programm verwendet die BSD-3-Lizenz von Raspberry Pi (Trading) Ltd.
- Der `ioLibrary_Driver` steht unter der MIT-Lizenz von Wiznet.