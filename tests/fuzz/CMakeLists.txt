#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.20)

project(zcbor_fuzz C)

set(TEST_CASE "pet" CACHE STRING
  "Test case (pet, manifest12, or everything)")
option(CANONICAL "Build with ZCBOR_CANONICAL" OFF)

set(CASES_DIR ${CMAKE_CURRENT_LIST_DIR}/../cases)
set(SEED_DIR ${PROJECT_BINARY_DIR}/fuzz_input)
file(REMOVE_RECURSE ${SEED_DIR})
file(MAKE_DIRECTORY ${SEED_DIR})

# Makes zcbor_assert()s fail with ZCBOR_ERR_ASSERTION, which the test cases report as crashes.
add_compile_definitions(ZCBOR_ASSERTS)
if (CANONICAL)
  add_compile_definitions(ZCBOR_CANONICAL)
endif()

add_executable(fuzz_target main_entry.c fuzz_common.c)
target_compile_options(fuzz_target PRIVATE -Wall -Werror)

# Generate code from CDDL, and link it into fuzz_target.
#   NAME:  Name of the generated library and files.
#   CDDL:  CDDL files.
#   ARGS:  Other arguments to "zcbor code", e.g. the entry types.
function(fuzz_code NAME)
  cmake_parse_arguments(ARG "" "" "CDDL;ARGS" ${ARGN})
  set(cddl_args)
  foreach(cddl ${ARG_CDDL})
    list(APPEND cddl_args -c ${cddl})
  endforeach()
  execute_process(
    COMMAND zcbor code ${cddl_args} ${ARG_ARGS} --output-cmake ${PROJECT_BINARY_DIR}/${NAME}.cmake
    COMMAND_ERROR_IS_FATAL ANY
    )
  include(${PROJECT_BINARY_DIR}/${NAME}.cmake)
  target_link_libraries(fuzz_target PRIVATE ${NAME})
endfunction()

# Convert an example to CBOR and place it in the seed corpus.
#   CDDL:  CDDL files.
#   ARGS:  Other arguments to "zcbor convert", e.g. the entry type and input file.
function(fuzz_seed NAME)
  cmake_parse_arguments(ARG "" "" "CDDL;ARGS" ${ARGN})
  set(cddl_args)
  foreach(cddl ${ARG_CDDL})
    list(APPEND cddl_args --cddl ${cddl})
  endforeach()
  execute_process(
    COMMAND zcbor convert ${cddl_args} ${ARG_ARGS}
      --output ${SEED_DIR}/${NAME}.cbor --output-as cbor
    COMMAND_ERROR_IS_FATAL ANY
    )
endfunction()

if (TEST_CASE STREQUAL pet)
  fuzz_code(pet CDDL ${CASES_DIR}/pet.cddl ARGS -t Pet -d -e)
  fuzz_seed(input0 CDDL ${CASES_DIR}/pet.cddl
    ARGS -t Pet --input ${CASES_DIR}/pet0.yaml --yaml-compatibility)
  target_sources(fuzz_target PRIVATE fuzz_pet.c)

elseif (TEST_CASE STREQUAL manifest12)
  # The --default-max-qty is needed for the examples to decode.
  fuzz_code(manifest12 CDDL ${CASES_DIR}/manifest12.cddl
    ARGS -t SUIT_Envelope_Tagged SUIT_Command_Sequence SUIT_Envelope -d --default-max-qty 16)
  foreach(n RANGE 0 5)
    fuzz_seed(input${n} CDDL ${CASES_DIR}/manifest12.cddl
      ARGS -t SUIT_Envelope --input ${CASES_DIR}/manifest12_example${n}.cborhex
        --default-max-qty 16)
  endforeach()
  target_sources(fuzz_target PRIVATE fuzz_manifest12.c)

elseif (TEST_CASE STREQUAL everything)
  # The --default-max-qty and 64 bit integers are needed for the examples to decode.
  fuzz_code(everything CDDL ${CASES_DIR}/everything.cddl
    ARGS -t EverythingList -d -e --default-max-qty 32 --default-bit-size 64)
  foreach(n RANGE 0 1)
    fuzz_seed(input${n} CDDL ${CASES_DIR}/everything.cddl
      ARGS -t EverythingList --input ${CASES_DIR}/everything_example${n}.yaml
        --yaml-compatibility --default-max-qty 32)
  endforeach()
  target_sources(fuzz_target PRIVATE fuzz_everything.c)

else()
  message(FATAL_ERROR "Invalid test case: ${TEST_CASE}")
endif()
