#!/usr/bin/env python3
"""Compile inactive board/SPI/PWM branches against the selected real HAL headers."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", required=True)
parser.add_argument("--compiler", required=True)
parser.add_argument("--include", action="append", required=True)
args = parser.parse_args()

with tempfile.TemporaryDirectory(prefix="stepper-ninja-profiles-") as directory:
    root = Path(directory) / "driver"
    # Dereference firmware header symlinks in this private copy only.
    shutil.copytree(args.source, root, symlinks=False,
                    ignore=shutil.ignore_patterns("build*", "tests"))
    config = (root / "config.h").read_text()
    for board, spi, pwm in [(0, 0, 0), (1, 0, 0), (2, 0, 0), (3, 0, 0),
                            (100, 0, 0), (0, 1, 0), (0, 0, 1)]:
        variant = config
        for name, value in [("breakout_board", board), ("raspberry_pi_spi", spi),
                            ("use_pwm", pwm)]:
            variant, count = re.subn(r"(#define " + name + r") 0\b",
                                    lambda m: f"{m[1]} {value}", variant)
            if count != 1:
                raise SystemExit(f"Profile test requires default {name}=0 configuration")
        (root / "config.h").write_text(variant)
        command = [args.compiler, "-c", str(root / "stepgen-ninja.c"),
                   "-o", str(root / "profile.o"), "-I" + str(root)]
        command += ["-I" + path for path in args.include]
        command += ["-DUSPACE", "-DRTAPI", "-D_GNU_SOURCE", "-Drealtime",
                    "-D__MODULE__", "-DSIM", "-Werror=implicit-function-declaration",
                    "-Werror=incompatible-pointer-types"]
        result = subprocess.run(command, text=True, capture_output=True)
        if result.returncode:
            raise SystemExit(f"Board {board}, SPI {spi}, PWM {pwm}:\n{result.stderr}")
        print(f"PASS: Board {board}, SPI {spi}, PWM {pwm}")
