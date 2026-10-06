cmake_minimum_required(VERSION 3.16)

project(stepper_ninja_hal_driver LANGUAGES C)

find_program(LINUXCNC_MAKE_PROGRAM NAMES gmake make REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED libgpiod)
string(REPLACE ";" " " GPIOD_CFLAGS_STR "${GPIOD_CFLAGS}")
find_library(GPIOD_STATIC_LIB
    NAMES libgpiod.a
    HINTS ${GPIOD_LIBRARY_DIRS}
    REQUIRED
)
set(GPIOD_LDFLAGS_STR "-Wl,--whole-archive ${GPIOD_STATIC_LIB} -Wl,--no-whole-archive")

set(LINUXCNC_SOURCE_DIR "" CACHE PATH
    "Unconfigured LinuxCNC source tree for a headers-only compile/test (no .so)")

if(LINUXCNC_SOURCE_DIR)
    foreach(header src/hal/hal.h src/rtapi/rtapi.h)
        if(NOT EXISTS "${LINUXCNC_SOURCE_DIR}/${header}")
            message(FATAL_ERROR "Missing LinuxCNC source header: ${header}")
        endif()
    endforeach()
    set(HAL_INCLUDE_DIRS "${LINUXCNC_SOURCE_DIR}/src/hal" "${LINUXCNC_SOURCE_DIR}/src/rtapi")
    message(STATUS "LinuxCNC source-header validation only: no loadable module or installation")
else()
    find_path(LINUXCNC_INCLUDE_DIR
        NAMES rtapi.h hal.h
        PATHS
            /usr/include/linuxcnc
            /usr/local/include/linuxcnc
            /home/ninja/linuxcnc/src/rtapi
        REQUIRED
    )

    find_file(LINUXCNC_MODINC
        NAMES Makefile.modinc
        PATHS
            /usr/share/linuxcnc
            /etc/linuxcnc
            /usr/lib/linuxcnc
            /home/ninja/linuxcnc/src
        REQUIRED
    )
    set(HAL_INCLUDE_DIRS "${LINUXCNC_INCLUDE_DIR}")
endif()

set(LINUXCNC_RT_MODULE_DIR "/usr/lib/linuxcnc/modules" CACHE PATH
    "Install directory for LinuxCNC realtime HAL modules")

file(GLOB HAL_DRIVER_TOPLEVEL_DEPS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

file(GLOB_RECURSE HAL_DRIVER_MODULE_DEPS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/*.h"
)

set(HAL_DRIVER_BUILD_DEPS
    ${HAL_DRIVER_TOPLEVEL_DEPS}
    ${HAL_DRIVER_MODULE_DEPS}
)

function(add_linuxcnc_rt_module module_name source_name)
    set(module_source "${CMAKE_CURRENT_SOURCE_DIR}/${source_name}")
    if(NOT EXISTS "${module_source}")
        message(FATAL_ERROR "Missing HAL driver source: ${module_source}")
    endif()

    if(LINUXCNC_SOURCE_DIR)
        add_library(${module_name} OBJECT EXCLUDE_FROM_ALL "${module_source}")
        target_include_directories(${module_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" ${HAL_INCLUDE_DIRS})
        target_compile_definitions(${module_name} PRIVATE
            USPACE RTAPI _GNU_SOURCE realtime __MODULE__ SIM MODULE_NAME="${module_name}")
        target_compile_options(${module_name} PRIVATE
            -Werror=implicit-function-declaration -Werror=incompatible-pointer-types)
        return()
    endif()

    set(module_build_dir "${CMAKE_CURRENT_BINARY_DIR}/${module_name}")
    set(module_makefile "${module_build_dir}/Makefile")
    set(module_output "${module_build_dir}/${module_name}.so")

    set(module_makefile_template [=[
obj-m += @module_name@.o
include @LINUXCNC_MODINC@
EXTRA_CFLAGS += -I@CMAKE_CURRENT_SOURCE_DIR@
EXTRA_CFLAGS += -I@LINUXCNC_INCLUDE_DIR@
EXTRA_CFLAGS += -DMODULE_NAME=\"@module_name@\"
EXTRA_CFLAGS += -Werror=implicit-function-declaration -Werror=incompatible-pointer-types
EXTRA_CFLAGS += @GPIOD_CFLAGS_STR@
LDFLAGS += @GPIOD_LDFLAGS_STR@

@module_name@.o: @module_source@
	$(ECHO) Compiling realtime $<
	$(Q)$(CC) -o $@ $(EXTRA_CFLAGS) -c $<
]=])

    string(CONFIGURE "${module_makefile_template}" module_makefile_content @ONLY)
    file(GENERATE OUTPUT "${module_makefile}" CONTENT "${module_makefile_content}")

    add_custom_command(
        OUTPUT "${module_output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${module_build_dir}"
        COMMAND ${CMAKE_COMMAND} -E chdir "${module_build_dir}"
                "${LINUXCNC_MAKE_PROGRAM}" "CC=${CMAKE_C_COMPILER}" -f "${module_makefile}" modules
        DEPENDS
            "${module_source}"
            "${module_makefile}"
            ${HAL_DRIVER_BUILD_DEPS}
        BYPRODUCTS
            "${module_build_dir}/${module_name}.o"
            "${module_build_dir}/${module_name}.tmp"
            "${module_build_dir}/${module_name}.ver"
        COMMENT "Building LinuxCNC HAL module ${module_name}"
        VERBATIM
        USES_TERMINAL
    )

    add_custom_target(${module_name} DEPENDS "${module_output}")

    install(
        FILES "${module_output}"
        DESTINATION "${LINUXCNC_RT_MODULE_DIR}"
        COMPONENT ${module_name}
    )
endfunction()

add_linuxcnc_rt_module(stepgen-ninja stepgen-ninja.c)
add_linuxcnc_rt_module(lubrication-guard lubrication-guard.c)

add_custom_target(hal-modules)
add_dependencies(hal-modules stepgen-ninja lubrication-guard)

message(STATUS "LinuxCNC include dirs: ${HAL_INCLUDE_DIRS}")
message(STATUS "LinuxCNC modinc: ${LINUXCNC_MODINC}")
message(STATUS "LinuxCNC module install dir: ${LINUXCNC_RT_MODULE_DIR}")

option(BUILD_TESTING "Build contract tests for the default Board-0 UDP configuration" OFF)
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
