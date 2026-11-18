# Stepper-Ninja Konfigurationsleitfaden

Die neuen Board-Konfigurationen und der I²C-I/O-Prototyp liegen im [test-Branch](https://github.com/atrex66/stepper-ninja/tree/test). Die Prüfung an physischer Hardware steht noch aus.

Dieser Leitfaden erklärt die wichtigsten `#define`-Optionen in `config.h` und wo sie verwendet werden.

Stepper-Ninja verwendet passende Konfigurationsdateien in:

- `firmware/inc/config.h`
- `hal-driver/config.h`

Im Normalfall sollte `hal-driver/config.h` ein Symlink auf `firmware/inc/config.h` sein und keine separat gepflegte Kopie.

Wenn die Symlinks fehlen, kannst du sie im Verzeichnis `hal-driver` so neu erzeugen:

```bash
./make_symlinks.sh
```

Das Skript stellt die vom HAL-Treiber verwendeten gemeinsamen Symlinks wieder her.

---

## Welche Datei solltest du bearbeiten?

- Bearbeite `firmware/inc/config.h`, wenn du das Verhalten der Pico-Firmware ändern willst.
- `hal-driver/config.h` sollte als Symlink auf diese Datei zeigen.
- Wenn `hal-driver/config.h` eine normale Datei statt eines Symlinks ist, führe `hal-driver/make_symlinks.sh` aus.

Hinweis: Einige Werte werden in `footer.h` automatisch überschrieben, wenn `breakout_board` nicht `0` ist.

---

## Netzwerk-Standardwerte

Diese Werte definieren die Start-Netzwerkkonfiguration, die im Flash gespeichert wird, bis sie über das serielle Terminal geändert wird.

| Define | Bedeutung |
|---|---|
| `DEFAULT_MAC` | Standard-MAC-Adresse |
| `DEFAULT_IP` | Standard-IP-Adresse |
| `DEFAULT_PORT` | UDP-Port für den HAL-Treiber |
| `DEFAULT_GATEWAY` | Standard-Gateway |
| `DEFAULT_SUBNET` | Standard-Subnetzmaske |
| `DEFAULT_TIMEOUT` | Kommunikations-Timeout in Mikrosekunden |

Wenn du später die seriellen `ipconfig`-Befehle benutzt, können die im Flash gespeicherten Laufzeitwerte diese Standardwerte überschreiben.

---

## Board-Auswahl

`breakout_board` wählt das Hardware-Layout.

| Wert | Bedeutung |
|---|---|
| `0` | Eigenes Pin-Mapping aus `config.h` |
| `1` | Stepper-Ninja Breakout Board |
| `2` | IO-Ninja Breakout Board |
| `3` | Analog-Ninja Breakout Board |
| `100` | BreakoutBoard100 |

Wenn `breakout_board` größer als `0` ist, ersetzt `footer.h` mehrere eigene Pin-Definitionen durch boardspezifische Werte.

### Weitere Board-Konfigurationen

- [Expatria PicoBOB Deluxe A3](../configurations/PicoBOB-DLX/picoBOB-DLX.h): korrigierte W5500-RESET/INT-, LED- und PWM-Zuordnung.
- [Phil Barrett RP23CNC V1.0/V2](../configurations/RP23CNC/README.md) (Englisch): fünf Achsen, 14 Eingänge, 12 Ausgänge, eine Spindel-PWM, optional WIZ850io/W5500. Build: `-DBOARD=rp23cnc -DWIZCHIP_TYPE=W5500`; `BOARD=pico2` wählt eine andere MCU-Variante mit 30 GPIOs.

Diese Vorlagen verwenden `breakout_board=0`, keine neuen nummerierten Board-IDs. Die gewünschte Vorlage nach `firmware/inc/config.h` kopieren und Firmware sowie HAL mit derselben Konfiguration neu bauen. Pinbelegung und optionale `GP_NULL`-LED/Reset-Einstellungen sind boardspezifisch.

---

## Bewegungs-Kanäle

Diese Defines legen fest, wie viele Kanäle vorhanden sind und welche Pins sie verwenden.

| Define | Bedeutung |
|---|---|
| `stepgens` | Anzahl der Stepgen-Kanäle |
| `stepgen_steps` | Step-Ausgangspins |
| `stepgen_dirs` | Richtungs-Pins |
| `step_invert` | Step-Signal pro Kanal invertieren |
| `encoders` | Anzahl der Encoder-Kanäle |
| `enc_pins` | Erster Pin jedes Quadratur-Encoder-Paars |
| `enc_index_pins` | Index-Pins der Encoder |
| `enc_index_active_level` | Aktiver Pegel jedes Index-Eingangs |

Für jeden Encoder verwendet `enc_pins` zwei GPIOs: `A` auf dem angegebenen Pin und `B` auf dem nächsten Pin.

---

## Encoder-Modus

Die Firmware unterstützt zwei Encoder-PIO-Implementierungen:

| Define | Bedeutung |
|---|---|
| `ENCODER_PIO_LEGACY` | Ursprüngliche Quadratur-Encoder-PIO |
| `ENCODER_PIO_SUBSTEP` | Neuere Substep-PIO |
| `encoder_pio_version` | Wählt die zu kompilierende Encoder-PIO |

Typische Einstellung:

```c
#define encoder_pio_version ENCODER_PIO_SUBSTEP
```

Für die alte Encoder-PIO:

```c
#define encoder_pio_version ENCODER_PIO_LEGACY
```

Aktuelles Verhalten:

- Die Geschwindigkeits-Schätzung des Encoders erfolgt im HAL-Treiber.
- Im Legacy-Modus sendet die Firmware Encoder-Zähldeltas pro Zyklus.
- Im Substep-Modus sendet die Firmware weiterhin Rohzählerdaten, aber die Geschwindigkeitsausgabe wird im HAL-Treiber erzeugt.

Du kannst den Modus auch beim Build überschreiben:

```bash
CFLAGS='-Dencoder_pio_version=ENCODER_PIO_LEGACY' cmake -S firmware -B build -DWIZCHIP_TYPE=W5500
```

---

## Digitale Eingänge und Ausgänge

| Define | Bedeutung |
|---|---|
| `in_pins` | Liste der Eingangs-GPIOs |
| `in_pullup` | Pull-up pro Eingang aktivieren |
| `out_pins` | Liste der Ausgangs-GPIOs |

Diese Werte werden nur im Custom-Modus verwendet oder wenn das ausgewählte Breakout-Board sie nicht überschreibt.

---

## Konfigurierbare digitale I²C-I/O (Prototyp)

Optionale digitale Ein-/Ausgänge für eigene **LinuxCNC**-Konfigurationen (`breakout_board=0`). Typen: `SN_IO_MCP23008`, `SN_IO_MCP23017`, `SN_IO_TCA9535`, `SN_IO_TCA9555`. Feste Breakout-Boards behalten ihren bisherigen Codepfad und die Bedeutung von `io_expanders`. Mach3 wird hier nicht unterstützt.

| Define | Bedeutung |
|---|---|
| `I2C_IO_ENABLE` | Modul aktivieren; Standard `0` |
| `I2C_IO_BUS_COUNT` | Busanzahl, aktiviert 1–2; Standard `0` |
| `I2C_IO_BUSES` | Busdeskriptoren; Standard `{}` |
| `I2C_IO_DEVICE_COUNT` | Geräte insgesamt, aktiviert 1–2; Standard `0` |
| `I2C_IO_DEVICES` | Gerätedeskriptoren; Standard `{}` |

Die Einstellungen **vor** `#include "footer.h"` einfügen. Der Footer ergänzt fehlende Werte mit `#ifndef`; alte Konfigurationen brauchen keinen neuen Abschnitt und initialisieren keinen zusätzlichen Bus. Firmware und HAL mit identischen Einstellungen neu bauen.

Beispiel: ein TCA9555, Bits 0–7 als Eingänge, 8–15 als Ausgänge, Abfrage alle 10 ms:

```c
#define I2C_IO_ENABLE 1
#define I2C_IO_BUS_COUNT 1
#define I2C_IO_BUSES {{0, GP00, GP01, 400000, 1000}}
#define I2C_IO_DEVICE_COUNT 1
#define I2C_IO_DEVICES { \
    {SN_IO_TCA9555, 0, 0x20, 0x00ff, 0, 0, 0, 0, 10} \
}
```

Busfelder: Controller (0/1), SDA-GPIO, SCL-GPIO, Frequenz in Hz (bis 400000), Zeitlimit je SDK-Transfer in µs (1–10000). Gerätefelder: Typ, Buslistenindex, 7-Bit-Adresse (0x20–0x27), Richtungsmaske (`1=Eingang`), Pull-up-Maske, Eingangs-Invertierung, Ausgangs-Invertierung, logische Ausgangswerte bei Start/Verbindungsverlust, Abfrageintervall in ms (1–60000).

GPIO0/1 im Beispiel müssen frei sein; keine Überschneidung mit STEP/DIR, Encoder/Index, nativer I/O, PWM, Tastatur, LED oder Kommunikation. Die [vollständige I/O-Vorlage](../configurations/I2C-IO/config.h) muss an das Board angepasst werden.

- Maximal zwei Geräte und 32 native Digitalausgänge. Jedes Gerät belegt 16 Bits, auch MCP23008. Beide nativen GPIO-Eingangsbanken bleiben erhalten; das Paketformat bleibt gleich.
- MCP23008: 8-Bit-Masken, sonst 16 Bit. Der neue Pfad lehnt MCP23017-Eingangsbits 7/15 ab (GPA7/GPB7 laut aktuellem Datenblatt nur Ausgänge); bestehende Boards bleiben unverändert.
- TCA-Pull-up-Maske immer null. TCA9535 besitzt keine internen I/O-Pull-ups; die festen Eingangs-Pull-ups des TCA9555 lassen sich damit nicht abschalten.
- HAL-Namen: `stepgen-ninja.0.expander.0.input.0`, `input.0-not`, `output.8`, `online`, `valid`. Nur die konfigurierte Richtung wird exportiert: physische Eingänge/Status HAL_OUT, Ausgangsbefehle HAL_IN.
- Buszugriffe laufen außerhalb der Step/Encoder-IRQs. Fehler löschen `online`/`valid`; Eingangswerte behalten die letzte erfolgreiche Probe. Bei defektem Bus ist die Ausgabe der Werte für Verbindungsverlust nicht garantiert.

[Details, Grenzen und Tests](I2C-IO.en.md) (Englisch). Physische I²C-Tests stehen noch aus.

---

## PWM-Optionen

| Define | Bedeutung |
|---|---|
| `use_pwm` | Aktiviert PWM-Unterstützung |
| `pwm_count` | Anzahl der PWM-Ausgänge |
| `pwm_pin` | PWM-Ausgangspins |
| `pwm_invert` | PWM pro Kanal invertieren |
| `default_pwm_frequency` | Standard-PWM-Frequenz |
| `default_pwm_maxscale` | HAL-Skalierungsgrenze |
| `default_pwm_min_limit` | Minimale PWM-Ausgabe |

Setze `use_pwm` nur dann auf `1`, wenn Kanalanzahl und Pins zu deiner Hardware passen.

---

## Raspberry-Pi-SPI-Modus

| Define | Bedeutung |
|---|---|
| `raspberry_pi_spi` | SPI-Verbindung zu einem Raspberry Pi statt Wiznet-Ethernet |
| `RASPI_INT_OUT_GPIO` | Interrupt-/Status-Pin zum Raspberry Pi |
| `RASPI_SPI_CS_GPIO` | SPI-Chip-Select-GPIO zum Raspberry Pi |
| `raspi_inputs` | Vom Raspberry Pi sichtbare Eingänge |
| `raspi_input_pullups` | Pull-up-Einstellung dieser Eingänge |
| `raspi_outputs` | Vom Raspberry Pi gesteuerte Ausgänge |

Wenn `raspberry_pi_spi` auf `0` steht, verwendet die Firmware den Wiznet-Ethernet-Pfad.

---

## Timing-Standardwerte

| Define | Bedeutung |
|---|---|
| `default_pulse_width` | Standard-Step-Pulsbreite in Nanosekunden |
| `default_step_scale` | Standard-Schritte pro Einheit |
| `use_timer_interrupt` | Aktiviert Ringpuffer und timergesteuerte Step-Ausgabe |

`use_timer_interrupt` kann sichtbares Jitter zwischen PC und Pico durch Zwischenspeicherung von Step-Kommandos reduzieren.

---

## Von footer.h gesteuerte Defines

Einige wichtige Defines liegen in `footer.h`, weil sie vom Board oder der Plattform abhängen.

| Define | Bedeutung |
|---|---|
| `use_stepcounter` | Verwendet Step Counter statt Quadratur-Encoder-Pfad |
| `debug_mode` | Zusätzliche Debug-Funktionen für Raspberry-Pi-Kommunikation |
| `max_statemachines` | Abgeleitete Gesamtzahl der PIO-State-Machines |
| `pico_clock` | Von der Firmware verwendeter Pico-Systemtakt |

Ändere diese Werte nicht leichtfertig, wenn du die Auswirkungen auf Timing und PIO-Belegung nicht kennst.

---

## Build-Optionen außerhalb von config.h

Einige nützliche Optionen werden in CMake statt in `config.h` gewählt.

| Option | Bedeutung |
|---|---|
| `-DWIZCHIP_TYPE=W5100S` | Build für W5100S |
| `-DWIZCHIP_TYPE=W5500` | Build für W5500 |
| `-DBOARD=pico` | Build für Pico |
| `-DBOARD=pico2` | Build für Pico2 |
| `-DBOARD=rp23cnc` | RP23CNC mit 48-GPIO-RP2350B (passende Vorlage und W5500 erforderlich) |
| `-DSTEPPER_NINJA_PROFILE=linuxcnc` / `mach3-3axis` | Hostprofil: `linuxcnc` (Standard) oder `mach3-3axis` (feste Pinbelegung) |
| `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` | Kopiert die Firmware vor der Ausführung in SRAM |

Beispiel:

```bash
cmake -S firmware -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_RUN_FROM_RAM=ON
```

Der Mach3-Prototyp nutzt ein eigenes festes Dreiachsprofil. Siehe [Mach3](MACH3.en.md) und [Build-Anleitung](MACH3-BUILD.en.md) (Englisch). Die Pico-Firmware lässt sich auch unter Windows bauen.

---

## Empfohlener Ablauf

1. Wähle das Board mit `breakout_board`.
2. Setze `stepgens`, `encoders` und die Pin-Listen, wenn du den Custom-Modus verwendest.
3. Wähle `encoder_pio_version`.
4. Aktiviere PWM nur, wenn deine Hardware es benötigt.
5. Halte Firmware- und HAL-Konfigurationsdatei synchron.
6. Baue Firmware und HAL-Treiber nach jeder relevanten Konfigurationsänderung neu.

Für Netzwerkeinstellungen zur Laufzeit siehe auch [IPCONFIG.de.md](IPCONFIG.de.md).
