# Contract tests use real HAL headers, with test-owned HAL and network storage.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(profile_include_args)
foreach(directory IN LISTS HAL_INCLUDE_DIRS)
    list(APPEND profile_include_args --include "${directory}")
endforeach()
add_test(NAME hal-compile-profiles COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/compile_profiles.py"
    --source "${PROJECT_SOURCE_DIR}" --compiler "${CMAKE_C_COMPILER}"
    ${profile_include_args})

add_executable(hal-driver-contract driver_contract.c)
target_include_directories(hal-driver-contract PRIVATE "${PROJECT_SOURCE_DIR}" ${HAL_INCLUDE_DIRS})
target_compile_definitions(hal-driver-contract PRIVATE
    DRIVER_SOURCE="${PROJECT_SOURCE_DIR}/stepgen-ninja.c"
    USPACE RTAPI _GNU_SOURCE realtime __MODULE__ SIM)
target_compile_options(hal-driver-contract PRIVATE
    -UNDEBUG -Werror=implicit-function-declaration -Werror=incompatible-pointer-types)
target_link_libraries(hal-driver-contract PRIVATE m)
add_test(NAME hal-driver-contract COMMAND hal-driver-contract)
foreach(pin 0 9 10 11 40 65)
    add_test(NAME hal-create-failure-${pin} COMMAND hal-driver-contract ${pin})
endforeach()

set(HAL_BASELINE_EXECUTABLE "" CACHE FILEPATH
    "Optional API-0 contract executable from a previous build for cross-version parity")
if(HAL_BASELINE_EXECUTABLE)
    add_test(NAME hal-cross-version-parity COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/compare.py"
        "${HAL_BASELINE_EXECUTABLE}" "$<TARGET_FILE:hal-driver-contract>")
endif()

foreach(component lubrication hal-util)
    if(component STREQUAL "lubrication")
        set(contract_source lubrication_contract.c)
    else()
        set(contract_source hal_util_contract.c)
    endif()
    add_executable(${component}-contract "${contract_source}")
    target_include_directories(${component}-contract PRIVATE "${PROJECT_SOURCE_DIR}" ${HAL_INCLUDE_DIRS})
    target_compile_definitions(${component}-contract PRIVATE USPACE RTAPI _GNU_SOURCE realtime __MODULE__ SIM)
    target_compile_options(${component}-contract PRIVATE
        -UNDEBUG -Werror=implicit-function-declaration -Werror=incompatible-pointer-types)
    add_test(NAME ${component}-contract COMMAND ${component}-contract)
    add_test(NAME ${component}-export-failure COMMAND ${component}-contract export)
endforeach()
foreach(pin RANGE 0 12)
    add_test(NAME lubrication-create-failure-${pin} COMMAND lubrication-contract ${pin})
endforeach()
add_test(NAME lubrication-ready-failure COMMAND lubrication-contract ready)
foreach(pin RANGE 0 3)
    add_test(NAME hal-util-create-failure-${pin} COMMAND hal-util-contract ${pin})
endforeach()

set(LUBRICATION_BASELINE_EXECUTABLE "" CACHE FILEPATH "Optional API-0 lubrication contract executable")
set(HAL_UTIL_BASELINE_EXECUTABLE "" CACHE FILEPATH "Optional API-0 HAL utility contract executable")
foreach(component lubrication hal-util)
    if(component STREQUAL "lubrication")
        set(baseline "${LUBRICATION_BASELINE_EXECUTABLE}")
    else()
        set(baseline "${HAL_UTIL_BASELINE_EXECUTABLE}")
    endif()
    if(baseline)
        add_test(NAME ${component}-cross-version-parity COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/compare.py" "${baseline}" "$<TARGET_FILE:${component}-contract>")
    endif()
endforeach()

# Execute the exact firmware handshake helper on the host.
add_executable(encoder-index-contract encoder_index_contract.c)
target_include_directories(encoder-index-contract PRIVATE "${PROJECT_SOURCE_DIR}/../firmware/inc")
target_compile_options(encoder-index-contract PRIVATE -UNDEBUG -Wall -Wextra -Werror)
add_test(NAME encoder-index-contract COMMAND encoder-index-contract)
