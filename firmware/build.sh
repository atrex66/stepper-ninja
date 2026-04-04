#!/bin/bash
rm -rf build
mkdir build && cd build
cmake -DBOARD=pico -DWIZCHIP_TYPE=W5500 ..
make
