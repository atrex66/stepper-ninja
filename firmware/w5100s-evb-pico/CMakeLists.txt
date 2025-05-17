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
    src/config.c
    src/sh1106.c
    src/serial_terminal.c
    ${WIZNET_SOURCES}
)

# PIO fájlok generálása
pico_generate_pio_header(stepper-ninja ${CMAKE_CURRENT_LIST_DIR}/src/quadrature_encoder.pio)
pico_generate_pio_header(stepper-ninja ${CMAKE_CURRENT_LIST_DIR}/src/freq_generator.pio)

target_include_directories(stepper-ninja PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ioLibrary_Driver
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_compile_definitions(stepper-ninja PRIVATE
    _WIZCHIP_=W5100S
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
)


target_compile_options(stepper-ninja PRIVATE)
pico_add_extra_outputs(stepper-ninja)
