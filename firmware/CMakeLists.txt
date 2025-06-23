cmake_minimum_required(VERSION 3.12)
set(PICO_SDK_PATH "$ENV{HOME}/pico-sdk")
set(CMAKE_BUILD_TYPE Release) # Release mód

# WIZnet chip típusának beállítása (alapértelmezett: W5100S)
set(BOARD "pico" CACHE STRING "Pico board type (pico or pico2)")
set_property(CACHE BOARD PROPERTY STRINGS "pico" "pico2")

# Ellenőrizzük az érvényes értékeket
if(NOT BOARD MATCHES "^(pico|pico2)$")
    message(FATAL_ERROR "Invalid board type: ${BOARD}. Must be pico or pico2")
endif()

set(WIZCHIP_TYPE "W5100S" CACHE STRING "WIZnet chip type (W5100S or W5500)")
set_property(CACHE WIZCHIP_TYPE PROPERTY STRINGS "W5100S" "W5500")

if(NOT WIZCHIP_TYPE MATCHES "^(W5100S|W5500)$")
    message(FATAL_ERROR "Invalid WIZCHIP_TYPE: ${WIZCHIP_TYPE}. Must be W5100S or W5500.")
endif()


set(PICO_BOARD ${BOARD})

include(pico_sdk_import.cmake)

project(stepper-ninja C CXX ASM)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/ioLibrary_Driver/README.md")
    message(STATUS "Extracting ioLibrary_Driver...")
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

pico_sdk_init()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/ioLibrary_Driver
    ${CMAKE_CURRENT_SOURCE_DIR}/ioLibrary_Driver/Ethernet
    ${CMAKE_CURRENT_SOURCE_DIR}/ioLibrary_Driver/Ethernet/W5100S
    ${CMAKE_CURRENT_SOURCE_DIR}/ioLibrary_Driver/Ethernet/W5500
)

add_executable(stepper-ninja
    src/main.c
    modules/flash_config.c
    modules/serial_terminal.c
    modules/transmission.c
    modules/pwm.c
    modules/pio_utils.c
    ${WIZNET_SOURCES}
)

pico_generate_pio_header(stepper-ninja ${CMAKE_CURRENT_LIST_DIR}/pio/quadrature_encoder.pio)
pico_generate_pio_header(stepper-ninja ${CMAKE_CURRENT_LIST_DIR}/pio/freq_generator.pio)
pico_generate_pio_header(stepper-ninja ${CMAKE_CURRENT_LIST_DIR}/pio/step_counter.pio)

target_include_directories(stepper-ninja PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ioLibrary_Driver
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/inc
)

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
    hardware_timer
    hardware_irq
)

set(TARGET_NAME_WITH_CHIP "stepper-ninja-${BOARD}-${WIZCHIP_TYPE}")
set_target_properties(stepper-ninja PROPERTIES OUTPUT_NAME ${TARGET_NAME_WITH_CHIP})

pico_add_extra_outputs(stepper-ninja)