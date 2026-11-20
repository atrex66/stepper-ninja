# Configurable I²C DAC outputs — prototype

Available on the [test branch](https://github.com/atrex66/stepper-ninja/tree/test).
This custom LinuxCNC feature adds analog voltage-output commands through I²C.
The existing fixed breakout-board DAC implementation is unchanged. Mach3 is not
supported by this prototype. **Physical DAC/output testing remains pending.**

## Supported parts and inexpensive candidates

| Manufacturer | Part / configuration type | Bits | Outputs per IC | 7-bit address |
|---|---|---:|---:|---|
| Microchip | `SN_DAC_MCP4725` | 12 | 1 | 0x60–0x67, actual factory option/A0 dependent |
| Texas Instruments | `SN_DAC_DAC5571` | 8 | 1 | 0x4c/0x4d |
| Texas Instruments | `SN_DAC_DAC6571` | 10 | 1 | 0x4c/0x4d |
| Texas Instruments | `SN_DAC_DAC7571` | 12 | 1 | 0x4c/0x4d |
| Analog Devices | `SN_DAC_AD5337` | 8 | 2 | 0x0c/0x0d |
| Analog Devices | `SN_DAC_AD5338` | 10 | 2 | 0x0c/0x0d; AD5338-1: 0x22/0x23 |
| Analog Devices | `SN_DAC_AD5339` | 12 | 2 | 0x0c/0x0d |

Cost references checked on 2026-11-20 (USD; quantities and IC/module differ):

- [Adafruit MCP4725 module](https://www.adafruit.com/category/42): listed at $4.95 for product 935. A complete breakout is convenient for initial tests. Microchip now marks [MCP4725](https://www.microchip.com/en-us/product/mcp4725) as not recommended for new designs and recommends MCP4726; **MCP4726 is not implemented here**.
- [TI DAC7571 at LCSC](https://www.lcsc.com/product-detail/C118112.html): listed at $3.3691 per IC for 1+, $1.8569 for 1000+. Board assembly is additional.
- [ADI AD5337](https://www.analog.com/en/products/ad5337.html): manufacturer 1000-piece list price starts at $2.31 per dual-channel IC. [AD5338](https://www.analog.com/en/products/ad5338.html): $3.24; [AD5339](https://www.analog.com/en/products/ad5339.html): $6.67 on the same 1000-piece list-price basis. These are not single-piece retail quotes.
- NXP [PCF8591](https://www.nxp.com/products/PCF8591) appears on inexpensive ADC/DAC modules, but NXP marks it End of Life. It is **not implemented** in this prototype. Its presence in a comparison does not imply ADC support.

Prices are reference listings, not a delivered-cost comparison or a stock
commitment. For an existing MCP4725 module use that driver; for a small custom
board consider the TI family or a dual AD5337/38 when 8/10 bits are sufficient.

## Shared bus and channel configuration

See the [complete mixed example](../configurations/I2C-DAC/config.h). It enables
four DAC channels on three ICs, two digital expanders, and native GPIO on one
bus. Motors/encoders and the keyboard matrix are disabled in this example.
GPIO0/1 are illustrative free I²C pins, not an overlay for arbitrary boards.

Place the settings before `#include "footer.h"`:

```c
#define I2C_IO_BUS_COUNT 1
#define I2C_IO_BUSES {{0, GP00, GP01, 400000, 1000}}
#define I2C_DAC_ENABLE 1
#define I2C_DAC_CHANNEL_COUNT 4
#define I2C_DAC_CHANNELS { \
    {SN_DAC_MCP4725, 0, 0x60, 0, 0, 10}, \
    {SN_DAC_DAC7571, 0, 0x4c, 0, 0, 10}, \
    {SN_DAC_AD5339, 0, 0x0c, 0, 0, 10}, \
    {SN_DAC_AD5339, 0, 0x0c, 1, 1024, 10} \
}
```

The bus list retains its existing `I2C_IO_*` names for compatibility and is
shared by both features. `I2C_IO_ENABLE` only enables digital expanders; it may
remain zero for DAC-only operation. Bus fields are controller (0/1), SDA, SCL,
frequency in Hz (up to 400000), timeout per SDK transfer in µs (1–10000).

DAC descriptor fields, in order:

| Field | Meaning |
|---|---|
| `type` | One of the supported `SN_DAC_*` values |
| `bus` | Index in `I2C_IO_BUSES`, starting at zero |
| `address` | Individual device's 7-bit I²C address, without the R/W bit |
| `channel` | `0` for single-output ICs; `0` = A, `1` = B for AD533x |
| `safe_code` | Native unsigned code on startup, HAL disable/invalid input and host timeout |
| `poll_ms` | Minimum refresh period, 1–60000 ms |

There are 1–4 **channel descriptors**, not necessarily four ICs. For both
outputs of one AD533x use two entries with the same type/bus/address and
channels 0/1. Duplicate channels, inconsistent types on one address, collision
with a digital expander and unsupported address ranges are rejected. An
unconfigured sibling AD533x output is not commanded; do not depend on its state.

The footer defaults to `I2C_DAC_ENABLE=0`, `I2C_DAC_CHANNEL_COUNT=0` and
`I2C_DAC_CHANNELS={}`. Existing configurations need no changes. When enabled,
`ANALOG_CH` is derived from the channel count unless explicitly specified; an
explicit value must match. Fixed breakout boards continue using their existing
`ANALOG_CH`/MCP4725 path, including their existing wire packing.

DAC-only operation does not reserve native digital output word 1. With digital
expanders enabled, their existing limit of 32 native digital outputs applies.
The bus is initialized once, validated against configured GPIO ownership, and
owned by core 1 for both kinds of devices. Use a suitable 3.3 V bus or proper
level translation; a DAC's 5 V supply does not make Pico GPIO 5 V tolerant.

## LinuxCNC HAL

Each descriptor exports these pins, for example channel zero:

| Pin | Direction | Meaning / initial value |
|---|---|---|
| `stepgen-ninja.0.dac.0.enable` | HAL_IN | Enable command scaling, initially false |
| `stepgen-ninja.0.dac.0.value` | HAL_IN float | Requested value, initially zero |
| `stepgen-ninja.0.dac.0.scale` | HAL_IN float | Positive full-scale command, initially 1.0 |
| `stepgen-ninja.0.dac.0.raw-command` | HAL_OUT u32 | Quantized native command; not a voltage measurement |
| `stepgen-ninja.0.dac.0.online` | HAL_OUT | Most recent complete write cycle acknowledged |
| `stepgen-ninja.0.dac.0.valid` | HAL_OUT | Same write-cycle validity in this prototype |

Enabled, finite commands are clipped to `0..scale`, mapped to
`0..(2^bits-1)`, and rounded to the nearest native code. Disabled channels,
non-finite values/scales and non-positive scales produce `safe_code`.
For normalized commands leave `scale=1`; for a 0..10 engineering-unit command
set `scale=10`. This does **not** create a physical 0–10 V output. MCP/TI use
supply-referenced outputs; AD533x needs its reference pin wired appropriately.
Any 0–10 V or bipolar output conditioning is separate hardware. Resolution does
not guarantee voltage accuracy or full rail swing under load.

## Runtime, packets and limits

The shared core-1 polling code performs only bounded-time volatile writes.
It first establishes the configured startup code, then applies host commands.
After a failed/short transfer, validity is cleared and the next scheduled cycle
retries from the startup code. Other devices continue to be serviced. A host
timeout forces a refresh of all configured disconnected levels without waiting
for their normal period. Bus failure can prevent that write: delivery is not
guaranteed, and a chip may hold its previous output. Nonzero `safe_code` is
intentional if configured (the example's fourth channel uses 1024).

Writes never change MCP4725 EEPROM. Its stored power-up value may therefore
appear before the firmware's first successful write. AD533x writes select only
the target channel, use normal operation and immediate update, and never issue
a global clear. Paired outputs are refreshed sequentially; simultaneous update
is not provided by this implementation. No ADC, readback measurement, external
LDAC/reset control, bipolar coding or EEPROM persistence is implemented.

Packet handling only exchanges short locked snapshots. With this custom DAC
profile, `analog_out[n]` carries one native raw code per descriptor; legacy
breakout packing remains separate. The existing configuration-dependent
`ANALOG_CH` field adds four request bytes per configured channel. **Rebuild
firmware and HAL with identical configurations**; this feature does not negotiate
layouts with old binaries. Reply size is unchanged: `inputs[3]` bits 16–19 are
DAC online flags and bits 24–27 valid flags. Digital expander bits stay at
0/1 and 8/9. Host watchdog expiration clears the corresponding HAL status pins.
A successful ACK does not prove the actual output voltage.

## Verification

The LinuxCNC CTest suite includes portable encoding, shared polling and HAL
contracts. Direct checks:

```sh
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/inc common/tests/i2c_dac_contract.c -o /tmp/i2c-dac-contract
/tmp/i2c-dac-contract
python3 common/tests/i2c_dac_runtime.py cc
python3 common/tests/i2c_dac_hal_contract.py --include /usr/include/linuxcnc
```

Tests cover all seven encodings, AD533x channel selection/control bits,
clipping/inversion-independent DAC scaling, invalid values, bus/address
conflicts, DAC-only/mixed operation, startup/disconnect levels, NAK/short-write
recovery, status mapping and HAL allocation failures. These are source/build
checks, not electrical or physical-machine validation.

Primary protocol references:
[Microchip MCP4725](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP4725-Data-Sheet-20002039E.pdf),
[TI DAC5571](https://www.ti.com/lit/ds/symlink/dac5571.pdf),
[TI DAC6571](https://www.ti.com/lit/ds/symlink/dac6571.pdf),
[TI DAC7571](https://www.ti.com/lit/ds/symlink/dac7571.pdf),
[ADI AD5337/5338/5339](https://www.analog.com/media/en/technical-documentation/data-sheets/AD5337_5338_5339.pdf).
