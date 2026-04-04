cmake_minimum_required(VERSION 3.16)

project(stepper_ninja_hal_driver LANGUAGES C)

find_program(LINUXCNC_MAKE_PROGRAM NAMES gmake make REQUIRED)

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

    set(module_build_dir "${CMAKE_CURRENT_BINARY_DIR}/${module_name}")
    set(module_makefile "${module_build_dir}/Makefile")
    set(module_output "${module_build_dir}/${module_name}.so")

    set(module_makefile_template [=[
obj-m += @module_name@.o
include @LINUXCNC_MODINC@
EXTRA_CFLAGS += -I@CMAKE_CURRENT_SOURCE_DIR@
EXTRA_CFLAGS += -I@LINUXCNC_INCLUDE_DIR@

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

add_linuxcnc_rt_module(stepper-ninja stepper-ninja.c)
add_linuxcnc_rt_module(plc-ninja plc-ninja.c)
add_linuxcnc_rt_module(polygon-ninja polygon-ninja.c)

add_custom_target(hal-modules)
add_dependencies(hal-modules stepper-ninja)
add_dependencies(hal-modules plc-ninja)
add_dependencies(hal-modules polygon-ninja)

message(STATUS "LinuxCNC include dir: ${LINUXCNC_INCLUDE_DIR}")
message(STATUS "LinuxCNC modinc: ${LINUXCNC_MODINC}")
message(STATUS "LinuxCNC module install dir: ${LINUXCNC_RT_MODULE_DIR}")
