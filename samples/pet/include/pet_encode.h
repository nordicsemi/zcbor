/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Generated using zcbor version 1.0.0
 * https://github.com/nordicsemi/zcbor
 */

#ifndef PET_ENCODE_H__
#define PET_ENCODE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "pet_types.h"

#ifdef __cplusplus
extern "C" {
#endif


int cbor_encode_Pet(
		uint8_t *payload, size_t payload_len,
		const struct Pet *input,
		size_t *payload_len_out);


#ifdef __cplusplus
}
#endif

#endif /* PET_ENCODE_H__ */
