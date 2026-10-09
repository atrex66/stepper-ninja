# Encoder index: folyamatos számláló és eltárolt indexállás

A működés a LinuxCNC encoder/Mesa HostMot2 elvét követi: a nyers számláló indexnél tovább számol, az index IRQ eltárolja a számlálóállást, a host pedig offsettel számolja a relatív pozíciót. Az `index-enable` HAL_IO pin: LinuxCNC emeli, a driver egy illeszkedő indexesemény után törli.

**A firmware-t és a HAL drivert együtt kell újrafordítani és frissíteni.** Megváltozott a csomagformátum: index-számláló, kérésazonosító és protokolljelölő került bele. Régi/új párosítást a csomagméret, checksum és protokolljelölő ellenőrzése elutasít; nincs régi protokollra visszaállás. Automatikus telepítés vagy firmware-feltöltés nem történt.

Az esemény addig megmarad a válaszokban, amíg vissza nem ér a hozzá tartozó alacsony kérés. Ismételt magas kérés nem élesít újra. A driver csak egyszer veszi át az indexállást, és egy új kérés elküldése előtt megvárja a régi esemény törlésének visszaigazolását. A nyolcbites kérésazonosító a késve érkező régi válaszokat és törléseket is megkülönbözteti; az összehasonlítás 128 befejezett kérésgenerációnál kisebb késést feltételez.

A nyers számláló és az indexállás ugyanabban a megszakítás ellen védett snapshotban kerül a válaszba. Az encoder IRQ és mintavétel a 0. magon fut. A másik mag watchdogja csak kéri az indexállapot letiltását; nem nullázza az encoder PIO/substep állapotát. Szétkapcsoláskor is folyamatos marad a nyers számláló.

A `raw-count` indexkor nem nullázódik. A `position = (raw_count - index_count) / scale`, 32 bites számlálókülönbséggel. Index után nem feltétlenül pontosan nulla a pillanatnyi pozíció: az index óta megtett utat is tartalmazza, mint a Mesa. A relatív pozíció nullázódása miatt a LinuxCNC `HOME_INDEX_NO_ENCODER_RESET` ennél a megoldásnál általában NO marad. A sebesség a folyamatos nyers számlálásból számolódik, ezért az index-offset változása nem okoz sebességugrást. Elveszett válasznál is azonos mintákhoz tartozó számláló- és időkülönbséget használ.

Ez nem FPGA-s hardverlatch: az index IRQ alatt kiolvasott PIO számláló pontosságát a megszakításkésés és FIFO-olvasás befolyásolja. A megszakításletiltott mintavétel idejét és az index ismétlési pontosságát a valós kártyán még mérni kell. A servo-threadben a driver olvasás/motion/írás sorrendje maradjon soros; több realtime szálban egyszerre futó közös driverbuffereket ez a javítás nem tesz biztonságossá.

Az eredeti, esetleges LinuxCNC segfault megszűnését csak gépi próbával/backtrace-szel lehet igazolni. Részletes protokollleírás és a tesztelt esetek: [ENCODER-INDEX.md](ENCODER-INDEX.md).
