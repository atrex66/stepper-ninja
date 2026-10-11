# Mach3 Feed Hold és G31: működés, példák, ellenőrzés

[English](MACH3-FEEDHOLD-G31.en.md)

Fejlesztési implementáció, 2026-10-11. A hordozható driver és a Pico firmware
fordítható és tesztelhető Linux alatt. A Win32 MFC SDK adapter fordítása,
Mach3-ban történő betöltése és a fizikai gép működése még nincs igazolva.
A mozgásprotokoll verziója **3**: a drivert és a firmware-t együtt kell fordítani.

## Milyen példák alapján készült?

- [Warp9 / SmoothStepper gyártói FAQ, Feed Hold](https://warp9td.com/index.php/faq/faq-mach3):
  megkülönbözteti a Mach3 által tervezett és az eszközön végzett megállítást.
  A natív változatnál a Mach3 fékezési pályát küld, az eszköz kifuttatja a
  puffert; folytatáskor a Mach3 indulópályát készít. A StepperNinja ezt használja.
- [Hivatalos Mach3 SDK](https://www.machsupport.com/downloads-updates/mach3-downloads/):
  `g100IO/MachDevImplementation.cpp` mutatja a Notify/EX_FEEDHOLD/EX_RUN
  kapcsolódást, de a hozzá tartozó GRex FeedHold/Run metódusok üres csonkok;
  ez önmagában nem működő mozgásvezérlési példa.
  `ncPod/ncPod.cpp` és `ncPod/ExternalMovement.cpp` mutatja a DoExternalProbe,
  DoProbe és Axis[0].Probing/Sync interfészeket. A koordinátaszámításhoz
  `Probing/MenuHandler.cpp` CalcPoint képlete a mérvadó:
  `DepthCount / StepsPerAxis - CurrentOrigins`.
- [Dynomotion probe leírás](https://dynomotion.com/Help/Russian/Mach3Plugin/Mach3Probe.html)
  és a [NotifyProbeMach3.c forráspélda](https://github.com/parhansson/KMotionX/blob/master/C%20Programs/NotifyProbeMach3.c):
  a kontaktus pozícióját rögzíti, majd megállítja a koordinált mozgást.
  A C fájl egy gyártói példa külső tükrözése; a KFLOP saját változóit és
  mozgáskezelését használja, nem közvetlenül bemásolható Pico driver.

## Feed Hold: miért nem küldjük vissza a mozgást?

A plugin `m_ExternalFeedHold=false` értéket állít be. Normál programfutáskor
az EX_VMS rekordok 1 ms-os XYZ lépésnövekmények. Feed Hold alatt is továbbítjuk
őket, így a Mach3 által megtervezett fékezés eljut a firmware-ig.
A `GMoves.Stop` / `BufferEnding` határnál END-et küldünk; a firmware akkor
jelent teljes nyugalmat, amikor a puffer és a fizikai lépésrekord is kifutott.
A driver nem törli a pályát Feed Hold miatt. A feltöltési cél 100 rekord,
vagyis körülbelül 100 ms előre elküldött mozgás: ehhez hozzáadódik a Mach3
fékezési ideje és a kommunikáció késleltetése.

Cycle Startkor a Mach3 állítja elő a következő indulópályát. Nem tekerjük vissza
az `Engine->TrajIndex` értékét és nem küldjük újra a régi rekordokat.
A TrajIndex csak sikeres BATCH ACK után lép; a tényleges végrehajtást külön
accepted/executed számláló és programsor-report mutatja.
Elveszett UDP ACK esetén ugyanazt a sequence-ű csomagot újraküldjük;
a firmware felismeri az ismétlést, és nem hajtja végre még egyszer.
Ez a hálózati újraküldés különbözik a megszakított pálya újraindításától.

Eszközoldali azonnali Feed Hold is megvalósítható lenne: ahhoz a részben
végrehajtott rekord maradékát, a hátralévő pályát és a sebességi állapotot
is meg kellene őrizni, majd koordinált visszagyorsítást tervezni.
A félbeszakadt rekord egyszerű visszapumpálása túlmozgatást okozhat.
A jelenlegi külön HOLD protokollparancs töröl és tengelyenként fékez;
az SDK adapter nem használja normál Feed Holdhoz.

Stop/Reset megszakít és töröl. A régi SDK stop értesítése `Notify(-1)`;
EX_ESTOP is stop kérést ad. A normál Feed Hold utáni folytatás nem jelent
Stop vagy hálózati hiba utáni automatikus program-visszaállítást.

## G31: érintési pont és megállási pont

1. A Mach3 probe jele legyen engedélyezve: port 1, pin 5, vagyis DI5 / GPIO26.
   Active Low egyezzen a plugin bemenetinvertálásával. Már aktív érzékelővel
   nem indul mozgás. Más DI probe-hozzárendelést az adapter elutasít.
2. A callback a Mach3 abszolút gépi XYZ probe célját küldi lépésekben.
   A feed `_setup->feed_rate`, gépi egység/perc. A jelenlegi adapter G94-et
   támogat; G93/G95 és A/B/C probe mozgás elutasítást kap.
   A domináns lépéssebesség az XYZ fizikai úthosszból számolódik, eltérő
   tengelyskálák mellett is. A firmware közös skaláris profillal mozgatja az XYZ-t,
   minden érintett tengely sebesség- és gyorsuláskorlátját figyelembe véve.
3. A kontaktus GPIO IRQ-ban megállítja a lépéskiadót, kiolvassa a még nem
   könyvelt fizikai impulzusokat, és eltárolja az érintési XYZ-t. A következő
   időciklusokban a keresés irányában kontrolláltan fékez. Az eredeti célon túl
   nem tervez további mozgást. Az érintési számlálás és a későbbi megállási
   pozíció külön mező; az érintést a fékezés nem írja felül.
4. Minden report tartalmazza a PROBE sequence azonosítóját és állapotát:
   NONE / ACTIVE / HIT / MISS / CANCELLED. Az SDK adapter azonosítót ellenőriz,
   és csak befejezett HIT-et fogad el mérésnek. A munkakoordináta:
   `latched_steps / StepsPerAxis - CurrentOrigins`.
5. Az adapter feltölti a DepthCount, ProbeX/Y/Z és 2000..2002 paramétereket,
   majd feloldja az Axis[0].Probing várakozást és Sync-et kér.
   A cél kontaktus nélküli elérése PROBE_MISS hibával leállítja és tiltja a
   kimeneteket. E-stop/limit/watchdog szintén megszakít. G31 közbeni Feed Hold
   kontrollált PROBE_CANCEL fékezést kér; ennek nincs sikeres mérési eredménye,
   és az adapter megszakítja a G31-et. Cycle Start nem folytatja ezt a keresést.

A fizikai bemeneti IRQ és a számlálókiolvasás késleltetése mérendő; a rögzítés
nem jelent nulla késleltetésű hardveres capture-t. A pulse PIO leállítása és a
következő 1 ms-os fékezési ciklus között rövid rés lehet. Bemeneti debounce nincs.

## Mit igazolnak a tesztek?

`common/tests/controller_udp_contract.cpp` valódi UDP-klienssel és a firmware
állapotgépével fut. A pályatervező helyettesítő normál rekordokat, fékezési
rekordokat és Stop határt ad, majd szünet és Cycle Start indulópálya következik.
A teszt ellenőrzi a végpozíciót, a programsort, az accepted/executed számlálót,
és hogy normál Feed Hold nem küld HOLD/ABORT-ot. Elveszett ACK nem dupláz mozgást.
Ugyanez G31 reportot is átvisz, külön kontaktus- és megállási pozícióval.

`controller_probe_contract.cpp` ellenőrzi a koordinált keresést, részleges
fizikai impulzusok egyszeri könyvelését, ismételt PROBE kérés idempotenciáját,
a kontaktus nélküli végpontot, az aktív érzékelővel indulás elutasítását,
a megszakítást és a munkakoordináta-átváltást.

## Windows és hardver ellenőrzési sorrend

- Win32 MFC DLL fordítás/betöltés, Config Plugins és mentett XML beállítások.
- Valódi Mach3 pályán Feed Hold / Cycle Start: a Mach3 küld-e Stop/BufferEnding
  határt, a puffer nem underflow-zik-e, a végpont nem duplázódik-e.
  Ismétlés rövid pályán, íven, sorhatárnál és több egymást követő megállítással.
- G31 után a `GetVar(2000..2002)` érték az érintéshez tartozik-e, és Sync után
  sem cserélődik-e a fékezési végpontra. Ellenőrzés G54/G92 és szerszámoffset
  mellett is. Ez még nincs igazolva a tényleges Mach3 interpreterrel.
- Mozgás nélküli bemenetteszt, majd megfelelő mechanikai tartalékkal kis
  sebességű próba: IRQ késés, érintési és megállási távolság mérése.
- Aktív érzékelő, sikertelen keresés, Feed Hold, limit, E-stop és kábelhiba.

A LinuxCNC alapértelmezett profil és konfigurálhatósága változatlan:
a Mach3 saját állapotgépe/probe-je csak `mach3-3axis` buildben kerül a firmware-be.
USB mozgástranszport nincs megvalósítva; jelenleg UDP működik.
