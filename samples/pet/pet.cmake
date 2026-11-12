#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#
# Generated using zcbor version 0.9.2
# https://github.com/NordicSemiconductor/zcbor
# Generated with a --default-max-qty of 3
#

add_library(pet)
target_sources(pet PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../src/zcbor_decode.c
    ${CMAKE_CURRENT_LIST_DIR}/../../src/zcbor_encode.c
    ${CMAKE_CURRENT_LIST_DIR}/../../src/zcbor_common.c
    ${CMAKE_CURRENT_LIST_DIR}/../../src/zcbor_print.c
    ${CMAKE_CURRENT_LIST_DIR}/src/pet_decode.c
    ${CMAKE_CURRENT_LIST_DIR}/src/pet_encode.c
    )
target_include_directories(pet PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/../../include
    ${CMAKE_CURRENT_LIST_DIR}/include
    )
