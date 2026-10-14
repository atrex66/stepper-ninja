# Mach3 shared-memory ABI

The plugin uses the plain-data structures supplied in the official Mach3 Plugin
Wizard 1.1a, not modern MFC CWnd/CView subclasses, for Mach3-owned data.
MFC remains in use for the plugin's own configuration UI.

On this PC the old VS2019/MFC build placed StepsPerAxis 144 bytes too late:
587048 instead of 586904. Live read-only inspection found the profile's
100 steps/mm at 586904, 166.66666666666666 mm/s at 586952, and 250 mm/s^2
at 587000. The old build read 0.0025 and zero velocities, rejected Motor Tuning,
and failed to enter normal input polling. Its standalone input monitor could
still show GPIO levels, which masked the runtime initialization failure.

The official plain-data structs reproduce the live offsets. Compile-time checks
pin the 32-bit ABI, the input array offset/stride, and the motor tuning offset.
Never replace these structs with the local MFC version's classes.

Build requirements: MACH3_SDK_DIR selects the official SDK 2.62 wrapper;
MACH3_ABI_DIR selects the official Wizard's extracted MachIncludes directory.
For this workspace it is build/dependencies/mach-plugin-wizard/MachIncludes.
The SDK/Wizard files are build dependencies and are not shipped in the package.

References checked on 2026-10-14:
- Official downloads: https://www.machsupport.com/downloads-updates/mach3-downloads/
- Wizard: https://www.machsupport.com/wp-content/uploads/2013/04/MachPluginWizardSetup1.1a.msi
- Independent working plugin using the same plain-data structs: https://github.com/SheetCamv2/CNCRemote/tree/master/drivers/Mach3/MachIncludes
- User's working MagorCP reference: C:/MagorCP/MachDevice.cpp, lines 739 and 985-988;
  it writes Engine->InSigs[].Activated, confirming the IO handoff mechanism.

The runtime config test checks shared connection ownership, input inversion,
firmware/Mach3 state agreement, and the E-stop trip/release/Reset cycle against
a simulated controller. The real Pico check never sends ARM or motion commands.
