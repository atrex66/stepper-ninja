# HAL Pins for stepgen-ninja Module

This document describes the Hardware Abstraction Layer (HAL) pins created by the `stepgen-ninja` module. The module is designed to interface with a real-time control system, handling step generation and encoder feedback over a UDP socket. It supports up to 4 instances, each with multiple step generators and encoders.

## Overview

The `stepgen-ninja` module creates HAL pins for each instance (up to `MAX_CHAN = 4`). Each instance corresponds to a specific IP address and port configuration for UDP communication. The pins are organized into categories for step generators, encoders, and general I/O control.

## Pin Naming Convention

All pins follow the naming format: `stepgen-ninja.<instance>.<category>.<index>.<name>`, where:

- `<instance>` is the instance number (0 to 3).
- `<category>` is either `stepgen`, `encoder`, or a general category.
- `<index>` is the index of the step generator or encoder (0 to 3).
- `<name>` is the specific pin name (e.g., `command`, `feedback`, etc.).

## HAL Pins

### General Pins (Per Instance)

These pins are created for each instance of the module.

| Pin Name | Type | Direction | Description |
|----------|------|-----------|-------------|
| `stepgen-ninja.<instance>.connected` | bit | IN | Indicates if the UDP socket connection is active (1 = connected, 0 = disconnected). |
| `stepgen-ninja.<instance>.period` | u32 | IN | The period of the control cycle in nanoseconds, used for timing calculations. |
| `stepgen-ninja.<instance>.io-ready-in` | bit | IN | Input pin for I/O readiness signal, typically used in an estop loop. |
| `stepgen-ninja.<instance>.io-ready-out` | bit | OUT | Output pin for I/O readiness signal, reflecting the state of `io-ready-in` unless a watchdog timeout occurs. |
| `stepgen-ninja.<instance>.stepgen.pulse-width` | u32 | IN | Pulse width for step generation in nanoseconds, used to configure the step pulse timing. |

### Debug Pins (Per Instance, Only if `debug == 1`)

These pins are available only when the module is compiled with `debug` set to 1.

| Pin Name | Type | Direction | Description |
|----------|------|-----------|-------------|
| `stepgen-ninja.<instance>.stepgen.max-freq-khz` | float | OUT | Maximum frequency in kHz, calculated based on the pulse width. (50% duty) |
| `stepgen-ninja.<instance>.stepgen.<index>.debug-steps` | u32 | OUT | Total number of steps generated|

### Step Generator Pins (Per Instance, Per Step Generator)

Each instance supports up to 4 step generators (`stepgens = 4`). The following pins are created for each step generator (indexed 0 to 3).

| Pin Name | Type | Direction | Description |
|----------|------|-----------|-------------|
| `stepgen-ninja.<instance>.stepgen.<index>.command` | float | IN | Command input for the step generator, representing position (in position mode) or velocity (in velocity mode). |
| `stepgen-ninja.<instance>.stepgen.<index>.step-scale` | float | IN | Scaling factor for the step generator command (default: 1.0). Converts command to steps. |
| `stepgen-ninja.<instance>.stepgen.<index>.feedback` | float | OUT | Feedback output, mirroring the command input for the step generator. |
| `stepgen-ninja.<instance>.stepgen.<index>.mode` | bit | IN | Mode of operation (0 = position mode, 1 = velocity mode, default: 0). |
| `stepgen-ninja.<instance>.stepgen.<index>.enable` | bit | IN | Enables or disables the step generator (1 = enabled, 0 = disabled, default: 0). |

### Encoder Pins (Per Instance, Per Encoder)

Each instance supports up to 4 encoders (`encoders = 4`). The following pins are created for each encoder (indexed 0 to 3).

| Pin Name | Type | Direction | Description |
|----------|------|-----------|-------------|
| `stepgen-ninja.<instance>.encoder.<index>.raw-count` | s32 | IN | Raw encoder count received from the remote device. |
| `stepgen-ninja.<instance>.encoder.<index>.scaled-count` | s32 | IN | Scaled encoder count, calculated as `raw-count * scale`. |
| `stepgen-ninja.<instance>.encoder.<index>.scale` | float | IN | Scaling factor for the encoder (default: 1.0). Converts raw counts to meaningful units. |
| `stepgen-ninja.<instance>.encoder.<index>.scaled-value` | float | OUT | Scaled encoder value, calculated as `raw-count * scale`. |

## Notes

- **Instances**: The number of instances is determined by parsing the `ip_address` module parameter, which contains IP:port pairs separated by semicolons. Up to 4 instances are supported (`MAX_CHAN = 4`).
- **Default Values**:
  - Step generator `mode` defaults to 0 (position mode).
  - Step generator `enable` defaults to 0 (disabled).
  - Step generator and encoder `scale` default to 1.0.
  - Debug `dormant-cycles` defaults to 10 (when `debug == 1`).
- **Debug Mode**: Debug pins are only created if the module is compiled with `debug` set to 1. These pins provide additional diagnostic information.
- **Watchdog**: Each instance includes a watchdog process that monitors UDP communication. If no data is received within the watchdog timeout (default: ~10 ms), the `io-ready-out` pin is set to 0, and an error is logged.
- **Commented Input Pins**: The code includes a commented section for input pins (e.g., `stepgen-ninja.<instance>.input.gp<index>`). These are not currently active but may be intended for future use with GPIO inputs.

## Example Pin Names

For instance 0, step generator 1, and encoder 2:

- `stepgen-ninja.0.stepgen.1.command`
- `stepgen-ninja.0.stepgen.1.feedback`
- `stepgen-ninja.0.encoder.2.raw-count`
- `stepgen-ninja.0.encoder.2.scaled-value`
- `stepgen-ninja.0.io-ready-out`

## Usage

These pins are intended to be connected to other HAL components in a LinuxCNC setup. For example:

- Connect `command` to a motion controller’s output for position or velocity control.
- Connect `feedback` to a motion controller’s input for closed-loop control.
- Use `io-ready-in` and `io-ready-out` in an estop chain to ensure safe operation.
- Monitor `connected` to detect communication issues with the remote device.

For more details on configuring the module, refer to the module parameters (`ip_address`) and the `parse_ip_port` function in the source code.
