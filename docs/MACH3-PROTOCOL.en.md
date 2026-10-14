# StepperNinja–Mach3: execution, reports, and protocol

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

## Per-input inversion

Open Config → Config Plugins → StepperNinja CONFIG. Under **Inputs - polarity and live state**,
each of the seven DIs has its own **Invert** checkbox, with its signal name
and GPIO number. Checked means active low; unchecked means active high.
**Save** stores the setting in the current profile and synchronises **Active Low**
for every Mach3 signal mapped to that input, including limit/home aliases.
**Cancel** preserves the settings. Mill and lathe profiles can use different
polarities. Existing InputInvert masks automatically populate the checkboxes.



[Magyar](MACH3-PROTOCOL.hu.md)

This document describes the development implementation in this repository.
Windows DLL compilation/loading and physical Pico timing have not been validated.
The firmware implements local jog, homing, dwell, buffered XYZ step execution,
and local coordinated G31 with contact position capture.
The plugin is an adapter written for Mach3 SDK 2.62, using native Mach3 Feed Hold.
Actual SDK integration still needs Windows validation; this is a development release.
For detailed examples, see [Feed Hold and G31](MACH3-FEEDHOLD-G31.en.md).

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

## 1. Responsibilities and project structure

Mach3 interprets G-code and plans the coordinated trajectory, including
acceleration. The plugin selects the `EX_VMS` interface with `ExTime = 0.001`.
In this mode, Mach3's `GMoves.ex/ey/ez` fields contain step increments for each
time interval; the plugin converts them to whole steps while retaining fractional
remainders. The firmware receives no G-code and does not plan a new acceleration
profile during program execution. The Pico generates STEP pulses and runs the
local homing/jog state machine.

| Layer | Source | Responsibility |
|---|---|---|
| Fixed physical profile | `common/profiles/mach3_3axis.h` | GPIO mapping and profile identifier |
| Packet encoding | `common/protocol/mach3_control.[ch]` | Version, fields, CRC; no native structures sent over the wire |
| Shared controller | `common/motion/controller.[ch]` | Ownership, buffer, jog, homing, faults, status report |
| Pico runtime | `firmware/modules/mach3_runtime.c` | 1 ms timer, PIO, physical step counting, GPIO/PWM |
| WIZ UDP | `firmware/modules/udp_transport.c` | Datagram receive/send, extracted from `main.c` |
| PC UDP client | `mach3-driver/src/controller_client.[h,cpp]` | Session, sequence, retransmission, report validation |
| Mach3 adapter | `mach3-driver/src/MachDevImplementation.cpp` | SDK callbacks, trajectory transfer, report updates |

LinuxCNC is the default firmware profile; Mach3 is an explicit build selection:
`STEPPER_NINJA_PROFILE=mach3-3axis`. Mach3 uses its own protocol and runtime;
it does not restrict LinuxCNC's existing configurability or packet format.
The UDP extraction preserves LinuxCNC's legacy `_sendto`/`_recvfrom` behavior.
The separate Mach3 datagram API validates size, discards and fully drains
oversized datagrams, and sets the actual destination for every reply.

## 2. Fixed I/O and configuration

| Function | GPIO | Role used by the plugin |
|---|---|---|
| X STEP / DIR | 0 / 1 | X |
| Y STEP / DIR | 2 / 3 | Y |
| Z STEP / DIR | 4 / 5 | Z |
| PWM | 6 | Spindle, 0–100% |
| DO1 | 7 | Motor enable; automatically active while armed |
| DO2 | 8 | Spindle RUN |
| DO3 | 9 | Spindle DIR, nonzero Mach3 `Spindle.Direction` |
| DO4 | 10 | Mist coolant |
| DO5 | 11 | Flood coolant |
| DO6 | 12 | Active Mach3 OutSig, port 1 / pin 6 |
| DI1 | 13 | Local E-stop |
| DI2 | 14 | X home + limit |
| DI3 | 15 | Y home + limit |
| DI4 | 22 | Z home + limit |
| DI5 | 26 | G31 probe; Mach3 port 1 / pin 5 |
| DI6 | 27 | General input |
| DI7 | 28 | General input |

Mach3 Input Signals port 1 / pins 1–7 refer to the logical DI1–DI7 numbers
listed here; pin numbers are not Pico GPIO numbers. The plugin updates the
`Activated` state of active signals assigned to these inputs from the physical
input and Mach3's `Negated` flag. Firmware local E-stop/home/limit detection
is determined separately by the plugin's InputInvert mask:
`active = physical level XOR invert`. The default is 127: all seven inputs
are active low with pull-ups. Mach3 signal polarity must agree with this.
Local home/limit assignments are fixed in the plugin; renaming general Input
Signals does not change the firmware's protective roles.

The plugin derives DO2–DO5 directly from spindle/coolant state; general Mach3
output pin assignments control only DO6 here. OutputInvert sets electrical
output polarity, and DirectionInvert sets DIR polarity. These masks are decimal
numbers; axis bits 0/1/2 correspond to X/Y/Z. OutSig `Negated` is not applied
separately to DO6.

The Config Plugins dialog saves these fields in the current profile's
`StepperNinjaMach3` XML section: IP, port, ReplyTimeoutMs, WatchdogMs, PWMHz,
PulseNs, DirSetupNs, InputInvert, OutputInvert, DirectionInvert, HomePositive,
HomeSlowPercent, HomeBackoff, and HomeTravel. Cancel does not save.
Configuration can be opened only in Reset/E-stop state; before opening, the
plugin disarms if necessary and keeps the existing session for input polling.
Save applies polarity while disarmed; only changed transport settings require reconnecting.

Defaults: `192.168.0.177:8888`, reply timeout 500 ms, watchdog 500 ms,
PWM 10 kHz, STEP high time 2500 ns, DIR setup 5000 ns, negative homing direction,
slow search at 10% of fast search speed, backoff 1 machine unit, and maximum
search travel 1000 machine units. The IP is the target firmware's address:
the plugin does not change the Pico's IP address or UDP port.

StepsPerAxis, Velocities, and Acceleration come from Mach3 Motor Tuning.
The plugin converts `steps/s = machine units/s × steps/machine unit`;
fast homing speed is Mach3's RefSpeed percentage, defaulting to 10% if missing.
Before ARM, it resends the full configuration. The plugin's DirectionInvert
mask is the source of direction inversion rather than individual GPIO direction
settings.

PWM wrap is `floor(clk_sys / PWMHz) - 1`, so the actual frequency is quantized.
The 16-bit duty range is `0..65535`; the plugin clamps Mach3's `Spindle.ratio`
to 0..1. Without RUN, and on DISARM/fault, PWM=0 and PWM hardware stops.
Outputs are logically inactive; an inverted output may be electrically high.
This is not spindle-speed feedback; the profile has no encoder.

## 3. Connection, sequence numbers, and ACK

The firmware permits one owner: source IPv4 + source UDP port + nonzero 32-bit
session. The plugin uses a persistent UDP socket connected to the target.
New connection: CLAIM(sequence=1) → CONFIG → SET_POSITION → wait for Reset release.
On connection/reconnection, the plugin asserts Mach3 EStop; ARM happens only
after the user releases Reset.

The next valid command sequence is `last + 1`. Resending the last command with
an identical sequence and CRC returns the same result code with a fresh status
snapshot, without repeating side effects. The same sequence with different
content, or a skipped sequence, is rejected. Only the last request can be
repeated; the client waits for one request's ACK at a time. A processed command
with invalid semantics also consumes its sequence number.

The client retransmits the same bytes up to three times within the overall
ReplyTimeoutMs budget. It ignores reports with invalid CRC, another version,
another session, another request identifier, or a different length.
If no reply arrives, it closes the socket and the plugin reports EStop;
the independent firmware watchdog also stops motion without the host.
Only a valid sequence request from the owner, or its identical retry, refreshes
the watchdog. Diagnostic probes and other senders do not refresh it.

On watchdog expiry, the firmware stops, releases the owner, and clears the
configured state. A new CLAIM is required. The diagnostic connection test is
independent: `SNM3` v1, 36-byte request, 28-byte reply, nonce, profile, counts,
and CRC32. The capabilities field currently advertises only diagnostic support;
the test alone does not establish that the motion protocol or machine works.

## 4. Motion packet header

All multibyte numbers are little-endian; signed values use two's complement.
A datagram is at most 512 bytes: 24-byte header + payload + 4-byte CRC32.

| Offset | Size | Contents |
|---:|---:|---|
| 0 | 4 | ASCII `SNC3` |
| 4 | 2 | Version = 3; update driver and firmware together |
| 6 | 2 | Command; reply uses `command OR 0x8000` |
| 8 | 4 | Session |
| 12 | 4 | Request sequence |
| 16 | 2 | Payload length |
| 18 | 2 | Reserved = 0 |
| 20 | 4 | Profile = `0x03070106` |
| 24 | variable | Payload |
| final 4 | 4 | CRC32 over all preceding bytes |

CRC: reflected polynomial `0xedb88320`, initial value `0xffffffff`, final
bitwise inversion (CRC-32/ISO-HDLC). Invalid header/profile/CRC/length receives
no reply. A decodable but rejected command receives a result report.
The reply header echoes the request's session and sequence; the payload's
acknowledged field identifies the sequence accepted by the owner state machine.
CRC and session provide error detection/connection identification, not
cryptographic authentication.

## 5. Commands

| ID | Command | Payload | Behavior |
|---:|---|---|---|
| 1 | CLAIM | empty | Claim exclusive connection ownership; sequence=1 |
| 2 | CONFIG | 128 bytes | Full configuration; disarmed, empty, and idle |
| 3 | ARM | empty | Configured, idle, E-stop inactive; clears previous fault, enable=1 |
| 4 | DISARM | empty | Immediate buffer/PIO stop, logical DO=0, PWM=0 |
| 5 | POLL | empty | Request full report, refresh watchdog |
| 6 | IO | `u8 DO`, `u16 duty`, `u8 reserved=0` | Armed; enable automatic, PWM gated by RUN |
| 7 | JOG | `u8 axis`, `i8 dir`, `u16 zero`, `u32 step/s` | Axis 0..2, dir ±1; accelerate to target velocity |
| 8 | JOG_STOP | `u8 axis`, 3 zero bytes | Brake that axis; other jog axes may continue |
| 9 | HOME | `u8 axis`, 3 zero bytes | Local homing of one axis |
| 10 | ABORT | empty | DISARM-like stop; existing fault code preserved |
| 11 | BATCH | `u32 first`, `u32 count`, `count × 16` records | Atomic buffer acceptance; count=1..20 |
| 12 | START | empty | Armed, idle; start program execution from a nonempty buffer |
| 13 | END | empty | Buffer draining is normal completion, not underflow |
| 14 | RELEASE | empty | Stop, release connection and configuration |
| 15 | SET_POSITION | 3 × i64 | Idle, empty buffer; set counters without motion |
| 16 | DWELL | `u32 ms` | Armed/idle; local wait of 1..3600000 ms |
| 17 | HOLD | empty | Armed; purge buffer, decelerate, keep enable |
| 18 | MOVE | `u8 axis`, 3 zero bytes, `i64 delta`, `u32 step/s` | Idle/armed; relative single-axis target move with acceleration |
| 19 | PROBE | 32 bytes, see below | Armed/idle; coordinated XYZ search, separate contact report |
| 20 | PROBE_CANCEL | empty | Controlled braking of an active G31; result is CANCELLED |

PROBE payload: absolute machine XYZ targets as `i64` step counts at offsets
0/8/16; `u32` dominant-axis velocity in steps/s at offset 24; `u8` input bit
index at offset 28 (the plugin sends 4 for DI5); offsets 29..31 reserved=0.
Starting with an active sensor is rejected. Velocity/acceleration are constrained
by the configuration of every moving axis. Reaching the target without contact
produces a PROBE_MISS fault.

BATCH record: `i32 X_delta`, `i32 Y_delta`, `i32 Z_delta`, `u32 Mach3_line`.
Each record lasts 1 ms. Maximum ±200 steps/axis/record and the configured axis
velocity apply; the firmware validates the whole batch and never accepts it
partially. First is the last accepted_frame+1; this increases within the session,
even after END/ABORT. Filling the buffer while idle does not move the machine.
A separate START is required.

Result codes: 0 OK, 1 BAD_PACKET (reserved; decoding failures are currently
silently discarded), 2 OWNER, 3 SEQUENCE, 4 STATE, 5 RANGE, 6 FULL, 7 UNSUPPORTED.
The plugin treats a rejected motion command as a fault and enters ABORT/EStop.

CONFIG offsets: 0 watchdog_ms/u32, 4 pwm_hz/u32, 8 pulse_ns/u32,
12 dir_setup_ns/u32, 16 input_invert/u8, 17 output_invert/u8,
18 dir_invert/u8, 19 reserved=0; followed by three 36-byte axis records
starting at offset 20. Within each axis record: 0 max_velocity/u32,
4 acceleration/u32, 8 home_fast/u32, 12 home_slow/u32, 16 backoff/u32,
20 home_travel/u32, 24 home_offset/i64, 32 home_direction/i8,
33 home_input/u8, 34 limit_input/u8 (255=disabled), 35 reserved=0.

Valid ranges: watchdog 100..2000 ms; PWM 4000..100000 Hz;
pulse 1000..10000 ns; DIR setup 1000..20000 ns; velocity 1..200000 steps/s;
acceleration 1..10000000 steps/s². HomeSlow ≤ HomeFast ≤ MaxVelocity;
0 < backoff ≤ home_travel ≤ 100000000 steps. HomeOffset and SET_POSITION
must be within ±10¹² steps. MOVE delta must be nonzero and within ±10⁹ steps.
Pulse timing adds another constraint:
`6 + ceil(max_velocity/1000) * 2 * ceil(pulse_ns/100) <= (900 - 25 - 30 - ceil(dir_setup_ns/1000)) * 10`.

## 6. Full report and Mach3 mapping

Every decoded command receives the same **112-byte status payload**.
The firmware currently operates by request/reply; there are no unsolicited
UDP reports. The plugin starts its own 25 ms Windows timer (nominally 40 Hz);
SDK Update/HighSpeedUpdate callbacks can also update it. The timer is not
real-time. Every command reply provides a fresh snapshot, so motion status
arrives in more than POLL replies. The Pico's 1 ms runtime is independent.

| Payload offset | Size | Field / meaning |
|---:|---:|---|
| 0 | u32 | Request result |
| 4 | u32 | Last processed command sequence |
| 8 | u32 | Accepted frame: last record accepted into the buffer |
| 12 | u32 | Executed frame: last fully emitted record |
| 16 | u32 | Mach3 program-line identifier of the executed record |
| 20 | u32 | Epoch: increases when buffer/stop state is cleared |
| 24 | u32 | 1 ms tick counter, modulo 2³²; not a host clock |
| 28 | u16 | Number of records waiting in the buffer |
| 30 | u16 | Free buffer slots, `256 - fill` |
| 32 | u8 | Mode |
| 33 | u8 | Fault |
| 34 | u8 | Armed |
| 35 | u8 | Configured |
| 36 | u8 | Active logical inputs, bits 0..6 |
| 37 | u8 | Logical outputs, bits 0..5 |
| 38 | u8 | Homed X/Y/Z mask |
| 39 | u8 | Homing phase |
| 40 | u16 | Current PWM duty |
| 42 | u8 | END received |
| 43 | u8 | In-flight time record not yet confirmed |
| 44 | 4 | Reserved = 0 |
| 48 / 56 / 64 | i64 / axis | X/Y/Z position from physical STEP/DIR counting |
| 72 / 80 / 88 | i64 / axis | Captured contact XYZ in physical steps |
| 96 | u32 | Probe identifier = accepted PROBE sequence |
| 100 | u8 | Probe state: NONE=0, ACTIVE=1, HIT=2, MISS=3, CANCELLED=4 |
| 101 | 1 | Raw DI1..DI7 GPIO levels, bits 0..6 |
| 102 | 1 | Capabilities: bit 0 = raw levels valid; bit 1 = maximum step rate valid |
| 103 | 4 | Maximum step rate / tengelyenkenti maximum STEP frekvencia, u32 Hz |
| 107 | 5 | Reserved = 0 |

| Mach3 field | Update |
|---|---|
| `Engine->TrajIndex` | Advances only after BATCH ACK: transfer, not physical completion |
| `Engine->Axis[i].Index` | Report actual position; fault outside the SDK's 32-bit range |
| `Engine->DisplayLine`, `MainPlanner->ELine` | Executed line, not the most recently transferred line |
| `MainPlanner->ExternalStill` | True only for IDLE + pending=0 + fill=0 |
| `Engine->Referenced[i]` | Firmware homed mask |
| `Engine->Jogging` | JOGGING or POSITIONING mode |
| `Engine->Homing` | HOMING mode or a queued home request |
| `MainPlanner->SoftWait / EXHOME` | Set on homing start; cleared after all homing completes |
| `Engine->DwellTime` | 1 on request; 0 after firmware dwell completes |
| `Engine->InSigs[].Activated` | Physical port 1/pin 1..7 state with Mach3 Negated applied |
| `Engine->Sync` | On transition from motion to standstill; request coordinate synchronization |
| `Engine->EStop` | True for connection error, firmware fault, or unsupported operation |

The program-line identifier is the SDK trajectory ID: its display/off-by-one
semantics must be checked with the target Mach3 version on Windows.
Actual position counts **emitted STEP signals**, not encoder measurements of
the motor or carriage. It cannot detect mechanical lost steps.

Example: after accepting BATCH 1..20, accepted=20, executed=0, and position is
unchanged. After START, records execute one at a time. A record's executed value
advances only if all three physical axis counters measured exactly the number
of steps requested and the PIO indicated completion. With a partial pulse train,
the actual partial position is preserved, the executed ID does not advance,
and a TIMING fault occurs. Fill does not include the pending record.

## 7. Buffer, stop boundaries, and program order

The firmware stores 256 records, up to 256 ms of queued motion, plus a separate
pending record. The plugin sends at most five batches of 20 records per update,
targets approximately 100 ms of buffered motion, and respects the report's free
count. START normally follows preloading at least 40 records; a short section
with a known ending can start with fewer.

At the SDK's `GMoves.Stop` flag, the batch ends with that record, the plugin
sends END, and subsequent records are transferred only after firmware motion
has drained. `BufferEnding` with an empty Mach3 transfer buffer also causes END.
An empty firmware buffer without END produces UNDERFLOW; old commands are not
repeated. At firmware completion, ExternalStill/Sync informs Mach3's wait states.

The adapter provides program-line display and motion-completion reporting.
Actual scheduling of M-codes/macros/tool changes and Mach3 planner stop boundaries
still needs a Windows integration run. I/O uses separate commands, not per-record
timestamps: the UDP ACK alone does not guarantee synchronization of M-codes
with motion. Correct program order requires correct handling of Mach3's stop/wait
boundaries and ExternalStill.

## 8. Local jog, homing, dwell, and stopping

JOG sets a target velocity; the firmware changes current velocity every 1 ms
subject to the acceleration limit. Multiple XYZ axes may jog simultaneously.
JOG_STOP brakes individual axes; ABORT/DISARM are immediate. Jog cannot start
during program execution or homing. The plugin's JogOn callback currently sends
continuous jog; MOVE is implemented in the protocol and shared controller,
but there is no separate incremental-jog SDK adapter branch.

The plugin queues home requests in X/Y/Z order; one axis homes at a time.
Firmware phases: 1 fast search; 2 back off the switch; 3 slow search;
4 final backoff. If the switch is already active, homing starts in phase 2.
The fast/slow edge stops the pulse PIO in the GPIO IRQ; partial step counts
are retained. At slow contact, position becomes HomeOffset and final backoff
is measured from there. The final position is therefore offset + backoff travel,
not necessarily zero. Homed becomes true only after the switch is inactive
and at least Backoff travel has completed. HomeTravel limits each phase.
Input debounce is not implemented.

The firmware counts DWELL using its local 1 ms timer; it emits no steps while
I/O, reports, and watchdog continue operating. The plugin converts the request
to milliseconds according to Mach3 DwellMilli.

Normal Feed Hold relies on Mach3's trajectory planner:
`m_ExternalFeedHold=false`. The plugin forwards deceleration records and the
buffer drains through the END/Stop boundary. Normal Feed Hold does not cause
HOLD, ABORT, or PurgeMovements. On Cycle Start, the plugin forwards Mach3's new
startup trajectory. The protocol's HOLD command is a separate destructive local
braking operation; it is not used for normal Mach3 Feed Hold. Stop/Reset purges;
there is no automatic trajectory replay. Feed Hold during G31 requests
PROBE_CANCEL: it brakes and cancels the measurement result.

Mode values: 0 IDLE, 1 JOGGING, 2 HOMING, 3 STREAM, 4 STOPPING,
5 DWELLING, 6 POSITIONING, 7 PROBING, 8 PROBE_BRAKING.
Fault values: 0 NONE, 1 ESTOP, 2 WATCHDOG, 3 LIMIT, 4 HOME_TRAVEL,
5 UNDERFLOW, 6 TIMING, 7 PROBE_MISS.
A fault clears pulse FIFO/PIO, sets armed=0, logical DO=0, PWM=0, and clears homed.
Physical E-stop and a moving axis's local limit also stop motion from the GPIO IRQ.
During homing, the axis's shared home/limit input is allowed for searching.
The plugin reports a newly detected fault as EStop; Reset is required to release it.
Firmware ARM clears the fault code if E-stop is already inactive.

A timer gap exceeding 1500 µs or incomplete PIO execution produces TIMING.
The 10 MHz pulse PIO high time is rounded to 100 ns; its period is
`(high_counter + low_counter + 7) × 100 ns`. The burst is scheduled within
the callback's first 900 µs, accounting for calculation/setup time and a staging
margin; the next record starts at 1 ms. The three PIO state machines start
synchronously. This distributes pulses within a time window; it is not a
perfectly uniform continuous DDS frequency generator. Deadlines and DIR hold
time must be measured on hardware. After an asynchronous stop, there is at least
20 µs of hold time before changing direction.

## 9. USB connection and console handover

The Mach3 profile now supports USB CDC and UDP. In Config Plugins select
**USB connection** and enter the Windows port name, for example `COM3` or
`COM12`. Leaving USB unchecked preserves the existing UDP IP/port settings.
Close the terminal application before connecting Mach3: the plugin opens the
COM port exclusively. The serial line is configured to 115200, 8N1, with DTR
asserted and no hardware/software flow control. CDC motion timing uses the
buffered controller, not the selected line baud rate.

The same CDC port starts as a configuration console. A complete, CRC-valid
protocol-v3 `SN_CLAIM` acquires the device and switches it to binary motion
communication. Partial console input is discarded. While either a USB or UDP
session owns the controller, console input and ordinary USB `printf` output
are disabled, including idle, E-stop and Feed Hold. Core0 alone handles console
commands and binary USB input; core1 keeps its Mach3 LED task.

USB wraps the unchanged SNC3 packets in SLIP: `C0` starts/ends a frame,
`C0` in data becomes `DB DC`, and `DB` becomes `DB DD`. Maximum packet size
is 512 bytes, maximum encoded frame size 1026 bytes. Header, version, profile,
length and CRC are checked before dispatch. Malformed escapes, overlong frames
and bad CRCs are rejected; decoding resynchronizes at a delimiter. USB uses a
transport-specific peer identity, so it cannot steal an active UDP session.
Existing sequence/ACK, retries, execution reports and watchdog apply to both.

Normal plugin cleanup sends `SN_DISARM`, then `SN_RELEASE`, receives its final
binary reply, and closes the port. The firmware clears motion and outputs before
handing back the console. USB disconnection/DTR loss immediately faults and
releases a USB owner; a hung or crashed host is also covered by the configured
watchdog (default 500 ms). Closing USB does not release a UDP owner. After release,
remaining input is discarded until 100 ms of receive silence, then the console
returns. Reconnection requires a new claim/configuration and starts in E-stop.

The plugin configuration dialog pauses updates and releases its connection,
then reconnects after the dialog closes. Its USB connection test temporarily
claims/releases the device without arming it; the UDP test remains a diagnostic
probe. Connection tests do not validate motion or physical I/O.

For a Pico without WIZnet hardware, build with
`-DSTEPPER_NINJA_MACH3_USB_ONLY=ON` as well as
`-DSTEPPER_NINJA_PROFILE=mach3-3axis`. This skips network hardware initialization
and polling and produces `stepper-ninja-pico-usb-mach3-3axis.uf2` for `BOARD=pico`.
The default Mach3 build retains both transports and expects its WIZnet hardware;
an Ethernet cable is not needed for USB. Both builds keep the fixed Mach3 pinout.
The USB-only option is rejected for LinuxCNC, which keeps its original USB console,
protocol, configuration and build default. For Mach3, CDC's 1200-baud reset shortcut
is disabled and binary output uses a bounded SDK write timeout.

Windows COM reads use the documented
[COMMTIMEOUTS first-byte/buffered-read behavior](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts).

## 10. Validation and remaining work

Linux tests check the packet/CRC/profile contract, controller state machine,
local UDP client retransmission and stale-report rejection, actual versus
accepted position, faults/watchdog, jog/homing/dwell/hold/move behavior,
the actual WIZ wrapper code with substitute registers/FIFOs, and PIO instruction
pulse counts/timing using a software model. Both Pico/W5500 firmware profiles
can be built separately.
Recorded local result (2026-10-14): 11/11 Mach3/USB/UDP/PIO/probe tests and
32/32 LinuxCNC HAL regression tests passed; Pico/W5500 LinuxCNC and Mach3
firmware builds succeeded, as did the Pico USB-only Mach3 build. This does not replace Windows SDK and hardware validation.

On Windows: Win32 MFC DLL build; plugin loading; XML/config; Reset/reconnect;
SDK units and jog/spindle direction; basic XYZ G-code and arc decomposition in
EX_VMS; program-line and stop/wait boundaries; dwell/M-code/macro order.
On hardware: pulse/low/DIR timing and timer load; step-counter direction;
homing edge and switch release; cable removal/E-stop/limits/output polarity.
For G31, verify contact parameters after Sync, including offset work coordinates.
For Feed Hold, verify actual Mach3 deceleration Stop/BufferEnding boundaries
and Cycle Start. Firmware jog soft limits, STEP inversion, and an incremental-jog
adapter are not implemented.

Sources: [official SDK download](https://www.machsupport.com/downloads-updates/mach3-downloads/),
[Art Fenerty's EX_VMS description](https://www.machsupport.com/forum/index.php?topic=8122.0),
and [Mach3 USB plugins](https://www.machsupport.com/downloads-updates/plugins/).
The SDK mapping also follows the downloaded SDK's `BlankMovement/MachDevice.cpp`,
`ncPod/ExternalMovement.cpp`, `MachIncludes/Engine.h`, and `MachIncludes/TrajectoryControl.h`.
Build commands: [driver README](../mach3-driver/README.md).
