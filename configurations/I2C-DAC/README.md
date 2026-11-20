# Shared I2C DAC and digital I/O prototype example

Custom LinuxCNC example: MCP4725, TI DAC7571 and both channels of one AD5339,
plus MCP23008/TCA9555 digital I/O and native GPIO on one bus. No motion channels
are enabled. The fourth DAC channel's startup/disconnect code is 1024.
Adapt the pins, addresses, references and output levels to your hardware.

- [English configuration guide](../../docs/I2C-DAC.en.md)
- [Magyar konfigurációs útmutató](../../docs/I2C-DAC.hu.md)
- [Complete config.h](config.h)

Rebuild matching firmware and HAL. Prototype: physical validation is pending.
