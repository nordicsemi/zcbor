#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test9_manifest14)
set(MAP_SEARCH_FLAGS ON)  # Because of --unordered maps
include(../../cmake/test_template.cmake)

if (NOT MANIFEST)
  set(MANIFEST manifest14)
endif()

set(py_command
  ${PYTHON_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/../../../zcbor/zcbor.py
  code
  -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/${MANIFEST}.cddl
  -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/cose.cddl
  --default-max-qty 4
  --output-cmake ${PROJECT_BINARY_DIR}/${MANIFEST}.cmake
  -t
  SUIT_Envelope_Tagged
  SUIT_Envelope
  SUIT_Manifest
  SUIT_Command_Sequence
  SUIT_Common_Sequence
  -d
  ${bit_arg}
  --unordered-maps
  )

execute_process(
  COMMAND
  ${py_command}
  COMMAND_ERROR_IS_FATAL ANY
)

set(py_command_convert
  ${PYTHON_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/../../../zcbor/zcbor.py
  convert
  -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/${MANIFEST}.cddl
  -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/cose.cddl
  --default-max-qty 4
  -t SUIT_Envelope_Tagged
  -i ${CMAKE_CURRENT_LIST_DIR}/../../cases/manifest14_example0.cborhex
  -o ${PROJECT_BINARY_DIR}/include/example0.h # The same dir as the generated header files from the above command.
  --c-code-var-name example0
  --c-code-columns 8
)

execute_process(
  COMMAND
  ${py_command_convert}
  COMMAND_ERROR_IS_FATAL ANY
)

include(${PROJECT_BINARY_DIR}/${MANIFEST}.cmake)

target_link_libraries(${MANIFEST} PRIVATE zephyr_interface)
target_link_libraries(app PRIVATE ${MANIFEST})

if (${MANIFEST} STREQUAL "manifest16")
  target_compile_definitions(app PRIVATE MANIFEST16)
endif()
