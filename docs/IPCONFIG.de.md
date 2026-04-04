# IPCONFIG - stepper-ninja Serielles-Terminal-Konfiguration

## Übersicht

Die `ipconfig`-Funktionalität in `serial_terminal.c` stellt eine serielle Terminalschnittstelle zur Konfiguration des stepper-ninja-Geräts bereit. Sie ermöglicht es Benutzern, Netzwerkparameter (IP, MAC, Port usw.) zu setzen, Timeouts zu verwalten, Konfigurationen im Flash zu speichern und das Gerät zurückzusetzen. Befehle werden über eine serielle Verbindung eingegeben, in Echtzeit verarbeitet und auf korrekte Formatierung geprüft.

## Voraussetzungen

- Ein serieller Terminal-Emulator (z. B. **minicom** unter Linux), der mit dem seriellen Port des Geräts (z. B. `/dev/ttyACM0`) bei der entsprechenden Baud-Rate (typischerweise 115200 8N1) verbunden ist.
- Der Benutzer muss Mitglied der Gruppe `dialout` sein, um unter Linux auf serielle Ports zuzugreifen:

  ```bash
  sudo usermod -a -G dialout $USER
  ```

- Das Gerät muss eingeschaltet und über das für die Geräteprogrammierung verwendete USB-Kabel verbunden sein.

## Einrichtung des seriellen Terminals (Linux)

1. minicom installieren:

   ```bash
   sudo apt update
   sudo apt install minicom
   ```

2. minicom für den entsprechenden seriellen Port starten (z. B. `/dev/ttyACM0`):

   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

3. Sicherstellen, dass Hardware-/Software-Flusssteuerung in minicom deaktiviert ist (`Ctrl+A`, dann `O`, **Serial port setup** wählen, **F** und **G** auf `No` setzen).

## Unterstützte Befehle

Das serielle Terminal akzeptiert Befehle, die in das Terminal eingegeben werden und durch einen Wagenrücklauf (`\r`, Enter-Taste) abgeschlossen werden. Befehle sind Groß-/Kleinschreibung sensitiv und müssen der exakten Syntax folgen. Ungültige Befehle oder falsche Formate geben eine Fehlermeldung zurück.

| Befehl | Beschreibung | Beispiel | Ausgabe/Wirkung |
|--------|--------------|---------|-----------------|
| `help` | Zeigt eine Liste der verfügbaren Befehle und deren Beschreibungen an. | `help` | Druckt das Hilfe-Menü. |
| `check` | Zeigt die aktuelle Konfiguration, einschließlich MAC, IP, Subnetz, Gateway, DNS, DHCP-Status, Port, PHY-Status (Duplex/Geschwindigkeit) und Timeout. | `check` | Druckt detaillierte Konfigurationsangaben, z. B. `IP: 192.168.1.100`. |
| `ip <x.x.x.x>` | Setzt die IP-Adresse des Geräts auf den angegebenen Wert. | `ip 192.168.1.100` | Aktualisiert die IP. Druckt `IP changed to 192.168.1.100`. |
| `ip` | Zeigt die aktuelle IP-Adresse an. | `ip` | Druckt `IP: 192.168.1.100`. |
| `port <Port>` | Setzt die Portnummer des Geräts. | `port 5000` | Aktualisiert den Port. Druckt `Port changed to 5000`. |
| `port` | Zeigt die aktuelle Portnummer an. | `port` | Druckt `Port: 5000`. |
| `mac <xx:xx:xx:xx:xx:xx>` | Setzt die MAC-Adresse des Geräts (hexadezimal, durch Doppelpunkte getrennt). | `mac 00:1A:2B:3C:4D:5E` | Aktualisiert die MAC. Druckt `MAC changed to 00:1A:2B:3C:4D:5E`. |
| `mac` | Zeigt die aktuelle MAC-Adresse an. | `mac` | Druckt `MAC: 00:1A:2B:3C:4D:5E`. |
| `timeout <Wert>` | Setzt den Timeout-Wert in Mikrosekunden. | `timeout 1000000` | Aktualisiert den Timeout. Druckt `Timeout changed to 1000000`. |
| `timeout` | Zeigt den aktuellen Timeout-Wert an. | `timeout` | Druckt `Timeout: 1000000`. |
| `tim <Wert>` | Setzt den Zeitkonstanten-Wert. | `tim 500` | Aktualisiert die Zeitkonstante. Druckt `Time const changed to 500`. |
| `defaults` | Stellt die Standardkonfiguration wieder her. | `defaults` | Setzt die Konfiguration auf Werksstandards zurück. |
| `reset` | Setzt das Gerät über den Watchdog-Timer zurück. | `reset` | Leitet einen Geräte-Reset ein. |
| `reboot` | Gleich wie `reset`, startet das Gerät neu. | `reboot` | Leitet einen Geräte-Reset ein. |
| `save` | Speichert die aktuelle Konfiguration im Flash-Speicher. | `save` | Schreibt Konfigurationsänderungen dauerhaft in den Flash. |

### Hinweise

- Befehle mit Parametern (z. B. `ip <x.x.x.x>`) erfordern exakte Formatierung. IP-Adressen müssen vier durch Punkte getrennte Dezimalzahlen sein (0-255), MAC-Adressen müssen sechs durch Doppelpunkte getrennte Hexadezimalbytes sein.
- Ungültige Formate lösen eine Fehlermeldung aus, z. B. `Invalid IP format` oder `Invalid MAC format`.
- Der Befehl `save` muss nach dem Ändern von Einstellungen (z. B. IP, Port, MAC) ausgegeben werden, um Änderungen dauerhaft im Flash zu speichern. Andernfalls gehen Änderungen beim Neustart verloren.
- Das Terminal kann sich je nach `timeout_error`-Zustand sperren/entsperren:
  - Wenn `timeout_error == 0`, sperrt das Terminal (`Terminal locked.`).
  - Wenn `timeout_error == 1`, entsperrt das Terminal (`Terminal unlocked.`).

## Verwendungsbeispiel

1. Verbindung zum Gerät mit minicom herstellen:

   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

2. Aktuelle Konfiguration prüfen:

   ```terminal
   check
   ```

   Ausgabe:

   ```terminal
   Current configuration:
   MAC: 00:1A:2B:3C:4D:5E
   IP: 192.168.1.100
   Subnet: 255.255.255.0
   Gateway: 192.168.1.1
   DNS: 8.8.8.8
   DHCP: 1 (1-Static, 2-Dynamic)
   PORT: 5000
   *******************PHY status**************
   PHY Duplex: Full
   PHY Speed: 100Mbps
   *******************************************
   Timeout: 1000000
   Ready.
   ```

3. Neue IP-Adresse setzen:

   ```terminal
   ip 192.168.1.200
   ```

   Ausgabe: `IP changed to 192.168.1.200`

4. Konfiguration speichern:

   ```terminal
   save
   ```

5. Gerät neu starten:

   ```terminal
   reset
   ```

## Technische Details

- **Eingabeverarbeitung**: Die Funktion `handle_serial_input` liest Zeichen nicht-blockierend über `getchar_timeout_us`. Sie ignoriert nicht druckbare ASCII-Zeichen (außer `\r`) und speichert gültige Eingaben in einem 64-Byte-Puffer. Wenn `\r` empfangen wird oder der Puffer voll ist, wird der Befehl verarbeitet und der Puffer geleert.
- **Befehlsverarbeitung**: Die Funktion `process_command` parst Befehle mit `strcmp` und `strncmp`. Bei Befehlen mit Parametern validiert `sscanf` die Eingabeformate (z. B. IP, MAC, Port). Änderungen werden auf globale Variablen (`net_info`, `port`, `TIMEOUT_US`, `time_constant`) angewendet und über `save_configuration` gespeichert.
- **Konfigurationsspeicherung**: Die Funktion `save_configuration` kopiert Einstellungen in eine `flash_config`-Struktur, die mit `save_config_to_flash` in den Flash geschrieben wird. Die Funktion `load_configuration` initialisiert Einstellungen beim Start aus dem Flash.
- **Sperrmechanismus**: Das Terminal sperrt, wenn `timeout_error == 0`, und entsperrt, wenn `timeout_error == 1`, und steuert so den Zugriff auf die serielle Schnittstelle.

## Fehlerbehebung

- **Keine Antwort vom Gerät**:
  - Richtigen seriellen Port prüfen (z. B. `ls /dev/tty*` zum Auflisten der Ports).
  - Korrekte Baud-Rate sicherstellen (Standard: 115200).
  - Serielle Kabelverbindungen und Stromversorgung des Geräts prüfen.
- **Zugriff auf seriellen Port verweigert**:
  - Benutzer zur Gruppe `dialout` hinzufügen (siehe Voraussetzungen).
- **Kryptische Ausgabe**:
  - Baud-Rate, Parität (8N1) und Flusssteuerungseinstellungen in minicom prüfen.
- **Fehler wegen ungültigem Format**:
  - Exakte Befehlssyntax sicherstellen (z. B. `ip 192.168.1.100`, nicht `ip 192.168.001.100`).
- **Änderungen nicht persistent**:
  - Befehl `save` nach dem Ändern von Einstellungen ausgeben.

## Einschränkungen

- Der Terminal-Puffer ist auf 63 Zeichen begrenzt (plus Null-Terminator). Längere Befehle werden abgeschnitten.
- Die serielle Schnittstelle ist nicht blockierend, daher kann schnelle Eingabe verloren gehen, wenn das Gerät beschäftigt ist.
- Der Sperrmechanismus (basierend auf `timeout_error`) kann den Zugriff unerwartet einschränken, wenn Netzwerkbedingungen Timeouts verursachen.

## Verwandte Dateien

- `serial_terminal.c`: Implementiert die serielle Terminalschnittstelle und Befehlsverarbeitung.
- `config.h`: Definiert die Struktur `configuration_t` und Funktionen wie `save_config_to_flash`, `load_config_from_flash` und `restore_default_config`.
- `wizchip_conf.h`: Definiert die Struktur `wiz_NetInfo` und Funktionen wie `getPHYSR` für die Netzwerkkonfiguration.
