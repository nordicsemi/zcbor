#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test3_corner_cases)
include(../../cmake/test_template.cmake)

if (TEST_DEFINES)
  set(defines_arg --defines)
endif()

set(py_command
  zcbor
  code
  -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/corner_cases.cddl
  --default-max-qty 6
  --output-cmake ${PROJECT_BINARY_DIR}/corner_cases.cmake
  --copy-sources
  ${defines_arg}
  -t
  NestedListMap NestedMapListMap
  Numbers
  Numbers2
  NumberMap
  TaggedUnion
  Strings
  Simple2
  Optional
  Union
  Map
  Level1
  Range
  ValueRange
  ValueRange2
  FloatRange
  SingleBstr
  SingleInt
  SingleInt2
  Unabstracted
  QuantityRange
  DoubleMap
  Floats
  Floats2
  CBORBstr
  MapLength
  UnionInt2
  Intmax1
  Intmax2
  Intmax4
  InvalidIdentifiers
  MapUnionPrimAlias
  EmptyContainer
  SingleElemList
  Choice1
  Choice2
  Choice3
  Choice4
  Choice5
  OptList
  UnionDefault
  CborBstrList
  -e
  ${bit_arg}
  --short-names
  )

execute_process(
  COMMAND ${py_command}
  COMMAND_ERROR_IS_FATAL ANY
  )

include(${PROJECT_BINARY_DIR}/corner_cases.cmake)

target_link_libraries(corner_cases PRIVATE zephyr_interface)
target_link_libraries(app PRIVATE corner_cases)
