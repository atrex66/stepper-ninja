# IPCONFIG - stepper-ninja Serial Terminal Configuration

## Overview
The `ipconfig` functionality in `serial_terminal.c` provides a serial terminal interface for configuring the stepper-ninja device. It allows users to set network parameters (IP, MAC, port, etc.), manage timeouts, save configurations to flash, and reset the device. Commands are entered via a serial connection, processed in real-time, and validated for correct formatting.

## Prerequisites
- A serial terminal emulator (e.g., **minicom** on Linux) configured to connect to the device's serial port (e.g., `/dev/ttyACM0`) at the appropriate baud rate (typically 115200 8N1).
- The user must be part of the `dialout` group to access serial ports on Linux:
  ```
  sudo usermod -a -G dialout $USER
  ```
- The device must be powered on and connected via a serial cable or USB-to-serial adapter.

## Serial Terminal Setup (Linux)
1. Install minicom:
   ```
   sudo apt update
   sudo apt install minicom
   ```
2. Start minicom for the appropriate serial port (e.g., `/dev/ttyACM0`):
   ```
   minicom -D /dev/ttyACM0 -b 115200
   ```
3. Ensure hardware/software flow control is disabled in minicom (`Ctrl+A`, then `O`, select **Serial port setup**, set **F** and **G** to `No`).

## Supported Commands
The serial terminal accepts commands typed into the terminal, terminated by a carriage return (`\r`, Enter key). Commands are case-sensitive and must follow the exact syntax described below. Invalid commands or incorrect formats return an error message.

| Command | Description | Example | Output/Effect |
|---------|-------------|---------|---------------|
| `help` | Displays a list of available commands and their descriptions. | `help` | Prints the help menu. |
| `check` | Shows the current configuration, including MAC, IP, subnet, gateway, DNS, DHCP status, port, PHY status (duplex/speed), and timeout. | `check` | Prints detailed configuration details, e.g., `IP: 192.168.1.100`. |
| `ip <x.x.x.x>` | Sets the device's IP address to the specified value. | `ip 192.168.1.100` | Updates IP and saves to configuration. Prints `IP changed to 192.168.1.100`. |
| `ip` | Displays the current IP address. | `ip` | Prints `IP: 192.168.1.100`. |
| `port <port>` | Sets the device's port number. | `port 5000` | Updates port and saves to configuration. Prints `Port changed to 5000`. |
| `port` | Displays the current port number. | `port` | Prints `Port: 5000`. |
| `mac <xx:xx:xx:xx:xx:xx>` | Sets the device's MAC address (hexadecimal, colon-separated). | `mac 00:1A:2B:3C:4D:5E` | Updates MAC and saves to configuration. Prints `MAC changed to 00:1A:2B:3C:4D:5E`. |
| `mac` | Displays the current MAC address. | `mac` | Prints `MAC: 00:1A:2B:3C:4D:5E`. |
| `timeout <value>` | Sets the timeout value in microseconds. | `timeout 1000000` | Updates timeout and saves to configuration. Prints `Timeout changed to 1000000`. |
| `timeout` | Displays the current timeout value. | `timeout` | Prints `Timeout: 1000000`. |
| `tim <value>` | Sets the time constant value. | `tim 500` | Updates time constant and saves to configuration. Prints `Time const changed to 500`. |
| `defaults` | Restores the default configuration. | `defaults` | Resets configuration to factory defaults. |
| `reset` | Resets the device using the watchdog timer. | `reset` | Initiates a device reset. |
| `reboot` | Same as `reset`, reboots the device. | `reboot` | Initiates a device reset. |
| `save` | Saves the current configuration to flash memory. | `save` | Persists configuration changes to flash. |

### Notes
- Commands with parameters (e.g., `ip <x.x.x.x>`) require exact formatting. For example, IP addresses must be four decimal numbers separated by dots (0-255), and MAC addresses must be six hexadecimal bytes separated by colons.
- Invalid formats trigger an error message, e.g., `Invalid IP format` or `Invalid MAC format`.
- The `save` command must be issued to persist changes to flash after modifying settings (e.g., IP, port, MAC). Otherwise, changes are lost on reset.
- The terminal may lock/unlock based on the `timeout_error` state:
  - If `timeout_error == 0`, the terminal locks (`Terminal locked.`).
  - If `timeout_error == 1`, the terminal unlocks (`Terminal unlocked.`).

## Usage Example
1. Connect to the device using minicom:
   ```
   minicom -D /dev/ttyACM0 -b 115200
   ```
2. Check the current configuration:
   ```
   check
   ```
   Output:
   ```
   Current configuration:
   MAC: 00:1A:2B:3C:4D:5E
   IP: 192.168.1.100
   Subnet: 255.255.255.0
   Gateway: 192.168.1.1
   DNS: 8.8.8.8
   DHCP: 1 (1-Static, 2-Dynamic)
   PORT: 5000
   *******************PHY status**************
   PHY Duplex: Full
   PHY Speed: 100Mbps
   *******************************************
   Timeout: 1000000
   Ready.
   ```
3. Set a new IP address:
   ```
   ip 192.168.1.200
   ```
   Output: `IP changed to 192.168.1.200`
4. Save the configuration:
   ```
   save
   ```
5. Reboot the device:
   ```
   reset
   ```

## Technical Details
- **Input Handling**: The `handle_serial_input` function reads characters non-blocking via `getchar_timeout_us`. It ignores non-printable ASCII characters (except `\r`) and stores valid input in a 64-byte buffer. When `\r` is received or the buffer is full, the command is processed and the buffer is cleared.
- **Command Processing**: The `process_command` function parses commands using `strcmp` and `strncmp`. For commands with parameters, `sscanf` validates input formats (e.g., IP, MAC, port). Changes are applied to global variables (`net_info`, `port`, `TIMEOUT_US`, `time_constant`) and saved via `save_configuration`.
- **Configuration Storage**: The `save_configuration` function copies settings to a `flash_config` structure, which is persisted to flash using `save_config_to_flash`. The `load_configuration` function initializes settings from flash on startup.
- **Locking Mechanism**: The terminal locks when `timeout_error == 0` and unlocks when `timeout_error == 1`, controlling access to the serial interface. Note: `src_ip[0]` is not used in the provided code, so the locking mechanism might depend on another condition (e.g., a network-related timeout).

## Troubleshooting
- **No response from device**:
  - Verify the correct serial port (e.g., `ls /dev/tty*` to list ports).
  - Ensure the baud rate is correct (default: 115200).
  - Check serial cable connections and power to the device.
- **Permission denied on serial port**:
  - Add user to the `dialout` group (see Prerequisites).
- **Garbled output**:
  - Verify baud rate, parity (8N1), and flow control settings in minicom.
- **Invalid format errors**:
  - Ensure exact command syntax (e.g., `ip 192.168.1.100`, not `ip 192.168.001.100`).
- **Changes not persisted**:
  - Issue the `save` command after modifying settings.

## Limitations
- The terminal buffer is limited to 63 characters (plus null terminator). Commands longer than this will be truncated.
- The serial interface is non-blocking, so rapid input may be missed if the device is busy.
- The locking mechanism (based on `timeout_error`) may restrict access unexpectedly if network conditions trigger timeouts.

## Related Files
- `serial_terminal.c`: Implements the serial terminal interface and command processing.
- `config.h`: Likely defines the `configuration_t` structure and functions like `save_config_to_flash`, `load_config_from_flash`, and `restore_default_config`.
- `wizchip_conf.h`: Defines the `wiz_NetInfo` structure and functions like `getPHYSR` for network configuration.

For further assistance, consult the stepper-ninja device documentation or contact the device manufacturer.
