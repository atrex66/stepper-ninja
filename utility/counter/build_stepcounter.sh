rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DWIFI_SSID="counter" -DWIFI_PASSWORD="DEADBEEF" -DENCODER_COUNTER=0
make -j$(nproc)
cd ..
