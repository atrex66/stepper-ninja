# stepgen-ninja LinuxCNC HAL Driver

## Overview

The `stepgen-ninja` is a LinuxCNC HAL driver that communicates with a remote device over UDP to control up to 4 step generators and read 4 encoders per instance, with a maximum of 4 instances. It uses non-blocking UDP sockets, checksum validation, and a watchdog for reliable real-time motion control.

## Functionality

1. **Initialization**:
   - Parses the `ip_address` module parameter (e.g., `192.168.1.100:5000`) to configure UDP communication.
   - Sets up a non-blocking UDP socket per instance, binding to a local port and setting the remote address.
   - Creates HAL pins for step generators, encoders, and status, and exports real-time functions: `watchdog-process`, `process-send`, and `process-recv`.

2. **Send Process**:
   - Reads position commands from `stepgen.<n>.command` pins, scales them using `step-scale`, and calculates step counts.
   - Generates a timing table based on the `period` pin for precise step timing (125MHz clock assumed on the remote device).
   - Sends 25-byte UDP packets with step commands and a checksum.

3. **Receive Process**:
   - Receives 25-byte UDP packets with encoder counts.
   - Validates packet checksums using a jump table.
   - Updates encoder pins (`raw-count`, `scaled-count`, `scaled-value`) based on received counts and `scale`.

4. **Watchdog**:
   - Monitors communication, triggering a ~10ms timeout if no valid packets are received.
   - On timeout, clears `io-ready-out` to break the E-stop loop and logs an error.

## HAL Pins

Pins are prefixed with `stepgen-ninja.<instance>` where `<instance>` is 0 to 3.

### General Pins

| Pin Name                            | Type   | Direction | Description                                                                 |
|-------------------------------------|--------|-----------|-----------------------------------------------------------------------------|
| `stepgen-ninja.<instance>.connected` | bit    | IN        | Shows UDP connection status (1 = connected, 0 = disconnected).              |
| `stepgen-ninja.<instance>.io-ready-in` | bit  | IN        | Input for I/O ready signal (part of E-stop loop).                           |
| `stepgen-ninja.<instance>.io-ready-out` | bit | OUT       | Output for I/O ready signal (1 if communication active and `io-ready-in` is 1, else 0). |
| `stepgen-ninja.<instance>.period`    | u32    | IN        | Control loop period in nanoseconds for step timing calculation.             |

### Step Generator Pins (for each `<n>`, 0 to 3)

| Pin Name                                      | Type   | Direction | Description                                                                 |
|-----------------------------------------------|--------|-----------|-----------------------------------------------------------------------------|
| `stepgen-ninja.<instance>.stepgen.<n>.command` | float  | IN        | Position command for the step generator in user units.                     |
| `stepgen-ninja.<instance>.stepgen.<n>.step-scale` | float | IN        | Scaling factor for step commands (steps per user unit).                    |
| `stepgen-ninja.<instance>.stepgen.<n>.feedback` | float | OUT       | Feedback position (not currently implemented).                              |
| `stepgen-ninja.<instance>.stepgen.<n>.mode` | bit | IN       | Position = 0 or Velocity = 1 mode                                             |

### Encoder Pins (for each `<n>`, 0 to 3)

| Pin Name                                        | Type   | Direction | Description                                                                 |
|-------------------------------------------------|--------|-----------|-----------------------------------------------------------------------------|
| `stepgen-ninja.<instance>.encoder.<n>.raw-count` | s32    | IN        | Raw encoder count from the remote device.                                   |
| `stepgen-ninja.<instance>.encoder.<n>.scaled-count` | s32 | IN        | Scaled encoder count (`raw-count` * `scale`).                               |
| `stepgen-ninja.<instance>.encoder.<n>.scale`     | float  | IN        | Scaling factor for encoder counts (user units per count, default = 1).      |
| `stepgen-ninja.<instance>.encoder.<n>.scaled-value` | float | OUT       | Scaled encoder value in user units (`raw-count` * `scale`).                 |