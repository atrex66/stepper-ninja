# Configurable I2C digital I/O — prototype

This optional LinuxCNC feature adds ordinary digital inputs and outputs to a
custom configuration (`breakout_board=0`). The existing fixed breakout-board
implementations remain independent. Mach3 is not supported by this prototype.
No step, encoder or PWM signals are generated through these expanders.

## Compatibility and limits

`footer.h` supplies disabled/empty defaults using `#ifndef`. Existing configs do
not need an I2C section. When both I²C I/O and DAC features are disabled, builds initialize no new bus, reserve no pins,
export no expander HAL pins, and keep the existing packet layout. Legacy
`io_expanders` retains its existing meaning and is not used by the new module.

The prototype supports 1–2 hardware I2C buses and 1–2 devices in total. A bus
controller may occur only once; devices with distinct addresses can share it.
Each device reserves a 16-bit packet slot, including an 8-bit MCP23008.
At most 32 native digital outputs can coexist with this feature. Inputs keep
both native GPIO banks, including RP2350B high GPIOs. Enabling the feature with
a fixed breakout board, Mach3, inconsistent counts or overlapping native output
word capacity is a build error. Device descriptors and GPIO ownership are also
validated before a bus is initialized. A GPIO conflict leaves all new buses
uninitialized and all expander status bits false, with a serial diagnostic.

## Configuration

See the complete [example](../configurations/I2C-IO/config.h). It intentionally
has no motors or encoders enabled, two native inputs (GPIO22/23), one native
output (GPIO24), and no keyboard matrix. It is a starting point for a custom
board, not an overlay to copy blindly onto an existing board's occupied pins.
Use the same config when rebuilding firmware and the LinuxCNC driver.

Put the following before `#include "footer.h"`:

```c
#define I2C_IO_ENABLE 1
#define I2C_IO_BUS_COUNT 2
#define I2C_IO_BUSES { \
    {0, GP00, GP01, 400000, 1000}, \
    {1, GP26, GP27, 400000, 1000} \
}
#define I2C_IO_DEVICE_COUNT 2
#define I2C_IO_DEVICES { \
    {SN_IO_MCP23008, 0, 0x20, 0x000f, 0x000f, 0x0001, 0x0000, 0x0000, 10}, \
    {SN_IO_TCA9555, 1, 0x20, 0x00ff, 0x0000, 0x0000, 0x0100, 0x0200, 10} \
}
```

Bus fields: controller number (0/1), SDA GPIO, SCL GPIO, requested frequency in
Hz (up to 400000), timeout in microseconds **per SDK transfer** (1–10000).
Use valid hardware I2C pins, available on the selected MCU, free of native I/O,
STEP/DIR, encoder/index, PWM, Pico keyboard, LED and transport pin assignments.
The SDK requests internal SDA/SCL pull-ups; use suitable external bus pull-ups
for the actual hardware. The example assumes 3.3 V logic on the Pico bus.

Device fields, in order:

| Field | Meaning |
|---|---|
| `type` | `SN_IO_MCP23008`, `SN_IO_MCP23017`, `SN_IO_TCA9535`, `SN_IO_TCA9555` |
| `bus` | Zero-based index in `I2C_IO_BUSES` |
| `address` | 7-bit address, 0x20–0x27 |
| `direction` | 1 = input, 0 = output |
| `pullup` | MCP input pull-up mask; must be zero for TCA devices |
| `input_invert` | Software inversion of input bits only |
| `output_invert` | Software inversion of output bits only |
| `safe_outputs` | Logical output levels at startup and host timeout, before output inversion |
| `poll_ms` | Minimum period between poll starts for this device, 1–60000 ms |

MCP23008 masks use bits 0–7. Other devices use bits 0–15: the low byte is
GPA/P0 and the high byte GPB/P1. MCP23017 GPA7/GPB7 are output-only according to
the current Microchip datasheet; the prototype rejects input masks using bits
7 or 15. This restriction applies only to the new path, not existing boards.
TCA9535 has no internal I/O pull-ups. TCA9555 has fixed input pull-ups, not
software-selectable MCP-style pull-ups; a zero `pullup` field does not disable
them. Both TCA types share a register handler.

The example configures MCP23008 bits 0–3 as inputs (bit 0 inverted), bits 4–7
as outputs. TCA9555 bits 0–7 are inputs and 8–15 outputs; bit 8 is inverted,
and logical bit 9 is high in the configured disconnected state. Select output
polarity and safe levels for your own hardware.

## LinuxCNC pins

For component instance 0 and expander 0, names are:

- `stepgen-ninja.0.expander.0.input.0` and `input.0-not` (HAL_OUT).
- `stepgen-ninja.0.expander.0.output.4` (HAL_IN).
- `stepgen-ninja.0.expander.0.online` and `.valid` (HAL_OUT).

Only the configured input/output direction is exported for each physical pin.
Device numbering follows the descriptor order; bit numbers are physical port
bits, including the 8–15 range. Native GPIO HAL names remain unchanged.
HAL output commands initialize to the configured logical safe levels.

`online` and `valid` are true after a complete successful configuration/output/
input cycle. Any failed or short transfer clears both and causes a full
initialization retry at the next poll. Input values retain their last successful
sample while invalid; use the status pins when consuming them. LinuxCNC's host
watchdog clears the status pins when feedback stops. Polling continues while
the host is disconnected, so the firmware can still sample inputs and retry.

## Runtime and wire mapping

The hardware bus is polled by core 1 outside step/encoder IRQs. Packet handling
only copies a short locked snapshot; it never performs an I2C transfer.
All transfers have bounded SDK timeouts, but a complete cycle contains several
transfers, and the poll period is not a guaranteed response deadline. A slow
or faulted bus can delay core 1; no hard real-time I/O timing is promised.

- `outputs[0]`: existing native output-list indices 0–31.
- `outputs[1]`: expander 0 bits 0–15; expander 1 bits 16–31.
- `inputs[0..1]`: existing native GPIO banks.
- `inputs[2]`: expander input slots, in the same order.
- `inputs[3]`: online bits 0/1, valid bits 8/9. Optional DAC online/valid flags use bits 16–19/24–27; other bits zero.

There is no new wire field or packet size. This does not negotiate the config:
firmware and HAL must be rebuilt with matching descriptors. An old HAL binary
will not automatically export the new pins.

At initialization/recovery the driver releases directions, loads safe output
latches, clears hardware polarity inversion, applies MCP pull-ups, then enables
the requested output directions. MCP devices must be in power-on BANK=0 mode;
reset/power-cycle a chip previously configured by other firmware. MCP RESET
and device interrupt lines are not driven by this prototype. Sampling uses
polling only. Host timeout forces an immediate safe-output poll without waiting
for the normal period. **A broken bus cannot guarantee delivery of safe output
levels**, and the I/O chips retain their own hardware state until a successful
write/reset.

## Verification

Run the LinuxCNC test build and CTest; the suite includes the new tests. Direct
checks, without connecting to hardware:

```sh
python3 common/tests/i2c_io_contract.py cc
python3 common/tests/i2c_io_hal_contract.py --include /usr/include/linuxcnc
```

Tests execute the production register driver against fake devices and the HAL
path against real LinuxCNC headers with test-owned pin storage. They cover all
four chip types, initialization order, masks, inversion, polling, NAK/short-read
failure, recovery, host disconnect, snapshot isolation, GPIO conflicts, legacy
disabled defaults, unsupported combinations, bit mapping, and pin allocation
failure propagation. These checks and firmware builds do not establish physical
I2C, electrical compatibility or actual output behavior. Hardware validation is
still required.

Primary references: [Microchip MCP23017 datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf),
[TI TCA9535](https://www.ti.com/product/TCA9535),
[TI TCA9555 datasheet](https://www.ti.com/lit/ds/symlink/tca9555.pdf).

DACs can share this bus table; see [configurable I²C DAC outputs](I2C-DAC.en.md). Their channel commands use the existing configuration-dependent analog fields, not digital output slots.
