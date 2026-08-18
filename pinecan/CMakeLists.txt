cmake_minimum_required(VERSION 3.22)

add_library(pinecan INTERFACE)

# Enforce that the top-level set this
if(NOT PINECAN_BOARD)
    message(FATAL_ERROR "PINECAN_BOARD not set (must be defined in target BuildConfig.json)")
endif()

target_include_directories(pinecan INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/common
    ${CMAKE_CURRENT_LIST_DIR}/boards/${PINECAN_BOARD}
)

target_sources(pinecan INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/common/pinecanCommon.c
    ${CMAKE_CURRENT_LIST_DIR}/boards/${PINECAN_BOARD}/pinecanBoard.c
)

# libcanard as a static lib
add_library(libcanard STATIC)

target_sources(libcanard PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/libcanard/canard.c
)

target_include_directories(libcanard PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/libcanard
)

# Add dsdl generated sources
add_subdirectory(../dsdl ${CMAKE_BINARY_DIR}/dsdl) # TODO: set up clever regeneration and seperately between targets?

# Consumers of pinecan automatically get libcanard linked in
target_link_libraries(pinecan INTERFACE libcanard dsdl)

# Keep assertions opt-in on embedded targets. STM32CubeIDE Debug does not define
# PINECAN_DEBUG, and newlib's default __assert_func attempts formatted I/O before
# aborting, which makes a failed transport assertion look like a dead MCU.
option(PINECAN_ENABLE_ASSERTS "Enable PineCAN runtime assertions" OFF)

if(PINECAN_ENABLE_ASSERTS)
    target_compile_definitions(pinecan INTERFACE PINECAN_DEBUG=1)
    message("PineCAN runtime assertions enabled")
endif()

# Validate that pinecan code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()


