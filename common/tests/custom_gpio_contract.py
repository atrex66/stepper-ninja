#!/usr/bin/env python3
"""Run the production custom-GPIO packet block with GPIO0..47 substitutes."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
main = (root / "firmware/src/main.c").read_text()
start = main.index("// --- Standard GPIO path (no breakout board) ---")
block = main[start:main.index("    #endif", start)]
cases = [list(range(32)), [22, 23, 24, 25, 26, 33, 34, 36, 37, 38, 39, 40],
         [40, 3, 47, 31, 35, 0], list(range(48)), list(reversed(range(48)))]
source = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static struct { uint32_t outputs[2]; } rx, *rx_buffer = &rx;
static struct { uint32_t inputs[4]; } tx, *tx_buffer = &tx;
static uint32_t input_buffer[4];
static uint64_t raw;
static unsigned levels[48], writes[48];
static uint64_t gpio_get_all64(void) { return raw; }
static void gpio_put(unsigned pin, unsigned level) {
    assert(pin < 48 && level <= 1);
    levels[pin] = level;
    ++writes[pin];
}
'''
for i, pins in enumerate(cases):
    source += f"static void case_{i}(void) {{\n"
    source += "const uint8_t output_pins[] = {" + ",".join(map(str, pins)) + "};\n"
    source += r'''
    uint32_t state = 0x61c88647;
    for (unsigned trial = 0; trial < 1000; ++trial) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        rx.outputs[0] = state;
        rx.outputs[1] = ~state;
        raw = ((uint64_t)state << 32) | ~state;
        memset(levels, 0, sizeof(levels));
        memset(writes, 0, sizeof(writes));
'''
    source += block
    source += r'''
        uint64_t packed = ((uint64_t)rx.outputs[1] << 32) | rx.outputs[0];
        for (unsigned j = 0; j < sizeof(output_pins); ++j) {
            assert(writes[output_pins[j]] == 1);
            assert(levels[output_pins[j]] == ((packed >> j) & 1u));
        }
        unsigned total = 0;
        for (unsigned pin = 0; pin < 48; ++pin) total += writes[pin];
        assert(total == sizeof(output_pins));
        assert(tx.inputs[0] == (uint32_t)raw && tx.inputs[1] == raw >> 32);
        assert(input_buffer[0] == tx.inputs[0] && input_buffer[1] == tx.inputs[1]);
        assert(!input_buffer[2] && !input_buffer[3]);
    }
}
'''
source += "int main(void) {\n"
source += "\n".join(f"case_{i}();" for i in range(len(cases)))
source += '\nputs("PASS: legacy low GPIO, RP23CNC high GPIO, mixed banks, 48 outputs and input banks");\n}\n'
with tempfile.TemporaryDirectory(prefix="sn-custom-gpio-") as directory:
    p = Path(directory)
    (p / "contract.c").write_text(source)
    subprocess.run([sys.argv[1], "-std=c11", "-Wall", "-Wextra", "-Werror",
                    str(p / "contract.c"), "-o", str(p / "contract")], check=True)
    subprocess.run([str(p / "contract")], check=True)
