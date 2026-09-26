#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test4_boundary)
include(../../cmake/test_template.cmake)

FILE(GLOB zcbor_sources ../../../src/*.c)
add_library(zcbor)
target_sources(zcbor PRIVATE ${zcbor_sources})
target_link_libraries(zcbor PRIVATE zephyr_interface)

target_include_directories(app PRIVATE ../../../include)
target_include_directories(zcbor PRIVATE ../../../include)

target_link_libraries(app PRIVATE zcbor)

target_compile_options(zcbor PRIVATE -Wpedantic -Wconversion -Wall -Wextra -Wdouble-promotion)
