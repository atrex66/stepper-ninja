rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
cp stepper-ninja-W5100S.uf2 ../../binary/stepper-ninja-W5100S.uf2
cd ..
rm -rf build
mkdir build && cd build
cmake -DWIZCHIP_TYPE=W5500 ..
make -j$(nproc)
cp stepper-ninja-W5500.uf2 ../../binary/stepper-ninja-W5500.uf2
