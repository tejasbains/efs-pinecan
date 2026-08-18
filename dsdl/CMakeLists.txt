cmake_minimum_required(VERSION 3.22)

# dsdl as a static lib
add_library(dsdl STATIC)

if(PINECAN_DSDL_SOURCES)
    set(DSDL_SOURCES "")
    foreach(DSDL_SOURCE IN LISTS PINECAN_DSDL_SOURCES)
        list(APPEND DSDL_SOURCES "${CMAKE_CURRENT_LIST_DIR}/generated/src/${DSDL_SOURCE}")
    endforeach()
else()
    file(GLOB DSDL_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_LIST_DIR}/generated/src/*.c
    )
endif()

target_sources(dsdl PRIVATE
    ${DSDL_SOURCES}
)

target_include_directories(dsdl SYSTEM PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/generated/include
)

target_link_libraries(dsdl PRIVATE libcanard)

# Validate that dsdl code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()
