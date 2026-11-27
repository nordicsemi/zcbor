/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Generated using zcbor version 1.0.0
 * https://github.com/nordicsemi/zcbor
 */

#ifndef PET_DECODE_H__
#define PET_DECODE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "pet_types.h"

#ifdef __cplusplus
extern "C" {
#endif


int cbor_decode_Pet(
		const uint8_t *payload, size_t payload_len,
		struct Pet *result,
		size_t *payload_len_out);


#ifdef __cplusplus
}
#endif

#endif /* PET_DECODE_H__ */
