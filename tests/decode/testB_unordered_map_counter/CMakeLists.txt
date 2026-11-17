#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

# Unordered maps without ZCBOR_MAP_SMART_SEARCH, i.e. with a count of the processed elements.
# Only types that don't need ZCBOR_MAP_SMART_SEARCH can be used here.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(testB_unordered_map_counter)
include(../../cmake/test_template.cmake)

set(py_command
  ${PYTHON_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/../../../zcbor/zcbor.py
  code
  -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/unordered_map.cddl
  --output-cmake ${PROJECT_BINARY_DIR}/unordered_map_counter.cmake
  -t
  UnorderedMap16
  -d
  ${bit_arg}
  --unordered-maps
  )

execute_process(
  COMMAND
  ${py_command}
  COMMAND_ERROR_IS_FATAL ANY
)

include(${PROJECT_BINARY_DIR}/unordered_map_counter.cmake)

target_link_libraries(unordered_map_counter PRIVATE zephyr_interface)
target_link_libraries(app PRIVATE unordered_map_counter)
