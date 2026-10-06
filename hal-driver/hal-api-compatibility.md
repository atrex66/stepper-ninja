# LinuxCNC 2.9 / 2.10 HAL compatibility

The `test` branch hardware driver `stepgen-ninja` now uses `hal_compat.h`.
The branch's spidev/libgpiod SPI implementation and linking rules are retained.
The header selects the API from `HAL_API_VERSION`, so the same driver source
builds with the old 2.9 API and the 2.10 API 1.

The adapter and initial Stepper-Ninja migration are adapted from
[FredericM88/linuxcnc-sim](https://github.com/FredericM88/linuxcnc-sim/tree/42ef3723da83b8ed88c123138c60437093255a22/compat/stepper-ninja),
revision `42ef3723da83b8ed88c123138c60437093255a22`, Copyright (c) 2026
Frederic Müller, MIT license. The imported code's license is retained in
`LICENSE.hal-compat.txt`. The original driver's attribution remains intact.

## API mapping

| Operation | LinuxCNC 2.9 | LinuxCNC 2.10 / API 1 |
|---|---|---|
| Bit pin | `hal_bit_t *` | `hal_bool_t` |
| Signed 32-bit pin | `hal_s32_t *` | `hal_sint_t` with si32 accessors |
| Unsigned 32-bit pin | `hal_u32_t *` | `hal_uint_t` with ui32 accessors |
| Float pin | `hal_float_t *` | `hal_real_t` |
| Creation | `hal_pin_*_newf` | `hal_pin_new_bool/si32/ui32/real` |
| Values | Pointer dereference | Typed `hal_get_*` / `hal_set_*` |
| Callback registration | Six arguments, including `uses_fp` | Five arguments |

API 1 stores integer values in 64-bit HAL storage. The adapter deliberately
uses si32/ui32 accessors to preserve the driver's 32-bit protocol semantics.
Pin names, directions, defaults, callback names, motion calculations and packet
layouts are retained. Board helpers 0, 1, 2, 3 and 100, the user template,
PWM and Raspberry Pi SPI pin accesses have been migrated.

Two existing defects were fixed along with this migration:

- A checksum error now writes zero to the `connected` pin's value rather than
  destroying its reference. The next valid packet can reconnect safely.
- The encoder velocity helper is compiled only when `encoders > 0`, allowing
  the encoder-free Board-2 configuration to compile.

This migration covers every HAL module entry point present in this `test`
checkout: `stepgen-ninja` and `lubrication-guard`. The shared `hal_util.c/.h`
pin-creation and callback helpers also use the adapter. Lubrication pin names,
directions, defaults, input inversion, timing, fault latching and reset behavior
are retained. Its callback remains non-reentrant (`reentrant=0`).
SPI builds require libgpiod 2.x development headers and its static library.

## Build for the installed LinuxCNC

Use the target installation's development headers and `Makefile.modinc`.
On a Pi3, build these commands on the Pi against its ARM64 LinuxCNC installation.
From the Stepper-Ninja checkout:

```sh
cmake -S hal-driver -B hal-driver/build-hal
cmake --build hal-driver/build-hal --target hal-modules -j2
```

Outputs:

```text
hal-driver/build-hal/stepgen-ninja/stepgen-ninja.so
hal-driver/build-hal/lubrication-guard/lubrication-guard.so
```

For a configured RIP installation, explicitly select both matching generated
headers and build rules in a fresh build directory:

```sh
cmake -S hal-driver -B hal-driver/build-hal-rip \
  -DLINUXCNC_INCLUDE_DIR=/path/to/linuxcnc/include \
  -DLINUXCNC_MODINC=/path/to/linuxcnc/src/Makefile.modinc \
  -DLINUXCNC_RT_MODULE_DIR=/path/to/linuxcnc/rtlib
cmake --build hal-driver/build-hal-rip --target hal-modules -j2
```

Building does not install or load a module. Installation is a separate step;
it replaces the corresponding installed module:

```sh
sudo cmake --install hal-driver/build-hal --component stepgen-ninja
sudo cmake --install hal-driver/build-hal --component lubrication-guard
```

## Tests and a headers-only 2.10 check

Contract tests require the repository's default Board-0 UDP configuration.
They are opt-in, so custom firmware configurations can build the driver alone.

```sh
cmake -S hal-driver -B hal-driver/build-test-29 -DBUILD_TESTING=ON
cmake --build hal-driver/build-test-29 --target hal-modules hal-driver-contract lubrication-contract hal-util-contract -j2
ctest --test-dir hal-driver/build-test-29 --output-on-failure
```

An unconfigured official LinuxCNC source tree can validate API 1 without
installing or building LinuxCNC itself:

```sh
cmake -S hal-driver -B hal-driver/build-test-210 \
  -DLINUXCNC_SOURCE_DIR=/path/to/linuxcnc-2.10-source \
  -DBUILD_TESTING=ON \
  -DHAL_BASELINE_EXECUTABLE="$PWD/hal-driver/build-test-29/tests/hal-driver-contract" \
  -DLUBRICATION_BASELINE_EXECUTABLE="$PWD/hal-driver/build-test-29/tests/lubrication-contract" \
  -DHAL_UTIL_BASELINE_EXECUTABLE="$PWD/hal-driver/build-test-29/tests/hal-util-contract"
cmake --build hal-driver/build-test-210 --target hal-modules hal-driver-contract lubrication-contract hal-util-contract -j2
ctest --test-dir hal-driver/build-test-210 --output-on-failure
```

`LINUXCNC_SOURCE_DIR` always selects **object compilation only**, even for a
configured tree. It produces no `.so` and has no module installation rules.
Use the explicit RIP paths above for a loadable module.

Validation on 2026-10-06, x86-64 host:

- Installed LinuxCNC 2.9.10: both real `.so` modules built; 30/30 tests passed.
- Official `v2.10.0-pre2` source headers (revision
  `49119ab84f260c208d598c8e0d7e3bcc9f37834c`): both module objects compiled;
  33/33 tests passed, including identical cross-version transcripts.
- Both APIs compiled Board 0, 1, 2, 3 and 100, Board-0 SPI, and Board-0 PWM.
- Lubrication and HAL utility transcripts matched their unchanged original
  2.9 sources and matched between API 0 and API 1. Tests cover normal hold,
  timeout/fault latching, reset, all input inversions, clamped negative times,
  and creation/export/ready failure handling.
- Driver tests exercise pin contracts, outgoing packet bytes, feedback, encoder/index
  handling, integer boundaries, creation failures and checksum-error recovery.

The contract executable uses test-owned HAL storage and intercepted network
calls with real HAL headers/accessors. These results do not establish live
LinuxCNC 2.10 shared-memory behavior, Raspberry Pi timing or physical hardware
operation. No installed HAL module was replaced or loaded during validation.
