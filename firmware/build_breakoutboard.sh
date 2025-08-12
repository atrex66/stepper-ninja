echo "Building Breakout Board firmware... (W5500, Pico2)"
rm -rf build
mkdir -p build
cd build
cmake -DWIZCHIP_TYPE=W5500 -DBOARD=pico2 ..
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "Build failed. Please check the output for errors."
    exit 1
fi
echo "Build completed successfully."