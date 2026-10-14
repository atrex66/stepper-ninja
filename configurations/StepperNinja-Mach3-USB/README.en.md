# StepperNinja – Mach3 USB package

Raspberry Pi Pico / RP2040 and Pico 2 / RP2350 USB firmware. Packaged: 2026-10-14. Motion protocol: 3.
Development build; not yet validated for physical machine motion.

## Contents and settings

- `firmware/`: USB-only UF2 firmware for Pico/RP2040 and Pico 2/RP2350, no WIZnet required.
- `driver/StepperNinja.dll`: 32-bit Windows Mach3 plugin.
- `driver/sn-mach3-usb-check.exe`: connection check without motion.
- `profiles/StepperNinjaMill.xml`: XYZ mill, 1024.set screen.
- `profiles/StepperNinjaTurn.xml`: XZ lathe, radius mode, 1024.lset screen.
- `macros/`: separate base macros; M6End performs no automatic traverse.
- `docs/Pico-bekotes-pinout.pdf`: bilingual visual wiring guide.
- `docs/Pico-wiring-pinout.en.pdf`: separate English-only pinout and wiring guide.
- `pin-map.csv`, `package-info.json`, `SHA256SUMS.txt`: pin table, build data and checksums.

Both profiles use **100 steps/mm**, **10,000 mm/min**, **250 mm/s²**, native mm
and USB **COM12**. Each STEP represents 0.01 mm. Initial jog speed is 10%, up to
1,000 mm/min. Lathe X uses radius mode (UseDiam=0). Disabled Y also has valid
tuning values because the driver validates all three axes. Both LPT ports are disabled.

## 1. Flash the firmware

1. Close Mach3 and any serial terminal.
2. Hold BOOTSEL while connecting the Pico by USB.
3. Copy the `.uf2` file under `firmware` to the RPI-RP2 drive.
4. The Pico restarts and the boot drive disappears. Check the COM port in Device Manager.

COM ports vary by board and PC; the Pico 2 is COM13 on this PC. Use the `pico` UF2 for RP2040 and the `pico2` UF2 for RP2350.
Use the firmware and DLL from this package together; motion protocol versions must match.

## 2. Install the Mach3 files

An existing Mach3 installation and the standard 1024.set / 1024.lset screens are
required. With Mach3 closed, back up files that will be replaced, then copy:

| Package file/folder | Destination |
|---|---|
| driver/StepperNinja.dll | C:\Mach3\PlugIns\StepperNinja.dll |
| profiles/StepperNinjaMill.xml | C:\Mach3\StepperNinjaMill.xml |
| profiles/StepperNinjaTurn.xml | C:\Mach3\StepperNinjaTurn.xml |
| complete macros/StepperNinjaMill folder | C:\Mach3\macros\StepperNinjaMill |
| complete macros/StepperNinjaTurn folder | C:\Mach3\macros\StepperNinjaTurn |

Use your actual Mach3 installation directory if different. The DLL is x86 even on
64-bit Windows. Another PC may need the Microsoft
[Visual C++ v14 x86 runtime](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).
Do not overwrite the original Mach3Mill/Mach3Turn profiles. This PC is already configured.

## 3. Start and check the connection

Use `launchers/Start-Mill.cmd` or `Start-Turn.cmd` for the default C:\Mach3 path,
or run:

```text
C:\Mach3\Mach3.exe /p StepperNinjaMill
C:\Mach3\Mach3.exe /p StepperNinjaTurn
```

Only one profile can own the Pico at a time. The plugin ID is
`StepperNinja_USB_UDP_v0_3_3axis`. Both profiles enable and select it as the
controller. If prompted, choose StepperNinja. If needed, use Function Cfg's →
Reset Device Sel, restart, and select StepperNinja instead of the printer port.

Open Config → Config Plugins → StepperNinja CONFIG. Select **USB connection**,
the actual COM port, then **Test connection**. Configure while in Reset.
With Mach3 closed, run the separate diagnostic from the package root:

```powershell
.\driver\sn-mach3-usb-check.exe COM12
```

It only sends CLAIM / POLL / RELEASE: it does not configure, arm or move the
controller. Close other serial applications if the port is busy. The PDF shows
a top view with USB at the top. Physical pin numbers differ from GPIO numbers.
The lathe uses X=GP0/GP1 and Z=GP4/GP5; the Y pair GP2/GP3 is unused.

## Per-input inversion

Open Config → Config Plugins → StepperNinja CONFIG. Under **Inputs - polarity and live state**,
each of the seven DIs has its own **Invert** checkbox, with its signal name
and GPIO number. Checked means active low; unchecked means active high.
**Save** stores the setting in the current profile and synchronises **Active Low**
for every Mach3 signal mapped to that input, including limit/home aliases.
**Cancel** preserves the settings. Mill and lathe profiles can use different
polarities. Existing InputInvert masks automatically populate the checkboxes.

## 200 kHz Mach3 mode

Update firmware and DLL together. The ceiling is 200 steps/ms/axis,
200,000 steps/s. Both STEP high and low time are checked.
1000 steps/mm at 10,000 mm/min needs about 166.7 kHz and fits with PulseNs=2500.
For the full 200 kHz use PulseNs<=2000 with DirSetupNs=5000.
Longer pulses remain supported at lower rates.
Separate **StepperNinjaMill1000** and **StepperNinjaTurn1000** sample profiles
use 1000 steps/mm, 10,000 mm/min and 250 mm/s^2. Use them only with matching
mechanical/microstep calibration; changing steps/mm alone does not increase
physical resolution. Copy their XMLs to C:\Mach3 and their macro folders to
C:\Mach3\macros. The extra 1000 launchers open those profiles.
Existing calibration is retained in the original profiles.
PIO emits short pulse trains in 1 ms frames; instantaneous pulse-train frequency
can exceed the average frame rate. Physical 200 kHz has not been scope-verified.

## RP2350 / Pico 2 firmware

File: `firmware/stepper-ninja-pico2-usb-mach3-3axis.uf2`.
Target: standard Raspberry Pi Pico 2, RP2350, ARM Cortex-M33 (rp2350-arm-s).
USB-only Mach3 profile, same 3 axes / 7 DI / 6 DO / PWM GPIO assignments.
Use the same Mach3 DLL and mill/lathe profiles.
In BOOTSEL mode, copy the `pico2` UF2 to the Pico 2 boot drive.
The RP2350 build completed; physical RP2350 hardware has not been tested.
For other RP2350 boards, check their GPIO breakout and board configuration.

## Mach3 compatibility fix

This build uses the official Plugin Wizard plain-data ABI. The previous modern
MFC layout read incorrect Motor Tuning values and could block normal input
polling while the plugin monitor still worked. See docs/MACH3-ABI.en.md.
The configuration dialog also displays the last runtime driver error; COM setup
errors include the exact Win32 error code.

## Live input diagnostics

When StepperNinja is the selected motion controller, input diagnostics starts
automatically in its configuration dialog using the driver's existing connection.
It does not open another USB connection or hand over controller ownership.
The display refreshes approximately every 50 ms. **H/L** shows the physical
GPIO level; **ON/off** shows the active/inactive state after the Invert checkbox.
Inversion previews immediately. **Save** updates firmware polarity and Mach3
input polarity; **Cancel** retains the previous settings. On closing the dialog,
the driver continues polling through the same connection. Edited connection
settings apply only after Save. Connection failures show **-- / --**.
The monitor never arms the controller. If the plugin is not selected as the
motion controller, **Start input monitor** starts a standalone check.
The updated UF2 from the preceding package is still compatible.
Verified on a physical USB Pico; physically toggling switches was not tested.

## Machine-specific settings and limits

GPIO uses 3.3 V logic. 5/24 V signals require a suitable interface. Inputs default
to active low, InputInvert=127. Mach3 port 1/pins 1..7 refer to logical DI1..DI7.
STEP high time is 2.5 µs, DIR setup 5 µs, watchdog and reply timeout 500 ms.
Verify switch polarity, axis directions, calibration and actual machine travels.
Default homing is negative direction, 1 mm backoff and a 1000 mm search limit;
these are not measured machine travels.

The spindle type, speed range and interface were not supplied, so no spindle
calibration is included; template spindle settings were retained.
**Spindle encoder feedback and synchronised threading are not implemented.**
The Turn profile is an XZ base configuration. STEP is limited to 200 kHz per axis,
not yet measured on hardware. The configured velocity needs about 16.7 kHz.
Verified: 10/10 Windows tests, x86 DLL loading and startup export, USB communication
on the physical RP2040, both profile XMLs, and opening the mill profile in Mach3.
Physical motion and full lathe operation have not been validated.

No Mach3 installer, licence or SDK is included. Licence: LICENSE.txt.
References: docs/pinout-sources.txt. The PDF uses an original vector drawing;
physical pins follow the official Raspberry Pi Pico pinout.
