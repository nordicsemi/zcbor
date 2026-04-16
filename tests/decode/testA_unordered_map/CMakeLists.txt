#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test9_manifest14)
set(MAP_SEARCH_FLAGS ON)  # Because of --unordered maps
set(MAP_SMART_SEARCH ON)
include(../../cmake/test_template.cmake)

set(py_command
  ${PYTHON_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/../../../zcbor/zcbor.py
  code
  -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/unordered_map.cddl
  --output-cmake ${PROJECT_BINARY_DIR}/unordered_map.cmake
  -t
  UnorderedMap1
  UnorderedMap2
  UnorderedMap3
  UnorderedMap4
  UnorderedMap5
  UnorderedMap6
  UnorderedMap7
  UnorderedMap8
  -d
  ${bit_arg}
  --unordered-maps
  --defines
  )

execute_process(
  COMMAND
  ${py_command}
  COMMAND_ERROR_IS_FATAL ANY
)

include(${PROJECT_BINARY_DIR}/unordered_map.cmake)

target_link_libraries(unordered_map PRIVATE zephyr_interface)
target_link_libraries(app PRIVATE unordered_map)
