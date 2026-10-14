# Mach3 support

[Magyar](MACH3.hu.md)

The Mach3 development implementation lives in a separate `mach3-driver/`
directory and uses a separate firmware profile. The shared protocol and
firmware state machine are under `common/`. LinuxCNC remains the default
profile with flexible configuration. For Mach3, select
`STEPPER_NINJA_PROFILE=mach3-3axis`.

The fixed pinout provides three STEP/DIR axes (GPIO 0..5), spindle PWM (GPIO 6),
six outputs (GPIO 7..12), and seven inputs (GPIO 13,14,15,22,26,27,28).
UDP handling is isolated in `firmware/modules/udp_transport.c`.

Implemented: sessions and ACKs; buffered 1 ms XYZ step records; execution reports
based on physical STEP counts; local jog with acceleration; edge-driven homing;
coordinated G31 with contact position capture and braking; Feed Hold forwarding
Mach3's planned deceleration; dwell; I/O and PWM; watchdog; Mach3 configuration
dialog and SDK adapter source.

Windows DLL compilation/loading and physical hardware operation still need
validation. G31 SDK reports and the actual Mach3 Feed Hold / Cycle Start path
must be verified on Windows. See [operation and examples](MACH3-FEEDHOLD-G31.en.md).
USB CDC and UDP are implemented; USB shares the configuration console port
with automatic session-based handover. Windows and physical USB validation remain pending.

For exact data fields, state transitions, Mach3 reports, I/O roles, and known
limitations, see [execution and protocol](MACH3-PROTOCOL.en.md).
For build instructions, see the [build guide](MACH3-BUILD.en.md)
and the [driver README](../mach3-driver/README.md).
