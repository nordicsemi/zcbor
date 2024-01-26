/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Generated using zcbor version 0.8.3
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 3
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_encode.h"
#include "pet_encode.h"
#include "zcbor_print.h"

#if DEFAULT_MAX_QTY != 3
#error "The type file was generated with a different default_max_qty than this file"
#endif

static bool encode_Pet(zcbor_state_t *state, const struct Pet *input);


static bool encode_Pet(
		zcbor_state_t *state, const struct Pet *input)
{
	zcbor_log("%s\r\n", __func__);

	bool tmp_result = (((zcbor_list_start_encode(state, 3) && ((((zcbor_list_start_encode(state, 3) && ((zcbor_multi_encode_minmax(1, 3, &(*input).names_count, (zcbor_encoder_t *)zcbor_tstr_encode, state, (&(*input).names), sizeof(struct zcbor_string))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3)))
	&& (((((((*input).birthday.len >= 8)
	&& ((*input).birthday.len <= 8)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).birthday))))
	&& ((((*input).species_choice == Pet_species_cat_c) ? ((zcbor_uint32_put(state, (1))))
	: (((*input).species_choice == Pet_species_dog_c) ? ((zcbor_uint32_put(state, (2))))
	: (((*input).species_choice == Pet_species_other_c) ? ((zcbor_uint32_put(state, (3))))
	: false))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	if (!tmp_result) {
		zcbor_trace_file(state);
		zcbor_log("%s error: %s\r\n", __func__, zcbor_error_str(zcbor_peek_error(state)));
	} else {
		zcbor_log("%s success\r\n", __func__);
	}

	return tmp_result;
}



int cbor_encode_Pet(
		uint8_t *payload, size_t payload_len,
		const struct Pet *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_Pet, sizeof(states) / sizeof(zcbor_state_t), 1);
}
