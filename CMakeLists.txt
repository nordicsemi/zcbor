#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.13.3)

# Read version from zcbor/VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/zcbor/VERSION" ZCBOR_VERSION_CONTENT)
string(STRIP "${ZCBOR_VERSION_CONTENT}" ZCBOR_VERSION)

project(zcbor VERSION ${ZCBOR_VERSION} LANGUAGES C)

option(ZCBOR_BUILD_SHARED "Build zcbor as a shared library" OFF)
option(ZCBOR_ENABLE_PRINT "Enable zcbor_print module" ON)
option(ZCBOR_ENABLE_TAGS "Enable zcbor_tags module" ON)

# zcbor compile-time configuration options
option(ZCBOR_CANONICAL "Assume canonical encoding (deterministic CBOR)" OFF)
option(ZCBOR_VERBOSE "Print log messages on encoding/decoding errors" OFF)
option(ZCBOR_ASSERTS "Enable asserts in zcbor" OFF)
option(ZCBOR_STOP_ON_ERROR "Enable stop_on_error functionality" OFF)
option(ZCBOR_BIG_ENDIAN "All decoded values are returned as big-endian" OFF)
option(ZCBOR_MAP_SMART_SEARCH "Enable smart search for unordered maps" OFF)

set(ZCBOR_SOURCES
    src/zcbor_common.c
    src/zcbor_decode.c
    src/zcbor_encode.c
)
set(ZCBOR_HEADERS
    include/zcbor_common.h
    include/zcbor_decode.h
    include/zcbor_encode.h
)

if(ZCBOR_ENABLE_PRINT)
    list(APPEND ZCBOR_SOURCES src/zcbor_print.c)
    list(APPEND ZCBOR_HEADERS include/zcbor_print.h)
endif()
if(ZCBOR_ENABLE_TAGS)
    list(APPEND ZCBOR_HEADERS include/zcbor_tags.h)
endif()


if(ZCBOR_BUILD_SHARED)
    add_library(zcbor SHARED ${ZCBOR_SOURCES})
else()
    add_library(zcbor STATIC ${ZCBOR_SOURCES})
endif()

# Set include directories
set_target_properties(zcbor PROPERTIES PUBLIC_HEADER "${ZCBOR_HEADERS}")
target_include_directories(zcbor PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Set compile definitions based on options
if(ZCBOR_CANONICAL)
    target_compile_definitions(zcbor PUBLIC ZCBOR_CANONICAL)
endif()
if(ZCBOR_VERBOSE)
    target_compile_definitions(zcbor PUBLIC ZCBOR_VERBOSE)
endif()
if(ZCBOR_ASSERTS)
    target_compile_definitions(zcbor PUBLIC ZCBOR_ASSERTS)
endif()
if(ZCBOR_STOP_ON_ERROR)
    target_compile_definitions(zcbor PUBLIC ZCBOR_STOP_ON_ERROR)
endif()
if(ZCBOR_BIG_ENDIAN)
    target_compile_definitions(zcbor PUBLIC ZCBOR_BIG_ENDIAN)
endif()
if(ZCBOR_MAP_SMART_SEARCH)
    target_compile_definitions(zcbor PUBLIC ZCBOR_MAP_SMART_SEARCH)
endif()

add_subdirectory(samples/hello_world)
add_subdirectory(samples/pet)

# Install rules
install(TARGETS zcbor
    EXPORT zcborTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)
install(DIRECTORY include/ DESTINATION include)

# Export the package for find_package
install(EXPORT zcborTargets
    FILE zcborTargets.cmake
    NAMESPACE zcbor::
    DESTINATION lib/cmake/zcbor
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/zcborConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/zcborConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/zcborConfig.cmake"
    INSTALL_DESTINATION lib/cmake/zcbor
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/zcborConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/zcborConfigVersion.cmake"
    DESTINATION lib/cmake/zcbor
)
