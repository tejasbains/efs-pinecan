cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# ======== Pre-build setup ========
# Default config to use from BuildConfigs.json. This can be overridden from command line.
# IMPORTANT: If you modify this value (or any CACHE value), you must do a clean re-configure of CMake
set(PINECAN_CONFIG_KEY "LIGHTING_rev5"
    CACHE STRING "Key in BuildConfigs.json to use")

# Repo root path. Adjust path to match your layout.
get_filename_component(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Load BuildConfig.json to get config parameters
set(PINECAN_CONFIG_FILE "${ROOT_DIR}/BuildConfigs.json")
if(NOT EXISTS "${PINECAN_CONFIG_FILE}")
    message(FATAL_ERROR "Config file not found: ${PINECAN_CONFIG_FILE}")
endif()

file(READ "${PINECAN_CONFIG_FILE}" _configs_json)

# Ensure the requested key exists
string(JSON _has_key ERROR_VARIABLE _json_err
       GET "${_configs_json}" "${PINECAN_CONFIG_KEY}")

if(_json_err)
    message(FATAL_ERROR
        "Config key '${PINECAN_CONFIG_KEY}' not found in BuildConfigs.json")
endif()

string(JSON _cfg GET "${_configs_json}" "${PINECAN_CONFIG_KEY}")

# Extract target_location and validate
string(JSON _target_location GET "${_cfg}" target_location)
if(NOT _target_location STREQUAL "targets/efs-can-lighting")
    message(FATAL_ERROR
        "Config key '${PINECAN_CONFIG_KEY}' has target_location '${_target_location}', "
        "expected 'targets/efs-can-lighting'")
endif()

# Extract board
string(JSON PINECAN_BOARD GET "${_cfg}" board)

# Get all keys under "symbols"
# Extract the 'symbols' object only
string(JSON _symbols_obj GET "${_cfg}" symbols)

# How many members does 'symbols' have?
string(JSON _sym_count LENGTH "${_symbols_obj}")
math(EXPR _sym_last "${_sym_count} - 1")

set(PINECAN_SYMBOL_DEFS "")

# Validate exactly one of REV4 or REV5 is present
set(_has_rev4 FALSE)
set(_has_rev5 FALSE)

foreach(_i RANGE 0 ${_sym_last})
    string(JSON _key MEMBER "${_symbols_obj}" ${_i})
    if(_key STREQUAL "REV4")
        set(_has_rev4 TRUE)
    elseif(_key STREQUAL "REV5")
        set(_has_rev5 TRUE)
    endif()
endforeach()

if(_has_rev4 AND _has_rev5)
    message(FATAL_ERROR
        "Config key '${PINECAN_CONFIG_KEY}' defines both REV4 and REV5 symbols. "
        "Exactly one board revision symbol is required.")
elseif(NOT _has_rev4 AND NOT _has_rev5)
    message(FATAL_ERROR
        "Config key '${PINECAN_CONFIG_KEY}' defines neither REV4 nor REV5 symbol. "
        "Exactly one board revision symbol is required.")
endif()

# Iterate members by index and get their names with MEMBER
foreach(_i RANGE 0 ${_sym_last})
    # Get the name of the _i-th member key
    string(JSON _key MEMBER "${_symbols_obj}" ${_i})

    # Value under that key
    string(JSON _val  GET  "${_symbols_obj}" "${_key}")
    string(JSON _type TYPE "${_symbols_obj}" "${_key}")

    # Decide quoting based on JSON type
    if(_type STREQUAL "STRING")
        # Strings need quotes in C preprocessor
        list(APPEND PINECAN_SYMBOL_DEFS "${_key}=\"${_val}\"")
    else()
        # NUMBER / BOOLEAN / NULL / OBJECT / ARRAY -> no quotes
        list(APPEND PINECAN_SYMBOL_DEFS "${_key}=${_val}")
    endif()
endforeach()

message("Using BuildConfig: ${PINECAN_CONFIG_KEY}")
message("  target_location: ${_target_location}")
message("  board: ${PINECAN_BOARD}")
message("  symbols: ${PINECAN_SYMBOL_DEFS}")


# ======= Build setup ========
# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME efs-can-lighting)


# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for C, CXX, and ASM languages
enable_language(C CXX ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Verify pinecan/libcanard submodule is initialized before adding pinecan
set(LIBCANARD_CHECK "${ROOT_DIR}/pinecan/libcanard/canard.c")
if(NOT EXISTS "${LIBCANARD_CHECK}")
    message(FATAL_ERROR
        "Missing file: ${LIBCANARD_CHECK}\n"
        "The pinecan/libcanard submodule requires initialisation.\n"
        "Run: git submodule update --init --recursive")
endif()

# Add pinecan sources
set(PINECAN_DSDL_SOURCES
    ardupilot.indication.NotifyState.c
    uavcan.protocol.GetNodeInfo_res.c
    uavcan.protocol.NodeStatus.c
)
add_subdirectory(${ROOT_DIR}/pinecan ${CMAKE_BINARY_DIR}/pinecan)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # C++ application sources
    ./Core/Src/main.cpp
    ./Core/Src/can_manager.cpp
    ./Lighting/Src/conversions.cpp
    ./Lighting/Src/new_lighting_controller.cpp
    ./Lighting/Src/new_pattern_table.cpp
    # C application sources
    ./Core/Src/can/can.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ./Core/Inc/can
    ./Lighting/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Pinecan symbols from BuildConfig.json
    ${PINECAN_SYMBOL_DEFS}
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    STM32_Drivers
    stm32cubemx
    # Pinecan libraries
    pinecan # Brings in libcanard + dsdl as well
    # Add user defined libraries
)

# Make the converted images real outputs so flash cannot use a stale HEX.
set(PINECAN_ELF $<TARGET_FILE:${CMAKE_PROJECT_NAME}>)
set(PINECAN_BIN "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin")
set(PINECAN_HEX "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex")

add_custom_command(
    OUTPUT ${PINECAN_BIN}
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PINECAN_ELF} ${PINECAN_BIN}
    DEPENDS ${CMAKE_PROJECT_NAME} ${PINECAN_ELF}
    VERBATIM
)

add_custom_command(
    OUTPUT ${PINECAN_HEX}
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${PINECAN_ELF} ${PINECAN_HEX}
    DEPENDS ${CMAKE_PROJECT_NAME} ${PINECAN_ELF}
    VERBATIM
)

add_custom_target(binaries ALL DEPENDS ${PINECAN_BIN} ${PINECAN_HEX})

# Add flash target (requires STM32_Programmer_CLI in PATH).
add_custom_target(flash
    COMMAND STM32_Programmer_CLI -c port=SWD -w ${PINECAN_HEX} -v -rst
    DEPENDS ${PINECAN_HEX}
    COMMENT "Flashing ${CMAKE_PROJECT_NAME}.hex to target via ST-LINK"
    VERBATIM
)
