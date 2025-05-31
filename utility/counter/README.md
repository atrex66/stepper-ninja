# Pico-W Counter Web Server

Welcome to the Pico-W Counter Web Server! This project turns your Raspberry Pi Pico W into a tiny web server that tracks and displays counts from up to 4 step/direction or encoder devices (like motors or sensors). You can view the counts or reset them using a web browser on your phone or computer. It's easy to set up and use!

## What It Does

- Runs a web server on your Pico W to show counter values.
- Lets you reset counters through a simple web page.
- Connects to your WiFi network to serve the page.
- Blinks the Pico W's LED to show it's connected to the wifi network and working.

## What You Need

- **Raspberry Pi Pico W**: The tiny board that runs the show.
- **Sensors or Devices**: Connect step/direction or encoder devices to GPIO pins 0, 2, 4, 6, 8, 10, 12, or 14 (check your wiring!).
- **WiFi Network**: A WiFi network to connect the Pico W.
- **Computer or Phone**: To access the web interface on the same network.

## Setup Instructions

Follow these simple steps to get started:

1. **Get the Code**:
   - Download or clone this project to your computer.

2. **Set Up WiFi**:
   - Open the ```build_xxxxxx.sh``` script in a text editor.
   - Find the part where it sets WiFi credentials (usually in a CMake command).
   - Update ```WIFI_SSID``` to your WiFi network name and ```WIFI_PASSWORD``` to your WiFi password.

3. **Build the Project**:
   - Run the build script from your terminal:

     ``` bash
     ./build_encodercounter.sh
     ```

     ``` bash
     ./build_stepcounter.sh
     ```

   - This creates a file in the build directory (usually ending in .uf2) to program your Pico W.

4. **Flash the Pico W**:
   - Hold the BOOTSEL button on your Pico W and plug it into your computer via USB.
   - It will appear as a USB drive. Copy the """*.uf2""" file from the build folder to this drive.
   - The Pico W will restart automatically.

5. **Find the IP Address**:
   - Connect the Pico W to a USB port and open a serial terminal (like """minicom""" on Linux or a similar tool on Windows/Mac).
   - The Pico W will print its IP address (e.g., """192.168.1.100""") to the terminal.
   - If you don’t have a terminal, check your router’s device list for the Pico W’s IP.

6. **Open the Web Interface**:
   - On a phone or computer connected to the same WiFi network, open a web browser.
   - Type the IP address (e.g., ```http://192.168.1.100```) into the address bar and press Enter.
   - Voila! You’ll see a web page showing the counter values and buttons to reset them.

## Using the Web Interface

- **View Counts**: The web page shows the current values of up to 4 counters.
- **Reset a Counter**: Click a button to reset a specific counter to zero.
- **Reset All Counters**: Use the "Reset All" button to clear all counters.
