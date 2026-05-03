RUN_FROM_RAM=${RUN_FROM_RAM:-OFF}

echo "Building Breakout Board firmware... (W5500, Pico2)"
rm -rf build
mkdir -p build
cd build
cmake -DWIZCHIP_TYPE=W5500 -DBOARD=pico2 -DSTEPPER_NINJA_RUN_FROM_RAM=${RUN_FROM_RAM} ..
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "Build failed. Please check the output for errors."
    exit 1
fi
echo "Build completed successfully."