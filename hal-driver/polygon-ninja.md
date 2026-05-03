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
| `enable-out` | `bit` | Goes high after `index-enable` has been cleared by the connected encoder/reset logic |
| `clear-out` | `bit` | High while disabled |
| `index-enable` | `bit` | Asserted on enable request, then expected to be cleared by the connected encoder/reset logic |
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

The component now performs a startup index handshake before it enables output motion:

1. On the rising edge of `enable`, it sets `index-enable = 1`.
2. While `index-enable` remains high, `enable-out = 0` and `eoffset-counts = 0`.
3. Another connected component is expected to clear `index-enable` after handling the index reset / alignment.
4. Once `index-enable` is observed low, `polygon-ninja` sets `enable-out = 1` and starts outputting the polygon offset.

This makes startup deterministic and lets the polygon offset begin only after the spindle index handshake is complete.

## LUT generation summary

The LUT generation is now split into a reusable geometry pipeline:

1. Polygon memory is populated with vertex coordinate arrays.
2. Regular polygons are currently generated into that memory on the unit circle.
3. The polygon memory is stored as an explicitly closed loop: the last point repeats the first point.
4. The builder validates that the polygon is closed and that no edge is degenerate.
5. Each LUT index represents one spindle angle.
6. A ray is cast from the origin for that angle.
7. A dedicated line/segment intersection helper finds the nearest edge hit.
8. `excenter-x` is applied in normalized space before storing the final radius.

This structure is more reusable than the old inline generator because future code can populate the
polygon memory from arbitrary coordinates, then reuse the same LUT builder unchanged.

If polygon memory is not closed, or contains zero-length edges, the component prints a runtime
error message and suppresses output until valid polygon memory is rebuilt.

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
- `index-enable` is exported as `HAL_IO` so another HAL component can clear it to acknowledge the startup handshake.
