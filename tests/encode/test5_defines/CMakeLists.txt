#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test5_defines)
include(../../cmake/test_template.cmake)

set(py_command
  ${PYTHON_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/../../../zcbor/zcbor.py
  code
  -c ${CMAKE_CURRENT_LIST_DIR}/../../cases/defines.cddl
  --output-cmake ${PROJECT_BINARY_DIR}/defines.cmake
  -t
  Values
  KeyValues
  UnionValues
  Ranges
  Bits
  Sizes
  Defaults
  Quantities
  DefaultQty
  Tags
  Nested
  Counts
  -e -d
  --defines
  ${bit_arg}
  )

# Change the generated #defines (including those used by union enums) to values that only work if
# the generated code (both encoding and decoding) actually uses them.
# The tests in src/main.c depend on these exact values.
set(new_values
  # VAL
  VALUES_UINT_VAL_VAL=7
  VALUES_NINT_VAL_VAL=-7
  "VALUES_TSTR_VAL_VAL=\"hello world\""
  "VALUES_BSTR_VAL_VAL=\"abcdef\""
  VALUES_FLOAT_VAL_VAL=2.5
  VALUES_BOOL_VAL_VAL=false
  KEYVALUES_UINT1UINT_KEY_VAL=100
  "KEYVALUES_KEY_KEY_VAL=\"other key\""
  # These VAL defines are also used to define the corresponding union enum values.
  UNIONVALUES_VALUE_UNION_UINT1_VAL=300
  UNIONVALUES_UNION_UINT2TSTR_KEY_VAL=70000
  UNIONVALUES_TAGGED_UNION_UINT2_VAL=7
  # MIN_VAL and MAX_VAL
  RANGES_UINT_RANGE_MIN_VAL=5
  RANGES_UINT_RANGE_MAX_VAL=200
  RANGES_INT_RANGE_MIN_VAL=-100
  RANGES_INT_RANGE_MAX_VAL=-5
  RANGES_FLOAT_RANGE_MIN_VAL=0.5
  RANGES_FLOAT_RANGE_MAX_VAL=3.5
  # MSK
  "BITS_MSK=(1 << flags_flag0_c) | (1 << flags_flag3_c) | (1 << 5)"
  # SIZE, MIN_SIZE, and MAX_SIZE
  SIZES_FIXED_BSTR_SIZE=5
  SIZES_RANGED_TSTR_MIN_SIZE=1
  SIZES_RANGED_TSTR_MAX_SIZE=8
  # DEFAULT_VAL
  DEFAULTS_UINT_DEFAULT_DEFAULT_VAL=7
  "DEFAULTS_TSTR_DEFAULT_DEFAULT_VAL=\"default\""
  # QTY, MIN_QTY, and MAX_QTY
  QUANTITIES_RANGE_QTY_UINT_MIN_QTY=1
  QUANTITIES_RANGE_QTY_UINT_MAX_QTY=10
  QUANTITIES_EXACT_QTY_UINT_QTY=4
  # The default max qty (--default-max-qty)
  ZCBOR_DEFINES_DEFAULT_MAX_QTY=30
  # TAG
  TAGS_TAGGED_UINT_TAG0=4321
  TAGS_TAGGED_TSTR_TAG0=100000
  # NUM_BACKUPS
  NESTED_NUM_BACKUPS=2
  # MAX_COUNT
  COUNTS_MAX_COUNT=300
  B_C_MAX_COUNT=300
  )

execute_process(
  COMMAND ${py_command}
  COMMAND_ERROR_IS_FATAL ANY
  )

execute_process(
  COMMAND
  ${PYTHON_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/modify_defines.py
  ${PROJECT_BINARY_DIR}/include/defines_types.h
  ${new_values}
  COMMAND_ERROR_IS_FATAL ANY
  )

include(${PROJECT_BINARY_DIR}/defines.cmake)

target_link_libraries(defines PRIVATE zephyr_interface)
target_link_libraries(app PRIVATE defines)

target_compile_options(defines PRIVATE -Wall -Wpedantic -Wdouble-promotion -Wshadow)
