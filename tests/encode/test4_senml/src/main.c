/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include "senml_encode.h"

#ifndef ZCBOR_CANONICAL
#define TEST_INDEFINITE_LENGTH_ARRAYS
#endif
#include <common_test.h>

ZTEST(cbor_encode_test4, test_senml)
{
	struct lwm2m_senml input = {
		.lwm2m_senml_record_m[0] = {
			.record_bn = {.value = "Foo", .len = 3},
			.record_bn_present = 1,
			.record_bt = 42,
			.record_bt_present = 1,
			.record_n = {.value = "Bar", .len = 3},
			.record_n_present = 1,
			.record_t = 7,
			.record_t_present = 1,
			.record_union = {
				.union_vb = true,
				.record_union_choice = union_vb_c,
			},
			.record_union_present = 1,
			.record_key_value_pair_m_count = 0,
		},
		.lwm2m_senml_record_m_count = 1,
	};
	uint8_t payload[100];
	size_t encode_len;
	uint8_t exp_payload1[] = {
		LIST(1), MAP(5), 0x21, 0x63, 'F', 'o', 'o', 0x22, 0x18, 42,
		0x00, 0x63, 'B', 'a', 'r', 0x06, 0x07, 0x04, 0xF5, END END
	};

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_lwm2m_senml(payload, sizeof(payload), &input, &encode_len), NULL);
	zassert_equal(sizeof(exp_payload1), encode_len, NULL);
	zassert_mem_equal(payload, exp_payload1, encode_len, NULL);
}

ZTEST_SUITE(cbor_encode_test4, NULL, NULL, NULL, NULL, NULL);
