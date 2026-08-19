# Test script to validate BuildConfigs.json parsing
# This script tests the parsing logic without a full CMake configuration

cmake_minimum_required(VERSION 3.22)

# Repo root path
get_filename_component(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

# Load BuildConfig.json
set(PINECAN_CONFIG_FILE "${ROOT_DIR}/BuildConfigs.json")
if(NOT EXISTS "${PINECAN_CONFIG_FILE}")
    message(FATAL_ERROR "Config file not found: ${PINECAN_CONFIG_FILE}")
endif()

file(READ "${PINECAN_CONFIG_FILE}" _configs_json)

# Test with LIGHTING_rev5
set(PINECAN_CONFIG_KEY "LIGHTING_rev5")

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

# Extract the 'symbols' object
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
    string(JSON _key MEMBER "${_symbols_obj}" ${_i})
    string(JSON _val  GET  "${_symbols_obj}" "${_key}")
    string(JSON _type TYPE "${_symbols_obj}" "${_key}")

    # Decide quoting based on JSON type
    if(_type STREQUAL "STRING")
        list(APPEND PINECAN_SYMBOL_DEFS "${_key}=\"${_val}\"")
    else()
        list(APPEND PINECAN_SYMBOL_DEFS "${_key}=${_val}")
    endif()
endforeach()

message("✓ Test passed for LIGHTING_rev5")
message("  Config key: ${PINECAN_CONFIG_KEY}")
message("  Target location: ${_target_location}")
message("  Board: ${PINECAN_BOARD}")
message("  Symbols: ${PINECAN_SYMBOL_DEFS}")

# Test with LIGHTING_rev4
set(PINECAN_CONFIG_KEY "LIGHTING_rev4")

string(JSON _has_key ERROR_VARIABLE _json_err
       GET "${_configs_json}" "${PINECAN_CONFIG_KEY}")

if(_json_err)
    message(FATAL_ERROR
        "Config key '${PINECAN_CONFIG_KEY}' not found in BuildConfigs.json")
endif()

string(JSON _cfg GET "${_configs_json}" "${PINECAN_CONFIG_KEY}")
string(JSON _target_location GET "${_cfg}" target_location)

if(NOT _target_location STREQUAL "targets/efs-can-lighting")
    message(FATAL_ERROR
        "Config key '${PINECAN_CONFIG_KEY}' has target_location '${_target_location}', "
        "expected 'targets/efs-can-lighting'")
endif()

string(JSON PINECAN_BOARD GET "${_cfg}" board)
string(JSON _symbols_obj GET "${_cfg}" symbols)
string(JSON _sym_count LENGTH "${_symbols_obj}")
math(EXPR _sym_last "${_sym_count} - 1")

set(PINECAN_SYMBOL_DEFS "")
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

foreach(_i RANGE 0 ${_sym_last})
    string(JSON _key MEMBER "${_symbols_obj}" ${_i})
    string(JSON _val  GET  "${_symbols_obj}" "${_key}")
    string(JSON _type TYPE "${_symbols_obj}" "${_key}")

    if(_type STREQUAL "STRING")
        list(APPEND PINECAN_SYMBOL_DEFS "${_key}=\"${_val}\"")
    else()
        list(APPEND PINECAN_SYMBOL_DEFS "${_key}=${_val}")
    endif()
endforeach()

message("✓ Test passed for LIGHTING_rev4")
message("  Config key: ${PINECAN_CONFIG_KEY}")
message("  Target location: ${_target_location}")
message("  Board: ${PINECAN_BOARD}")
message("  Symbols: ${PINECAN_SYMBOL_DEFS}")

message("")
message("========================================")
message("All tests passed!")
message("========================================")
