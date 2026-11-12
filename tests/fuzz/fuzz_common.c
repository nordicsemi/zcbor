/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Helpers for the test cases. Kept separate from main_entry.c so the test cases can also be built
 * into the regression tests in tests/decode/. */

#include "main_entry.h"
#include <stdio.h>
#include <stdlib.h>
#include <zcbor_print.h>


void fuzz_fail(const char *file, int line, const char *msg)
{
	fprintf(stderr, "%s:%d: Check failed: %s\n", file, line, msg);
	abort();
}


int fuzz_check_err(int err)
{
	switch (err) {
	case ZCBOR_ERR_BAD_STATE:
	case ZCBOR_ERR_ASSERTION:
	case ZCBOR_ERR_BACKUP_MISMATCH:
	case ZCBOR_ERR_UNKNOWN:
		/* These indicate a bug in zcbor, not invalid input. */
		fuzz_fail(__FILE__, __LINE__, zcbor_error_str(err));
		break;
	default:
		break;
	}
	return err;
}
