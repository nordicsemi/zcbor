/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "main_entry.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#ifdef LIBFUZZER
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	(void) fuzz_one_input(data, size);
	return 0;
}
#else

#ifndef AFL_MAX_SIZE
	#define AFL_MAX_SIZE 8192
#endif

static uint8_t tmp_buffer[AFL_MAX_SIZE];

/* Read the input into tmp_buffer, then copy it into a heap buffer of exactly the input's size.
 * Reads past the end of the input then go outside the heap buffer, where ASan detects them,
 * instead of into the rest of tmp_buffer. */
static size_t afl_read(uint8_t **data)
{
	ssize_t size = read(0, tmp_buffer, AFL_MAX_SIZE);

	*data = NULL;
	if (size <= 0) {
		return 0;
	}
	*data = (uint8_t *) malloc(size);
	if (NULL == *data) {
		return 0;
	}
	memcpy(*data, tmp_buffer, size);
	return size;
}

#define AFL_READ_AND_EXECUTE                        \
	do {                                        \
		size = afl_read(&data);             \
		if (NULL == data || 0 == size) {    \
			return 0;                   \
		}                                   \
		ret = fuzz_one_input(data, size);   \
		free(data);                         \
	} while (0)

int main(void)
{
	size_t size;
	uint8_t *data = NULL;
	bool ret;

	#ifdef __AFL_LOOP
	while (__AFL_LOOP(1000)) {
		AFL_READ_AND_EXECUTE;
	}
	(void)ret;
	return 0;
	#else
		AFL_READ_AND_EXECUTE;
		return ret ? 0 : -1;
	#endif
}
#endif
