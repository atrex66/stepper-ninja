#!/usr/bin/env python3
"""Check host selection and packet layout without a Pico SDK or device."""
import pathlib
import subprocess
import sys
import tempfile
root = pathlib.Path(__file__).resolve().parents[2]
source = r'''
#include "transmission.h"
#include <stdio.h>
int main(void) {
#ifdef KBMATRIX
    (void)key_names;
#endif
#if defined(SN_HOST_MACH3) && SN_HOST_MACH3
    _Static_assert(stepgens == 3 && encoders == 0 && use_pwm == 1 && pwm_count == 1, "Mach3 counts");
    _Static_assert(raspberry_pi_spi == 0 && breakout_board == 0, "Mach3 transport");
#ifdef KBMATRIX
#error Mach3 pins must not be used by keyboard scanning
#endif
    _Static_assert(sizeof(transmission_pc_pico_t) == 36, "Mach3 request capacity");
    _Static_assert(sizeof(transmission_pico_pc_t) == 28, "Mach3 reply capacity");
#else
    _Static_assert(SN_PROTOCOL_MAGIC == 0x534e4902u, "LinuxCNC protocol identity");
#endif
    printf("%d %d %d %d %zu %zu\n", stepgens, encoders, use_pwm, pwm_count,
        sizeof(transmission_pc_pico_t), sizeof(transmission_pico_pc_t));
}
'''
with tempfile.TemporaryDirectory(prefix="sn-profile-contract-") as directory:
    path = pathlib.Path(directory)
    (path / "profile.c").write_text(source)
    outputs = []
    for defines in ([], ["-DSN_HOST_MACH3=0"], ["-DSN_HOST_MACH3=1"],
                    ["-DSN_HOST_MACH3=1", "-DSN_MACH3_USB_ONLY=1"]):
        subprocess.run([sys.argv[1], "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-I" + str(root / "firmware/inc"),
                        "-I" + str(root / "firmware/modules/inc"), *defines,
                        str(path / "profile.c"), "-o", str(path / "profile")], check=True)
        outputs.append(subprocess.check_output([str(path / "profile")], text=True).strip())
    assert outputs[0] == outputs[1], outputs
    assert outputs[2] == outputs[3], outputs
    print("LinuxCNC default and explicit OFF:", outputs[0])
    print("Mach3:", outputs[2])
