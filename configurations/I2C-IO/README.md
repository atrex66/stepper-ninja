# I2C digital I/O prototype example

Custom LinuxCNC configuration, **not a fixed breakout-board profile**.
The example enables two buses with an MCP23008 and TCA9555, plus native GPIO.
It deliberately enables no motors or encoders. Adapt it to your hardware and
rebuild both firmware and HAL with the same configuration.

- [English documentation](../../docs/I2C-IO.en.md)
- [Magyar dokumentáció](../../docs/I2C-IO.hu.md)
- [Example config.h](config.h)

Prototype only: builds and host tests do not establish physical hardware operation.
