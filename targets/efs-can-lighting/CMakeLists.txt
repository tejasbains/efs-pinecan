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

# ======== Diagnostics ========
# Enables the SRAM2 trace buffer in Core/Inc/debug_trace.h. Read it back with:
#     cmake --build --preset=<preset> --target trace
# Defaults ON for Debug builds. Defining it also replaces newlib's __assert_func with
# a local one that records to the trace buffer instead of pulling in stdio/malloc.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_trace_default ON)
else()
    set(_trace_default OFF)
endif()

option(LIGHTING_TRACE "Record boot/ISR diagnostics into the SRAM2 trace buffer" ${_trace_default})

if(LIGHTING_TRACE)
    message("LIGHTING_TRACE enabled - trace buffer at 0x10000000")
endif()

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Pinecan symbols from BuildConfig.json
    ${PINECAN_SYMBOL_DEFS}
    # Add user defined symbols
    $<$<BOOL:${LIGHTING_TRACE}>:LIGHTING_TRACE=1>
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    STM32_Drivers
    stm32cubemx
    # Pinecan libraries
    pinecan # Brings in libcanard + dsdl as well
    # Add user defined libraries
)

# Configure clean target to remove build artifacts
# Note: .elf and .o files are already removed by CMake's default clean target.
# .bin/.hex are declared as real command outputs below, so CMake cleans them too.
set_property(TARGET ${CMAKE_PROJECT_NAME}
    APPEND PROPERTY ADDITIONAL_CLEAN_FILES
    "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map"
)

# ======== Binary artifacts ========
# These are generated with add_custom_command(OUTPUT ...) rather than POST_BUILD so that
# the .bin/.hex are first-class build outputs which other targets can depend on BY FILE.
#
# Do not convert these back to POST_BUILD + BYPRODUCTS. Under Ninja that attaches them to
# the link edge as a whole, so a .hex that is stale or deleted is never regenerated, and the
# flash target below silently programs an image that does not match the current ELF.
# See .kiro/specs/efs-can-lighting-build-config/debugging.md
set(PINECAN_ELF $<TARGET_FILE:${CMAKE_PROJECT_NAME}>)
set(PINECAN_BIN "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin")
set(PINECAN_HEX "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex")

# Generate .bin file (raw binary image)
add_custom_command(
    OUTPUT ${PINECAN_BIN}
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PINECAN_ELF} ${PINECAN_BIN}
    # Target name gives build ordering; $<TARGET_FILE:...> gives the file-level
    # dependency that actually triggers regeneration when the ELF is relinked.
    DEPENDS ${CMAKE_PROJECT_NAME} ${PINECAN_ELF}
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.bin"
    VERBATIM
)

# Generate .hex file (Intel HEX format)
add_custom_command(
    OUTPUT ${PINECAN_HEX}
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${PINECAN_ELF} ${PINECAN_HEX}
    DEPENDS ${CMAKE_PROJECT_NAME} ${PINECAN_ELF}
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.hex"
    VERBATIM
)

# Build .bin/.hex as part of a normal `cmake --build`, matching the previous POST_BUILD
# behaviour now that they are no longer attached to the link step.
add_custom_target(binaries ALL
    DEPENDS ${PINECAN_BIN} ${PINECAN_HEX}
)

# Add flash target (requires STM32_Programmer_CLI in PATH)
# DEPENDS lists the .hex FILE, not the executable target, so the image is always
# regenerated from the current ELF before being programmed.
add_custom_target(flash
    COMMAND STM32_Programmer_CLI -c port=SWD -w ${PINECAN_HEX} -v -rst
    DEPENDS ${PINECAN_HEX}
    COMMENT "Flashing ${CMAKE_PROJECT_NAME}.hex to target via ST-LINK"
    VERBATIM
)

# Read back and decode the SRAM2 trace buffer (requires LIGHTING_TRACE).
# Connects in HOTPLUG mode and does NOT reset, so a wedged MCU can be inspected
# in place. The ELF is passed so the decoder can resolve string pointers to text.
find_program(PINECAN_PYTHON NAMES python3 python py)
if(NOT PINECAN_PYTHON)
    set(PINECAN_PYTHON python)
endif()

add_custom_target(trace
    COMMAND ${PINECAN_PYTHON} ${CMAKE_CURRENT_LIST_DIR}/tools/read_trace.py
            --elf $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Reading SRAM2 trace buffer via ST-LINK (no reset)"
    USES_TERMINAL
    VERBATIM
)

# ======== Debug / diagnostic targets ========
# Documented in DEBUG_README.md. These exist to support the CMake-vs-CubeIDE
# investigation and are intended to live on the debug branch; only an isolated fix
# should be promoted to main.

# Invalidate the trace buffer magic word so the next run starts from a clean buffer.
# Useful before a test run, since SRAM2 survives a warm reset.
add_custom_target(trace-clear
    COMMAND STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -w32 0x10000000 0x00000000
    COMMENT "Clearing trace buffer magic at 0x10000000 (no reset)"
    USES_TERMINAL
    VERBATIM
)

# Flash using a HARDWARE reset instead of the software reset that plain `flash` uses.
# STM32CubeIDE resets differently from STM32_Programmer_CLI's default, so this
# isolates "is the failure a reset-mode artifact rather than a code difference".
add_custom_target(flash-hardrst
    COMMAND STM32_Programmer_CLI -c port=SWD mode=UR reset=HWrst -w ${PINECAN_HEX} -v --hardRst
    DEPENDS ${PINECAN_HEX}
    COMMENT "Flashing with connect-under-reset + hardware reset"
    VERBATIM
)

# Reset the target without reprogramming, to re-run the image already in flash.
add_custom_target(reset
    COMMAND STM32_Programmer_CLI -c port=SWD mode=HOTPLUG --hardRst
    COMMENT "Hardware-resetting the target (no reprogramming)"
    USES_TERMINAL
    VERBATIM
)

# Section/memory breakdown of the current ELF.
add_custom_target(size
    COMMAND ${CMAKE_SIZE} -A -x $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    DEPENDS ${CMAKE_PROJECT_NAME}
    COMMENT "Section sizes for ${CMAKE_PROJECT_NAME}.elf"
    VERBATIM
)

# Reports whether newlib assert/stdio/malloc got linked in, and which object provides
# __assert_func. This is the check that identified the live PINECAN_ASSERT in Debug
# builds: if __assert_func resolves to libc rather than main.cpp.obj, a tripped assert
# hangs the MCU invisibly.
add_custom_target(symcheck
    COMMAND ${PINECAN_PYTHON} ${CMAKE_CURRENT_LIST_DIR}/tools/elf_tools.py symcheck
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            --map ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map
    DEPENDS ${CMAKE_PROJECT_NAME}
    COMMENT "Checking for unwanted libc pull-in (assert/stdio/malloc)"
    USES_TERMINAL
    VERBATIM
)

# Full disassembly listing, for diffing this image against the STM32CubeIDE one.
add_custom_target(listing
    COMMAND ${PINECAN_PYTHON} ${CMAKE_CURRENT_LIST_DIR}/tools/elf_tools.py listing
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            --out ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.list
    DEPENDS ${CMAKE_PROJECT_NAME}
    COMMENT "Generating disassembly listing"
    VERBATIM
)

# Compare this image against a reference ELF (the STM32CubeIDE build) function by
# function, ignoring addresses. Reports functions missing from either side and any
# whose instruction sequence differs. Override the reference with -DPINECAN_REF_ELF=...
if(NOT DEFINED PINECAN_REF_ELF)
    set(PINECAN_REF_ELF "${CMAKE_CURRENT_LIST_DIR}/REV5 Debug/${CMAKE_PROJECT_NAME}.elf")
endif()

add_custom_target(compare-ref
    COMMAND ${PINECAN_PYTHON} ${CMAKE_CURRENT_LIST_DIR}/tools/elf_tools.py compare
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}> "${PINECAN_REF_ELF}"
    DEPENDS ${CMAKE_PROJECT_NAME}
    COMMENT "Comparing against the STM32CubeIDE reference ELF"
    USES_TERMINAL
    VERBATIM
)
