# Encoder index latch protocol (v2)

This change follows the LinuxCNC encoder/HostMot2 model: keep raw counts continuous, latch the count on index, and subtract the index offset to report position. The implementation is original code; the reference behavior comes from LinuxCNC `encoder.c`, `mesa-hostmot2/encoder.c`, and `homing.c`.

## Matched firmware and driver required

The shared transmission header adds the latched count and an encoder request tag to responses, and request tags to commands. Both packet directions carry `SN_PROTOCOL_MAGIC` (`0x534e4902`). Rebuild and update BOTH firmware and HAL driver from this source revision. Legacy packet sizes/checksums or a different protocol marker are rejected; there is no legacy fallback. No module installation or firmware flashing is performed by this source change.

Existing HAL pin names remain the same. `encoder.N.index-enable` is a bit HAL_IO pin: motion asserts it and the driver clears it on a matching index completion. `raw-count` remains continuous across an index; `position` is `(raw_count - index_count) / scale`, with 32-bit wrapping count differences. A small nonzero position after detection is normal: it represents motion since the index, like Mesa. The relative position resets, so LinuxCNC `HOME_INDEX_NO_ENCODER_RESET` normally remains NO for this implementation.

## Transaction and synchronization

- A validated high command with a new request tag arms the index. A GPIO index IRQ reads the PIO raw count, stores it, and disables that index IRQ. It never resets the PIO counter or substep estimator.
- Repeated high commands cannot rearm a completed request. The hit/count/tag are repeated in responses until the matching low command arrives, allowing retransmission after a lost response.
- The driver applies the matching offset once and clears the HAL_IO pin. It holds the wire request low until a response confirms the event has cleared. A newly asserted HAL request waits for that confirmation rather than being cleared by an old repeated hit.
- Tags also reject delayed old index replies and stale low commands. They are eight-bit serial numbers; comparison assumes delays are shorter than 128 completed request generations. The counter wrap and retries are covered by the firmware helper test.
- On attachment the driver synchronizes the tag and first clears any old firmware hit before arming a new request. A firmware restart accepts the first fresh request tag regardless of its numeric value.
- Command updates, encoder sampling and event snapshots run on core 0, serialized against its GPIO IRQ. Core 1's watchdog requests disarming through a pending flag; it no longer resets encoder PIO/substep state concurrently. Raw counting continues during disconnection. Host watchdog expiry resets host synchronization and offset state.
- Velocity uses raw-count delta and elapsed time between the same received samples. Index offset changes therefore do not inject a velocity spike, and lost responses do not mix a one-packet velocity delta with a multi-packet time interval.

Keep driver read/motion/write functions serialized in the servo-thread according to the machine's existing I/O cycle. The driver is not made safe for concurrent execution of the shared buffers in multiple realtime threads by this change.

## Hardware precision and validation boundary

The count is read in the GPIO IRQ. This matches the latch/offset semantics but is not an FPGA latch: GPIO IRQ latency and the PIO FIFO read affect physical index accuracy. Quadrature/substep PIO reads have bounded hardware-dependent latency; measure maximum interrupt-masked sampling time and index repeatability on the actual board before treating it as equivalent to Mesa hardware accuracy.

Source validation: host HAL contract/profile tests, firmware helper tests for duplicate commands/hits, lost-response retries, stale clear, cancel, request wrap and restart, plus driver tests for matching completion, a new pending request, stale replies, first-sample completion and wrong protocol rejection. Pico/Pico2, substep/quadrature and SPI firmware builds are checked separately. Host ASan/UBSan tests do not prove live Raspberry Pi/LinuxCNC homing. The reported intermittent segfault still requires a real backtrace if it recurs.

Verified in this change: 31/31 installed LinuxCNC 2.9 tests; 32/32 LinuxCNC 2.10.0-pre1 source-header tests including identical driver transcripts; host ASan/UBSan contract runs; loadable host HAL modules; firmware UF2 builds for Pico substep, Pico quadrature, Pico2 substep and Pico SPI. The firmware profiles use the repository configuration; they are not universal binaries for every breakout board. No runtime module installation or board flashing was performed.

Build from your machine/board configuration:

```sh
cmake -S hal-driver -B /tmp/sn-index-hal -DBUILD_TESTING=ON
cmake --build /tmp/sn-index-hal --target hal-driver-contract encoder-index-contract hal-modules
ctest --test-dir /tmp/sn-index-hal --output-on-failure
cmake -S firmware -B /tmp/sn-index-firmware -DBOARD=pico
cmake --build /tmp/sn-index-firmware
```

Use `BOARD=pico2` for RP2350. Configure the actual breakout board, pin map and SPI/UDP mode before building. Before live homing, verify the new HAL pin direction is I/O and check a single manual index request: it must clear once, raw count must remain continuous, and a second request must complete independently. Measure repeatability with the actual encoder and wiring. If LinuxCNC still crashes, retain the process backtrace; this source correction is not proof that the reported segfault has been fixed.
