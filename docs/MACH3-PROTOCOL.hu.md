# StepperNinja–Mach3: végrehajtás, reportok és protokoll

## 200 kHz-es Mach3 mód

A firmware és a DLL együtt frissítendő. A felső határ 200 lépés/ms/tengely,
200 000 lépés/s. A magas és alacsony STEP-időt egyaránt ellenőrzi.
1000 steps/mm és 10 000 mm/perc kb. 166,7 kHz: 2500 ns STEP-idővel használható.
A teljes 200 kHz-hez legfeljebb 2000 ns STEP-idő kell (DIR setup 5000 ns).
Hosszabb impulzust is lehet állítani, de kisebb sebesség szükséges hozzá.
A külön **StepperNinjaMill1000** és **StepperNinjaTurn1000** mintaprofil
1000 steps/mm, 10 000 mm/perc és 250 mm/s² értékeket tartalmaz.
Csak az 1000 steps/mm-nek megfelelő mechanikai/mikrolépéses beállításhoz használd;
a steps/mm puszta átírása nem növeli a fizikai felbontást.
A profil XML-eket C:\Mach3 alá, a hozzájuk tartozó makrókat azonos nevű
C:\Mach3\macros alkönyvtárba másold. Az új 1000-es indítók ezeket nyitják.
A meglévő profilok kalibrációja megmarad. A PIO rövid impulzusvonatokat ad
1 ms-os frame-ekben; az impulzusvonaton belüli frekvencia magasabb lehet az
átlagos frame-frekvenciánál. A fizikai 200 kHz oszcilloszkópos mérése még nincs meg.

## Bemenetenkénti invertálás

A Config → Config Plugins → StepperNinja CONFIG ablak **Inputs - polarity and live state**
részében mind a hét DI mellett külön **Invert** jelölőnégyzet van, a jel
nevével és GPIO-számával. Bejelölve aktív alacsony, üresen aktív magas.
A **Save** a jelenlegi profilba ment, és az adott bemenetre rendelt összes
Mach3 jel **Active Low** értékét is összehangolja (például X++, X-- és X Home).
A **Cancel** nem ment. A két gépprofil polaritása külön állítható.
A régi profilok InputInvert értékei automatikusan betöltődnek a hét kapcsolóba.



[English](MACH3-PROTOCOL.en.md)

Ez a fájl a repóban megvalósított fejlesztési verzió működését írja le.
A Windows DLL fordítása/betöltése és a fizikai Pico időzítése még nincs igazolva.
A firmware önálló jogot, homingot, dwellt és pufferelt XYZ lépésvégrehajtást
valósít meg, továbbá helyi koordinált G31-et érintési pozíciórögzítéssel.
A plugin Mach3 SDK 2.62-re írt adapter, natív Mach3 Feed Hold útvonallal.
A valódi SDK integráció Windows alatt még ellenőrizendő; ez fejlesztési kiadás.
Részletes példák: [Feed Hold és G31](MACH3-FEEDHOLD-G31.hu.md).

## Élő bemenetdiagnosztika

A futó StepperNinja driver konfigurációjában a bemenetkijelzés automatikusan
elindul, a driver meglévő kapcsolatán. Nem nyit másik USB-kapcsolatot, és
nem adja át a vezérlő tulajdonjogát. A kijelzés körülbelül 50 ms-onként frissül.
**H/L** = fizikai GPIO-szint, **ON/off** = az Invert szerinti aktív/inaktív állapot.
Az invertálás előnézete azonnali. A **Save** a firmware és a Mach3 bemeneti
polaritását is frissíti, a **Cancel** megtartja a korábbi beállításokat.
Az ablak bezárása után a driver ugyanazon a kapcsolaton folytatja a lekérdezést.
A kapcsolat mezőinek szerkesztése csak Save után érvényesül.
Kapcsolathibánál **-- / --** látszik. A monitor nem élesíti a vezérlőt.
Ha a plugin nincs mozgásvezérlőnek kiválasztva, a **Start input monitor**
gombbal indítható az önálló ellenőrzés.
A korábbi csomag frissített UF2-je továbbra is megfelelő.
Valódi USB-s Picóval ellenőrizve; a fizikai kapcsolók kapcsolgatása nincs tesztelve.

## 1. Feladatmegosztás és projektstruktúra

A Mach3 értelmezi a G-kódot és tervezi a koordinált pályát, gyorsulásokkal.
A plugin az `EX_VMS` interfészt választja, `ExTime = 0.001` idővel. A Mach3
`GMoves.ex/ey/ez` mezői ebben az üzemmódban az adott időszakasz lépésnövekményei;
a plugin ezeket törtmaradékos kerekítéssel alakítja egész lépéssé. A firmware
nem kap G-kódot, és programfuttatásnál nem tervez új gyorsulási profilt.
A STEP impulzusokat és a helyi homing/jog állapotgépet a Pico hajtja végre.

| Réteg | Forrás | Feladat |
|---|---|---|
| Fix fizikai profil | `common/profiles/mach3_3axis.h` | GPIO-kiosztás és profilazonosító |
| Csomagkódolás | `common/protocol/mach3_control.[ch]` | Verzió, mezők, CRC; nincs natív struktúra átküldése |
| Közös vezérlő | `common/motion/controller.[ch]` | Tulajdonos, puffer, jog, homing, hibák, állapotreport |
| Pico végrehajtó | `firmware/modules/mach3_runtime.c` | 1 ms timer, PIO, fizikai lépésszámlálás, GPIO/PWM |
| WIZ UDP | `firmware/modules/udp_transport.c` | Datagram fogadás/küldés; a `main.c`-ből kiemelve |
| PC UDP-kliens | `mach3-driver/src/controller_client.[h,cpp]` | Session, sorozatszám, újraküldés, report ellenőrzése |
| Mach3 adapter | `mach3-driver/src/MachDevImplementation.cpp` | SDK callbackek, pályaátadás, reportok visszaírása |

A LinuxCNC az alapértelmezett firmware-profil, a Mach3 külön buildválasztás:
`STEPPER_NINJA_PROFILE=mach3-3axis`. A Mach3 saját protokollt és végrehajtót
használ; a LinuxCNC meglévő konfigurálhatóságát és csomagformátumát nem szűkíti.
Az UDP-kiemelés a LinuxCNC régi `_sendto`/`_recvfrom` viselkedését megtartja.
A Mach3 datagram API külön ellenőrzi a méretet, eldobja és teljesen kiüríti a
túlméretes datagramot, valamint minden válasznál beállítja a tényleges címzettet.

## 2. Fix I/O és beállítások

| Funkció | GPIO | Pluginben használt szerep |
|---|---|---|
| X STEP / DIR | 0 / 1 | X |
| Y STEP / DIR | 2 / 3 | Y |
| Z STEP / DIR | 4 / 5 | Z |
| PWM | 6 | Főorsó, 0–100% |
| DO1 | 7 | Motor enable; ARM alatt automatikusan aktív |
| DO2 | 8 | Főorsó RUN |
| DO3 | 9 | Főorsó DIR, a Mach3 `Spindle.Direction` nemnulla értéke |
| DO4 | 10 | Ködhűtés |
| DO5 | 11 | Árasztásos hűtés |
| DO6 | 12 | Mach3 aktív OutSig, port 1 / pin 6 |
| DI1 | 13 | Helyi E-stop |
| DI2 | 14 | X home + limit |
| DI3 | 15 | Y home + limit |
| DI4 | 22 | Z home + limit |
| DI5 | 26 | G31 probe; Mach3 port 1 / pin 5 |
| DI6 | 27 | Általános bemenet |
| DI7 | 28 | Általános bemenet |

A Mach3 Input Signals port 1 / pin 1–7 az itt felsorolt DI1–DI7 logikai sorszáma;
a pin szám nem a Pico GPIO-száma. A plugin az aktív, ehhez rendelt jelek
`Activated` állapotát frissíti a fizikai bemenetből és a Mach3 `Negated` jelzőből.
A firmware helyi E-stop/home/limit érzékelését külön a plugin InputInvert maszkja
határozza meg: `aktív = fizikai szint XOR invert`. Alapérték 127, mind a hét
bemenet aktív alacsony, felhúzóval. A Mach3 signal-polaritását ezzel össze kell
hangolni. A helyi home/limit hozzárendelés a pluginben fix; az általános
Input Signals átnevezése nem változtatja meg a firmware védelmi szerepeit.

DO2–DO5 szerepét a plugin közvetlenül a főorsó/hűtés állapotából képzi;
a Mach3 általános kimeneti pin-hozzárendelése itt csak DO6-ot vezérli.
Az elektromos kimeneti polaritást az OutputInvert maszk állítja, a DIR-t a
DirectionInvert. Ezek a maszkok decimális számok, tengelyenként X/Y/Z bit 0/1/2.
Az OutSig `Negated` nem kerül külön alkalmazásra DO6-ra.

A Config Plugins ablak az adott profil `StepperNinjaMach3` XML szekciójába ment:
IP, port, ReplyTimeoutMs, WatchdogMs, PWMHz, PulseNs, DirSetupNs, InputInvert,
OutputInvert, DirectionInvert, HomePositive, HomeSlowPercent, HomeBackoff,
HomeTravel. Cancel nem ment. A konfiguráció csak Reset/E-stop állapotban nyitható;
a plugin szükség esetén DISARM-ot küld, a kapcsolatot megtartja a bemenetek lekérdezéséhez.
A Save letiltott vezérlő mellett frissíti a polaritást; új kapcsolat csak a kapcsolat
beállításainak módosításakor szükséges.

Alapérték: `192.168.0.177:8888`, válasz-timeout 500 ms, watchdog 500 ms,
PWM 10 kHz, STEP magas idő 2500 ns, DIR setup 5000 ns, negatív homing,
lassú keresés a gyors sebesség 10%-a, visszaállás 1 gépi egység,
maximális keresőút 1000 gépi egység. Az IP itt a cél firmware címe:
a plugin nem állítja át a Pico IP-címét vagy UDP-portját.

A StepsPerAxis, Velocities és Acceleration a Mach3 Motor Tuning értékeiből jön.
A plugin `lépés/s = gépi egység/s × lépés/gépi egység` átszámítást végez;
a homing gyors sebessége a Mach3 RefSpeed százaléka (hiányzó értéknél 10%).
ARM előtt újra elküldi a teljes konfigurációt. A saját GPIO-iránybeállítások
helyett a plugin DirectionInvert maszkja az irányfordítás forrása.

A PWM wrap értéke `floor(clk_sys / PWMHz) - 1`, tehát a tényleges frekvencia
kvantált. A 16 bites duty `0..65535`; a plugin a Mach3 `Spindle.ratio` értékét
0..1 közé korlátozza. RUN nélkül és DISARM/hiba esetén PWM=0, a PWM hardver
leáll. A kimenetek logikailag inaktívak; az elektromos szint invertált kimenetnél
magas lehet. Ez nem fordulatszám-visszacsatolás, nincs encoder ebben a profilban.

## 3. Kapcsolat, sorozatszám, ACK

A firmware egyetlen tulajdonost enged: forrás IPv4 + forrás UDP-port + nemnulla
32 bites session. A plugin egy tartós, a célhoz kapcsolt UDP socketet használ.
Új kapcsolat: CLAIM(sequence=1) → CONFIG → SET_POSITION → várakozás Resetre.
Csatlakozáskor/újracsatlakozáskor a plugin aktívra állítja a Mach3 EStopot;
ARM csak a felhasználó Reset feloldása után történik.

A szabályos következő parancs sorozatszáma `utolsó + 1`. Az utolsó parancs
azonos sequence és CRC melletti újraküldése ugyanazt az eredménykódot adja,
friss állapotpillanatképpel, mellékhatás ismétlése nélkül. Azonos sequence,
eltérő tartalom vagy kihagyott sequence elutasítás. Csak az utolsó kérés
ismételhető; a kliens egyszerre egy kérés ACK-jára vár. A feldolgozott,
jelentéstanilag hibás parancs is elfogyasztja a sequence-t.

A kliens legfeljebb háromszor küldi újra ugyanazokat a bájtokat, a teljes
ReplyTimeoutMs időkereten belül. A hibás CRC-jű, más verziójú, más sessionű,
más kéréshez tartozó vagy eltérő hosszúságú reportot figyelmen kívül hagyja.
Válaszhiánynál bezárja a socketet, a plugin EStopot jelez; az önálló firmware
watchdog a host nélkül is leállít. A watchdogot csak a tulajdonos szabályos,
elfogadott sequence-ű kérése vagy annak azonos ismétlése frissíti.
A diagnosztikai probe és idegen küldő nem frissíti.

Watchdog-lejáratkor a firmware leállít, elengedi a tulajdonost és törli a
konfigurált állapotot. Új CLAIM kell. A diagnosztikai kapcsolatteszt ettől
független: `SNM3` v1, 36 bájt kérés, 28 bájt válasz, nonce, profil, darabszámok,
CRC32. A capabilities mező jelenleg csak a diagnosztikai képességet hirdeti;
a teszt önmagában nem bizonyítja a mozgásprotokollt vagy a működő gépet.

## 4. Mozgáscsomag fejléc

Minden többbájtos szám little-endian; signed értékek kettes komplemensűek.
A teljes datagram legfeljebb 512 bájt: 24 bájt fejléc + payload + 4 bájt CRC32.

| Offset | Méret | Tartalom |
|---:|---:|---|
| 0 | 4 | ASCII `SNC3` |
| 4 | 2 | Verzió = 3; driver és firmware együtt frissítendő |
| 6 | 2 | Command; válaszban `command OR 0x8000` |
| 8 | 4 | Session |
| 12 | 4 | Request sequence |
| 16 | 2 | Payload hossza |
| 18 | 2 | Reserved = 0 |
| 20 | 4 | Profil = `0x03070106` |
| 24 | változó | Payload |
| utolsó 4 | 4 | CRC32 az összes korábbi bájton |

CRC: visszafordított polinom `0xedb88320`, kezdet `0xffffffff`, végeredmény
bitenként invertálva (CRC-32/ISO-HDLC). Érvénytelen fejléc/profil/CRC/hossz
esetén nincs válasz. A dekódolható, de elutasított parancs eredményreportot kap.
A válasz fejléce a kérés sessionjét és sequence-ét visszhangozza; a payload
acknowledged mezője mutatja, melyik sequence-t fogadta el a tulajdonos állapotgépe.
A CRC és a session hibadetektálás/kapcsolatazonosítás, nem kriptográfiai hitelesítés.

## 5. Parancsok

| ID | Parancs | Payload | Viselkedés |
|---:|---|---|---|
| 1 | CLAIM | üres | Kizárólagos kapcsolat foglalása; sequence=1 |
| 2 | CONFIG | 128 bájt | Teljes beállítás; disarmed, üres és idle állapotban |
| 3 | ARM | üres | Konfigurált, idle, inaktív E-stop mellett; törli a korábbi hibát, enable=1 |
| 4 | DISARM | üres | Azonnali puffer/PIO leállítás, logikai DO=0, PWM=0 |
| 5 | POLL | üres | Teljes report kérése, watchdog frissítése |
| 6 | IO | `u8 DO`, `u16 duty`, `u8 reserved=0` | Armed állapotban; enable automatikus, PWM RUN által kapuzva |
| 7 | JOG | `u8 axis`, `i8 dir`, `u16 zero`, `u32 step/s` | Axis 0..2, dir ±1; gyorsulással célsebességre áll |
| 8 | JOG_STOP | `u8 axis`, 3 zero | Az adott tengelyt lefékezi; más jogtengely maradhat |
| 9 | HOME | `u8 axis`, 3 zero | Egy tengely helyi homingja |
| 10 | ABORT | üres | DISARM jellegű leállítás; a korábbi hibakód megmarad |
| 11 | BATCH | `u32 first`, `u32 count`, `count × 16` rekord | Atomikus pufferátvétel; count=1..20 |
| 12 | START | üres | Armed, idle, nemüres pufferből programfuttatás |
| 13 | END | üres | A puffer kifutása szabályos befejezés, nem underflow |
| 14 | RELEASE | üres | Leállít, elengedi a kapcsolatot és a konfigurációt |
| 15 | SET_POSITION | 3 × i64 | Idle, üres puffer; számlálóbeállítás, nem mozgatás |
| 16 | DWELL | `u32 ms` | Armed/idle; 1..3600000 ms helyi várakozás |
| 17 | HOLD | üres | Armed; puffer eldobása, sebesség lefékezése, enable marad |
| 18 | MOVE | `u8 axis`, 3 zero, `i64 delta`, `u32 step/s` | Idle/armed; relatív egytengelyes célmozgás, gyorsulással |
| 19 | PROBE | 32 bájt, lásd alább | Armed/idle; koordinált XYZ keresés, külön kontaktusreport |
| 20 | PROBE_CANCEL | üres | Futó G31 kontrollált fékezése; eredménye CANCELLED |

PROBE payload: abszolút gépi XYZ cél `i64` lépésben a 0/8/16 offseten;
`u32` domináns tengelysebesség lépés/s a 24 offseten; `u8` bemenetbit
a 28 offseten (a plugin DI5-höz 4-et küld); 29..31 reserved=0.
Az aktív érzékelővel indulást elutasítja. Sebesség/gyorsulás minden érintett
tengely beállításával korlátozott. Hiányzó kontaktus a cél végén PROBE_MISS hiba.

BATCH rekord: `i32 X_delta`, `i32 Y_delta`, `i32 Z_delta`, `u32 Mach3_line`.
Minden rekord 1 ms. Maximum ±200 lépés/tengely/rekord és a konfigurált tengelysebesség;
a firmware az egész batch-et ellenőrzi, részlegesen nem veszi át.
A first az utolsó accepted_frame+1; ez sessionön belül nő, END/ABORT után is.
A feltöltés idle állapotban még nem mozgat. START külön szükséges.

Eredménykódok: 0 OK, 1 BAD_PACKET (fenntartott; hibás dekódolásnál most csendes
eldobás), 2 OWNER, 3 SEQUENCE, 4 STATE, 5 RANGE, 6 FULL, 7 UNSUPPORTED.
A plugin az elutasított mozgásparancsot hibának kezeli és ABORT/EStop állapotba lép.

CONFIG offsetek: 0 watchdog_ms/u32, 4 pwm_hz/u32, 8 pulse_ns/u32,
12 dir_setup_ns/u32, 16 input_invert/u8, 17 output_invert/u8,
18 dir_invert/u8, 19 reserved=0; majd három 36 bájtos tengelyrekord offset 20-tól.
Tengelyrekordon belül: 0 max_velocity/u32, 4 acceleration/u32,
8 home_fast/u32, 12 home_slow/u32, 16 backoff/u32, 20 home_travel/u32,
24 home_offset/i64, 32 home_direction/i8, 33 home_input/u8,
34 limit_input/u8 (255=tiltva), 35 reserved=0.

Érvényes tartományok: watchdog 100..2000 ms; PWM 4000..100000 Hz;
pulse 1000..10000 ns; DIR setup 1000..20000 ns; sebesség 1..200000 lépés/s;
gyorsulás 1..10000000 lépés/s². HomeSlow ≤ HomeFast ≤ MaxVelocity;
0 < backoff ≤ home_travel ≤ 100000000 lépés. HomeOffset és SET_POSITION
±10¹² lépésen belül. MOVE delta nemnulla, legfeljebb ±10⁹ lépés.
Az impulzusidő további korlátot ad:
`6 + ceil(max_velocity/1000) * 2 * ceil(pulse_ns/100) <= (900 - 25 - 30 - ceil(dir_setup_ns/1000)) * 10`.

## 6. A teljes report és Mach3 megfeleltetés

Minden dekódolt parancs válasza azonos, **112 bájtos állapot-payload**.
A firmware jelenleg kérés–válasz üzemű; nincs önálló, kéretlen UDP-report.
A plugin saját 25 ms Windows timert indít (névleg 40 Hz), valamint az SDK
Update/HighSpeedUpdate callbackje is frissíthet. A timer nem valós idejű.
Minden parancsválasz új pillanatképet ad, ezért a mozgás közbeni állapotok
nem csak a POLL válaszban érkeznek. A Pico 1 ms végrehajtója ettől független.

| Payload offset | Méret | Mező / jelentés |
|---:|---:|---|
| 0 | u32 | A kérés eredménye |
| 4 | u32 | Utolsó feldolgozott command sequence |
| 8 | u32 | Accepted frame: utolsó pufferbe vett rekord |
| 12 | u32 | Executed frame: utolsó teljesen kiadott rekord |
| 16 | u32 | Az executed rekord Mach3 programsor-azonosítója |
| 20 | u32 | Epoch: puffer/leállítási állapot törlésekor nő |
| 24 | u32 | 1 ms tick számláló, modulo 2³²; nem host óra |
| 28 | u16 | Pufferben várakozó rekordok száma |
| 30 | u16 | Szabad pufferhelyek, `256 - fill` |
| 32 | u8 | Mode |
| 33 | u8 | Fault |
| 34 | u8 | Armed |
| 35 | u8 | Configured |
| 36 | u8 | Aktív logikai bemenetek, bit0..6 |
| 37 | u8 | Logikai kimenetek, bit0..5 |
| 38 | u8 | Homed X/Y/Z maszk |
| 39 | u8 | Homing fázis |
| 40 | u16 | Aktuális PWM duty |
| 42 | u8 | END megérkezett |
| 43 | u8 | Folyamatban lévő, még nem igazolt időrekord |
| 44 | 4 | Reserved = 0 |
| 48 / 56 / 64 | i64 / tengely | Fizikai STEP/DIR számlálás szerinti X/Y/Z pozíció |
| 72 / 80 / 88 | i64 / tengely | Rögzített érintési XYZ, fizikai lépésben |
| 96 | u32 | Probe azonosító = elfogadott PROBE sequence |
| 100 | u8 | Probe állapot: NONE=0, ACTIVE=1, HIT=2, MISS=3, CANCELLED=4 |
| 101 | 1 | Nyers DI1..DI7 GPIO-szintek, 0..6 bit |
| 102 | 1 | Képességek: 0. bit = nyers bemenetek; 1. bit = STEP-frekvenciahatár érvényes |
| 103 | 4 | Maximum step rate / tengelyenkenti maximum STEP frekvencia, u32 Hz |
| 107 | 5 | Reserved = 0 |

| Mach3 mező | Frissítés |
|---|---|
| `Engine->TrajIndex` | Csak BATCH ACK után lép előre: átadás, nem fizikai készre jelentés |
| `Engine->Axis[i].Index` | Report actual position; az SDK 32 bites tartományán kívül hiba |
| `Engine->DisplayLine`, `MainPlanner->ELine` | Executed line, nem a legutóbb átküldött sor |
| `MainPlanner->ExternalStill` | Csak IDLE + pending=0 + fill=0 esetén igaz |
| `Engine->Referenced[i]` | Firmware homed maszk |
| `Engine->Jogging` | JOGGING vagy POSITIONING mode |
| `Engine->Homing` | HOMING mode vagy még sorban váró home kérés |
| `MainPlanner->SoftWait / EXHOME` | Homing indításkor set; minden home befejezése után clear |
| `Engine->DwellTime` | Kéréskor 1; firmware dwell befejezése után 0 |
| `Engine->InSigs[].Activated` | Port 1/pin 1..7 fizikai állapot, Mach3 Negated alkalmazásával |
| `Engine->Sync` | Mozgóból nyugalmi állapotba átmenetkor; koordinátaszinkron kérése |
| `Engine->EStop` | Kapcsolathiba, firmware fault vagy nem támogatott művelet esetén true |

A programsor-azonosító az SDK trajectory ID-ja: annak megjelenítési/off-by-one
szemantikáját az adott Mach3 verzióval Windows alatt kell ellenőrizni.
Az actual pozíció a **kiadott STEP jel számlálója**, nem a motor vagy a szán
encoderes mérése. A lépésvesztést mechanikailag nem érzékeli.

Példa: BATCH 1..20 átvétele után accepted=20, executed=0, position még változatlan.
START után a rekordok egyenként végrehajtódnak. Egy rekord executed értéke
csak akkor lép előre, ha mindhárom tengely fizikai számlálója pontosan annyi
lépést mért, amennyit a rekord kért, és a PIO befejezést jelzett.
Részleges impulzussor esetén a tényleges részpozíció megmarad, az executed ID
nem nő, TIMING fault keletkezik. A fill nem tartalmazza a pending rekordot.

## 7. Puffer, stop-határok és programsorrend

A firmware 256 rekordot tárol, legfeljebb 256 ms előretekintéssel és egy külön
pending rekorddal. A plugin frissítésenként legfeljebb öt 20-as batch-et küld,
100 ms körüli feltöltést céloz, és a report free értékét figyelembe veszi.
START általában legalább 40 rekord előtöltése után történik; rövid, lezárt
szakasz ennél kevesebbel is elindul.

Az SDK `GMoves.Stop` jelzőjénél a batch az adott rekordnál véget ér, a plugin
END-et küld, és a későbbi rekordokat csak a firmware kifutása után adja át.
`BufferEnding` és üres Mach3 átadási puffer szintén END-et eredményez.
Üres firmware-puffer END nélkül UNDERFLOW hiba, nincs korábbi parancs ismétlése.
A firmware végén ExternalStill/Sync ad visszajelzést a Mach3 várakozó állapotainak.

A programsor kijelzése és a mozgás készre jelentése megvan az adapterben.
Az M-kódok/makrók/tool-change és a Mach3 planner stop-határainak tényleges
ütemezését még Windows integrációs futtatással kell igazolni. Az IO külön
parancs, nem rekordonként időbélyegzett: azonos UDP ACK nem garantál önmagában
M-kód és mozgás szinkronizációt. Pontos programsorrendhez a Mach3 stop/wait
határainak és ExternalStill kezelésének helyes működése szükséges.

## 8. Helyi jog, homing, dwell és leállítás

A JOG célsebességet ad, a firmware 1 ms-onként gyorsulási korláttal változtatja
az aktuális sebességet. Több XYZ tengely egyszerre jogolhat. JOG_STOP tengelyenként
fékez; ABORT/DISARM azonnali. Program és homing közben jog nem indul.
A plugin JogOn callbackje jelenleg folytonos jogot küld; a MOVE parancs a protokollban
és a közös vezérlőben kész, de nincs külön incremental-jog SDK adapterág.

A home kéréseket a plugin X/Y/Z sorrendben sorba állítja; egyszerre egy tengely
homingol. Firmware fázisok: 1 gyors keresés; 2 visszaállás a kapcsolóról;
3 lassú keresés; 4 végső visszaállás. Már aktív kapcsolóról a 2. fázissal indul.
A gyors/lassú él a GPIO IRQ-ban leállítja a pulse PIO-t; a részlépésszámlálás
utána megmarad. A lassú érintésnél a pozíció HomeOffset lesz, majd a végső
visszaállás innen számolódik. A kész pozíció így offset + visszaállási út,
nem szükségképpen nulla. Homed csak az inaktív kapcsoló és legalább Backoff út
után lesz igaz. Fázisonként HomeTravel korlát él. Bemeneti debounce még nincs.

A firmware helyi 1 ms timerrel számolja a DWELL-t; közben nem ad lépést,
IO/report/watchdog működik. A plugin a Mach3 DwellMilli alapján alakít ms-ra.

A normál Feed Hold a Mach3 pályatervezőjére támaszkodik:
`m_ExternalFeedHold=false`. A plugin továbbküldi a fékezési rekordokat,
majd az END/Stop határig kifut a puffer. Nincs HOLD, ABORT vagy PurgeMovements
normál Feed Hold miatt. Cycle Startkor a Mach3 új indulópályáját továbbítja.
A protokoll HOLD parancsa külön, destruktív helyi fékezés; nem a normál
Mach3 Feed Hold eszköze. Stop/Reset viszont töröl; nincs automatikus pálya-visszajátszás.
G31 közbeni Feed Hold PROBE_CANCEL-t kér: lefékez és megszakítja a mérés eredményét.

Mode értékek: 0 IDLE, 1 JOGGING, 2 HOMING, 3 STREAM, 4 STOPPING,
5 DWELLING, 6 POSITIONING, 7 PROBING, 8 PROBE_BRAKING. Fault értékek: 0 NONE, 1 ESTOP,
2 WATCHDOG, 3 LIMIT, 4 HOME_TRAVEL, 5 UNDERFLOW, 6 TIMING, 7 PROBE_MISS.
Hiba: pulse FIFO/PIO törlés, armed=0, logikai DO=0, PWM=0, homed törlés.
A fizikai E-stop és mozgó tengely helyi limitje GPIO IRQ-ból is leállít.
Home alatt a saját, közös home/limit bemenet kereséshez megengedett.
A plugin újonnan észlelt hibát EStopként jelent; feloldásához Reset szükséges.
Firmware ARM törli a hibakódot, feltéve hogy az E-stop már inaktív.

A timer >1500 µs kihagyása vagy hiányos PIO-befejezés TIMING fault.
A 10 MHz pulse PIO magas ideje 100 ns-ra kerekített; a periódus
`(high_counter + low_counter + 7) × 100 ns`. A burst a callback első 900 µs-os
keretén belülre van tervezve, számítási/setup idővel és staging tartalékkal;
a következő rekord 1 ms-nál indul. Szinkron indul a három PIO SM.
Ez időablakon belüli impulzuselosztás, nem tökéletesen egyenletes, folyamatos
DDS frekvenciagenerátor. A határidőt és DIR hold időt hardveren mérni kell.
Aszinkron leállítás után irányváltás előtt legalább 20 µs hold van.

## 9. USB kapcsolat és a konzol átadása

A Mach3 profil USB CDC és UDP kapcsolatot is támogat. A Config Plugins ablakban
jelöld be a **USB connection** mezőt, és add meg a Windows port nevét, például
`COM3` vagy `COM12`. Kikapcsolt USB mezővel a meglévő UDP IP/port beállítás működik.
Mach3 csatlakoztatása előtt zárd be a terminálprogramot: a plugin kizárólagosan
nyitja meg a COM portot. A soros beállítás 115200, 8N1, aktív DTR, hardveres és
szoftveres flow control nélkül. A CDC mozgásidőzítését a pufferelt vezérlő adja,
nem a beállított soros baud rate.

Ugyanaz a CDC port induláskor konfigurációs konzol. Egy teljes, helyes CRC-jű,
3-as protokollverziójú `SN_CLAIM` lefoglalja az eszközt, és bináris
mozgáskommunikációra kapcsol. A félbehagyott konzolbevitel törlődik. Amíg USB
vagy UDP session birtokolja a vezérlőt, a konzolbevitel és a szokásos USB `printf`
kimenet tiltott, álló gépnél, E-stop és Feed Hold alatt is. A konzolparancsokat
és a bináris USB-bevitelt kizárólag core0 kezeli; core1 a Mach3 LED feladatot végzi.

USB-n a változatlan SNC3 csomagokat SLIP keretezés veszi körül: `C0` indítja/zárja
a keretet, az adatbeli `C0` helyett `DB DC`, a `DB` helyett `DB DD` megy.
A maximális csomagméret 512 bájt, a maximális kódolt keret 1026 bájt.
Feldolgozás előtt ellenőrzött a fejléc, verzió, profil, hossz és CRC. Hibás escape,
túl hosszú keret és hibás CRC elutasított; a dekóder határolónál újraszinkronizál.
Az USB saját transportazonosítós peer kulcsot használ, ezért aktív UDP sessiont
nem vehet át. Mindkét kapcsolatra érvényes a meglévő sequence/ACK, újraküldés,
végrehajtási report és watchdog.

Normál pluginleállításkor `SN_DISARM`, majd `SN_RELEASE` megy, beérkezik az utolsó
bináris válasz, és a port bezárul. A firmware a konzol átadása előtt leállítja
a mozgást és a kimeneteket. USB-s kapcsolatnál a kihúzás/DTR megszűnése azonnal
hibát jelez és felszabadítja a vezérlőt; lefagyott vagy összeomlott hostnál a
beállított watchdog is működik (alapérték 500 ms). Az USB bezárása UDP tulajdonost
nem szabadít fel. Felszabadítás után a maradék bemenet eldobódik, és 100 ms
beviteli csend után visszatér a konzol. Újracsatlakozáshoz új claim/konfiguráció
kell, a kapcsolat E-stop állapotból indul.

A plugin konfigurációs ablaka szünetelteti a frissítést és felszabadítja a
kapcsolatot, majd bezárás után újracsatlakozik. Az USB kapcsolatteszt átmenetileg
lefoglalja/felszabadítja az eszközt, engedélyezés nélkül; UDP-n továbbra is
diagnosztikai probe működik. A kapcsolatteszt nem igazolja a mozgást vagy a fizikai I/O-t.

WIZnet hardver nélküli Pico esetén a
`-DSTEPPER_NINJA_PROFILE=mach3-3axis` mellé add meg a
`-DSTEPPER_NINJA_MACH3_USB_ONLY=ON` opciót. Ez kihagyja a hálózati hardver
inicializálását és lekérdezését; `BOARD=pico` esetén a kimenet
`stepper-ninja-pico-usb-mach3-3axis.uf2`. Az alap Mach3 build mindkét kapcsolatot
megtartja, és WIZnet hardvert vár; USB használathoz Ethernet kábel nem kell.
Mindkét build megtartja a fix Mach3 lábkiosztást. LinuxCNC profilnál az USB-only
opció elutasított; az eredeti USB-konzol, protokoll, konfiguráció és alap build
megmarad. Mach3 esetén a CDC 1200 baudos reset gyorsbillentyűje tiltott,
a bináris kimenethez pedig korlátozott SDK írási timeout tartozik.

A Windows COM-olvasás a dokumentált
[COMMTIMEOUTS első bájtra váró/pufferelt olvasását](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts)
használja.

## 10. Ellenőrzések és fennmaradó munka

A Linux tesztek ellenőrzik a csomag/CRC/profil szerződést, a vezérlő állapotgépet,
a helyi UDP-kliens újraküldését és a régi report kiszűrését, az actual vs accepted
pozíciót, a hibákat/watchdogot, a jog/homing/dwell/hold/move működést,
a tényleges WIZ wrapper kódját regiszter/FIFO helyettesítővel, és a PIO program
utasításainak impulzusszámát/időzítését szoftveres modellel.
A Pico/W5500 firmware mindkét profilja külön fordítható.
Helyi eredmény (2026-10-14): 11/11 Mach3/USB/UDP/PIO/probe teszt és 32/32 LinuxCNC HAL
regressziós teszt sikeres; Pico/W5500 LinuxCNC és Mach3 firmware build sikeres,
ahogy a Pico USB-only Mach3 build is.
Ez nem helyettesíti a Windows SDK- és hardverellenőrzést.

Windows alatt: Win32 MFC DLL build; plugin betöltése; XML/config; Reset/reconnect;
SDK egységek és jog/spindle irányok; sima XYZ G-kód és ívfelbontás EX_VMS-ben;
programsor és stop/wait határok; dwell/M-kód/makró sorrend. Hardveren: pulse/low/DIR
idők és timer terhelés; lépésszámláló iránya; homing éle és elhagyása;
kábelkihúzás/E-stop/limit/kimeneti polaritás. G31: az érintési paramétereket
Sync után is ellenőrizni kell, eltolt munkakoordinátában is; Feed Hold:
a valódi Mach3 fékezési Stop/BufferEnding határok és Cycle Start ellenőrizendők.
Firmware jog soft-limit, STEP-invert és incremental-jog adapter nincs kész.

Források: [hivatalos SDK letöltés](https://www.machsupport.com/downloads-updates/mach3-downloads/),
[Art Fenerty EX_VMS leírása](https://www.machsupport.com/forum/index.php?topic=8122.0),
[Mach3 USB pluginok](https://www.machsupport.com/downloads-updates/plugins/).
A konkrét SDK megfeleltetés alapja továbbá a letöltött SDK `BlankMovement/MachDevice.cpp`,
`ncPod/ExternalMovement.cpp`, `MachIncludes/Engine.h`, `MachIncludes/TrajectoryControl.h`.
Buildparancsok: [driver README](../mach3-driver/README.md).
