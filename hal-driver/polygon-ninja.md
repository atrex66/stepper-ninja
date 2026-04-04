# polygon-ninja

`polygon-ninja` is a LinuxCNC realtime HAL component for encoder-synchronous polygon turning.
It builds a lookup table from spindle angle to polygon radius, then outputs an external offset in
counts for `motion.eoffset-counts`.

## Current behavior

- Single-instance HAL component.
- Lookup-table based runtime path with no trig in the normal servo cycle.
- Supports regular polygons from 3 to 64 sides.
- Supports X-direction excenter shift with `excenter-x`.
- Supports three output modes:
    - `0` = absolute radius
    - `1` = radius from polygon minimum
    - `2` = radius centered around mean radius
- Rebuilds the LUT when `polygon-sides`, `excenter-x`, or `circumscribed-radius` changes.

## Module parameter

- `encoder_scale` (`int`, default `4096`)
    - LUT size and encoder wrap size.
    - Must be a power of 2.

## HAL pins

All pins are prefixed with `polygon-ninja.`

### Inputs

| Pin | Type | Meaning |
|---|---|---|
| `encoder-count` | `s32` | Raw spindle encoder count |
| `enable` | `bit` | Enables polygon output |
| `phase-offset` | `s32` | Angular offset added before LUT lookup |
| `circumscribed-radius` | `float` | Polygon circumscribed radius in mm |
| `excenter-x` | `float` | X-axis center shift in mm |
| `polygon-sides` | `s32` | Polygon side count, clamped to `3..64` |
| `mode` | `s32` | Output mode: `0`, `1`, or `2` |

### Outputs

| Pin | Type | Meaning |
|---|---|---|
| `eoffset-counts` | `s32` | External offset output in counts, scaled as `mm * 1000` |
| `enable-out` | `bit` | Goes high after the first real offset change after enable |
| `clear-out` | `bit` | High while disabled |
| `index-enable` | `bit` | High while enabled |
| `debug-r-abs-mm` | `float` | Instantaneous absolute radius |
| `debug-r-min-mm` | `float` | Minimum radius for current shape |
| `debug-r-max-mm` | `float` | Maximum radius for current shape |
| `debug-x-target-mm` | `float` | Final selected radius/offset before count scaling |

## Runtime flow

Per servo cycle the component does this:

1. Clamp side count to `3..64`.
2. Rebuild the LUT if shape parameters changed.
3. If disabled, clear outputs and reset the eoffset handshake.
4. Compute LUT index with bitmask wrapping:

```c
idx = (encoder_count + phase_offset) & (encoder_scale - 1);
```

5. Read normalized radius from the LUT and scale it by `circumscribed-radius`.
6. Convert that radius to the selected mode.
7. Export `eoffset-counts = lroundf(x_target_mm * 1000.0f)`.

## Enable handshake

The current implementation does not assert `enable-out` immediately when `enable` goes high.
Instead it:

1. Arms the current `eoffset-counts` value on the enable edge.
2. Waits until the computed offset changes from that armed value.
3. Sets `enable-out = 1` only after motion has effectively started.

This matches the current code and avoids enabling the external offset before the polygon output
has actually moved away from its armed position.

## LUT generation summary

The LUT is rebuilt by ray-casting against a unit polygon:

- polygon vertices are placed on the unit circle
- each LUT index represents one spindle angle
- the nearest edge intersection gives the normalized radius
- `excenter-x` is applied in normalized space before storing the final radius

Only `excenter-x` is implemented in the current code. There is no `excenter-y` input.

## HAL example

```hal
loadrt polygon-ninja encoder_scale=4096
addf polygon-ninja.process servo-thread

net spindle-count encoder.0.rawcounts => polygon-ninja.encoder-count
net poly-eofs     polygon-ninja.eoffset-counts => motion.eoffset-counts

setp motion.external-offset-enable 1
setp polygon-ninja.polygon-sides 6
setp polygon-ninja.circumscribed-radius 20.0
setp polygon-ninja.excenter-x 0.0
setp polygon-ninja.mode 1
```

## Notes

- The component currently exports counts, not a float X offset.
- The LUT is allocated from HAL shared memory during module init.
- Default startup shape is a 4-sided polygon with `circumscribed-radius = 20.0`.
- `index-enable` is exported as `HAL_IO` in code, even though runtime behavior is output-like.
