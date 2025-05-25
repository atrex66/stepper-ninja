# HAL Pins for stepgen-ninja Module`

## Global Pins (per instance)

``` .hal
module_name.j.connected - (BIT, IN) - Connection status with hardware
module_name.j.period - (U32, IN) - Period in nanoseconds
module_name.j.io-ready-in - (BIT, IN) - Input ready signal
module_name.j.io-ready-out - (BIT, OUT) - Output ready signal
module_name.j.stepgen.pulse-width - (U32, IN) - Pulse width in nanoseconds
```

## Stepgen Pins (per channel)

``` .hal
module_name.j.stepgen.i.command - (FLOAT, IN) - Command input (position/velocity)
module_name.j.stepgen.i.step-scale - (FLOAT, IN) - Steps per unit scaling
module_name.j.stepgen.i.feedback - (FLOAT, OUT) - Feedback position/velocity
module_name.j.stepgen.i.mode - (BIT, IN) - 0=position, 1=velocity mode
module_name.j.stepgen.i.enable - (BIT, IN) - Enable channel
```

## Encoder Pins (per channel)

``` .hal
module_name.j.encoder.i.raw-count - (S32, IN) - Raw encoder counts
module_name.j.encoder.i.scaled-count - (S32, OUT) - Scaled encoder counts
module_name.j.encoder.i.scale - (FLOAT, IN) - Encoder scaling factor
module_name.j.encoder.i.scaled-value - (FLOAT, OUT) - Scaled position value
```

## Digital I/O Pins

### Inputs:

``` .hal
module_name.j.input.gpX - (BIT, OUT) - Digital input state (X = pin number)
```

### Outputs (when use_outputs=1):

``` .hal
module_name.j.output.gpX - (BIT, IN) - Digital output control (X = pin number)
```

## PWM Pins (when use_pwm=1)

``` .hal
module_name.j.pwm.enable - (BIT, IN) - PWM enable
module_name.j.pwm.duty - (U32, IN) - PWM duty cycle (minlimit-maxscale)
module_name.j.pwm.frequency - (U32, IN) - PWM frequency (1907-1000000 Hz)
module_name.j.pwm.max-scale - (U32, IN) - Maximum duty cycle scale
module_name.j.pwm.min-limit - (U32, IN) - Minimum duty cycle output
```

## Debug Pins (when debug=1)

``` .hal
module_name.j.stepgen.max-freq-khz - (FLOAT, OUT) - Maximum step frequency
module_name.j.stepgen.i.debug-steps - (U32, OUT) - Step counter for debugging
```

## Functions (per instance)

``` .hal
module_name.j.watchdog-process - Watchdog timer function
module_name.j.process-send - UDP transmission function
module_name.j.process-recv - UDP reception function
```

## Configuration Parameters

``` .hal
ip_address - Array of IP:port strings (e.g. "192.168.1.100:5000;192.168.1.101:5001")
```

## Notes

1. Supports up to 4 stepgen and 4 encoder channels per instance
2. Multiple instances can be created with different IP:port pairs
3. Watchdog timeout is ~10ms
4. All time values are in nanoseconds
5. Stepgen supports both position and velocity modes
6. PWM frequency range: 1907Hz to 1MHz
7. Digital I/O pins are configurable via input_pins/output_pins arrays