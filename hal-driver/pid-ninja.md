# pid-ninja — Realtime PID Controller HAL Component

A professional, multi-instance PID controller written as a LinuxCNC HAL realtime component.
Designed to be loaded into a fast servo thread and supports up to **8 independent instances**
in a single `loadrt` call.

---

## Table of Contents

1. [Overview](#overview)
2. [Theory of Operation](#theory-of-operation)
3. [HAL Pins](#hal-pins)
4. [HAL Parameters](#hal-parameters)
5. [Loading the Module](#loading-the-module)
6. [Wiring Example](#wiring-example)
7. [Tuning Guide](#tuning-guide)
8. [Anti-Windup](#anti-windup)
9. [Derivative Filter](#derivative-filter)
10. [Feed-Forward Terms](#feed-forward-terms)
11. [Enable / Disable Behaviour](#enable--disable-behaviour)
12. [Limits and Clamps](#limits-and-clamps)

---

## Overview

`pid-ninja` implements a full **PID + Feed-Forward** control loop suitable for closed-loop
axis control, spindle speed, temperature regulation, or any process where a measured value
must track a commanded setpoint.

Each instance runs one control loop per servo thread cycle:

```
output = Pgain·e  +  Igain·∫e·dt  +  Dgain·(de/dt)  +  FF0·cmd  +  FF1·ċmd  +  FF2·c̈md  +  bias
```

where `e = command − feedback`.

---

## Theory of Operation

### Proportional term (P)

Produces an output proportional to the current error.  
A higher `Pgain` gives faster response but risks oscillation.

$$P = K_p \cdot e(t)$$

### Integral term (I)

Accumulates error over time to eliminate steady-state offset.  
`Igain` scales the accumulated integral.

$$I = K_i \cdot \int_0^t e(\tau)\,d\tau$$

The integral is updated each cycle:

```
integral += e × dt
```

### Derivative term (D)

Reacts to the *rate of change* of error, providing damping.  
`Dgain` scales the derivative.

$$D = K_d \cdot \frac{de}{dt}$$

An optional IIR low-pass filter (`deriv-filter-weight`) smooths noisy derivative estimates.

### Feed-Forward terms

Feed-forward bypasses the feedback loop and directly maps the command trajectory to output,
reducing the tracking error without increasing loop gain:

| Term | Formula | Use |
|------|---------|-----|
| FF0  | $K_{ff0} \cdot cmd$ | Position / DC offset |
| FF1  | $K_{ff1} \cdot \dot{cmd}$ | Velocity feed-forward |
| FF2  | $K_{ff2} \cdot \ddot{cmd}$ | Acceleration feed-forward |

Velocity and acceleration are estimated numerically from successive command samples.

---

## HAL Pins

All pins are prefixed with `pid-ninja.<N>.` where `<N>` is the zero-based instance index.

### Input Pins

| Pin | Type | Description |
|-----|------|-------------|
| `command` | `float` | Setpoint — the desired value the process should track |
| `feedback` | `float` | Process variable — the measured actual value |
| `enable` | `bit` | `1` = controller active; `0` = outputs zeroed and state reset |

### Output Pins

| Pin | Type | Description |
|-----|------|-------------|
| `output` | `float` | Control output sent to the actuator / drive |
| `error` | `float` | Raw error: `command − feedback` (pre-deadband, for monitoring) |
| `error-I` | `float` | Current value of the integral accumulator |
| `error-D` | `float` | Current filtered derivative of the error |
| `saturated` | `bit` | `1` when `output` has been clamped by `max-output` |

---

## HAL Parameters

All parameters are prefixed with `pid-ninja.<N>.` and are read/write (`setp`).  
A value of `0.0` disables any limit parameter.

| Parameter | Default | Description |
|-----------|---------|-------------|
| `Pgain` | `1.0` | Proportional gain |
| `Igain` | `0.0` | Integral gain |
| `Dgain` | `0.0` | Derivative gain |
| `FF0` | `0.0` | 0th-order (position) feed-forward gain |
| `FF1` | `0.0` | 1st-order (velocity) feed-forward gain |
| `FF2` | `0.0` | 2nd-order (acceleration) feed-forward gain |
| `bias` | `0.0` | Constant offset added to output (e.g. gravity compensation) |
| `max-output` | `0.0` | Symmetric output clamp ±value; `0` = unlimited |
| `max-error` | `0.0` | Symmetric error clamp ±value applied before P/I/D; `0` = unlimited |
| `max-error-I` | `0.0` | Integrator anti-windup clamp ±value; `0` = unlimited |
| `max-error-D` | `0.0` | Derivative term clamp ±value; `0` = unlimited |
| `deadband` | `0.0` | Error deadband: errors smaller than this value are treated as zero |
| `deriv-filter-weight` | `0.0` | IIR low-pass weight for D term: `0` = unfiltered, `0.9` = heavy smoothing |

---

## Loading the Module

```bash
# Single instance (default)
loadrt pid-ninja

# Three instances — for X, Y, Z axes
loadrt pid-ninja instances=3
```

Add each instance's process function to a realtime thread:

```bash
addf pid-ninja.0.process  servo-thread
addf pid-ninja.1.process  servo-thread
addf pid-ninja.2.process  servo-thread
```

> **Thread placement**: always add `pid-ninja.<N>.process` to the same thread that reads
> encoder feedback and writes axis output, so command, feedback, and output are updated
> atomically within one servo period.

---

## Wiring Example

Complete X-axis closed-loop example connecting a stepper-ninja encoder back to pid-ninja:

```bash
loadrt pid-ninja instances=1

addf pid-ninja.0.process  servo-thread

# Command from LinuxCNC motion controller
net x-pos-cmd  motion.axis.0.motor-pos-cmd  => pid-ninja.0.command

# Feedback from encoder
net x-pos-fb   stepper-ninja.0.enc-position  => pid-ninja.0.feedback

# Output to step generator
net x-output   pid-ninja.0.output  => stepper-ninja.0.command

# Enable from machine on signal
net machine-on  halui.machine.is-on  => pid-ninja.0.enable

# Tuning
setp pid-ninja.0.Pgain        100.0
setp pid-ninja.0.Igain          5.0
setp pid-ninja.0.Dgain          2.0
setp pid-ninja.0.FF1            1.0
setp pid-ninja.0.max-output    10.0
setp pid-ninja.0.max-error-I    2.0
setp pid-ninja.0.deriv-filter-weight  0.5
```

---

## Tuning Guide

Start with all gains at zero and increase one at a time.

### Step 1 — Proportional only

```
setp pid-ninja.0.Pgain  <X>
setp pid-ninja.0.Igain   0.0
setp pid-ninja.0.Dgain   0.0
```

Increase `Pgain` until the axis tracks well but just before it starts to oscillate.
Reduce by ~20 % for a stability margin.

### Step 2 — Add Derivative damping

```
setp pid-ninja.0.Dgain  <X>
```

Add `Dgain` to suppress oscillation caused by P.  
Enable `deriv-filter-weight ≈ 0.3–0.7` if the feedback signal is noisy to avoid D-term amplification of noise.

### Step 3 — Eliminate steady-state error with Integral

```
setp pid-ninja.0.Igain  <X>
```

Small values (relative to Pgain) are sufficient. Always set `max-error-I` to prevent windup
during large moves.

### Step 4 — Feed-Forward for trajectory tracking

For position-controlled axes set `FF1 = 1.0` to make the output proportional to commanded
velocity, dramatically reducing following error during constant-velocity moves.

---

## Anti-Windup

When the output saturates (hits `max-output`), the integrator would otherwise continue
accumulating, causing **integrator windup** — a large overshoot when the actuator
eventually comes back into range.

`pid-ninja` uses **back-calculation anti-windup**:

```
if saturated:
    integral -= e × dt   # undo the integration that just happened
```

This keeps the integral at the value it had when saturation first occurred, preventing
runaway accumulation.  Set `max-error-I` as an additional hard clamp on the integral itself
for extra protection (e.g. during homing or E-stop scenarios).

---

## Derivative Filter

Raw numerical differentiation amplifies high-frequency noise in the feedback signal.
The optional IIR (Infinite Impulse Response) low-pass filter smooths the derivative:

$$y[n] = w \cdot y[n-1] + (1 - w) \cdot x[n]$$

where `w` is `deriv-filter-weight` ∈ [0, 1).

| `deriv-filter-weight` | Effect |
|---|---|
| `0.0` (default) | No filtering — raw derivative |
| `0.3` | Light smoothing |
| `0.7` | Heavy smoothing — recommended for encoders with noise |
| `0.95` | Very heavy — trades noise rejection for phase lag |

> **Rule of thumb**: use the lightest filter that makes the D term usable.
> Heavier filtering introduces phase lag that can destabilise the loop at high bandwidths.

---

## Feed-Forward Terms

Feed-forward decouples tracking performance from disturbance rejection:

| Term | When to use |
|------|-------------|
| `FF0` | Rarely needed; compensates a DC load proportional to position |
| `FF1` | The most important — set to `1.0` to cancel velocity-proportional following error |
| `FF2` | Compensates inertia during acceleration; tune last |

FF velocity and acceleration are derived from the `command` pin over successive cycles, so
they require no additional input wiring.

---

## Enable / Disable Behaviour

When `enable` transitions to `0`:

- `output` is forced to `0.0`
- The integral accumulator is cleared to `0`
- The derivative filter state is cleared
- Previous command history is reset to the current command value

This ensures a **bumpless re-enable**: when `enable` returns to `1`, the controller starts
from a clean state without a derivative spike or integration kick.

On the **first active cycle** after enable, the derivative and feed-forward velocity/acceleration
terms produce zero output to avoid a large transient from undefined history.
