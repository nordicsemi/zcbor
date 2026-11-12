/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MAIN_ENTRY_H__
#define MAIN_ENTRY_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

bool fuzz_one_input(const uint8_t *data, size_t size);

#endif /* MAIN_ENTRY_H__ */
