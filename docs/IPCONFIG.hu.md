# IPCONFIG - stepper-ninja soros terminál konfiguráció

## Áttekintés

A `serial_terminal.c`-ben lévő `ipconfig` funkcionalitás soros terminál interfészt biztosít a stepper-ninja eszköz konfigurálásához. Lehetővé teszi a felhasználóknak a hálózati paraméterek (IP, MAC, port stb.) beállítását, a timeout kezelését, a konfiguráció flash memóriába mentését és az eszköz visszaállítását. A parancsokat soros kapcsolaton keresztül kell megadni, valós időben dolgozza fel őket, és ellenőrzi a helyes formátumot.

## Előfeltételek

- Egy soros terminál emulátor (pl. Linux-on **minicom**), amely az eszköz soros portjához (pl. `/dev/ttyACM0`) csatlakozik a megfelelő baud rátán (általában 115200 8N1).
- A felhasználónak a `dialout` csoport tagjának kell lennie a Linux soros portok eléréséhez:

  ```bash
  sudo usermod -a -G dialout $USER
  ```

- Az eszköznek bekapcsoltnak kell lennie és az eszköz programozásához használt USB-kábellel csatlakoztatva.

## Soros terminál beállítása (Linux)

1. Telepítsd a minicom-ot:

   ```bash
   sudo apt update
   sudo apt install minicom
   ```

2. Indítsd el a minicom-ot a megfelelő soros porthoz (pl. `/dev/ttyACM0`):

   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

3. Győződj meg arról, hogy a hardveres/szoftveres folyamvezérlés le van tiltva a minicom-ban (`Ctrl+A`, majd `O`, válaszd a **Serial port setup** lehetőséget, állítsd az **F** és **G** értékét `No`-ra).

## Támogatott parancsok

A soros terminál a terminálba gépelt parancsokat fogadja el, amelyeket kocsi-vissza (`\r`, Enter gomb) zár le. A parancsok kis- és nagybetű-érzékenyek, és az alábbiakban leírt pontos szintaxist kell követniük. Érvénytelen parancsok vagy helytelen formátumok hibaüzenetet adnak vissza.

| Parancs | Leírás | Példa | Kimenet/Hatás |
|---------|--------|-------|---------------|
| `help` | Megjeleníti az elérhető parancsok listáját és leírásaikat. | `help` | Kiírja a súgó menüt. |
| `check` | Megjeleníti az aktuális konfigurációt, beleértve a MAC-t, IP-t, alhálózatot, átjárót, DNS-t, DHCP-állapotot, portot, PHY-állapotot (duplex/sebesség) és timeoutot. | `check` | Részletes konfigurációs adatokat nyomtat ki, pl. `IP: 192.168.1.100`. |
| `ip <x.x.x.x>` | Az eszköz IP-címét a megadott értékre állítja. | `ip 192.168.1.100` | Frissíti az IP-t. `IP changed to 192.168.1.100`-t nyomtat ki. |
| `ip` | Megjeleníti az aktuális IP-címet. | `ip` | `IP: 192.168.1.100`-t nyomtat ki. |
| `port <port>` | Az eszköz portszámát állítja be. | `port 5000` | Frissíti a portot. `Port changed to 5000`-t nyomtat ki. |
| `port` | Megjeleníti az aktuális portszámot. | `port` | `Port: 5000`-t nyomtat ki. |
| `mac <xx:xx:xx:xx:xx:xx>` | Az eszköz MAC-címét állítja be (hexadecimális, kettősponttal elválasztva). | `mac 00:1A:2B:3C:4D:5E` | Frissíti a MAC-t. `MAC changed to 00:1A:2B:3C:4D:5E`-t nyomtat ki. |
| `mac` | Megjeleníti az aktuális MAC-címet. | `mac` | `MAC: 00:1A:2B:3C:4D:5E`-t nyomtat ki. |
| `timeout <érték>` | A timeout értéket mikroszekundumban állítja be. | `timeout 1000000` | Frissíti a timeoutot. `Timeout changed to 1000000`-t nyomtat ki. |
| `timeout` | Megjeleníti az aktuális timeout értéket. | `timeout` | `Timeout: 1000000`-t nyomtat ki. |
| `tim <érték>` | Az időállandó értékét állítja be. | `tim 500` | Frissíti az időállandót. `Time const changed to 500`-t nyomtat ki. |
| `defaults` | Visszaállítja az alapértelmezett konfigurációt. | `defaults` | A konfigurációt gyári alapértékekre állítja vissza. |
| `reset` | A watchdog timer segítségével visszaállítja az eszközt. | `reset` | Eszköz-visszaállítást indít. |
| `reboot` | Megegyezik a `reset`-tel, újraindítja az eszközt. | `reboot` | Eszköz-visszaállítást indít. |
| `save` | Az aktuális konfigurációt flash memóriába menti. | `save` | A konfigurációs változásokat flash-be menti. |

### Megjegyzések

- A paraméterekkel rendelkező parancsok (pl. `ip <x.x.x.x>`) pontos formázást igényelnek. Például az IP-címeknek négy, ponttal elválasztott decimális számnak kell lenniük (0-255), a MAC-címeknek pedig hat, kettősponttal elválasztott hexadecimális bájtnak.
- Érvénytelen formátumok hibaüzenetet generálnak, pl. `Invalid IP format` vagy `Invalid MAC format`.
- A `save` parancsot ki kell adni, hogy a változások flash-ben maradjanak a beállítások módosítása után (pl. IP, port, MAC). Ellenkező esetben a visszaállításkor elvesznek.
- A terminál a `timeout_error` állapot alapján zárolhat/feloldhat:
  - Ha `timeout_error == 0`, a terminál zárol (`Terminal locked.`).
  - Ha `timeout_error == 1`, a terminál felold (`Terminal unlocked.`).

## Használati példa

1. Csatlakozz az eszközhöz minicom segítségével:

   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

2. Ellenőrizd az aktuális konfigurációt:

   ```terminal
   check
   ```

   Kimenet:

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

3. Állíts be új IP-címet:

   ```terminal
   ip 192.168.1.200
   ```

   Kimenet: `IP changed to 192.168.1.200`

4. Mentsd a konfigurációt:

   ```terminal
   save
   ```

5. Indítsd újra az eszközt:

   ```terminal
   reset
   ```

## Technikai részletek

- **Bevitel kezelése**: A `handle_serial_input` függvény nem blokkoló módon olvas karaktereket a `getchar_timeout_us` segítségével. Figyelmen kívül hagyja a nem nyomtatható ASCII karaktereket (`\r` kivételével), és az érvényes bevitelt egy 64 bájtos pufferben tárolja. Amikor `\r` érkezik vagy a puffer megtelik, a parancs feldolgozódik és a puffer törlődik.
- **Parancs-feldolgozás**: A `process_command` függvény `strcmp` és `strncmp` segítségével elemzi a parancsokat. Paraméteres parancsoknál az `sscanf` validálja a beviteli formátumokat (pl. IP, MAC, port). A változások globális változókra (`net_info`, `port`, `TIMEOUT_US`, `time_constant`) kerülnek, és `save_configuration` révén mentődnek.
- **Konfiguráció tárolása**: A `save_configuration` függvény a beállításokat egy `flash_config` struktúrába másolja, amelyet a `save_config_to_flash` flash-be ment. A `load_configuration` függvény indításkor a flash-ből inicializálja a beállításokat.
- **Zárolási mechanizmus**: A terminál zárol, ha `timeout_error == 0`, és felold, ha `timeout_error == 1`, szabályozva a soros interfészhez való hozzáférést.

## Hibaelhárítás

- **Nincs válasz az eszköztől**:
  - Ellenőrizd a helyes soros portot (pl. `ls /dev/tty*` a portok listázásához).
  - Győződj meg arról, hogy a baud ráta helyes (alapértelmezett: 115200).
  - Ellenőrizd a soros kábel csatlakozásait és az eszköz tápellátását.
- **Soros porton hozzáférés megtagadva**:
  - Add a felhasználót a `dialout` csoporthoz (lásd Előfeltételek).
- **Érthetetlen kimenet**:
  - Ellenőrizd a baud rátát, paritást (8N1) és a folyamvezérlés beállításait a minicom-ban.
- **Érvénytelen formátum hibák**:
  - Biztosítsd a pontos parancsszintaxist (pl. `ip 192.168.1.100`, nem `ip 192.168.001.100`).
- **Változások nem maradnak meg**:
  - Add ki a `save` parancsot a beállítások módosítása után.

## Korlátozások

- A terminál puffere 63 karakterre korlátozott (plusz null terminátor). Az ennél hosszabb parancsok le lesznek vágva.
- A soros interfész nem blokkoló, ezért gyors bevitel elveszhet, ha az eszköz foglalt.
- A zárolási mechanizmus (amely a `timeout_error`-on alapul) váratlanul korlátozhatja a hozzáférést, ha a hálózati feltételek timeoutot okoznak.

## Kapcsolódó fájlok

- `serial_terminal.c`: A soros terminál interfészt és a parancs-feldolgozást valósítja meg.
- `config.h`: A `configuration_t` struktúrát és a `save_config_to_flash`, `load_config_from_flash`, valamint `restore_default_config` függvényeket definiálja.
- `wizchip_conf.h`: A `wiz_NetInfo` struktúrát és a `getPHYSR` függvényt definiálja a hálózati konfigurációhoz.
