# StepperNinja Mach3 prototype driver

**Prototype:** Windows DLL compilation/loading, real Mach3 integration, and
physical hardware behavior have not been validated.

Build instructions: [English](../docs/MACH3-BUILD.en.md) /
[Magyar](../docs/MACH3-BUILD.hu.md).

A fixed three-axis Mach3 SDK adapter, portable protocol/controller core, and
Pico firmware runtime. The implementation includes buffered XYZ execution,
actual STEP-count reports, local ramped jog/homing/dwell, I/O/PWM, session
ownership, retransmission suppression and watchdogs. Windows DLL build/loading
and physical hardware behavior are not validated yet. G31 captures contact
separately from the braking endpoint. Native Mach3 Feed Hold drains the planned
braking path; Cycle Start supplies a new startup trajectory. The portable UDP
tests model this behavior; actual Mach3 planner and probe reports need Windows
validation. See [implementation and examples](../docs/MACH3-FEEDHOLD-G31.en.md)
([Hungarian](../docs/MACH3-FEEDHOLD-G31.hu.md)).
Motion protocol version 3 requires matching driver and firmware builds.

The default `linuxcnc` firmware profile keeps its flexible configuration and
existing protocol. Explicit `mach3-3axis` selects 3 axes, 7 DI, 6 DO and GP6 PWM.
GPIO mapping: `common/profiles/mach3_3axis.h`. UDP firmware transport is isolated
in `firmware/modules/udp_transport.c`; motion state is in `common/motion`.

Detailed operation, report mapping, wire format, known limitations and proposed
USB extension: [English protocol documentation](../docs/MACH3-PROTOCOL.en.md)
([Hungarian](../docs/MACH3-PROTOCOL.hu.md)).
Overview: [English](../docs/MACH3.en.md) / [Hungarian](../docs/MACH3.hu.md).
USB is not implemented; the current Pico USB interface is a console.

## Building

See the dedicated [English](../docs/MACH3-BUILD.en.md) or
[Hungarian](../docs/MACH3-BUILD.hu.md) build guide for dependencies, Linux
core tests, Pico firmware, optional LinuxCNC regression checks, Windows Win32
MFC DLL commands, output paths, and troubleshooting.

Linux builds test the portable core and firmware; building the Mach3 DLL requires
Windows, MSVC with x86 MFC, and the official Mach3 SDK. Build commands do not
install modules or flash firmware. Pico firmware can also be built natively
on Windows, so a Linux machine is not required. After real Mach3 and hardware
testing confirms reliable operation, a release with prebuilt firmware and driver
binaries is planned; that validated release is not available yet.

## Configuration

Config Plugins stores IP/port, reply timeout, watchdog, PWM frequency, STEP/DIR
timing, inversion masks and homing parameters in `StepperNinjaMach3` in the
current Mach3 XML profile. Axis scale/velocity/acceleration come from Motor
Tuning. Configuration requires Reset. Input Signals port 1/pins 1..7 represent
the fixed DI order, not physical GPIO numbers; firmware home/limit roles remain
fixed. Output roles and polarity details are in the protocol document.

The connection test uses a separate nonmoving diagnostic probe. It validates
nonce/CRC/version/profile/counts; success proves a diagnostic receiver answered,
not that motion, spindle or homing works. It does not renew the motion watchdog.
