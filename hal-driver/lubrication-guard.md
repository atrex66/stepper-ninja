# Lubrication Guard

Lubrication Guard is a LinuxCNC HAL component for controlling a lubrication pump with pressure-switch feedback.

When enabled, it starts the pump and waits for pressure. Once the pressure switch activates, the pump keeps running for a configurable hold time, then stops. If pressure doesn't arrive before the timeout, it stops the pump and latches a fault until reset.

The defaults are a 5-second pressure timeout and a 1-second hold time. Both are adjustable, and the inputs can be inverted to match your wiring. It also provides motor, busy, fault and pressure-timeout outputs for your HAL setup.

Enable is level-triggered: leaving it active starts another cycle after the previous one finishes. Removing enable during a cycle does not cancel that cycle. The pressure switch is checked while waiting for pressure; pressure loss during the hold phase does not trigger a fault.

## HAL setup

Add the following to your machine's `.hal` file after the servo thread has been created. The module must already be installed in LinuxCNC's module directory.

```hal
loadrt lubrication-guard
addf lubrication-guard.process servo-thread

# Timing in seconds
setp lubrication-guard.params.timeout-seconds 5.0
setp lubrication-guard.params.hold-seconds 1.0

# Set to 1 if the corresponding input uses active-low logic
setp lubrication-guard.params.invert-enable 0
setp lubrication-guard.params.invert-pressure-ok 0
setp lubrication-guard.params.invert-reset-fault 0

# Commands and pressure feedback
net lube-request     => lubrication-guard.inputs.enable
net lube-pressure-ok => lubrication-guard.inputs.pressure-ok
net lube-reset       => lubrication-guard.inputs.reset-fault

# Pump command and status
net lube-pump     <= lubrication-guard.outputs.motor
net lube-fault    <= lubrication-guard.outputs.fault
net lube-timeout  <= lubrication-guard.outputs.pressure-timeout
net lube-busy     <= lubrication-guard.outputs.busy
net lube-state    <= lubrication-guard.outputs.state
```

These lines create the signals and connect the component. Connect your hardware and control pins to the same signals. For example, replace the placeholder pin names below with the actual names from your configuration:

```hal
# Examples only: replace <...> before using these lines
net lube-request     <= <lubrication-request-output-pin>
net lube-pressure-ok <= <pressure-switch-input-pin>
net lube-reset       <= <reset-button-output-pin>
net lube-pump        => <pump-relay-output-pin>
net lube-fault       => <fault-indicator-input-pin>
net lube-busy        => <cycle-active-indicator-input-pin>
```

The pressure-switch pin supplies feedback from the machine; the pump-relay pin receives the motor command. Each signal should have only one output pin driving it. Status outputs do not automatically stop LinuxCNC: connect `lube-fault` to the appropriate machine interlock if required by your configuration. GUI pins created by the display belong in your post-GUI HAL file.

For one lubrication cycle, assert `lube-request` long enough for the component to detect it, then release it before the cycle finishes. A timer or PLC can generate this request. Leaving it active repeats cycles. To clear a fault, release the request, pulse `lube-reset`, then release reset before issuing another request.

## Pin reference

All names below have the prefix `lubrication-guard.`. Directions are relative to this component.

| Pin | Type | Direction | Purpose |
|---|---|---|---|
| `inputs.enable` | bit | IN | Start a cycle when idle; repeat cycles while active. |
| `inputs.pressure-ok` | bit | IN | Pressure-switch feedback; active means pressure has been reached. |
| `inputs.reset-fault` | bit | IN | Clear the latched fault and timeout status while active. |
| `outputs.motor` | bit | OUT | Pump command. |
| `outputs.fault` | bit | OUT | Latched pressure-timeout fault. |
| `outputs.pressure-timeout` | bit | OUT | Latched timeout status. |
| `outputs.busy` | bit | OUT | Pump cycle is running. |
| `outputs.state` | u32 | OUT | Diagnostic state: 0 = idle, 1 = waiting for pressure, 2 = hold. |
| `params.hold-seconds` | float | IN | Pump run time after pressure is detected; default 1.0 s. |
| `params.timeout-seconds` | float | IN | Maximum wait for pressure; default 5.0 s. |
| `params.invert-enable` | bit | IN | Invert the enable input. |
| `params.invert-pressure-ok` | bit | IN | Invert the pressure input. |
| `params.invert-reset-fault` | bit | IN | Invert the reset input. |

Despite the `params.` prefix, these settings are HAL input pins. Use `setp` for a fixed value on an unconnected pin, or `net` to supply a value from another component.
