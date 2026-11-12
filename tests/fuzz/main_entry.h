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
#include <zcbor_common.h>

/** Implemented by each test case.
 *
 * @retval true   If the input was decoded successfully.
 * @retval false  If the input was rejected.
 */
bool fuzz_one_input(const uint8_t *data, size_t size);

/** Report a crash (abort) if @p err means that zcbor has a bug, as opposed to the input being
 *  invalid. Returns @p err so calls can be wrapped. */
int fuzz_check_err(int err);

/** Report a crash (abort) with a message. */
void fuzz_fail(const char *file, int line, const char *msg);

#endif /* MAIN_ENTRY_H__ */
