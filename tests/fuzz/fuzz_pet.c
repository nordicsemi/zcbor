/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pet_decode.h"
#include "main_entry.h"

bool fuzz_one_input(const uint8_t *data, size_t size)
{
	size_t payload_len_out = 0;
	struct Pet result;

	if (cbor_decode_Pet(data, size, &result, &payload_len_out) != ZCBOR_SUCCESS) {
		return false;
	}

	return true;
}
