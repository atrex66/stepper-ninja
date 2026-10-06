# Possible patch for Pi SPI first-run bug — needs testing

Status: investigation notes and a proposed experiment. No SPI startup patch
has been applied. The cause has not been confirmed on physical hardware.

## Reported symptom

With the `test` branch's Raspberry Pi spidev/libgpiod implementation, SPI
communication often fails on the first LinuxCNC start and works after restarting
LinuxCNC. The Pi and Pico remain powered between these attempts. Failed attempts
may also show unreliable transfer rather than a clean initialization failure.

## Findings in the current source

- In `hal-driver/stepgen-ninja.c`, `_send()` drives the INT output low and then
  immediately calls `SPI_IOC_MESSAGE(1)`. There is no explicit wait for the Pico
  to become ready.
- In `firmware/src/main.c`, `handle_udp()` polls the INT input. Only after it
  sees the low level does it prepare the outgoing frame and call
  `spi_read_fulldup()`, which configures and starts the DMA channels.
- If the Pi starts clocking before the Pico is ready, the initial response may
  be incomplete or misaligned. This is a hypothesis, not a measured result.
- `spi_read_fulldup()` blocks waiting for RX DMA completion without an explicit
  timeout or frame-resynchronization path.
- On driver unload, `rtapi_app_exit()` drives INT low before releasing its GPIO
  request. This may prepare the Pico for the next LinuxCNC start, explaining why
  the second attempt can behave differently. The level after GPIO release is
  not guaranteed.
- The driver watchdog expires after more than 10 watchdog callbacks without
  a valid response (approximately 11 ms at a 1 ms callback period). Once expired,
  both send and receive callbacks return early, preventing normal communication
  from recovering within that run.
- SPI errors are not propagated reliably: `_send()` reports a received packet
  length even after a failed transfer, and `_receive()` always reports a full
  packet. Some initialization failures are logged without failing module load.

These behaviors were present in the original `test` branch SPI implementation;
they were not introduced by the LinuxCNC 2.10 HAL API migration.

## Experiment without changing the driver

Before starting LinuxCNC, pull the Pi INT output low briefly with an external
GPIO tool. The current driver configuration uses GPIO25 on `/dev/gpiochip0`.
Verify those values against the actual board and configuration.

For **libgpiod 2.x**:

```sh
gpioset --version
sudo gpioset -c gpiochip0 --hold-period 10ms -t0 25=0
```

The second command holds GPIO25 low for 10 ms and then exits. Start LinuxCNC
**after the command finishes**. Do not keep `gpioset` running while loading the
HAL driver: the driver must obtain its own request for the same GPIO.

The GPIO level after `gpioset` exits is not guaranteed. The purpose of this
experiment is to let the Pico see the low level and prepare its DMA before the
driver's first transfer, rather than to hold the line low during LinuxCNC startup.

Run comparable trials:

1. With LinuxCNC stopped, reset or power-cycle the Pico and let its firmware
   finish booting. Start LinuxCNC normally and record the result.
2. Return the Pico to the same initial state and allow the same boot time.
   Run the GPIO command, then start LinuxCNC and record the result.
3. Repeat both cases several times. Record initialization errors, checksum
   errors, watchdog messages and whether communication remains stable.

Reliable improvement with the GPIO preparation step would support the startup
ordering hypothesis. A failed experiment would not by itself rule it out,
particularly if the GPIO request failed or the Pico was not yet running its
communication loop.

## Possible driver patch — not yet applied

As an isolated diagnostic change:

1. Request the INT GPIO with an initial **low** output value, instead of high.
2. Wait approximately **100 microseconds during module initialization**, before
   the first SPI transfer can occur. Keep this wait outside the servo callback.
3. Leave the packet format, motion calculations and other transfer behavior
   unchanged for this initial comparison.

The 100 microsecond delay is an experimental starting value, not a verified
minimum or a readiness guarantee. The external test's 10 ms pulse is deliberately
longer to make the preparation experiment easier to observe.

If this change fixes first-start behavior, further work is still needed to
address readiness between subsequent frames, DMA timeout/resynchronization,
correct idle INT behavior on unload, and propagation of SPI/GPIO errors.
A startup delay alone does not resolve all of these issues.

## Boot configuration observations

The available `c64zero/config.txt` copy contains `dtparam=spi=on`, which enables
SPI0. It also enables UART while the driver's default general-purpose input
list includes GPIO14 and GPIO15, the primary UART pins on a Pi3. That is a
potential GPIO ownership conflict and should be checked in the actual system.

Another saved boot-partition `config.txt` lacks the SPI enable setting. These
are local copies, not proof of the currently active Pi boot configuration.
Check the actual boot configuration and GPIO ownership before attributing the
failure to either copy. No boot configuration change is proposed as part of
the initial timing experiment.

## Useful measurement

A logic analyzer capture of **INT, chip select, SCLK and MISO** on the first
attempt and after restarting LinuxCNC would help confirm the ordering and frame
alignment. If possible, add a Pico diagnostic signal marking DMA readiness.
Compare that signal with the first SCLK edge.

## References

- [libgpiod gpioset documentation](https://libgpiod.readthedocs.io/en/master/gpioset.html)
- [Linux spidev API](https://www.kernel.org/doc/html/latest/spi/spidev.html)
- [Raspberry Pi SPI configuration](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#spi-software)
- [Raspberry Pi UART pin assignments](https://www.raspberrypi.com/documentation/computers/configuration.html#primary-and-secondary-uarts)
- Driver: `hal-driver/stepgen-ninja.c`
- Firmware: `firmware/src/main.c`
