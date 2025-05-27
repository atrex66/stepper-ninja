cmake_minimum_required(VERSION 3.12)
set(PICO_SDK_PATH "$ENV{HOME}/pico-sdk")
set(CMAKE_BUILD_TYPE Release) # Release mód

include(pico_sdk_import.cmake)

project(encoder-hussar C CXX ASM)

# Ellenőrizzük, hogy létezik-e a könyvtár
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/ioLibrary_Driver")
    message(STATUS "Extracting ioLibrary_Driver...")
    # Szkript futtatása, ha a könyvtár nem létezik
    execute_process(
        COMMAND bash ${CMAKE_SOURCE_DIR}/ioLibraryDriver.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
else()
    message(STATUS "IoLibrary_Driver already exists.")
endif()

# WIZnet forrásfájlok
file(GLOB_RECURSE WIZNET_SOURCES
    "ioLibrary_Driver/Internet/DHCP/*.c"
    "ioLibrary_Driver/Ethernet/*.c"
    "ioLibrary_Driver/Ethernet/W5100S/*.c"
    "ioLibrary_Driver/Ethernet/W5500/*.c"
)

# Cortex-M0+ cél explicit megadása
set(PICO_PLATFORM rp2040)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m0plus -mthumb")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-m0plus -mthumb")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -mcpu=cortex-m0plus -mthumb")

add_compile_options(-g -O3)

pico_sdk_init()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/ioLibrary_Driver
    ${CMAKE_CURRENT_SOURCE_DIR}/ioLibrary_Driver/Ethernet
    ${CMAKE_CURRENT_SOURCE_DIR}/ioLibrary_Driver/Ethernet/W5100S
    ${CMAKE_CURRENT_SOURCE_DIR}/ioLibrary_Driver/Ethernet/W5500
)

add_executable(stepper-ninja
    src/main.c
    src/flash_config.c
    src/serial_terminal.c
    src/transmission.c
    src/ringbuffer.c
    ${WIZNET_SOURCES}
)

# PIO fájlok generálása
pico_generate_pio_header(stepper-ninja ${CMAKE_CURRENT_LIST_DIR}/src/quadrature_encoder.pio)
pico_generate_pio_header(stepper-ninja ${CMAKE_CURRENT_LIST_DIR}/src/freq_generator.pio)

target_include_directories(stepper-ninja PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ioLibrary_Driver
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

# WIZnet chip típusának beállítása (alapértelmezett: W5100S)
set(WIZCHIP_TYPE "W5100S" CACHE STRING "WIZnet chip type (W5100S or W5500)")
set_property(CACHE WIZCHIP_TYPE PROPERTY STRINGS "W5100S" "W5500")

# Ellenőrizzük az érvényes értékeket
if(NOT WIZCHIP_TYPE MATCHES "^(W5100S|W5500)$")
    message(FATAL_ERROR "Invalid WIZCHIP_TYPE: ${WIZCHIP_TYPE}. Must be W5100S or W5500.")
endif()

# Fordítási definíciók
target_compile_definitions(stepper-ninja PRIVATE
    _WIZCHIP_=${WIZCHIP_TYPE}
    WIZCHIP_USE_CS=1
)

target_link_libraries(stepper-ninja
    pico_stdlib
    hardware_spi
    pico_multicore
    pico_stdio_usb
    hardware_gpio
    hardware_dma
    hardware_i2c
    hardware_pio
    hardware_pll
    hardware_pwm
)

# Set output filename based on WIZCHIP_TYPE
set(TARGET_NAME_WITH_CHIP "stepper-ninja-${WIZCHIP_TYPE}")
set_target_properties(stepper-ninja PROPERTIES OUTPUT_NAME ${TARGET_NAME_WITH_CHIP})

pico_add_extra_outputs(stepper-ninja)