cmake_minimum_required(VERSION 3.13)

# Set Pico-W board before project()
set(PICO_BOARD pico_w)

# Initialize the SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(step_dir_webserver C CXX ASM)

# Initialize the SDK after project()
pico_sdk_init()

# WiFi credentials - set these or pass via command line
if(NOT DEFINED WIFI_SSID)
    set(WIFI_SSID "your_wifi_ssid")
endif()
if(NOT DEFINED WIFI_PASSWORD)
    set(WIFI_PASSWORD "your_wifi_password")
endif()
if(NOT DEFINED ENCODER_COUNTER)
    set(ENCODER_COUNTER 0)
endif()
# Add executable
add_executable(step_dir_webserver
    counter_webserver.c
    webpage.c
)

pico_generate_pio_header(step_dir_webserver ${CMAKE_CURRENT_SOURCE_DIR}/step_counter.pio)
pico_generate_pio_header(step_dir_webserver ${CMAKE_CURRENT_SOURCE_DIR}/encoder_counter.pio)

# Compile definitions
target_compile_definitions(step_dir_webserver PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    ENCODER_COUNTER=${ENCODER_COUNTER}
    PICO_CYW43_ARCH_POLL=1
)

# Include directories
target_include_directories(step_dir_webserver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PICO_SDK_PATH}/lib/cyw43-driver/src
    ${PICO_SDK_PATH}/lib/lwip/src/include
)

# Link libraries - IMPORTANT ORDER MATTERS
target_link_libraries(step_dir_webserver
    pico_cyw43_arch_lwip_poll
    pico_stdlib
    hardware_pio
    hardware_dma
    pico_lwip
)

# Enable USB output
pico_enable_stdio_usb(step_dir_webserver 1)
pico_enable_stdio_uart(step_dir_webserver 0)

if (${ENCODER_COUNTER} EQUAL 1)
    set(TARGET_NAME "encoder-counter")
else()
    set(TARGET_NAME "step-dir-counter")
endif()

set_target_properties(step_dir_webserver PROPERTIES OUTPUT_NAME ${TARGET_NAME})

# Add additional outputs
pico_add_extra_outputs(step_dir_webserver)