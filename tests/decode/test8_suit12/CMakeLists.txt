#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test8_suit12)
include(../../cmake/test_template.cmake)

set(py_command
  zcbor
  code
  -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/manifest12.cddl
  --default-max-qty 8 # Needed to decode manifest12_example5.
  --output-cmake ${PROJECT_BINARY_DIR}/manifest12.cmake
  -t
  SUIT_Envelope_Tagged
  SUIT_Command_Sequence
  SUIT_Envelope
  -d
  ${bit_arg}
  )

if (UNORDERED_MAPS)
  list(APPEND py_command --unordered-maps)
endif()

execute_process(
  COMMAND
  ${py_command}
  COMMAND_ERROR_IS_FATAL ANY
  )

# Convert the examples (untagged SUIT_Envelope) to C arrays.
foreach(n RANGE 5)
  execute_process(
    COMMAND
    zcbor
    convert
    -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/manifest12.cddl
    -t SUIT_Envelope
    -i ${CMAKE_CURRENT_LIST_DIR}/../../cases/manifest12_example${n}.cborhex
    -o ${PROJECT_BINARY_DIR}/include/manifest12_example${n}.h # The same dir as the generated header files from the above command.
    --c-code-var-name manifest12_example${n}
    --c-code-columns 8
    COMMAND_ERROR_IS_FATAL ANY
    )
endforeach()

target_sources(app PRIVATE
  ../../fuzz/fuzz_manifest12.c
  ../../fuzz/fuzz_common.c
  )

include(${PROJECT_BINARY_DIR}/manifest12.cmake)

target_link_libraries(manifest12 PRIVATE zephyr_interface)
target_link_libraries(app PRIVATE manifest12)

if (UNORDERED_MAPS)
  target_compile_definitions(app PRIVATE UNORDERED_MAPS)
endif()
