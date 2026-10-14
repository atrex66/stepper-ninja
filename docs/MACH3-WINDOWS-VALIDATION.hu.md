# Mach3 Windows ellenőrzés – 2026-10-14

A 32 bites DLL és az RP2040 USB firmware Windows alatt lefordult. A DLL külön
32 bites folyamatban betölthető, a Mach3 belépési pontjai elérhetők. A valódi
RP2040-en a CLAIM / POLL / RELEASE kapcsolatpróba sikeres. A Mach3 felületén
belüli működés és a fizikai mozgás még nincs igazolva.

## Használt környezet

- Visual Studio Community 2019, MSVC v142 14.29.30133, x86 MFC.
- Windows SDK 10.0.19041.0: a korábbi telepítésből hiányzott a Desktop C++ SDK;
  ezt a Microsoft hivatalos 10.0.19041.5609 telepítőjével pótoltuk.
- CMake 3.31.5, Ninja 1.12.1, Python 3.10.0.
- Mach3 SDK 2.62: `build/dependencies/mach3-sdk-2.62/SDK`.
- Pico SDK 2.2.0, Arm GNU 14_2_Rel1, pioasm 2.2.0, picotool 2.2.0-a4.

A CMake a MachIncludes fejléceket a buildkönyvtárba másolja. Csak ebben a
másolatban váltja ki a ShockwaveFlash COM-importot az `IShockwaveFlash`
előzetes deklarációjával. A pointer és az osztály adattagjai megmaradnak;
a letöltött SDK nem változik. A fordításhoz nincs szükség Flash telepítésére.

## DLL és tesztek

A parancsok a `C:\stepper-ninja-test` könyvtárból futtathatók. Az alábbi
eszközútvonalak ezen a gépen érvényesek:

```powershell
$cmake = 'C:\Users\atrex\.pico-sdk\cmake\v3.31.5\bin\cmake.exe'
$ctest = 'C:\Users\atrex\.pico-sdk\cmake\v3.31.5\bin\ctest.exe'
& $cmake -S mach3-driver -B build/mach3-windows -G 'Visual Studio 16 2019' -A Win32 '-DCMAKE_SYSTEM_VERSION=10.0.19041.0' -DSN_BUILD_MACH3_PLUGIN=ON -DBUILD_TESTING=ON '-DMACH3_SDK_DIR=C:/stepper-ninja-test/build/dependencies/mach3-sdk-2.62/SDK'
& $cmake --build build/mach3-windows --config Release --parallel 4
& $ctest --test-dir build/mach3-windows -C Release --output-on-failure
& $cmake --build build/mach3-windows --config Release --target sn-mach3-usb-check
& ./build/mach3-windows/Release/sn-mach3-usb-check.exe COM12
```

Eredmény: 4/4 Windows magteszt sikeres. A külön x86 `LoadLibrary` ellenőrzés
és a szükséges Mach3 exportok ellenőrzése sikeres.
A DLL: `build/mach3-windows/Release/StepperNinja.dll`.
Telepített másolata: `C:\Mach3\PlugIns\StepperNinja.dll`.

Az USB diagnosztika nem élesít és nem küld mozgási parancsot. Csak akkor
futtasd, amikor más program nem használja a vezérlőt. A portszám újracsatlakozás
után változhat. A fizikai RP2040 válasza COM12-n:

```text
USB protocol 3, port COM12: mode=0, armed=0, fault=0, inputs=0, queue=0, free=256
PASS: CLAIM / POLL / RELEASE; no motion or ARM commands sent.
```

Ez a kommunikációt ellenőrzi; a STEP/DIR, bemenetek, PWM és a Mach3
tervezőjének hardveres viselkedését nem igazolja.

## USB firmware

```powershell
$env:PATH = 'C:\Users\atrex\.pico-sdk\toolchain\14_2_Rel1\bin;C:\Users\atrex\.pico-sdk\ninja\v1.12.1;C:\Users\atrex\AppData\Local\Programs\Python\Python310;' + $env:PATH
if (-not (Test-Path firmware/ioLibrary_Driver/README.md)) {
    Expand-Archive -LiteralPath firmware/ioLibrary_Driver.zip -DestinationPath firmware -Force
}
& $cmake -S firmware -B build/mach3-usb-firmware -G Ninja -DBOARD=pico -DSTEPPER_NINJA_PROFILE=mach3-3axis -DSTEPPER_NINJA_MACH3_USB_ONLY=ON '-DPICO_SDK_PATH=C:/Users/atrex/.pico-sdk/sdk/2.2.0' '-Dpioasm_DIR=C:/Users/atrex/.pico-sdk/tools/2.2.0/pioasm' '-Dpicotool_DIR=C:/Users/atrex/.pico-sdk/picotool/2.2.0-a4/picotool'
& $cmake --build build/mach3-usb-firmware --parallel 4
```

Kimenet: `build/mach3-usb-firmware/stepper-ninja-pico-usb-mach3-3axis.uf2`,
240128 bájt. A firmware-t feltöltöttük a `D:` RPI-RP2 bootmeghajtóra;
ezután az RP2040 `COM12` portként jelentkezett be. A PIO impulzusmodell
`common/tests/pio_pulse_contract.py` ellenőrzése is sikeres.

## Következő Mach3 ellenőrzés

1. Indítsd el a Mach3-at külön tesztprofillal.
2. A Config → Config Plugins listában engedélyezd a StepperNinja plugint;
   ha szükséges, indítsd újra a Mach3-at, és válaszd ki mozgásvezérlőként.
3. A plugin Config ablakában válaszd az **USB connection** lehetőséget,
   a port legyen **COM12**, majd futtasd a kapcsolatpróbát.
4. A fizikai mozgást a [részletes ellenőrzési sorrend](MACH3-FEEDHOLD-G31.hu.md)
   szerint ellenőrizd. A Motor Tuning és a bemeneti polaritások a tényleges
   géphez állítandók; ezeket a telepítés nem módosította.
## Indulási hiba javítása – Art Code 200702

Az első telepített DLL mellett a Mach3 induláskor Art Code 200702 hibával
leállt. A DLL átnevezésével a Mach3 újra elindult. A hiba külön folyamatban,
a `SetProName` export meghívásával is reprodukálható volt (access violation).

A régi SDK statikus MFC-re készült wrapperéből a `SetProName` függvényben
hiányzott az `AFX_MANAGE_STATE(AfxGetStaticModuleState())` váltás. A modern,
dinamikusan linkelt MFC-nek erre szüksége van, hogy az `AfxGetApp()` a DLL
saját alkalmazásobjektumát használja. A CMake ezt a buildmásolatban pótolja;
a letöltött SDK változatlan. A Microsoft előírása:
[Regular MFC DLLs Dynamically Linked to MFC](https://learn.microsoft.com/en-us/cpp/build/regular-dlls-dynamically-linked-to-mfc?view=msvc-170).

Új regressziós teszt: `mach3-plugin-startup`. A hívó folyamat szándékosan nem
hoz létre modern MFC alkalmazásobjektumot. Saját teszt-XML-lel a valódi
`SetProName` exportot hívja, hardvert nem használ. A javítás előtt összeomlott,
a javítás után sikeres; összesen 5/5 teszt átment.

A javított DLL visszatelepítése után a Mach3 elindult: a `Mach3 CNC Demo`
főablak válaszol, Art Code hibaablak nem jelent meg az ellenőrzéskor.
Ez az indulást igazolja; a plugin konfigurációját és a fizikai mozgást továbbra
is külön kell ellenőrizni. A korábbi DLL-ek a projekt build/plugin-backups mappájában maradtak meg.

## Engedélyezés és vezérlőválasztás – 9991

A 200702-es javítás után a felhasználó szerint a Mach3 már nem fagyott le
induláskor, de a plugin engedélyezésekor 9991-es hibát jelzett. A Windows
19:03:53-as hibajelentése a Mach3.exe 0x0011c4d8 címén access violationt rögzített;
a kód ezen a címen egy XML-bejegyzés null pointerét dereferálta `put_text` előtt.
Az engedélyezési hiba teljes felületi visszaellenőrzése még szükséges.

Két további javítás készült:

- A plugin neve `StepperNinja_USB_UDP_v0_3_3axis`. A korábbi megjelenítési név
  szóközt, `/` jelet és zárójeleket tartalmazott, így XML-elemként nem hozható
  létre. Az új név `IXMLDOMDocument::createElement` tesztje sikeres; a régi név
  szerepe a Mach3 konkrét engedélyezési hibájában még következtetés.
- A wrapper `InitControl` exportja `false` értéket ad vissza, így nem kéri a
  printer port motor indítását. A `myInitControl` már eddig is külső
  mozgásvezérlőt jelzett a `m_PrinterOn=false` adattaggal.

A kibővített regressziós teszt mindkét feltételt ellenőrzi; 5/5 teszt sikeres.
A javított DLL a `C:\Mach3\PlugIns\StepperNinja.dll` helyre került. A régi
DLL-ek a projekt `build/plugin-backups` könyvtárában vannak, nem a PlugIns alatt.

A vizsgált `Mach3Mill.xml` profilban a `Control/Device` értéke `Printer` volt.
A plugin engedélyezése és a mozgásvezérlő kiválasztása külön lépés: a Mach3
**Function Cfg's → Reset Device Sel.** menüjével új eszközválasztást kell kérni,
majd újraindításkor a StepperNinját kiválasztani. Az USB beállítás **COM12**.
A profil vezérlőválasztását nem módosítottuk közvetlen XML-írással.
A [Machsupport fórumon közölt beállítási sorrend](https://www.machsupport.com/forum/index.php?topic=8968.0)
azonos 9991-es engedélyezési tünetnél ezt az eszközválasztási lépést javasolja;
ez nem bizonyítja, hogy a mi esetünkben is kizárólag ez okozta a hibát.
