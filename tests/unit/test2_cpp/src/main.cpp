/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include <stdio.h>
#include <pet_decode.h>

int main(void)
{
	uint8_t payload[100] = {0};
	int32_t five = 5;
	int64_t six = 6;
	uint32_t seven = 7;
	uint64_t eight = 8;
	size_t nine = 9;
	uint32_t tag_nine = 9;
	bool false_ = false;
	float eleven_six = 11.6;
	double thirteen_eight = 13.8;
	size_t one = 1;
	bool one_b = 1;
	struct zcbor_string dummy_string;

	ZCBOR_STATE_E(state_e, 3, payload, sizeof(payload), 0);
	ZCBOR_STATE_D(state_d, 3, payload, sizeof(payload), 30, 0);

	state_e->constant_state->stop_on_error = true;
	state_d->constant_state->stop_on_error = true;

	/* All succeed since the error has been popped. */
	zcbor_int32_put(state_e, 1);
	zcbor_int64_put(state_e, 2);
	zcbor_uint32_put(state_e, 3);
	zcbor_uint64_put(state_e, 4);
	zcbor_size_put(state_e, 10);
	zcbor_int32_encode(state_e, &five);
	zcbor_int64_encode(state_e, &six);
	zcbor_uint32_encode(state_e, &seven);
	zcbor_uint64_encode(state_e, &eight);
	zcbor_size_encode(state_e, &nine);
	zcbor_bstr_put_lit(state_e, "Hello");
	zcbor_tstr_put_lit(state_e, "World");
	zcbor_tag_put(state_e, 9);
	zcbor_tag_put(state_e, 10);
	zcbor_bool_put(state_e, true);
	zcbor_bool_encode(state_e, &false_);
	zcbor_float32_put(state_e, 10.5);
	zcbor_float32_encode(state_e, &eleven_six);
	zcbor_float64_put(state_e, 12.7);
	zcbor_float64_encode(state_e, &thirteen_eight);
	zcbor_nil_put(state_e, NULL);
	zcbor_undefined_put(state_e, NULL);
	zcbor_bstr_start_encode(state_e);
	zcbor_bstr_end_encode(state_e, NULL);
	zcbor_list_start_encode(state_e, 1);
	zcbor_map_start_encode(state_e, 0);
	zcbor_map_end_encode(state_e, 0);
	zcbor_list_end_encode(state_e, 1);
	zcbor_multi_encode(1, (zcbor_encoder_t *)zcbor_int32_put, state_e, (void*)14, 0);
	bool ret = zcbor_multi_encode_minmax(1, 1, &one, (zcbor_encoder_t *)zcbor_int32_put, state_e, (void*)15, 0);

	if (!ret) {
		printf("Encode error: %d\r\n", zcbor_peek_error(state_e));
		return 1;
	}

	/* All succeed since the error has been popped. */
	zcbor_int32_expect(state_d, 1);
	zcbor_int64_expect(state_d, 2);
	zcbor_uint32_expect(state_d, 3);
	zcbor_uint64_expect(state_d, 4);
	zcbor_size_expect(state_d, 10);
	zcbor_int32_decode(state_d, &five);
	zcbor_int64_decode(state_d, &six);
	zcbor_uint32_decode(state_d, &seven);
	zcbor_uint64_decode(state_d, &eight);
	zcbor_size_decode(state_d, &nine);
	zcbor_bstr_expect_lit(state_d, "Hello");
	zcbor_tstr_expect_lit(state_d, "World");
	zcbor_tag_decode(state_d, &tag_nine);
	zcbor_tag_expect(state_d, 10);
	zcbor_bool_expect(state_d, true);
	zcbor_bool_decode(state_d, &false_);
	zcbor_float32_expect(state_d, 10.5);
	zcbor_float32_decode(state_d, &eleven_six);
	zcbor_float64_expect(state_d, 12.7);
	zcbor_float64_decode(state_d, &thirteen_eight);
	zcbor_nil_expect(state_d, NULL);
	zcbor_undefined_expect(state_d, NULL);
	zcbor_bstr_start_decode(state_d, &dummy_string);
	zcbor_bstr_end_decode(state_d);
	zcbor_list_start_decode(state_d);
	zcbor_map_start_decode(state_d);
	zcbor_map_end_decode(state_d);
	zcbor_list_end_decode(state_d);
	zcbor_multi_decode(1, 1, &one, (zcbor_decoder_t *)zcbor_int32_expect, state_d, (void*)14, 0);
	ret = zcbor_present_decode(&one_b, (zcbor_decoder_t *)zcbor_int32_expect, state_d, (void*)15);

	if (!ret) {
		printf("Decode error: %d\r\n", zcbor_peek_error(state_d));
		return 1;
	}


	struct Pet pet;
	uint8_t input[] = {
		0x83, 0x82, 0x63, 0x66, 0x6f, 0x6f, 0x63, 0x62, 0x61, 0x72,
		0x48, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x02};
	int int_ret = cbor_decode_Pet(input, sizeof(input), &pet, NULL);

	if (int_ret != ZCBOR_SUCCESS) {
		printf("Decode error: %d\r\n", int_ret);
		return 1;
	}

	printf("Success!\r\n");

	return 0;
}
