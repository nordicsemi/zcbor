#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.20)

project(zcbor_fuzz C)

set(TEST_CASE "pet" CACHE STRING
  "Test case (pet, manifest12, manifest14, everything, or fragments)")
option(CANONICAL "Build with ZCBOR_CANONICAL" OFF)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CASES_DIR ${CMAKE_CURRENT_LIST_DIR}/../cases)
set(ZCBOR_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
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

# Write a hex string to the seed corpus as binary.
function(fuzz_seed_hex NAME HEX)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c
      "import sys; open(sys.argv[1], 'wb').write(bytes.fromhex(sys.argv[2]))"
      ${SEED_DIR}/${NAME}.cbor ${HEX}
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

elseif (TEST_CASE STREQUAL manifest14)
  # Same arguments as tests/decode/test9_manifest14.
  set(manifest14_cddl ${CASES_DIR}/manifest14.cddl ${CASES_DIR}/cose.cddl)
  fuzz_code(manifest14 CDDL ${manifest14_cddl}
    ARGS -t SUIT_Envelope_Tagged SUIT_Envelope SUIT_Manifest SUIT_Command_Sequence
      SUIT_Common_Sequence -d --unordered-maps --default-max-qty 4)
  foreach(n RANGE 0 5)
    fuzz_seed(input${n} CDDL ${manifest14_cddl}
      ARGS -t SUIT_Envelope_Tagged --input ${CASES_DIR}/manifest14_example${n}.cborhex)
  endforeach()
  target_sources(fuzz_target PRIVATE fuzz_manifest14.c)

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

elseif (TEST_CASE STREQUAL fragments)
  # The fragmented string API is used directly, without generated code.
  target_sources(fuzz_target PRIVATE
    fuzz_fragments.c
    ${ZCBOR_DIR}/src/zcbor_common.c
    ${ZCBOR_DIR}/src/zcbor_decode.c
    ${ZCBOR_DIR}/src/zcbor_print.c
    )
  target_include_directories(fuzz_target PRIVATE ${ZCBOR_DIR}/include)
  target_compile_definitions(fuzz_target PRIVATE ZCBOR_FRAGMENTS)

  # The first byte is the configuration, see fuzz_fragments.c.
  # Sections of 8 bytes. A sequence of strings and other elements.
  string(CONCAT seed0
    07782a48656c6c6f2c20776f726c64212054686973206973206120667261676d656e74656420737472696e672e
    182a5828000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f2021222324252627
    824101616160)
  fuzz_seed_hex(input0 ${seed0})
  # Sections of 3 bytes, split after headers and strings. A bstr containing a CBOR sequence.
  fuzz_seed_hex(input1 e257182a6b48656c6c6f20576f726c6482f5f6440102030460)
  # Sections of 1 byte, split after headers. A sequence of short strings and a map.
  fuzz_seed_hex(input2 20436162636378797a40a10102)

else()
  message(FATAL_ERROR "Invalid test case: ${TEST_CASE}")
endif()
