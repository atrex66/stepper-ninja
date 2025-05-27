#!/bin/bash
# This script builds the firmware for both W5100S and W5500 using CMake.
datum=$(date +%Y-%m-%d %H:%M)
echo "Building firmware for W5100S and W5500 on $datum"

rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
cp stepper-ninja-W5100S.uf2 ../../../binary/stepper-ninja-W5100S.uf2
cd ..
rm -rf build
mkdir build && cd build
cmake -DWIZCHIP_TYPE=W5500 ..
make -j$(nproc)
cp stepper-ninja-W5500.uf2 ../../../binary/stepper-ninja-W5500.uf2
cd ../../..

git add binary/stepper-ninja-W5100S.uf2
git add binary/stepper-ninja-W5500.uf2

datum=$(date +%Y-%m-%d %H:%M:%S)
git commit -m "Update firmware binaries for W5100S and W5500 on $datum"
git push origin master

echo "Firmware build and push completed for W5100S and W5500 on $datum"