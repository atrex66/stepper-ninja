# BreakoutBoard 1 Test Program

This document describes how to run the BreakoutBoard 1 Qt test panel and verify I/O, analog outputs, and encoder feedback.

## Important startup order

You must start the HAL runtime driver first:

1. Start the HAL runtime with the Stepper/Stepgen component.
2. Start the Qt test panel.

If HAL runtime is not running first, the panel can start but pins will not connect correctly.

## Prerequisites

- LinuxCNC tools available in PATH (halrun, halcmd)
- HAL module installed (stepgen-ninja and/or stepper-ninja)
- Python 3
- One Qt binding for Python:
	- PySide6 (recommended)
	- or PyQt5

Install Qt binding if needed:

```bash
pip install PySide6
```

## Files involved

- HAL runtime starter: ../halrun-stepper-ninja.sh
- Qt panel: bb1_qt_panel.py
- HAL pin wiring loaded by the panel: bb1_postgui.hal

## Run the test

From hal-driver directory:

```bash
cd /home/ninja/stepper-ninja/hal-driver
```

1. Start HAL runtime first (required):

```bash
./halrun-stepper-ninja.sh 192.168.0.177:8888
```

Keep this terminal open.

2. In another terminal, start the Qt test panel:

```bash
cd /home/ninja/stepper-ninja/hal-driver/test_config
python3 bb1_qt_panel.py --skip-halrun
```

Notes:

- The panel currently does not auto-start halrun, so manual startup is required.
- The panel loads bb1_postgui.hal by default (unless --skip-load-postgui is used).

## What the test covers

- Driver status:
	- connected
	- io_ready
	- jitter
- Digital inputs:
	- IN00..IN15 shown as LEDs
- Digital outputs:
	- OUT00..OUT07 toggle buttons
- Analog outputs:
	- analog0, analog1 (0.0 to 10.0)
- Encoder monitor:
	- encoder 0 raw count, position, velocity (RPS, RPM)
	- index-enable control
	- velocity scope for ENC0/ENC1

## Keyboard shortcuts

- 1..8: toggle OUT00..OUT07
- Q/A: analog0 up/down
- W/S: analog1 up/down
- I: request encoder index-enable
- E: switch scope between ENC0 and ENC1
- Esc: quit panel

## HAL wiring summary

The postgui file connects panel pins to instance 0 of stepgen-ninja:

- Status:
	- bb1-ui.connected <= stepgen-ninja.0.connected
	- bb1-ui.io_ready <= stepgen-ninja.0.io-ready-out
- Inputs:
	- bb1-ui.in00..in15 <= stepgen-ninja.0.inputs.0..15
- Outputs:
	- bb1-ui.out00..out07 => stepgen-ninja.0.outputs.0..7
- Analog:
	- bb1-ui.analog0 => stepgen-ninja.0.analog.0.value
	- bb1-ui.analog1 => stepgen-ninja.0.analog.1.value
- Encoder:
	- bb1-ui.enc0_raw_count <= stepgen-ninja.0.encoder.0.raw-count
	- bb1-ui.enc0_position <= stepgen-ninja.0.encoder.0.position
	- bb1-ui.enc0_velocity_rps <= stepgen-ninja.0.encoder.0.velocity-rps
	- bb1-ui.enc0_velocity_rpm <= stepgen-ninja.0.encoder.0.velocity-rpm
	- bb1-ui.enc0_index_enable => stepgen-ninja.0.encoder.0.index-enable

## Troubleshooting

- Panel starts but values do not change:
	- Confirm halrun-stepper-ninja.sh is running first.
	- Check IP/port passed to halrun-stepper-ninja.sh.
	- Run halcmd show pin stepgen-ninja.0.* in the halrun terminal.
- Qt import error:
	- Install PySide6 or PyQt5.
- HAL load error for bb1_postgui.hal:
	- Ensure the panel is started from test_config, or pass an absolute --postgui path.
