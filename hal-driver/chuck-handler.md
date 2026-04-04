# chuck-handler

Author: Viola Zsolt  
License: MIT  
HAL component name: `chuck-handler`

## Status

This module is partially implemented.

- Direction validation and fault signaling are implemented.
- IO-ready daisy-chain gating is implemented.
- OD/ID limit and pressure checks are implemented and used for readiness.
- Foot pedal control is not implemented yet.
  - `inputs.open-close` has partial toggle behavior in code.
  - `inputs.close` and `inputs.open` are currently not used to drive outputs.
- `inputs.inhibit` is exported but currently not used in the `process()` logic.

## What The Module Currently Does

The `process()` function runs every servo cycle and executes this flow:

1. Read direction mode:
	- OD mode from `inputs.direction-od`
	- ID mode from `inputs.direction-id`
2. Validate direction selection.
	- Fault condition: both direction pins are equal (`OD=ID`, meaning both 0 or both 1).
	- If any movement input (`inputs.open-close`, `inputs.close`, `inputs.open`) is active during this invalid direction state, `outputs.fault-trigger` is asserted and an error is logged.
	- In invalid direction state, both motion outputs are forced low:
	  - `outputs.close = 0`
	  - `outputs.open = 0`
	- `outputs.io-ready` is forced low.
3. If direction is valid, compute pressure/limit readiness:
	- `id_pressure_ok = (!inputs.id-limit) && inputs.pressure-id && inputs.direction-id`
	- `od_pressure_ok = (!inputs.od-limit) && inputs.pressure-od && inputs.direction-od`
4. Set daisy-chain ready output:
	- `outputs.io-ready = inputs.io-ready && (id_pressure_ok || od_pressure_ok)`
5. If either `id_pressure_ok` or `od_pressure_ok` is true, set `outputs.ready = 1` and return.
6. Otherwise run current toggle behavior from `inputs.open-close` and drive:
	- `outputs.close` from toggle state
	- `outputs.open` as inverse of toggle state

## HAL Pins

All pins are prefixed with `chuck-handler.`

### Inputs

| Pin name | Type | Direction | Current usage |
|---|---|---|---|
| `inputs.inhibit` | `bit` | IN | Exported, currently unused by `process()`. |
| `inputs.io-ready` | `bit` | IN | Daisy-chain permission input. |
| `inputs.open-close` | `bit` | IN | Used for partial toggle behavior (not a complete pedal implementation). |
| `inputs.close` | `bit` | IN | Read only for fault detection (`any_switch`), not used for motion control yet. |
| `inputs.open` | `bit` | IN | Read only for fault detection (`any_switch`), not used for motion control yet. |
| `inputs.od-limit` | `bit` | IN | OD limit condition input (`active` means blocked). |
| `inputs.id-limit` | `bit` | IN | ID limit condition input (`active` means blocked). |
| `inputs.pressure-od` | `bit` | IN | OD pressure-OK input (`active` means pressure OK). |
| `inputs.pressure-id` | `bit` | IN | ID pressure-OK input (`active` means pressure OK). |
| `inputs.direction-od` | `bit` | IN | Select OD gripping direction/mode. |
| `inputs.direction-id` | `bit` | IN | Select ID gripping direction/mode. |

### Outputs

| Pin name | Type | Direction | Current usage |
|---|---|---|---|
| `outputs.io-ready` | `bit` | OUT | Daisy-chain output; low on invalid direction, otherwise gated by pressure/limit checks. |
| `outputs.close` | `bit` | OUT | Chuck close command (currently from toggle state when not in ready/pressure state). |
| `outputs.open` | `bit` | OUT | Chuck open command (inverse of close in toggle path). |
| `outputs.ready` | `bit` | OUT | Set high when direction is valid and OD/ID pressure+limit condition is satisfied. |
| `outputs.fault` | `bit` | OUT | High during invalid direction state. |
| `outputs.fault-trigger` | `bit` | OUT | Asserted only when invalid direction and a movement input is active. |

## Foot Pedal Implementation Note

Foot pedal support is not complete yet.

- Single-pedal toggle (`inputs.open-close`) has partial logic.
- Dual-pedal inputs (`inputs.close`, `inputs.open`) are declared but not implemented as motion commands.
- The current behavior should be treated as work-in-progress until dedicated open/close pedal logic and inhibit handling are fully implemented.

## Minimal HAL Wiring Example (Current State)

```hal
loadrt chuck-handler
addf chuck-handler.process servo-thread

# Direction mode select
net chuck-dir-od => chuck-handler.inputs.direction-od
net chuck-dir-id => chuck-handler.inputs.direction-id

# Pressure and limits
net chuck-od-limit    => chuck-handler.inputs.od-limit
net chuck-id-limit    => chuck-handler.inputs.id-limit
net chuck-pressure-od => chuck-handler.inputs.pressure-od
net chuck-pressure-id => chuck-handler.inputs.pressure-id

# Optional movement inputs (logic still incomplete)
net chuck-toggle => chuck-handler.inputs.open-close
net chuck-close  => chuck-handler.inputs.close
net chuck-open   => chuck-handler.inputs.open

# Outputs
net chuck-close-valve chuck-handler.outputs.close =>
net chuck-open-valve  chuck-handler.outputs.open  =>
net chuck-fault       chuck-handler.outputs.fault-trigger =>

# Daisy chain
net io-ready-in  => chuck-handler.inputs.io-ready
net io-ready-out chuck-handler.outputs.io-ready =>
```

## Build And Install

The component is built as part of the hal-driver CMake project. Use `install.sh` for installation in the target system setup.
