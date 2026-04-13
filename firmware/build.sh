#!/bin/bash
RUN_FROM_RAM=${RUN_FROM_RAM:-OFF}

rm -rf build
mkdir build && cd build
cmake -DBOARD=pico -DWIZCHIP_TYPE=W5500 -DSTEPPER_NINJA_RUN_FROM_RAM=${RUN_FROM_RAM} ..
make
