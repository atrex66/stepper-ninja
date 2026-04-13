# Stepper-Ninja Konfigurationsleitfaden

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
| `raspi_int_out` | Interrupt-/Status-Pin zum Raspberry Pi |
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
| `-DSTEPPER_NINJA_RUN_FROM_RAM=ON` | Kopiert die Firmware vor der Ausführung in SRAM |

Beispiel:

```bash
cmake -S firmware -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_RUN_FROM_RAM=ON
```

---

## Empfohlener Ablauf

1. Wähle das Board mit `breakout_board`.
2. Setze `stepgens`, `encoders` und die Pin-Listen, wenn du den Custom-Modus verwendest.
3. Wähle `encoder_pio_version`.
4. Aktiviere PWM nur, wenn deine Hardware es benötigt.
5. Halte Firmware- und HAL-Konfigurationsdatei synchron.
6. Baue Firmware und HAL-Treiber nach jeder relevanten Konfigurationsänderung neu.

Für Netzwerkeinstellungen zur Laufzeit siehe auch [IPCONFIG.de.md](IPCONFIG.de.md).