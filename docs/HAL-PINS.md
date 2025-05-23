# HAL Pins for stepgen-ninja Module

## Global Pins

### For each instance (indexed by `j`):

``` .hal
module_name.j.connected - (BIT, IN) - Connection status with the hardware
module_name.j.period - (U32, IN) - Period in nanoseconds
module_name.j.io-ready-in - (BIT, IN) - Input ready signal
module_name.j.io-ready-out - (BIT, OUT) - Output ready signal
module_name.j.stepgen.pulse-width - (U32, IN) - Pulse width in nanoseconds
```

## Stepgen Pins (per channel, indexed by `i`)

``` .hal
module_name.j.stepgen.i.command - (FLOAT, IN) - Command input (position or velocity)
module_name.j.stepgen.i.step-scale - (FLOAT, IN) - Scaling factor for steps
module_name.j.stepgen.i.feedback - (FLOAT, OUT) - Feedback output
module_name.j.stepgen.i.mode - (BIT, IN) - Mode selector (0=position, 1=velocity)
module_name.j.stepgen.i.enable - (BIT, IN) - Enable signal
```

## Encoder Pins (per channel, indexed by `i`)

``` .hal
module_name.j.encoder.i.raw-count - (S32, IN) - Raw encoder count
module_name.j.encoder.i.scaled-count - (S32, IN) - Scaled encoder count
module_name.j.encoder.i.scale - (FLOAT, IN) - Encoder scaling factor
module_name.j.encoder.i.scaled-value - (FLOAT, OUT) - Scaled encoder value
```

## Debug Pins (only available when debug=1)

``` .hal
module_name.j.stepgen.max-freq-khz - (FLOAT, OUT) - Debug: maximum frequency in kHz
module_name.j.stepgen.dormant-cycles - (U32, IN) - Debug: dormant cycles setting
```

## Functions

For each instance (indexed by `j`):

``` .hal
module_name.j.watchdog-process - Watchdog timer function
module_name.j.process-send - UDP send processing function
module_name.j.process-recv - UDP receive processing function
```

## Configuration Parameters

The module accepts these parameters when loaded:

``` .hal
ip_address - Array of IP:port strings for UDP communication (e.g., "192.168.1.100:5000")
```

## Notes

1. The module supports up to 4 stepgen channels and 4 encoder channels per instance.
2. Multiple instances can be created by providing multiple IP:port pairs separated by semicolons.
3. The watchdog timeout is set to approximately 10ms.
4. The module implements both position and velocity modes for step generation.
5. All time-related parameters are in nanoseconds.