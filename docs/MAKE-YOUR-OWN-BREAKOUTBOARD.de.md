# Eigenes Breakout Board für stepper-ninja bauen

Diese Anleitung erklärt, wie du ein eigenes Breakout Board für stepper-ninja entwirfst und integrierst — von der elektrischen Planung über die Firmware-/HAL-Integration bis zur LinuxCNC-Inbetriebnahme.

## 1. Was du baust

Ein eigenes Board hat in diesem Projekt zwei Software-Seiten:

- Firmware-Seite auf Pico/Pico2:
  - Dateimuster: `firmware/modules/breakoutboard_<ID>.c`
  - Verwaltet die physische Hardware (GPIO, I2C-Expander, DACs, Sicherheitszustand).
  - Bildet den rohen Board-Zustand auf Paketfelder ab.

- LinuxCNC-HAL-Seite:
  - Dateimuster: `hal-driver/modules/breakoutboard_hal_<ID>.c`
  - Exportiert HAL-Pins und ordnet Paketfelder diesen Pins zu.
  - Packt HAL-Ausgaben zurück in die an die Firmware gesendeten Paketfelder.

Beide Seiten werden über `breakout_board` in `firmware/inc/config.h` und `hal-driver/config.h` ausgewählt.

## 2. Voraussetzungen

- Funktionierende stepper-ninja-Build-Umgebung.
- Pico SDK installiert (oder bereits durch `firmware/CMakeLists.txt` nutzbar).
- LinuxCNC-Entwickler-Header/-Werkzeuge für den HAL-Modul-Build installiert.
- Ein bekannt funktionierendes bestehendes Board als Referenz:
  - `firmware/modules/breakoutboard_1.c`
  - `hal-driver/modules/breakoutboard_hal_1.c`

## 3. Hardware-Planungs-Checkliste

Kläre vor dem Codieren folgende Punkte:

- Versorgungsschienen und Logikpegel:
  - 3,3-V-Logikkompatibilität mit Pico-GPIO.
  - Geeignetes Level-Shifting/Optokoppler wo nötig.

- Sicherheitsverhalten:
  - Definiere genau, was Ausgänge bei Kommunikations-Timeout/Verbindungsabbruch tun müssen.
  - Stelle sicher, dass Enable-Leitungen sicher abfallen (Motor-Enable aus, Spindel deaktiviert, Analogausgänge auf null).

- I/O-Budget:
  - Anzahl digitaler Ein-/Ausgänge.
  - Anzahl Analogausgänge und DAC-Typ.
  - Encoder-Kanäle und Index-Verhalten.

- Bus-/Peripherie-Belegung:
  - I2C-Bus/-Pins für Expander/DACs.
  - SPI/WIZnet-Verdrahtung und Reset-/Interrupt-Leitungen.

- Signalintegrität:
  - Halte Step-/Dir-Leitungen sauber und kurz.
  - Füge Pull-ups/-downs hinzu, wo der Zustand beim Booten wichtig ist.

## 4. Neue Board-ID wählen

Wähle eine noch nicht verwendete ganzzahlige ID, zum Beispiel `42`.

Diese ID verwendest du überall:

- `firmware/modules/breakoutboard_42.c`
- `hal-driver/modules/breakoutboard_hal_42.c`
- `#define breakout_board 42` in den Konfigurations-Headern
- Auswahlzweige in den Firmware- und HAL-Build-Pfaden

## 5. Firmware-Seite integrieren

### 5.1 Firmware-Modul erstellen

1. Vorlage kopieren:

```bash
cp firmware/modules/breakoutboard_user.c firmware/modules/breakoutboard_42.c
```

2. In `firmware/modules/breakoutboard_42.c` ändern:

```c
#if breakout_board == 255
```

zu:

```c
#if breakout_board == 42
```

3. Erforderliche Callbacks implementieren:

- `breakout_board_setup()`:
  - GPIO/I2C/SPI-Geräte auf deinem Board initialisieren.
  - Expander und DACs erkennen und konfigurieren.

- `breakout_board_disconnected_update()`:
  - Alle Ausgänge in den sicheren Zustand zwingen.
  - DAC-Ausgänge auf null oder sicheren Bias setzen.

- `breakout_board_connected_update()`:
  - Ausgaben aus dem empfangenen Paket auf die Hardware anwenden.
  - Eingangs-Snapshots von der Hardware aktualisieren.

- `breakout_board_handle_data()`:
  - Ausgehende Paketfelder befüllen (`tx_buffer->inputs[]`, usw.).
  - Eingehende Felder verarbeiten (`rx_buffer->outputs[]`, Analognutzlast).

### 5.2 Modul zum Firmware-Build hinzufügen

`firmware/CMakeLists.txt` bearbeiten und neue Quelldatei in `add_executable(...)` eintragen:

```cmake
modules/breakoutboard_42.c
```

### 5.3 Board-Makros im Firmware-Konfigurations-Footer definieren

`firmware/inc/footer.h` bearbeiten und neuen Block hinzufügen:

```c
#if breakout_board == 42
    // deine Pin-Zuordnung und Board-spezifische Makros
    // Beispiele:
    // #define I2C_SDA 26
    // #define I2C_SCK 27
    // #define I2C_PORT i2c1
    // #define MCP23017_ADDR 0x20
    // #define ANALOG_CH 2
    // #define MCP4725_BASE 0x60

    // stepgen/encoder/in/out/pwm-Makros nach Bedarf überschreiben
#endif
```

Vorhandene Blöcke für IDs 1, 2, 3, 100 als Stilreferenz verwenden.

### 5.4 Board in der Firmware-Konfiguration auswählen

`firmware/inc/config.h` bearbeiten:

```c
#define breakout_board 42
```

## 6. HAL-Treiber-Seite integrieren

### 6.1 HAL-Board-Modul erstellen

1. Vorlage kopieren:

```bash
cp hal-driver/modules/breakoutboard_hal_user.c hal-driver/modules/breakoutboard_hal_42.c
```

2. In `hal-driver/modules/breakoutboard_hal_42.c` ändern:

```c
#if breakout_board == 255
```

zu:

```c
#if breakout_board == 42
```

3. Implementieren:

- `bb_hal_setup_pins(...)`:
  - Alle erforderlichen Pins exportieren (`hal_pin_bit_newf`, `hal_pin_float_newf`, usw.).
  - Standardwerte initialisieren.

- `bb_hal_process_recv(...)`:
  - `rx_buffer->inputs[...]` und weitere Felder in HAL-Ausgabe-Pins entpacken.

- `bb_hal_process_send(...)`:
  - HAL-Befehls-Pins in `tx_buffer->outputs[...]` und Analogfelder packen.

### 6.2 HAL-Board-Include registrieren

`hal-driver/stepper-ninja.c` im Board-Auswahlbereich bearbeiten und hinzufügen:

```c
#elif breakout_board == 42
#include "modules/breakoutboard_hal_42.c"
```

### 6.3 Board-ID in der HAL-Konfiguration auswählen

`hal-driver/config.h` bearbeiten:

```c
#define breakout_board 42
```

Die `breakout_board`-IDs in Firmware und HAL müssen identisch sein.

## 7. Paketzuordnungsregeln (Wichtig)

Verwende einen stabilen Vertrag zwischen beiden Seiten.

- Firmware-Seite schreibt Maschine-zu-Host-Daten:
  - `tx_buffer->inputs[0..3]`
  - Encoder-Feedback-/Jitter-Felder

- HAL-Seite liest diese in HAL-Pins in `bb_hal_process_recv()`.

- HAL-Seite schreibt Host-zu-Firmware-Befehle:
  - `tx_buffer->outputs[0..1]`
  - Analog- und optionale Steuerfelder

- Firmware-Seite liest diese Befehle über `rx_buffer` und wendet sie in `breakout_board_connected_update()` an.

Jede Abweichung hier verursacht vertauschte Pins oder tote I/O. Führe während der Entwicklung eine Zuordnungstabelle in Kommentaren.

## 8. Firmware bauen und flashen

Vom Repository-Stammverzeichnis:

```bash
cd firmware
cmake -S . -B build -DBOARD=pico2 -DWIZCHIP_TYPE=W5500
cmake --build build -j"$(nproc)"
ls build/*.uf2
```

Für Pico v1 `-DBOARD=pico` verwenden.

Flashen:

1. BOOTSEL gedrückt halten, während der Pico verbunden wird.
2. Generierte `.uf2`-Datei auf das gemountete RPI-RP2-Laufwerk kopieren.
3. Neu starten und die serielle Ausgabe auf Init-Logs prüfen.

## 9. LinuxCNC-HAL-Modul bauen und installieren

Vom Repository-Stammverzeichnis:

```bash
cd hal-driver
cmake -S . -B build-cmake
cmake --build build-cmake --target stepper-ninja
sudo cmake --install build-cmake --component stepper-ninja
```

Dies installiert `stepper-ninja.so` in dein LinuxCNC-Modulverzeichnis.

## 10. LinuxCNC HAL in Betrieb nehmen (Minimal)

Beispiel-Gerüst in deiner HAL-Datei:

```hal
loadrt stepper-ninja ip_address="192.168.0.177:8888"
addf stepgen-ninja.0.watchdog-process servo-thread
addf stepgen-ninja.0.process-send     servo-thread
addf stepgen-ninja.0.process-recv     servo-thread

# Beispiel Board-I/O-Nets (Namen hängen von deiner bb_hal_setup_pins-Implementierung ab)
# net estop-in      stepgen-ninja.0.inputs.0     => some-signal
# net coolant-out   some-command                 => stepgen-ninja.0.outputs.0
```

Anschließend mit `halshow`/`halcmd show pin stepgen-ninja.0.*` überprüfen.

## 11. Inbetriebnahmeprozedur (Empfohlen)

Halte genau diese Reihenfolge ein:

1. Nur-Strom-Test:
  - Versorgungsschienen überprüfen, keine Überhitzung, kein übermäßiger Ruhestrom.

2. Bus-Erkennungstest:
  - Bestätigen, dass die erwarteten I2C-Adressen von der Firmware erkannt werden.

3. Trennungs-Sicherheitstest:
  - LinuxCNC-Kommunikation stoppen.
  - Bestätigen, dass Ausgänge und Analogkanäle in den sicheren Zustand gehen.

4. Eingangs-Polaritätstest:
  - Jeden physischen Eingang umschalten und entsprechenden HAL-Pin-Zustand prüfen.
  - `-not`-Komplemente validieren, wenn exportiert.

5. Ausgangstest:
  - Jeden HAL-Ausgangs-Pin umschalten und physischen Ausgang verifizieren.

6. Encodertest:
  - Langsam und schnell drehen, Zählrichtung und Index-Behandlung prüfen.

7. Bewegungstest ohne Motor:
  - Zuerst Step-/Dir-Polarität und Impulszeitsteuerung validieren.

8. Bewegungstest mit angeschlossenem Motor:
  - Mit konservativer Geschwindigkeit/Beschleunigung beginnen.

## 12. Fehlerbehebung

- Keine I/O-Reaktion:
  - Prüfen, ob `breakout_board`-ID in Firmware- und HAL-Konfiguration übereinstimmt.
  - Prüfen, ob neue Dateien in Build-Pfaden eingebunden sind.

- Build erfolgreich, aber Board-Callbacks nicht aktiv:
  - `#if breakout_board == 42`-Guard prüfen.
  - Footer-/Konfigurationszweig auf Existenz und Kompilierbarkeit prüfen.

- Verschobene/falsche Bit-Reihenfolge bei Eingängen:
  - Bit-Packing/-Unpacking zwischen folgenden Dateien prüfen:
    - `firmware/modules/breakoutboard_42.c`
    - `hal-driver/modules/breakoutboard_hal_42.c`

- Zufälliges Trennungsverhalten:
  - Timeout-sicheren Code in `breakout_board_disconnected_update()` prüfen.
  - Netzwerkstabilität und Paket-Watchdog-Verdrahtung prüfen.

- Analogausgang clippt/überläuft:
  - HAL-Werte vor dem Packen von DAC-Wörtern begrenzen.
  - Skalierung gegen DAC-Auflösung prüfen.

## 13. Versionsverwaltungsempfehlungen

Für jedes benutzerdefinierte Board-Änderungsset in dieser Reihenfolge committen:

1. Firmware-Modul + Footer-/Konfigurationsaktualisierungen.
2. HAL-Modul + Include-/Konfigurationsaktualisierungen.
3. LinuxCNC-HAL-Konfigurationsaktualisierungen.
4. Dokumentation für Verdrahtung und Pin-Zuordnung.

Pflege eine Board-spezifische Markdown-Datei mit:

- Schaltplan-Link
- Pin-Zuordnungstabelle
- I2C-Adresstabelle
- Safe-State-Richtlinie
- HAL-Pin-Namensvertrag

## 14. Schnelle Datei-Checkliste

Für eine neue Board-ID müssen alle folgenden Dateien angepasst werden:

- `firmware/modules/breakoutboard_42.c`
- `firmware/CMakeLists.txt`
- `firmware/inc/footer.h`
- `firmware/inc/config.h`
- `hal-driver/modules/breakoutboard_hal_42.c`
- `hal-driver/stepper-ninja.c`
- `hal-driver/config.h`
- (optional) `docs/dein-board-name.md`

Wenn diese vollständig und die IDs konsistent sind, ist die Integration eines eigenen Breakout Boards in der Regel unkompliziert.
