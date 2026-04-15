#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.13.3)
project (sample_hello_world)

set(zcbor_src ../../src)
add_executable(${PROJECT_NAME}
  src/main.c
  ${zcbor_src}/zcbor_encode.c
  ${zcbor_src}/zcbor_decode.c
  ${zcbor_src}/zcbor_common.c)
target_include_directories(${PROJECT_NAME} PRIVATE ../../include)
