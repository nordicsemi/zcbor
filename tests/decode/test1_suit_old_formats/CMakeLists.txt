#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test1_suit_old_formats)
include(../../cmake/test_template.cmake)

set(py_command3
  zcbor
  code
  -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/manifest-moran3.cddl
  --no-prelude # manifest-moran3.cddl contains types that collide with types in prelude.cddl.
  --default-max-qty MY_DEFAULT_MAX_QTY
  --output-cmake ${PROJECT_BINARY_DIR}/manifest-moran3.cmake
  -t OuterWrapper
  --decode
  ${bit_arg}
  )

set(py_command4
  zcbor
  code
  -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/manifest-moran4.cddl
  --no-prelude
  --default-max-qty MY_DEFAULT_MAX_QTY
  --output-cmake ${PROJECT_BINARY_DIR}/manifest-moran4.cmake
  -t SUIT_Outer_Wrapper
  --decode
  ${bit_arg}
  )

execute_process(
  COMMAND
  ${py_command3}
  COMMAND_ERROR_IS_FATAL ANY
)

execute_process(
  COMMAND
  ${py_command4}
  COMMAND_ERROR_IS_FATAL ANY
)

include(${PROJECT_BINARY_DIR}/manifest-moran3.cmake)
include(${PROJECT_BINARY_DIR}/manifest-moran4.cmake)

target_link_libraries(manifest-moran3 PRIVATE zephyr_interface)
target_link_libraries(manifest-moran4 PRIVATE zephyr_interface)
target_link_libraries(app PRIVATE manifest-moran3 manifest-moran4)

zephyr_compile_definitions(MY_DEFAULT_MAX_QTY=6)
