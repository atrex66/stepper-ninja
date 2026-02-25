# HAL Pins for stepgen-ninja Module

## Pin Naming

HAL pins use the Pico's **device name** as the instance identifier. The device name is
set at compile time via `DEVICE_NAME` in `firmware/inc/config.h` (default: `"stepper-ninja"`)
and is broadcast in the auto-discovery packet.

- **Auto-discovery** (`ip_address="auto"`): pin names use the device name received from
  the Pico, e.g. `stepgen-ninja.my-pico.stepgen.0.command`.
- **Manual IP** (`ip_address="192.168.0.177:8888"`): if the Pico's name is known from a
  previous discovery it is used; otherwise the instance index (`0`, `1`, …) is used,
  e.g. `stepgen-ninja.0.stepgen.0.command`.

The number of pins created matches the **feature set reported by the Pico** at discovery
time (stepgens, encoders, inputs, outputs, PWM channels). No recompilation of the HAL
driver is required when the Pico firmware configuration changes.

In the examples below `<name>` is the device name (or index) and `i` is the channel index
(0-based).

---

## Global Pins (per instance)

```
stepgen-ninja.<name>.connected        (BIT, IN)  - Connection status with hardware
stepgen-ninja.<name>.period           (U32, IN)  - Period in nanoseconds
stepgen-ninja.<name>.io-ready-in      (BIT, IN)  - Input ready signal (connect to estop chain)
stepgen-ninja.<name>.io-ready-out     (BIT, OUT) - Output ready signal (connect to estop chain)
stepgen-ninja.<name>.jitter           (U32, OUT) - Pico interrupt jitter in Pico clock cycles
```

---

## Stepgen Pins (per channel, only created when n_stepgens > 0)

```
stepgen-ninja.<name>.stepgen.pulse-width          (U32, IN)  - Step pulse width in nanoseconds (96–6300 ns @ 125 MHz)
stepgen-ninja.<name>.stepgen.<i>.command          (FLOAT, IN)  - Command input (position units or velocity units/s)
stepgen-ninja.<name>.stepgen.<i>.step-scale       (FLOAT, IN)  - Steps per unit
stepgen-ninja.<name>.stepgen.<i>.feedback         (FLOAT, OUT) - Feedback position/velocity
stepgen-ninja.<name>.stepgen.<i>.mode             (BIT, IN)  - 0 = position mode, 1 = velocity mode
stepgen-ninja.<name>.stepgen.<i>.enable           (BIT, IN)  - Enable channel (0 = stop)
```

---

## Encoder Pins (per channel, only created when n_encoders > 0)

```
stepgen-ninja.<name>.encoder.<i>.raw-count        (S32, OUT) - Raw encoder counts (quadrature)
stepgen-ninja.<name>.encoder.<i>.count-latched    (S32, OUT) - Latched count (at index pulse)
stepgen-ninja.<name>.encoder.<i>.scale            (FLOAT, IN)  - Encoder scaling factor (counts/unit)
stepgen-ninja.<name>.encoder.<i>.position         (FLOAT, OUT) - Scaled position value
stepgen-ninja.<name>.encoder.<i>.position-latched (FLOAT, OUT) - Scaled position at last index pulse
stepgen-ninja.<name>.encoder.<i>.velocity         (FLOAT, OUT) - Estimated velocity (units/s)
stepgen-ninja.<name>.encoder.<i>.index-enable     (BIT, IN)  - Latch position on next index pulse
stepgen-ninja.<name>.encoder.<i>.debug-reset      (BIT, IN)  - Reset encoder count (debug)
```

> **Note**: When the firmware is built with `use_stepcounter 1` the prefix changes from
> `encoder` to `stepcounter`.

---

## Digital Input Pins (per input, only created when n_inputs > 0)

Inputs are indexed sequentially from `0` regardless of the underlying GPIO number.

```
stepgen-ninja.<name>.input.<i>     (BIT, OUT) - Digital input state
stepgen-ninja.<name>.input.<i>-not (BIT, OUT) - Inverted digital input state
```

---

## Digital Output Pins (per output, only created when n_outputs > 0)

```
stepgen-ninja.<name>.output.<i>    (BIT, IN)  - Digital output control
```

---

## Analog Pins (breakout board only, per channel)

Only created when the firmware is built with `breakout_board > 0`.

```
stepgen-ninja.<name>.analog.<i>.enable   (BIT, IN)   - Enable analog output channel
stepgen-ninja.<name>.analog.<i>.minimum  (FLOAT, IN) - Minimum value (clamp)
stepgen-ninja.<name>.analog.<i>.maximum  (FLOAT, IN) - Maximum value (clamp / full-scale)
stepgen-ninja.<name>.analog.<i>.value    (FLOAT, IN) - Output value (scaled to 0–4095 DAC)
```

---

## PWM Pins (per channel, only created when n_pwm > 0)

```
stepgen-ninja.<name>.pwm.<i>.enable    (BIT, IN)  - PWM enable
stepgen-ninja.<name>.pwm.<i>.duty      (U32, IN)  - PWM duty cycle (0 – max-scale)
stepgen-ninja.<name>.pwm.<i>.frequency (U32, IN)  - PWM frequency in Hz (1907–1 000 000)
stepgen-ninja.<name>.pwm.<i>.max-scale (U32, IN)  - Maximum duty cycle scale value
stepgen-ninja.<name>.pwm.<i>.min-limit (U32, IN)  - Minimum duty cycle output
```

---

## Debug Pins (always created when n_stepgens > 0)

```
stepgen-ninja.<name>.stepgen.max-freq-khz         (FLOAT, OUT) - Maximum achievable step frequency (kHz)
stepgen-ninja.<name>.stepgen.debug-steps-reset    (BIT, IN)  - Reset debug step counters
stepgen-ninja.<name>.stepgen.<i>.debug-steps      (S32, OUT) - Accumulated step counter for channel i
```

---

## Functions (per instance)

```
stepgen-ninja.<name>.watchdog-process  - Watchdog timer function (add to fast thread)
stepgen-ninja.<name>.process-send      - Build and send UDP packet to Pico (add to servo thread)
stepgen-ninja.<name>.process-recv      - Receive UDP packet from Pico (add to servo thread)
```

---

## Module Parameter

```
ip_address  (string) - IP:port of the Pico, or "auto" for multicast auto-discovery.
                       Multiple instances: semicolon-separated list
                       e.g. "192.168.0.177:8888;192.168.0.178:8888"
                       Use "auto" to let the HAL driver find the Pico automatically.
```

---

## Notes

1. **Runtime resource allocation**: the number of pins created for each instance is
   determined at load time from the feature counts broadcast in the Pico's discovery
   packet (or from the compile-time defaults when using a manual IP address). No
   recompilation of the HAL driver is needed.
2. **Maximum supported counts**: up to 8 stepgens, 4 encoders, 8 PWM channels, 4 analog
   channels per Pico instance.
3. **Multiple Pico instances**: supply a semicolon-separated `ip_address` list. Each Pico
   gets its own set of HAL pins named after its `DEVICE_NAME`.
4. **Watchdog timeout**: ~10 ms. If no packet is received within the timeout the
   `io-ready-out` pin is cleared, breaking the estop chain.
5. **Stepgen modes**: position mode (mode=0, default) and velocity mode (mode=1).
6. **PWM frequency range**: 1907 Hz to 1 MHz.
7. **Encoder velocity**: estimated using a low-pass filter; accuracy improves at higher
   encoder resolution and speeds.