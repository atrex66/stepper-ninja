# Mach3 Feed Hold and G31: operation, examples, and validation

[Magyar](MACH3-FEEDHOLD-G31.hu.md)

Development implementation, 2026-10-11. The portable driver and Pico firmware
can be built and tested on Linux. Compilation of the Win32 MFC SDK adapter,
loading it in Mach3, and physical machine operation have not been validated.
The motion protocol version is **3**: build the driver and firmware together.

## Which examples informed the implementation?

- [Warp9 / SmoothStepper vendor FAQ, Feed Hold](https://warp9td.com/index.php/faq/faq-mach3):
  distinguishes stopping planned by Mach3 from stopping performed by the device.
  In the native mode, Mach3 sends a deceleration trajectory and the device drains
  its buffer; on resume, Mach3 creates a startup trajectory. StepperNinja uses
  this approach.
- [Official Mach3 SDK](https://www.machsupport.com/downloads-updates/mach3-downloads/):
  `g100IO/MachDevImplementation.cpp` shows the Notify/EX_FEEDHOLD/EX_RUN
  integration, but the corresponding GRex FeedHold/Run methods are empty stubs;
  this is not a working motion-control example on its own.
  `ncPod/ncPod.cpp` and `ncPod/ExternalMovement.cpp` show the DoExternalProbe,
  DoProbe, and Axis[0].Probing/Sync interfaces. The coordinate calculation follows
  the CalcPoint formula in `Probing/MenuHandler.cpp`:
  `DepthCount / StepsPerAxis - CurrentOrigins`.
- [Dynomotion probing documentation](https://dynomotion.com/Help/Russian/Mach3Plugin/Mach3Probe.html)
  and the [NotifyProbeMach3.c source example](https://github.com/parhansson/KMotionX/blob/master/C%20Programs/NotifyProbeMach3.c):
  capture the contact position, then stop coordinated motion.
  The C file is an external mirror of a vendor example; it uses KFLOP variables
  and motion handling and cannot be copied directly into a Pico driver.

## Feed Hold: why do we not resend the motion?

The plugin sets `m_ExternalFeedHold=false`. During normal program execution,
EX_VMS records contain 1 ms XYZ step increments. We continue forwarding them
while Feed Hold is active, so Mach3's planned deceleration reaches the firmware.
At a `GMoves.Stop` / `BufferEnding` boundary, we send END. The firmware reports
complete standstill only after both the buffer and the physical step record
have finished. The driver does not purge the trajectory because of Feed Hold.
The target buffer fill is 100 records, approximately 100 ms of motion already
sent ahead; Mach3's deceleration time and communication latency add to this.

On Cycle Start, Mach3 generates the next startup trajectory. We do not rewind
`Engine->TrajIndex` or resend old records. TrajIndex advances only after a
successful BATCH ACK; separate accepted/executed counters and a program-line
report describe actual execution. If a UDP ACK is lost, the same packet is
resent with the same sequence number; the firmware recognizes the duplicate
and does not execute it again. This network retransmission is separate from
restarting an interrupted trajectory.

Immediate device-side Feed Hold could also be implemented. It would require
preserving the remainder of the partially executed record, the remaining
trajectory, and the velocity state, followed by coordinated acceleration back
into the path. Simply pushing an interrupted record back into the buffer can
cause excess motion. The existing separate HOLD protocol command purges the
buffer and brakes each axis independently; the SDK adapter does not use it
for normal Feed Hold.

Stop/Reset interrupts and purges. The old SDK stop notification is `Notify(-1)`;
EX_ESTOP also requests a stop. Resuming after normal Feed Hold does not imply
automatic program recovery after Stop or a network failure.

## G31: contact position and stopping position

1. Enable the Mach3 probe signal on port 1, pin 5, corresponding to DI5 / GPIO26.
   Active Low must match the plugin's input inversion. Motion will not start
   with the sensor already active. The adapter rejects other probe DI mappings.
2. The callback sends Mach3's absolute machine XYZ probe target in steps.
   Feed comes from `_setup->feed_rate`, in machine units per minute. The current
   adapter supports G94; G93/G95 and A/B/C probing motion are rejected.
   Dominant-axis step velocity is calculated from the physical XYZ path length,
   including differing axis scales. The firmware moves XYZ using a common scalar
   profile, respecting the velocity and acceleration limits of every moving axis.
3. Contact stops the pulse generator in the GPIO IRQ, reads physical pulses not
   yet accounted for, and captures the contact XYZ position. Subsequent timer
   cycles perform controlled braking in the probing direction. No further motion
   is planned beyond the original target. Captured contact counts and the later
   stopping position are separate fields; braking does not overwrite contact.
4. Every report contains the PROBE sequence identifier and state:
   NONE / ACTIVE / HIT / MISS / CANCELLED. The SDK adapter checks the identifier
   and accepts only a completed HIT as a measurement. Work coordinates are:
   `latched_steps / StepsPerAxis - CurrentOrigins`.
5. The adapter fills DepthCount, ProbeX/Y/Z, and parameters 2000..2002, then
   releases the Axis[0].Probing wait and requests Sync. Reaching the target
   without contact stops motion and disables outputs with a PROBE_MISS fault.
   E-stop, limits, and watchdog also interrupt probing. Feed Hold during G31
   requests controlled PROBE_CANCEL braking; this produces no successful
   measurement, and the adapter aborts G31. Cycle Start does not resume that
   probing move.

Physical input IRQ and counter-read latency must be measured; this is not a
zero-latency hardware capture. There may be a short gap between stopping the
pulse PIO and the next 1 ms braking cycle. Input debounce is not implemented.

## What do the tests verify?

`common/tests/controller_udp_contract.cpp` runs with the actual UDP client and
firmware state machine. A replacement trajectory planner supplies normal records,
deceleration records, and a Stop boundary, followed by a pause and a Cycle Start
startup trajectory. The test checks the final position, program line,
accepted/executed counters, and that normal Feed Hold sends no HOLD/ABORT.
A lost ACK does not duplicate motion. The same test transfers G31 reports with
separate contact and stopping positions.

`controller_probe_contract.cpp` checks coordinated probing, accounting for
partial physical pulses exactly once, idempotent repeated PROBE requests,
the endpoint without contact, rejection of an already active sensor,
cancellation, and work-coordinate conversion.

## Windows and hardware validation sequence

- Compile/load the Win32 MFC DLL; verify Config Plugins and saved XML settings.
- Feed Hold / Cycle Start on a real Mach3 trajectory: check that Mach3 supplies
  Stop/BufferEnding boundaries, the buffer does not underflow, and the endpoint
  is not executed twice. Repeat with short paths, arcs, line boundaries, and
  several consecutive holds.
- After G31, check that `GetVar(2000..2002)` refers to contact and is not replaced
  by the braking endpoint after Sync. Repeat with G54/G92 and tool offsets.
  This has not yet been verified with the actual Mach3 interpreter.
- Test inputs without motion, then run low-speed trials with sufficient
  mechanical clearance: measure IRQ latency, contact position, and stopping
  distance.
- Test an already active sensor, missed contact, Feed Hold, limits, E-stop,
  and cable failure.

The default LinuxCNC profile and its configurability remain unchanged:
the Mach3 state machine and probing implementation are included in the firmware
only for a `mach3-3axis` build. USB CDC and UDP use the same buffered motion protocol;
Windows and physical USB validation remain pending.
