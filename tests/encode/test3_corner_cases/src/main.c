/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include "corner_cases_encode.h"

#ifndef ZCBOR_CANONICAL
#define TEST_INDEFINITE_LENGTH_ARRAYS
#endif
#include <common_test.h>
#include <zcbor_print.h>


ZTEST(cbor_encode_test3, test_numbers)
{
	const uint8_t exp_payload_numbers1[] = {
		LIST(A), // List start
			0x01, // 1
			0x21, // -2
			0x05, // 5
			0x19, 0x01, 0x00, // 256
			0x1A, 0x01, 0x02, 0x03, 0x04, // 0x01020304
			0x39, 0x13, 0x87, // -5000
			0x1A, 0xEE, 0x6B, 0x28, 0x00, // 4000000000
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -2^31
			0x00, // 0
			0xD9, 0xFF, 0xFF, 0x01, // 1
		END
	};

	struct Numbers numbers = {0};
	uint8_t output[100];
	size_t out_len;

	numbers.fourtoten = 3; // Invalid
	numbers.twobytes = 256;
	numbers.onetofourbytes = 0x01020304;
	numbers.minusfivektoplustwohundred = -5000;
	numbers.negint = -2147483648;
	numbers.posint = 0;
	numbers.tagged_int = 1;

	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_Numbers(output,
		sizeof(output), &numbers, &out_len), NULL);

	numbers.fourtoten = 11; // Invalid
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_Numbers(output,
		sizeof(output), &numbers, &out_len), NULL);

	numbers.fourtoten = 5; // Valid
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Numbers(output,
		sizeof(output), &numbers, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_numbers1), out_len, "%d != %d\r\n", sizeof(exp_payload_numbers1), out_len);
	zassert_mem_equal(exp_payload_numbers1, output, sizeof(exp_payload_numbers1), NULL);

	numbers.negint = 1; // Invalid
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_Numbers(output,
		sizeof(output), &numbers, &out_len), NULL);

	numbers.negint = -1; // Valid
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Numbers(output,
		sizeof(output), &numbers, &out_len), NULL);

	numbers.minusfivektoplustwohundred = -5001; // Invalid
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_Numbers(output,
		sizeof(output), &numbers, &out_len), NULL);

	numbers.minusfivektoplustwohundred = 201; // Invalid
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_Numbers(output,
		sizeof(output), &numbers, &out_len), NULL);

	numbers.minusfivektoplustwohundred = 200; // Valid
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Numbers(output,
		sizeof(output), &numbers, &out_len), NULL);
}

ZTEST(cbor_encode_test3, test_numbers2)
{
	const uint8_t exp_payload_numbers2[] = {
		LIST(8),
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000
			0x1B, 0x01, 2, 3, 4, 5, 6, 7, 8, // 0x0102030405060708
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x00, // 0
			0x3A, 0x80, 0x00, 0x00, 0x00, // -0x8000_0001
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000
			0xD9, 0x04, 0xD2, 0x03, // #6.1234(3)
		END
	};
	struct Numbers2 numbers2 = {
		.threebytes = 0x123456,
		.int32 =  -0x80000000L,
		.big_int = 0x0102030405060708,
		.big_uint = 0x1102030405060709,
		.big_uint2 = 0,
		.tagged_int = 3,
	};
	uint8_t output[100];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Numbers2(output,
		sizeof(output), &numbers2, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_numbers2), out_len, "%d != %d\r\n",
		sizeof(exp_payload_numbers2), out_len);
	zassert_mem_equal(exp_payload_numbers2, output, sizeof(exp_payload_numbers2), NULL);
}

ZTEST(cbor_encode_test3, test_tagged_union)
{
	size_t encode_len;
	const uint8_t exp_payload_tagged_union1[] = {0xD9, 0x10, 0xE1, 0xF5};
	const uint8_t exp_payload_tagged_union2[] = {0xD9, 0x09, 0x29, 0x10};

	uint8_t output[5];

	struct TaggedUnion input;
	input.TaggedUnion_choice = TaggedUnion_t4321bool_c;
	input.t4321bool = true;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_TaggedUnion(output,
		sizeof(output), &input, &encode_len));

	zassert_equal(sizeof(exp_payload_tagged_union1), encode_len, NULL);
	zassert_mem_equal(exp_payload_tagged_union1, output, sizeof(exp_payload_tagged_union1), NULL);

	input.TaggedUnion_choice = TaggedUnion_t2345uint_c;
	input.t2345uint = 0x10;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_TaggedUnion(output,
		sizeof(output), &input, &encode_len), NULL);

	zassert_equal(sizeof(exp_payload_tagged_union2), encode_len, NULL);
	zassert_mem_equal(exp_payload_tagged_union2, output, sizeof(exp_payload_tagged_union2), NULL);
}

/** Test that nested tags are encoded in order, outermost first. */
ZTEST(cbor_encode_test3, test_nested_tags)
{
	const uint8_t exp_payload_nested_tags1[] = {LIST(8),
		0xC6, 0xC7, 0x01, /* nt_direct: #6.6(#6.7(1)) */
		0xC6, 0xC7, 0x02, /* nt_ref: #6.6(#6.7(2)) */
		0xC6, 0xC7, 0x03, /* nt_alias: #6.6(#6.7(3)) */
		0xCA, 0xC6, 0xC7, 0x04, /* nt_triple: #6.10(#6.6(#6.7(4))) */
		0xC6, 0xC7, 0x05, /* nt_union: #6.6(#6.7(5)) */
		0x43, 0xC6, 0xC7, 0x06, /* nt_cbor: << #6.6(#6.7(6)) >> */
		0xC6, 0xC7, 0x07, /* nt_opt: #6.6(#6.7(7)) */
		MAP(1), 0xC6, 0xC7, 0x01, 0xC6, 0xC7, 0x08, END /* nt_map: {#6.6(#6.7(1)): #6.6(#6.7(8))} */
		END
	};
	const uint8_t exp_payload_nested_tags2[] = {LIST(7),
		0xC6, 0xC7, 0x01, /* nt_direct: #6.6(#6.7(1)) */
		0xC6, 0xC7, 0x02, /* nt_ref: #6.6(#6.7(2)) */
		0xC6, 0xC7, 0x03, /* nt_alias: #6.6(#6.7(3)) */
		0xCA, 0xC6, 0xC7, 0x04, /* nt_triple: #6.10(#6.6(#6.7(4))) */
		0xC8, 0xC9, 0x61, 0x61, /* nt_union: #6.8(#6.9("a")) */
		0x43, 0xC6, 0xC7, 0x06, /* nt_cbor: << #6.6(#6.7(6)) >> */
		MAP(1), 0xC6, 0xC7, 0x01, 0xC6, 0xC7, 0x08, END /* nt_map: {#6.6(#6.7(1)): #6.6(#6.7(8))} */
		END
	};
	struct NestedTags input = {
		.nt_direct = 1,
		.nt_ref = 2,
		.nt_alias = 3,
		.nt_triple = 4,
		.nt_union_choice = NestedTags_nt_union_nt_union_uint_c,
		.nt_union_uint = 5,
		.nt_cbor_cbor = 6,
		.nt_opt_present = true,
		.nt_opt = 7,
		.NestedTagsKey = 8,
	};
	uint8_t output[sizeof(exp_payload_nested_tags1) + 1];
	size_t encode_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedTags(output,
		sizeof(output), &input, &encode_len), NULL);
	zassert_equal(sizeof(exp_payload_nested_tags1), encode_len, NULL);
	zassert_mem_equal(exp_payload_nested_tags1, output, encode_len, NULL);

	input.nt_union_choice = NestedTags_nt_union_nt_union_tstr_c;
	input.nt_union_tstr.value = (const uint8_t *)"a";
	input.nt_union_tstr.len = 1;
	input.nt_opt_present = false;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedTags(output,
		sizeof(output), &input, &encode_len), NULL);
	zassert_equal(sizeof(exp_payload_nested_tags2), encode_len, NULL);
	zassert_mem_equal(exp_payload_nested_tags2, output, encode_len, NULL);
}

ZTEST(cbor_encode_test3, test_number_map)
{
	size_t encode_len = 0xFFFFFFFF;
	const uint8_t exp_payload_number_map1[] = {
		MAP(3),
			0x64, 'b', 'y', 't', 'e',
			0x18, 42,
			0x69, 'o', 'p', 't', '_', 's', 'h', 'o', 'r', 't',
			0x19, 0x12, 0x34,
			0x68, 'o', 'p', 't', '_', 'c', 'b', 'o', 'r',
			0x45, 0x1A, 0x12, 0x34, 0x56, 0x78,
		END
	};

	const uint8_t exp_payload_number_map2[] = {
		MAP(2),
			0x64, 'b', 'y', 't', 'e',
			0x18, 42,
			0x68, 'o', 'p', 't', '_', 'c', 'b', 'o', 'r',
			0x45, 0x1A, 0x12, 0x34, 0x56, 0x78,
		END
	};

	uint8_t payload[80];

	struct NumberMap number_map1 = {
		.byte = 42,
		.opt_short_present = true,
		.opt_short = 0x1234,
		.opt_cbor_present = true,
		.opt_cbor.opt_cbor_cbor = 0x12345678,
	};

	int res = cbor_encode_NumberMap(payload,
		sizeof(payload), &number_map1, &encode_len);
	zassert_equal(ZCBOR_SUCCESS, res, "%d\r\n", res);
	zassert_equal(encode_len, sizeof(exp_payload_number_map1), NULL);
	zassert_mem_equal(payload, exp_payload_number_map1, encode_len, NULL);

	struct NumberMap number_map2 = {
		.byte = 42,
		.opt_short_present = false,
		.opt_cbor_present = true,
		.opt_cbor.opt_cbor_cbor = 0x12345678,
	};

	res = cbor_encode_NumberMap(payload,
		sizeof(payload), &number_map2, &encode_len);
	zassert_equal(ZCBOR_SUCCESS, res, "%d\r\n", res);
	zassert_equal(encode_len, sizeof(exp_payload_number_map2), NULL);
	zassert_mem_equal(payload, exp_payload_number_map2, encode_len, NULL);

	struct NumberMap number_map3 = {
		.byte = 42,
		.opt_short_present = false,
		.opt_cbor_present = true,
		.opt_cbor.opt_cbor.value = (uint8_t[]){0x1A, 0x12, 0x34, 0x56, 0x78},
		.opt_cbor.opt_cbor.len = 5,
	};

	res = cbor_encode_NumberMap(payload,
		sizeof(payload), &number_map3, &encode_len);
	zassert_equal(ZCBOR_SUCCESS, res, "%d\r\n", res);
	zassert_equal(encode_len, sizeof(exp_payload_number_map2), NULL);
	zassert_mem_equal(payload, exp_payload_number_map2, encode_len, NULL);

	struct NumberMap number_map4_inv = {
		.byte = 42,
		.opt_short_present = false,
		.opt_cbor_present = true,
		.opt_cbor.opt_cbor.value = (uint8_t[]){0x19, 0x12, 0x34},
		.opt_cbor.opt_cbor.len = 3,
	};

	res = cbor_encode_NumberMap(payload,
		sizeof(payload), &number_map4_inv, &encode_len);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, res, "%d\r\n", res);
}


ZTEST(cbor_encode_test3, test_strings)
{
	const uint8_t exp_payload_strings1[] = {
		LIST(6),
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x59, 0x01, 0x2c, // 300 bytes
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0xC0, 0x78, 0x1E, // 30 bytes
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
#ifndef ZCBOR_CANONICAL
		0x58, 30, // Numbers (len: 30)
#else
		0x58, 29, // Numbers (len: 29)
#endif
			LIST(A), // List start
				0x01, // 1
				0x21, // -2
				0x05, // 5
				0x19, 0xFF, 0xFF, // 0xFFFF
				0x18, 0x18, // 24
				0x00, // 0
				0x1A, 0xEE, 0x6B, 0x28, 0x00, // 4000000000
				0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -2^31
				0x1A, 0xFF, 0xFF, 0xFF, 0xFF, // 0xFFFFFFFF
				0xD9, 0xFF, 0xFF, 9, // 9
				END
#ifndef ZCBOR_CANONICAL
		0x55, // Simples (len: 21)
#else
		0x52, // Simples (len: 18)
#endif
			LIST(5), // List start
				0xF5, // True
				0xF4, // False
				0xF4, // False
				0xF6, // Nil
				0xF7, // Undefined
				END
			LIST(5), // List start
				0xF5, // True
				0xF4, // False
				0xF5, // True
				0xF6, // Nil
				0xF7, // Undefined
				END
			LIST(5), // List start
				0xF5, // True
				0xF4, // False
				0xF4, // False
				0xF6, // Nil
				0xF7, // Undefined
				END
#ifndef ZCBOR_CANONICAL
		0x59, 0x01, 0x6B, // Strings (len: 363)
#else
		0x59, 0x01, 0x68, // Strings (len: 360)
#endif
			LIST(5),
			0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
			0x59, 0x01, 0x2c, // 300 bytes
			0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
			0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
			0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
			0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
			0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
			0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
			0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
			0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
			0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
			0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
			0xC0, 0x6A, // 10 bytes
			0,1,2,3,4,5,6,7,8,9,
#ifndef ZCBOR_CANONICAL
			0x58, 30, // Numbers (len: 30)
#else
			0x58, 29, // Numbers (len: 29)
#endif
				LIST(A), // List start
					0x01, // 1
					0x21, // -2
					0x05, // 5
					0x19, 0xFF, 0xFF, // 0xFFFF
					0x18, 0x18, // 24
					0x00, // 0
					0x1A, 0xEE, 0x6B, 0x28, 0x00, // 4000000000
					0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -2^31
					0x1A, 0xFF, 0xFF, 0xFF, 0xFF, // 0xFFFFFFFF
					0xD9, 0xFF, 0xFF, 0x29, // -10
					END
#ifndef ZCBOR_CANONICAL
			0x47, // Simples (len: 7)
#else
			0x46, // Simples (len: 6)
#endif
				LIST(5), // List start
					0xF5, // True
					0xF4, // False
					0xF4, // False
					0xF6, // Nil
					0xF7, // Undefined
					END
			END
		END
	};

	const uint8_t bytes300[] = {
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
		0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,
	};

	struct Strings strings1 = {0};
	struct Strings strings2 = {0};
	struct Numbers numbers1 = {0};
	uint8_t output1[100];
	uint8_t output2[100];
	uint8_t output3[400];
	uint8_t output4[800];
	size_t out_len;

	numbers1.fourtoten = 5;
	numbers1.twobytes = 0xFFFF;
	numbers1.onetofourbytes = 24;
	numbers1.minusfivektoplustwohundred = 0;
	numbers1.negint = -2147483648;
	numbers1.posint = 0xFFFFFFFF;
	numbers1.tagged_int = 9;

	strings1.optCborStrings_present = true;
	strings1.threehundrebytebstr.len = 300;
	strings1.threehundrebytebstr.value = bytes300;
	strings1.tentothirtybytetstr.len = 30;
	strings1.tentothirtybytetstr.value = bytes300;
	strings1.cborseqSimples_cbor_count = 3;
	strings1.cborseqSimples_cbor[0].boolval = false;
	strings1.cborseqSimples_cbor[1].boolval = true;
	strings1.cborseqSimples_cbor[2].boolval = false;

	strings2.threehundrebytebstr.len = 300;
	strings2.threehundrebytebstr.value = bytes300;
	strings2.tentothirtybytetstr.len = 9; // Invalid
	strings2.tentothirtybytetstr.value = bytes300;
	strings2.cborseqSimples_cbor_count = 1;
	strings2.cborseqSimples_cbor[0].boolval = false;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Numbers(output2, sizeof(output2), &numbers1, &out_len), NULL);
	strings1.cborNumbers.value = output2;
	strings1.cborNumbers.len = out_len;
	numbers1.tagged_int = -10;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Numbers(output1, sizeof(output1), &numbers1, &out_len), NULL);
	strings2.cborNumbers.value = output1;
	strings2.cborNumbers.len = out_len;
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_Strings(output3, sizeof(output3), &strings2, &out_len), NULL);
	strings2.tentothirtybytetstr.len = 31; // Invalid
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_Strings(output3, sizeof(output3), &strings2, &out_len), NULL);
	strings2.tentothirtybytetstr.len = 10; // Valid
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Strings(output3, sizeof(output3), &strings2, &out_len), NULL);
	strings1.optCborStrings.value = output3;
	strings1.optCborStrings.len = out_len;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Strings(output4, sizeof(output4), &strings1, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_strings1), out_len, "expected: %d, actual: %d\r\n", sizeof(exp_payload_strings1), out_len);

	zassert_mem_equal(exp_payload_strings1, output4, sizeof(exp_payload_strings1), NULL);
}

ZTEST(cbor_encode_test3, test_simples)
{
	uint8_t exp_payload_simple1[] = {LIST(5), 0xF5, 0xF4, 0xF4, 0xF6, 0xF7, END};
	uint8_t exp_payload_simple2[] = {LIST(5), 0xF5, 0xF4, 0xF5, 0xF6, 0xF7, END};

	struct Simples input;
	size_t len_encode;
	uint8_t output[10];

	input.boolval = false;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Simple2(output, sizeof(output), &input, &len_encode), NULL);
	zassert_equal(len_encode, sizeof(exp_payload_simple1), NULL);
	zassert_mem_equal(exp_payload_simple1, output, sizeof(exp_payload_simple1), NULL);

	input.boolval = true;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Simple2(output, sizeof(output), &input, &len_encode), NULL);
	zassert_equal(len_encode, sizeof(exp_payload_simple2), NULL);
	zassert_mem_equal(exp_payload_simple2, output, sizeof(exp_payload_simple2), NULL);
}

ZTEST(cbor_encode_test3, test_optional)
{
	const uint8_t exp_payload_optional1[] = {
		LIST(3) /* List start */, 0xCA /* tag */, 0xF4 /* False */, 0x02, 0x03, END
	};
	const uint8_t exp_payload_optional2[] = {
		LIST(2) /* List start */, 0xCA /* tag */, 0xF4 /* False */, 0x03, END
	};
	const uint8_t exp_payload_optional3[] = {
		LIST(3) /* List start */, 0xCA /* tag */, 0xF4 /* False */, 0x02, 0x01, END
	};
	const uint8_t exp_payload_optional4[] = {
		LIST(3) /* List start */, 0xCA /* tag */, 0xF5 /* True */, 0x02, 0x02, END
	};
	const uint8_t exp_payload_optional5[] = {
		LIST(4) /* List start */, 0xCA /* tag */, 0xF5 /* True */, 0xF4 /* False */, 0x02, 0x02, END
	};
	const uint8_t exp_payload_optional6[] = {
		LIST(5) /* List start */, 0xCA /* tag */, 0xF5 /* True */, 0xF4 /* False */, 0x02, 0x02, 0x08, END
	};
	const uint8_t exp_payload_optional7[] = {
		LIST(7) /* List start */, 0xCA /* tag */, 0xF5 /* True */, 0xF4 /* False */, 0x02, 0x02, 0x08, 0x08, 0x08, END
	};

	struct Optional optional1 = {.opttwo_present = true, .manduint = 3};
	struct Optional optional2 = {.manduint = 3};
	struct Optional optional3 = {.opttwo_present = true, .manduint = 1};
	struct Optional optional4 = {.boolval = true, .opttwo_present = true,
				.manduint = 2};
	struct Optional optional5 = {.boolval = true, .optbool_present = true,
				.opttwo_present = true, .manduint = 2};
	struct Optional optional6 = {.boolval = true, .optbool_present = true,
				.opttwo_present = true, .manduint = 2,
				.multi8_count = 1};
	struct Optional optional7 = {.boolval = true, .optbool_present = true,
				.opttwo_present = true, .manduint = 2,
				.multi8_count = 3};
	uint8_t output[20];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Optional(output,
			sizeof(output), &optional1, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_optional1), out_len, NULL);
	zassert_mem_equal(exp_payload_optional1, output, sizeof(exp_payload_optional1), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Optional(output,
			sizeof(output), &optional2, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_optional2), out_len, NULL);
	zassert_mem_equal(exp_payload_optional2, output, sizeof(exp_payload_optional2), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Optional(output,
			sizeof(output), &optional3, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_optional3), out_len, NULL);
	zassert_mem_equal(exp_payload_optional3, output, sizeof(exp_payload_optional3), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Optional(output,
			sizeof(output), &optional4, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_optional4), out_len, NULL);
	zassert_mem_equal(exp_payload_optional4, output, sizeof(exp_payload_optional4), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Optional(output,
			sizeof(output), &optional5, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_optional5), out_len, NULL);
	zassert_mem_equal(exp_payload_optional5, output, sizeof(exp_payload_optional5), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Optional(output,
			sizeof(output), &optional6, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_optional6), out_len, NULL);
	zassert_mem_equal(exp_payload_optional6, output, sizeof(exp_payload_optional6), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Optional(output,
			sizeof(output), &optional7, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_optional7), out_len, NULL);
	zassert_mem_equal(exp_payload_optional7, output, sizeof(exp_payload_optional7), NULL);
}

ZTEST(cbor_encode_test3, test_union)
{
	const uint8_t exp_payload_union1[] = {0x01, 0x21};
	const uint8_t exp_payload_union2[] = {0x03, 0x23};
	const uint8_t exp_payload_union3[] = {0x03, 0x04};
	const uint8_t exp_payload_union4[] = {
		0x67, 0x22, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x22 // "\"hello\""
	};
	const uint8_t exp_payload_union5[] = {
		0x03, 0x23, 0x03, 0x23, 0x03, 0x23,
		0x03, 0x23, 0x03, 0x23, 0x03, 0x23
	};

	struct Union union1 = {.Union_choice = Union_Group_m_c};
	struct Union union2 = {.Union_choice = Union_MultiGroup_m_c, .MultiGroup_m.MultiGroup_count = 1};
	struct Union union3 = {.Union_choice = Union_uint3_l_c};
	struct Union union4 = {.Union_choice = Union_hello_tstr_c};
	struct Union union5 = {.Union_choice = Union_MultiGroup_m_c, .MultiGroup_m.MultiGroup_count = 6};
	struct Union union6_inv = {.Union_choice = Union_MultiGroup_m_c, .MultiGroup_m.MultiGroup_count = 7};

	uint8_t output[15];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Union(output, sizeof(output),
				&union1, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_union1), out_len, NULL);
	zassert_mem_equal(exp_payload_union1, output, sizeof(exp_payload_union1), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Union(output, sizeof(output),
				&union2, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_union2), out_len, NULL);
	zassert_mem_equal(exp_payload_union2, output, sizeof(exp_payload_union2), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Union(output, sizeof(output),
				&union3, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_union3), out_len, NULL);
	zassert_mem_equal(exp_payload_union3, output, sizeof(exp_payload_union3), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Union(output, sizeof(output),
				&union4, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_union4), out_len, NULL);
	zassert_mem_equal(exp_payload_union4, output, sizeof(exp_payload_union4), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Union(output, sizeof(output),
				&union5, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_union5), out_len, NULL);
	zassert_mem_equal(exp_payload_union5, output, sizeof(exp_payload_union5), NULL);

	zassert_equal(ZCBOR_ERR_ITERATIONS, cbor_encode_Union(output, sizeof(output),
				&union6_inv, &out_len), NULL);
}

ZTEST(cbor_encode_test3, test_levels)
{
	const uint8_t exp_payload_levels1[] = {
		LIST(1), // Level1
		LIST(2), // Level2
		LIST(4), // Level3 no 1
		LIST(1), 0x00, END // Level4 no 1
		LIST(1), 0x00, END // Level4 no 2
		LIST(1), 0x00, END // Level4 no 3
		LIST(1), 0x00, END // Level4 no 4
		END
		LIST(4), // Level3 no 2
		LIST(1), 0x00, END // Level4 no 1
		LIST(1), 0x00, END // Level4 no 2
		LIST(1), 0x00, END // Level4 no 3
		LIST(1), 0x00, END // Level4 no 4
		END END END
	};
	uint8_t output[32];
	size_t out_len;

	struct Level2 level1 = {.Level3_m_count = 2, .Level3_m = {
		{.Level4_m_count = 4}, {.Level4_m_count = 4}
	}};
	_Static_assert(sizeof(exp_payload_levels1) <= sizeof(output),
		"Payload is larger than output");
	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, cbor_encode_Level1(output,
		sizeof(exp_payload_levels1)-1, &level1, &out_len), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Level1(output,
		sizeof(output), &level1, &out_len), NULL);

	zassert_equal(sizeof(exp_payload_levels1), out_len, "%d != %d", sizeof(exp_payload_levels1), out_len);
	zassert_mem_equal(exp_payload_levels1, output, sizeof(exp_payload_levels1), NULL);
}


ZTEST(cbor_encode_test3, test_map)
{
	const uint8_t exp_payload_map1[] = {
		MAP(4), LIST(2), 0x05, 0x06, END 0xF4, // [5,6] => false
		0x07, 0x01, // 7 => 1
		0xf6, 0x45, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // nil => "hello"
		0xf6, 0x40, // nil => ""
		END
	};
	const uint8_t exp_payload_map2[] = {
		MAP(5), LIST(2), 0x05, 0x06, END 0xF5, // [5,6] => true
		0x07, 0x01, // 7 => 1
		0xf6, 0x45, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // nil => "hello"
		0xf6, 0x40, // nil => ""
		0xf6, 0x40, // nil => ""
		END
	};
	const uint8_t exp_payload_map3[] = {
		MAP(4), LIST(2), 0x05, 0x06, END 0xF4, // [5,6] => false
		0x27, 0x01, // -8 => 1
		0xf6, 0x45, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // nil => "hello"
		0xf6, 0x40, // nil => ""
		END
	};

	struct Map map1 = {
		.Union_choice = union_uint7uint_c,
		.uint7uint = 1,
		.twotothree_count = 2,
		.twotothree = {
			{.value = "hello", .len = 5},
			{.len = 0},
		}
	};
	struct Map map2 = {
		.listkey = true,
		.Union_choice = union_uint7uint_c,
		.uint7uint = 1,
		.twotothree_count = 3,
		.twotothree = {
			{.value = "hello", .len = 5},
			{.len = 0},
			{.len = 0},
		}
	};
	struct Map map3 = {
		.Union_choice = union_nint8uint_c,
		.nint8uint = 1,
		.twotothree_count = 2,
		.twotothree = {
			{.value = "hello", .len = 5},
			{.len = 0},
		}
	};

	uint8_t output[25];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Map(output, sizeof(output),
			&map1, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_map1), out_len, NULL);
	zassert_mem_equal(exp_payload_map1, output, sizeof(exp_payload_map1), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Map(output, sizeof(output),
			&map2, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_map2), out_len, NULL);
	zassert_mem_equal(exp_payload_map2, output, sizeof(exp_payload_map2), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Map(output, sizeof(output),
			&map3, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_map3), out_len, NULL);
	zassert_mem_equal(exp_payload_map3, output, sizeof(exp_payload_map3), NULL);
}

ZTEST(cbor_encode_test3, test_nested_list_map)
{
	const uint8_t exp_payload_nested_lm1[] = {LIST(0), END};
	const uint8_t exp_payload_nested_lm2[] = {LIST(1), MAP(0), END END};
	const uint8_t exp_payload_nested_lm3[] = {LIST(1), MAP(1), 0x01, 0x04, END END};
	const uint8_t exp_payload_nested_lm4[] = {LIST(2), MAP(0), END MAP(1), 0x01, 0x04, END END};
	const uint8_t exp_payload_nested_lm5[] = {LIST(3), MAP(0), END MAP(0), END MAP(0), END END};
	struct NestedListMap listmap1 = {
		.map_count = 0,
	};
	struct NestedListMap listmap2 = {
		.map_count = 1,
		.map = {
			{.uint4_present = false},
		}
	};
	struct NestedListMap listmap3 = {
		.map_count = 1,
		.map = {
			{.uint4_present = true},
		}
	};
	struct NestedListMap listmap4 = {
		.map_count = 2,
		.map = {
			{.uint4_present = false},
			{.uint4_present = true},
		}
	};
	struct NestedListMap listmap5 = {
		.map_count = 3,
		.map = {
			{.uint4_present = false},
			{.uint4_present = false},
			{.uint4_present = false},
		}
	};
	uint8_t output[40];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedListMap(output,
			sizeof(output), &listmap1, &out_len), NULL);

	zassert_equal(sizeof(exp_payload_nested_lm1), out_len, NULL);
	zassert_mem_equal(exp_payload_nested_lm1, output, sizeof(exp_payload_nested_lm1), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedListMap(output,
			sizeof(output), &listmap2, &out_len), NULL);

	zassert_equal(sizeof(exp_payload_nested_lm2), out_len, NULL);
	zassert_mem_equal(exp_payload_nested_lm2, output, sizeof(exp_payload_nested_lm2), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedListMap(output,
			sizeof(output), &listmap3, &out_len), NULL);

	zassert_equal(sizeof(exp_payload_nested_lm3), out_len, NULL);
	zassert_mem_equal(exp_payload_nested_lm3, output, sizeof(exp_payload_nested_lm3), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedListMap(output,
			sizeof(output), &listmap4, &out_len), NULL);

	zassert_equal(sizeof(exp_payload_nested_lm4), out_len, NULL);
	zassert_mem_equal(exp_payload_nested_lm4, output, sizeof(exp_payload_nested_lm4), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedListMap(output,
			sizeof(output), &listmap5, &out_len), NULL);

	zassert_equal(sizeof(exp_payload_nested_lm5), out_len, "%d != %d", sizeof(exp_payload_nested_lm5), out_len);
	zassert_mem_equal(exp_payload_nested_lm5, output, sizeof(exp_payload_nested_lm5), NULL);
}

ZTEST(cbor_encode_test3, test_nested_map_list_map)
{
	const uint8_t exp_payload_nested_mlm1[] = {MAP(1), LIST(0), END LIST(0), END END};
	const uint8_t exp_payload_nested_mlm2[] = {MAP(1), LIST(0), END LIST(1), MAP(0), END END END};
	const uint8_t exp_payload_nested_mlm3[] = {MAP(1), LIST(0), END LIST(2), MAP(0), END MAP(0), END END END};
	const uint8_t exp_payload_nested_mlm4[] = {MAP(2), LIST(0), END LIST(0), END LIST(0), END LIST(0), END END};
	const uint8_t exp_payload_nested_mlm5[] = {MAP(3), LIST(0), END LIST(0), END LIST(0), END LIST(0), END LIST(0), END LIST(2), MAP(0), END MAP(0), END END END};
	struct NestedMapListMap maplistmap1 = {
		.map_l_count = 1,
		.map_l = {{0}}
	};
	struct NestedMapListMap maplistmap2 = {
		.map_l_count = 1,
		.map_l = {
			{.map_count = 1}
		}
	};
	struct NestedMapListMap maplistmap3 = {
		.map_l_count = 1,
		.map_l = {
			{.map_count = 2}
		}
	};
	struct NestedMapListMap maplistmap4 = {
		.map_l_count = 2,
		.map_l = {
			{.map_count = 0},
			{.map_count = 0},
		}
	};
	struct NestedMapListMap maplistmap5 = {
		.map_l_count = 3,
		.map_l = {
			{.map_count = 0},
			{.map_count = 0},
			{.map_count = 2},
		}
	};
	uint8_t output[30];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedMapListMap(output,
			sizeof(output), &maplistmap1, &out_len), NULL);

	zassert_equal(sizeof(exp_payload_nested_mlm1), out_len, NULL);
	zassert_mem_equal(exp_payload_nested_mlm1, output, sizeof(exp_payload_nested_mlm1), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedMapListMap(output,
			sizeof(output), &maplistmap2, &out_len), NULL);

	zassert_equal(sizeof(exp_payload_nested_mlm2), out_len, "%d != %d", sizeof(exp_payload_nested_mlm2), out_len);
	zassert_mem_equal(exp_payload_nested_mlm2, output, sizeof(exp_payload_nested_mlm2), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedMapListMap(output,
			sizeof(output), &maplistmap3, &out_len), NULL);

	zassert_equal(sizeof(exp_payload_nested_mlm3), out_len, "%d != %d", sizeof(exp_payload_nested_mlm3), out_len);
	zassert_mem_equal(exp_payload_nested_mlm3, output, sizeof(exp_payload_nested_mlm3), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedMapListMap(output,
			sizeof(output), &maplistmap4, &out_len), NULL);

	zassert_equal(sizeof(exp_payload_nested_mlm4), out_len, NULL);
	zassert_mem_equal(exp_payload_nested_mlm4, output, sizeof(exp_payload_nested_mlm4), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedMapListMap(output,
			sizeof(output), &maplistmap5, &out_len), NULL);

	zassert_equal(sizeof(exp_payload_nested_mlm5), out_len, "%d != %d", sizeof(exp_payload_nested_mlm5), out_len);
	zassert_mem_equal(exp_payload_nested_mlm5, output, sizeof(exp_payload_nested_mlm5), NULL);

	for (size_t i = 0; i < sizeof(exp_payload_nested_mlm5); i++) {
		int err = cbor_encode_NestedMapListMap(output, i, &maplistmap5, &out_len);

		zassert_equal(ZCBOR_ERR_NO_PAYLOAD, err, "len %zu: %s\n", i, zcbor_error_str(err));
	}
}


ZTEST(cbor_encode_test3, test_range)
{
	const uint8_t exp_payload_range1[] = {LIST(3),
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x0,
		END
	};

	const uint8_t exp_payload_range2[] = {LIST(6),
		0x05,
		0x08, 0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x00, 0x0A,
		END
	};

	const uint8_t exp_payload_range3[] = {LIST(5),
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x07,
		END
	};
	const uint8_t exp_payload_range4[] = {LIST(4),
		0x28,
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x0,
		END
	};

	struct Range input1 = {
		.optMinus5to5_present = false,
		.optStr3to6_present = false,
		.optMinus9toMinus6excl_present = false,
		.multi8_count = 1,
		.multiHello_count = 1,
		.multi0to10_count = 1,
		.multi0to10 = {0},
	};
	struct Range input2 = {
		.optMinus5to5_present = true,
		.optMinus5to5 = 5,
		.optStr3to6_present = false,
		.optMinus9toMinus6excl_present = false,
		.multi8_count = 2,
		.multiHello_count = 1,
		.multi0to10_count = 2,
		.multi0to10 = {0, 10},
	};
	struct Range input3 = {
		.optMinus5to5_present = false,
		.optStr3to6_present = true,
		.optStr3to6 = {
			.value = "hello",
			.len = 5,
		},
		.optMinus9toMinus6excl_present = false,
		.multi8_count = 1,
		.multiHello_count = 2,
		.multi0to10_count = 1,
		.multi0to10 = {7},
	};
	struct Range input4 = {
		.optMinus5to5_present = false,
		.optStr3to6_present = false,
		.optMinus9toMinus6excl_present = true,
		.optMinus9toMinus6excl = -9,
		.multi8_count = 1,
		.multiHello_count = 1,
		.multi0to10_count = 1,
		.multi0to10 = {0},
	};
	struct Range input5_inv = {
		.optMinus5to5_present = false,
		.optStr3to6_present = false,
		.optMinus9toMinus6excl_present = true,
		.optMinus9toMinus6excl = -6,
		.multi8_count = 1,
		.multiHello_count = 1,
		.multi0to10_count = 1,
		.multi0to10 = {0},
	};

	uint8_t output[50];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Range(output, sizeof(output), &input1,
				&out_len), NULL);
	zassert_equal(sizeof(exp_payload_range1), out_len, NULL);
	zassert_mem_equal(exp_payload_range1, output, sizeof(exp_payload_range1), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Range(output, sizeof(output), &input2,
				&out_len), NULL);
	zassert_equal(sizeof(exp_payload_range2), out_len, NULL);
	zassert_mem_equal(exp_payload_range2, output, sizeof(exp_payload_range2), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Range(output, sizeof(output), &input3,
				&out_len), NULL);
	zassert_equal(sizeof(exp_payload_range3), out_len, NULL);
	zassert_mem_equal(exp_payload_range3, output, sizeof(exp_payload_range3), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Range(output, sizeof(output), &input4,
				&out_len), NULL);
	zassert_equal(sizeof(exp_payload_range4), out_len, NULL);
	zassert_mem_equal(exp_payload_range4, output, sizeof(exp_payload_range4), NULL);

	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_Range(output, sizeof(output), &input5_inv,
				&out_len), NULL);
}

ZTEST(cbor_encode_test3, test_value_range)
{
	const uint8_t exp_payload_value_range1[] = {LIST(8),
		11,
		0x19, 0x03, 0xe7, // 999
		0x29, // -10
		1,
		0x18, 42, // 42
		0x65, 'w', 'o', 'r', 'l', 'd', // "world"
		0xFB, 0x40, 0x09, 0x1e, 0xb8, 0x51, 0xeb, 0x85, 0x1f, // 3.14
		0xF5,
		END
	};

	const uint8_t exp_payload_value_range2[] = {LIST(8),
		0x18, 100, // 100
		0x39, 0x03, 0xe8, // -1001
		0x18, 100, // 100
		0,
		0x18, 42, // 42
		0x65, 'w', 'o', 'r', 'l', 'd', // "world"
		0xFB, 0x40, 0x09, 0x1e, 0xb8, 0x51, 0xeb, 0x85, 0x1f, // 3.14
		0xF5,
		END
	};

	struct ValueRange input1 = {
		.greater10 = 11,
		.less1000 = 999,
		.greatereqmin10 = -10,
		.lesseq1 = 1,
	};
	struct ValueRange input2 = {
		.greater10 = 100,
		.less1000 = -1001,
		.greatereqmin10 = 100,
		.lesseq1 = 0,
	};
	struct ValueRange input3_inval = {
		.greater10 = 10,
		.less1000 = 999,
		.greatereqmin10 = -10,
		.lesseq1 = 1,
	};
	struct ValueRange input4_inval = {
		.greater10 = 11,
		.less1000 = 1000,
		.greatereqmin10 = -10,
		.lesseq1 = 1,
	};
	struct ValueRange input5_inval = {
		.greater10 = 11,
		.less1000 = 999,
		.greatereqmin10 = -11,
		.lesseq1 = 1,
	};
	struct ValueRange input6_inval = {
		.greater10 = 11,
		.less1000 = 999,
		.greatereqmin10 = -10,
		.lesseq1 = 2,
	};
	struct ValueRange input7_inval = {
		.greater10 = 1,
		.less1000 = 999,
		.greatereqmin10 = -10,
		.lesseq1 = 1,
	};
	struct ValueRange input8_inval = {
		.greater10 = 11,
		.less1000 = 10000,
		.greatereqmin10 = -10,
		.lesseq1 = 1,
	};
	struct ValueRange input9_inval = {
		.greater10 = 11,
		.less1000 = 999,
		.greatereqmin10 = -100,
		.lesseq1 = 1,
	};
	struct ValueRange input10_inval = {
		.greater10 = 11,
		.less1000 = 999,
		.greatereqmin10 = -10,
		.lesseq1 = 21,
	};

	uint8_t output[35];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_ValueRange(output, sizeof(output), &input1,
				&out_len), NULL);
	zassert_equal(sizeof(exp_payload_value_range1), out_len, "%d != %d\n", sizeof(exp_payload_value_range1), out_len);
	zassert_mem_equal(exp_payload_value_range1, output, sizeof(exp_payload_value_range1), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_ValueRange(output, sizeof(output), &input2,
				&out_len), NULL);
	zassert_equal(sizeof(exp_payload_value_range2), out_len, NULL);
	zassert_mem_equal(exp_payload_value_range2, output, sizeof(exp_payload_value_range2), NULL);

	int ret = cbor_encode_ValueRange(output, sizeof(output), &input3_inval, &out_len);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, ret, "%s\n", zcbor_error_str(ret));
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_ValueRange(output, sizeof(output), &input4_inval,
				&out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_ValueRange(output, sizeof(output), &input5_inval,
				&out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_ValueRange(output, sizeof(output), &input6_inval,
				&out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_ValueRange(output, sizeof(output), &input7_inval,
				&out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_ValueRange(output, sizeof(output), &input8_inval,
				&out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_ValueRange(output, sizeof(output), &input9_inval,
				&out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_ValueRange(output, sizeof(output), &input10_inval,
				&out_len), NULL);
}




ZTEST(cbor_encode_test3, test_value_range2)
{
	const uint8_t exp_payload_value_range2_1[] = {LIST(2),
		0x18, 100, // 100
		0x18, 50, // 50
		END
	};

	struct ValueRange2 input1 = {
		.le100ge100 = 100,
		.gt49lt51 = 50,
	};
	struct ValueRange2 input2_inv = {
		.le100ge100 = 99,
		.gt49lt51 = 50,
	};
	struct ValueRange2 input3_inv = {
		.le100ge100 = 101,
		.gt49lt51 = 50,
	};
	struct ValueRange2 input4_inv = {
		.le100ge100 = 100,
		.gt49lt51 = 49,
	};
	struct ValueRange2 input5_inv = {
		.le100ge100 = 100,
		.gt49lt51 = 51,
	};

	size_t out_len;
	uint8_t output[10];

	int ret = cbor_encode_ValueRange2(output, sizeof(output), &input1, &out_len);
	zassert_equal(ZCBOR_SUCCESS, ret, "%s\n", zcbor_error_str(ret));
	zassert_mem_equal(exp_payload_value_range2_1, output, sizeof(exp_payload_value_range2_1), NULL);

	ret = cbor_encode_ValueRange2(output, sizeof(output), &input2_inv, &out_len);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, ret, "%s\n", zcbor_error_str(ret));

	ret = cbor_encode_ValueRange2(output, sizeof(output), &input3_inv, &out_len);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, ret, "%s\n", zcbor_error_str(ret));

	ret = cbor_encode_ValueRange2(output, sizeof(output), &input4_inv, &out_len);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, ret, "%s\n", zcbor_error_str(ret));

	ret = cbor_encode_ValueRange2(output, sizeof(output), &input5_inv, &out_len);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, ret, "%s\n", zcbor_error_str(ret));
}


ZTEST(cbor_encode_test3, test_float_range)
{
	uint8_t output[35];
	size_t out_len;

	struct FloatRange input1 = {
		.min4to1 = -4.2,
		.min6milToMin1 = -6543210.0,
		.zeroTo100 = 0.0,
	};
	struct FloatRange input2 = {
		.min4to1 = 0.99999999999,
		.min6milToMin1 = -0.99,
		.zeroTo100 = 100.0,
	};
	struct FloatRange input3_inv = {
		.min4to1 = 1.0, // Fails
		.min6milToMin1 = -0.99,
		.zeroTo100 = 100.0,
	};
	struct FloatRange input4_inv = {
		.min4to1 = 0.99999999999,
		.min6milToMin1 = -0.98999999999, // Fails
		.zeroTo100 = 100.0,
	};
	struct FloatRange input5_inv = {
		.min4to1 = 0.99999999999,
		.min6milToMin1 = -0.99,
		.zeroTo100 = *(double*)&(uint64_t){0x8000000000000001}, // Fails (smallest negative subnormal, less than 0)
	};

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_FloatRange(output, sizeof(output), &input1,
				&out_len), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_FloatRange(output, sizeof(output), &input2,
				&out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_FloatRange(output, sizeof(output), &input3_inv,
				&out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_FloatRange(output, sizeof(output), &input4_inv,
				&out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_FloatRange(output, sizeof(output), &input5_inv,
				&out_len), NULL);
}


ZTEST(cbor_encode_test3, test_single)
{
	uint8_t exp_payload_single0[] = {0x45, 'h', 'e', 'l', 'l', 'o'};
	uint8_t exp_payload_single1[] = {0x18, 52,};
	uint8_t exp_payload_single2[] = {9};
	uint8_t output[10];
	size_t out_len;
	struct zcbor_string input_single0 = {
		.value = "hello",
		.len = 5
	};
	size_t input_single1 = 52;
	size_t input_single2_ign = 53;
	uint8_t input_single3 = 9;
	uint8_t input_single4_inv = 10;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_SingleBstr(output, sizeof(output), &input_single0, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_single0), out_len, NULL);
	zassert_mem_equal(exp_payload_single0, output, sizeof(exp_payload_single0), NULL);
	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, cbor_encode_SingleBstr(output, 5, &input_single0, &out_len), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_SingleInt_uint52(output, sizeof(output), &input_single1, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_single1), out_len, NULL);
	zassert_mem_equal(exp_payload_single1, output, sizeof(exp_payload_single1), NULL);
	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, cbor_encode_SingleInt_uint52(output, 1, &input_single1, &out_len), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_SingleInt_uint52(output, sizeof(output), &input_single2_ign, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_single1), out_len, NULL);
	zassert_mem_equal(exp_payload_single1, output, sizeof(exp_payload_single1), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_SingleInt2(output, sizeof(output), &input_single3, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_single2), out_len, NULL);
	zassert_mem_equal(exp_payload_single2, output, sizeof(exp_payload_single2), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_SingleInt2(output, sizeof(output), &input_single4_inv, &out_len), NULL);
}

ZTEST(cbor_encode_test3, test_unabstracted)
{
	uint8_t exp_payload_unabstracted0[] = {LIST(2), 0x01, 0x03, END};
	uint8_t exp_payload_unabstracted1[] = {LIST(2), 0x02, 0x04, END};
	struct Unabstracted result_unabstracted0 = {
		.unabstractedunion1_choice = Unabstracted_unabstractedunion1_choice1_c,
		.unabstractedunion2_choice = Unabstracted_unabstractedunion2_uint3_c,
	};
	struct Unabstracted result_unabstracted1 = {
		.unabstractedunion1_choice = Unabstracted_unabstractedunion1_choice2_c,
		.unabstractedunion2_choice = Unabstracted_unabstractedunion2_choice4_c,
	};
	uint8_t output[4];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Unabstracted(output, sizeof(output),
					&result_unabstracted0, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_unabstracted0), out_len, "was %d\n", out_len);
	zassert_mem_equal(exp_payload_unabstracted0, output, sizeof(exp_payload_unabstracted0), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Unabstracted(output, sizeof(output),
					&result_unabstracted1, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_unabstracted1), out_len, NULL);
	zassert_mem_equal(exp_payload_unabstracted1, output, sizeof(exp_payload_unabstracted1), NULL);
}

ZTEST(cbor_encode_test3, test_string_overflow)
{
	struct zcbor_string input_overflow0 = {
		.value = "",
		.len = 0xFFFFFF00, /* overflows to before this object. */
	};
	uint8_t output[10];
	size_t out_len;

	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, cbor_encode_SingleBstr(output, sizeof(output), &input_overflow0, &out_len), NULL);
}

ZTEST(cbor_encode_test3, test_quantity_range)
{
	uint8_t exp_payload_qty_range1[] = {0xF5, 0xF5, 0xF5};
	uint8_t exp_payload_qty_range2[] = {0xF6, 0xF6, 0xF6, 0xF6, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5};
	struct QuantityRange result_qty_range1 = {
		.upto4nils_count = 0,
		.from3true_count = 3,
	};
	struct QuantityRange result_qty_range2 = {
		.upto4nils_count = 4,
		.from3true_count = 6,
	};
	struct QuantityRange result_qty_range3_inv = {
		.upto4nils_count = 5,
		.from3true_count = 3
	};
	struct QuantityRange result_qty_range4_inv = {
		.upto4nils_count = 0,
		.from3true_count = 2
	};
	uint8_t output[12];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_QuantityRange(output, sizeof(output),
			&result_qty_range1, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_qty_range1), out_len, "was %d\n", out_len);
	zassert_mem_equal(exp_payload_qty_range1, output, sizeof(exp_payload_qty_range1), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_QuantityRange(output, sizeof(output),
			&result_qty_range2, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_qty_range2), out_len, "was %d\n", out_len);
	zassert_mem_equal(exp_payload_qty_range2, output, sizeof(exp_payload_qty_range2), NULL);

	zassert_equal(ZCBOR_ERR_ITERATIONS, cbor_encode_QuantityRange(output, sizeof(output),
			&result_qty_range3_inv, &out_len), NULL);

	zassert_equal(ZCBOR_ERR_ITERATIONS, cbor_encode_QuantityRange(output, sizeof(output),
			&result_qty_range4_inv, &out_len), NULL);
}

ZTEST(cbor_encode_test3, test_nested_quantifiers)
{
	uint8_t exp_payload_nested_qty1[] = {LIST(9), 0x01, 0x02, 0x03, 0xF5, 0x61, 'a', 0x61, 'b',
		0x40, 0x41, 'c', MAP(2), 0x01, 0xF6, 0x02, 0xF6, END END};
	uint8_t exp_payload_nested_qty2[] = {LIST(5), 0x61, 'a', 0x61, 'b', 0x61, 'c', 0x61, 'd',
		MAP(0), END END};
	struct zcbor_string a = {(const uint8_t *)"a", 1};
	struct zcbor_string b = {(const uint8_t *)"b", 1};
	struct zcbor_string c = {(const uint8_t *)"c", 1};
	struct zcbor_string d = {(const uint8_t *)"d", 1};
	struct zcbor_string empty = {(const uint8_t *)"", 0};
	struct NestedQuantifiers input1 = {
		.anyopt = {1, 2, 3},
		.anyopt_count = 3,
		.optopt = true,
		.optopt_present = true,
		.pairs = {{.tstr = {a, b}, .tstr_count = 2}},
		.pairs_count = 1,
		.optpair = {.bstr = {empty, c}, .bstr_count = 2},
		.optpair_present = true,
		.uintnil = {{.uintnil_key = 1}, {.uintnil_key = 2}},
		.uintnil_count = 2,
	};
	struct NestedQuantifiers input2 = {
		.pairs = {{.tstr = {a, b}, .tstr_count = 2}, {.tstr = {c, d}, .tstr_count = 2}},
		.pairs_count = 2,
	};
	struct NestedQuantifiers input3_inv = {
		.pairs = {{.tstr = {a, b}, .tstr_count = 2}},
		.pairs_count = 3,
	};
	struct NestedQuantifiers input4_inv = {
		.pairs_count = 0,
	};
	uint8_t output[25];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedQuantifiers(output, sizeof(output),
			&input1, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_nested_qty1), out_len, "was %d\n", out_len);
	zassert_mem_equal(exp_payload_nested_qty1, output, sizeof(exp_payload_nested_qty1), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_NestedQuantifiers(output, sizeof(output),
			&input2, &out_len), NULL);
	zassert_equal(sizeof(exp_payload_nested_qty2), out_len, "was %d\n", out_len);
	zassert_mem_equal(exp_payload_nested_qty2, output, sizeof(exp_payload_nested_qty2), NULL);

	zassert_equal(ZCBOR_ERR_ITERATIONS, cbor_encode_NestedQuantifiers(output, sizeof(output),
			&input3_inv, &out_len), NULL);
	zassert_equal(ZCBOR_ERR_ITERATIONS, cbor_encode_NestedQuantifiers(output, sizeof(output),
			&input4_inv, &out_len), NULL);
}

ZTEST(cbor_encode_test3, test_doublemap)
{
	uint8_t exp_payload_doublemap0[] = {MAP(2), 0x01, MAP(1), 0x01, 0x01, END 0x02, MAP(1), 0x02, 0x02, END END};
	struct DoubleMap result_doublemap = {
		.uintmap_count = 2,
		.uintmap = {
			{
				.uintmap_key = 1,
				.MyKeys_m = {
					.uint1int_present = true,
					.uint1int = 1,
				}
			},
			{
				.uintmap_key = 2,
				.MyKeys_m = {
					.uint2int_present = true,
					.uint2int = 2,
				}
			},
		}
	};
	uint8_t output[20];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_DoubleMap(output,
					sizeof(output),
					&result_doublemap, &out_len), NULL);
	zassert_equal(out_len, sizeof(exp_payload_doublemap0), "%d != %d\n",
			out_len, sizeof(exp_payload_doublemap0));
	zassert_mem_equal(exp_payload_doublemap0, output, out_len, NULL);
}

ZTEST(cbor_encode_test3, test_floats)
{
	uint8_t exp_floats_payload1[] = {LIST(5), 0xF9, 0, 0, /* 0.0 */
			0xFA, 0, 0, 0, 0 /* 0.0 */,
			0xFB, 0, 0, 0, 0, 0, 0, 0, 0 /* 0.0 */,
			0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			END
	};
	uint8_t exp_floats_payload2[] = {LIST(5), 0xF9, 0x42, 0x48, /* 3.1415 */
			0xFA, 0xc7, 0xc0, 0xe6, 0xb7 /* -98765.4321 */,
			0xFB, 0x41, 0x32, 0xd6, 0x87, 0xe3, 0xd7, 0xa, 0x3d /* 1234567.89 */,
			0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			END
	};

	uint8_t exp_floats_payload3[] = {LIST(5), 0xF9, 0xfc, 0x0, /* -98765.4321 (-infinity) */
			0xFA, 0x49, 0x96, 0xb4, 0x3f /* 1234567.89 */,
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			END
	};

	uint8_t exp_floats_payload4[] = {LIST(8), 0xF9, 0x42, 0x48, /* 3.1415 */
			0xFA, 0x49, 0x96, 0xb4, 0x3f /* 1234567.89 */,
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			0xFB, 0x3f, 0x31, 0xa5, 0x9d, 0xe0, 0x0, 0x0, 0x0 /* 123/456789 */,
			0xFB, 0x3f, 0x31, 0xa5, 0x9d, 0xd9, 0x57, 0x14, 0x64 /* 123/456789 */,
			0xFB, 0xbd, 0x50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 /* -2^(-42) */,
			END
	};

	struct Floats input;
	size_t num_encode;
	uint8_t output[80];

	input.float_16 = (float)0.0;
	input.float_32 = (float)0.0;
	input.float_64 = (double)0.0;
	input.floats_count = 0;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Floats(
		output, sizeof(output), &input, &num_encode), NULL);

	zassert_equal(sizeof(exp_floats_payload1), num_encode, NULL);
	zassert_mem_equal(exp_floats_payload1, output, num_encode, NULL);

	input.float_16 = (float)3.1415;
	input.float_32 = (float)-98765.4321;
	input.float_64 = (double)1234567.89;
	input.floats_count = 0;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Floats(
		output, sizeof(output), &input, &num_encode), NULL);
	zassert_equal(sizeof(exp_floats_payload2), num_encode, NULL);
	zassert_mem_equal(exp_floats_payload2, output, num_encode, NULL);

	input.float_16 = (float)-98765.4321;
	input.float_32 = (float)1234567.89;
	input.float_64 = (double)-98765.4321;
	input.floats_count = 0;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Floats(
		output, sizeof(output), &input, &num_encode), NULL);
	zassert_equal(sizeof(exp_floats_payload3), num_encode, NULL);
	zassert_mem_equal(exp_floats_payload3, output, num_encode, NULL);

	input.float_16 = (float)3.1415;
	input.float_32 = (float)1234567.89;
	input.float_64 = (double)-98765.4321;
	input.floats_count = 3;
	input.floats[0] = (float)(123.0/456789.0);
	input.floats[1] = (double)(123.0/456789.0);
	input.floats[2] = (double)(-1.0/(1LL << 42));
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Floats(
		output, sizeof(output), &input, &num_encode), NULL);
	zassert_equal(sizeof(exp_floats_payload4), num_encode, NULL);
	zassert_mem_equal(exp_floats_payload4, output, num_encode, NULL);

}


/* Test using ranges (greater/less than) on floats. */
ZTEST(cbor_encode_test3, test_floats2)
{
	uint8_t exp_floats2_payload1[] = {LIST(2),
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			0xFB, 0xbd, 0x50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 /* -2^(-42) */,
			END
	};
	uint8_t exp_floats2_payload2[] = {LIST(2),
			0xFB, 0xbd, 0x50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 /* -2^(-42) */,
			0xFB, 0xc0, 0xc3, 0x88, 0x0, 0x0, 0x0, 0x0, 0x0 /* -10000 */,
			END
	};
	size_t num_encode;
	struct Floats2 input;
	uint8_t output[40];

	input.float_lt_1 = -98765.4321;
	input.float_ge_min_10000 = (-1.0/(1LL << 42));
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Floats2(
		output, sizeof(output), &input, &num_encode), NULL);
	zassert_equal(sizeof(exp_floats2_payload1), num_encode, NULL);
	zassert_mem_equal(exp_floats2_payload1, output, num_encode, NULL);

	input.float_lt_1 = (-1.0/(1LL << 42));
	input.float_ge_min_10000 = -10000;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Floats2(
		output, sizeof(output), &input, &num_encode), NULL);
	zassert_equal(sizeof(exp_floats2_payload2), num_encode, NULL);
	zassert_mem_equal(exp_floats2_payload2, output, num_encode, NULL);
}

ZTEST(cbor_encode_test3, test_cbor_bstr)
{
	uint8_t exp_cbor_bstr_payload1[] = {
#ifdef ZCBOR_CANONICAL
		0x58, 33,
#else
		0x58, 34,
#endif
			LIST(4),
				0x46, 0x65, 'H', 'e', 'l', 'l', 'o',
				0x49, 0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
				0x4C, 0xC2, 0x4A, 0x42, 2, 3, 4, 5, 6, 7, 8, 9, 10 /* 0x4202030405060708090A */,
				0x41, 0xF6 /* nil */,
			END
	};

	struct CBORBstr input = {0};
	size_t num_encode;
	uint8_t output[70];

	input.big_uint_bstr_cbor.value = (uint8_t []){0x42, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	input.big_uint_bstr_cbor.len = 10;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_CBORBstr(output, sizeof(output), &input, &num_encode), NULL);

	zassert_equal(sizeof(exp_cbor_bstr_payload1), num_encode, "%d != %d\r\n", sizeof(exp_cbor_bstr_payload1), num_encode);
	zassert_mem_equal(exp_cbor_bstr_payload1, output, num_encode, NULL);
}


/* The entry functions use the generated number of states, so the CborAny and CborNestedAny cases
 * (and CborNilUndef with ZCBOR_CANONICAL, where lists need a backup) fail with
 * ZCBOR_ERR_NO_BACKUP_MEM if the backups needed for the bstrs are not counted. CborSeqAny's
 * contents have a count variable, so its backup was counted already. */
ZTEST(cbor_encode_test3, test_cbor_bstr_no_var)
{
	/* any is encoded as nil. */
	uint8_t exp_any_payload[] = {0x41, 0xF6};
	uint8_t exp_nil_undef_payload[] = {LIST(2), 0x41, 0xF6, 0x41, 0xF7, END};
	uint8_t exp_nested_any_payload[] = {0x42, 0x41, 0xF6};
	uint8_t exp_seq_any_payload[] = {LIST(1), 0x42, 0xF6, 0xF6, END};

	struct zcbor_string any_input = {0};
	struct CborNilUndef nil_undef_input = {0};
	struct CborNestedAny nested_any_input = {0};
	struct CborSeqAny seq_any_input = {.bstr_cbor_count = 2};
	uint8_t output[10];
	size_t num_encode;
	int res;

	res = cbor_encode_CborAny(output, sizeof(output), &any_input, &num_encode);
	zassert_equal(ZCBOR_SUCCESS, res, "%s\n", zcbor_error_str(res));
	zassert_equal(sizeof(exp_any_payload), num_encode);
	zassert_mem_equal(exp_any_payload, output, num_encode);

	res = cbor_encode_CborNilUndef(output, sizeof(output), &nil_undef_input, &num_encode);
	zassert_equal(ZCBOR_SUCCESS, res, "%s\n", zcbor_error_str(res));
	zassert_equal(sizeof(exp_nil_undef_payload), num_encode);
	zassert_mem_equal(exp_nil_undef_payload, output, num_encode);

	res = cbor_encode_CborNestedAny(output, sizeof(output), &nested_any_input, &num_encode);
	zassert_equal(ZCBOR_SUCCESS, res, "%s\n", zcbor_error_str(res));
	zassert_equal(sizeof(exp_nested_any_payload), num_encode);
	zassert_mem_equal(exp_nested_any_payload, output, num_encode);

	res = cbor_encode_CborSeqAny(output, sizeof(output), &seq_any_input, &num_encode);
	zassert_equal(ZCBOR_SUCCESS, res, "%s\n", zcbor_error_str(res));
	zassert_equal(sizeof(exp_seq_any_payload), num_encode);
	zassert_mem_equal(exp_seq_any_payload, output, num_encode);
}


ZTEST(cbor_encode_test3, test_map_length)
{
	uint8_t exp_map_length_payload1[] = {MAP(2),
		0x61, 'r', 0x01,
		0x61, 'm', 0x46, 1, 2, 3, 4, 5, 6, END
	};
	uint8_t exp_map_length_payload2[] = {MAP(3),
		0x61, 'r', 0x01,
		0x61, 'm', 0x46, 1, 2, 3, 4, 5, 6,
		0x61, 'e', LIST(0), END END
	};
	uint8_t exp_map_length_payload3[] = {MAP(3),
		0x61, 'r', 0x01,
		0x61, 'm', 0x46, 1, 2, 3, 4, 5, 6,
		0x61, 'e', LIST(1), 0x50, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, END END
	};
	uint8_t exp_map_length_payload4[] = {MAP(3),
		0x61, 'r', 0x01,
		0x61, 'm', 0x46, 1, 2, 3, 4, 5, 6,
		0x61, 'e', LIST(2), 0x50, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1,
			0x50, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, END END
	};

	struct MapLength input;
	size_t num_encode;
	uint8_t output[60];

	uint8_t mac[] = {1, 2, 3, 4, 5, 6};
	uint8_t uuid_m[] = {8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, 0};

	input.result = 1;
	input.mac_addr.len = 6;
	input.mac_addr.value = mac;
	input.end_device_array_present = false;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_MapLength(output,
		sizeof(output), &input, &num_encode), NULL);
	zassert_equal(sizeof(exp_map_length_payload1), num_encode, "%d != %d\r\n", sizeof(exp_map_length_payload1), num_encode);
	zassert_mem_equal(exp_map_length_payload1, output, num_encode, NULL);

	input.end_device_array_present = true;
	input.end_device_array.uuid_m_count = 0;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_MapLength(output,
		sizeof(output), &input, &num_encode), NULL);
	zassert_equal(sizeof(exp_map_length_payload2), num_encode, "%d != %d\r\n", sizeof(exp_map_length_payload2), num_encode);
	zassert_mem_equal(exp_map_length_payload2, output, num_encode, NULL);

	input.end_device_array.uuid_m_count = 1;
	input.end_device_array.uuid_m[0].len = 16;
	input.end_device_array.uuid_m[0].value = uuid_m;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_MapLength(output,
		sizeof(output), &input, &num_encode), NULL);
	zassert_equal(sizeof(exp_map_length_payload3), num_encode, "%d != %d\r\n", sizeof(exp_map_length_payload3), num_encode);
	zassert_mem_equal(exp_map_length_payload3, output, num_encode, NULL);

	input.end_device_array.uuid_m_count = 2;
	input.end_device_array.uuid_m[1].len = 16;
	input.end_device_array.uuid_m[1].value = uuid_m;
	int err = cbor_encode_MapLength(output,
		sizeof(output), &input, &num_encode);
	zassert_equal(ZCBOR_SUCCESS, err, "%d\r\b", err);
	zassert_equal(sizeof(exp_map_length_payload4), num_encode, "%d != %d\r\n", sizeof(exp_map_length_payload4), num_encode);
	zassert_mem_equal(exp_map_length_payload4, output, num_encode, NULL);

	input.mac_addr.len--;
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_MapLength(output,
		sizeof(output), &input, &num_encode), NULL);

	input.mac_addr.len++;
	input.end_device_array.uuid_m[1].len++;
	err = cbor_encode_MapLength(output,
		sizeof(output), &input, &num_encode);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, err, "%d\r\b", err);
}


ZTEST(cbor_encode_test3, test_union_int)
{
	uint8_t exp_union_int_payload1[] = {LIST(2),
		0x05, 0x6E, 'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 'a', ' ', 'f', 'i', 'v', 'e',
		END
	};
	uint8_t exp_union_int_payload2[] = {LIST(2),
		0x19, 0x03, 0xE8, 0x50, 'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 't', 'h', 'o', 'u', 's', 'a', 'n', 'd',
		END
	};
	uint8_t exp_union_int_payload3[] = {LIST(3),
		0x3A, 0x00, 0x01, 0x86, 0x9F, 0xF6, 0x01,
		END
	};
	uint8_t exp_union_int_payload4[] = {LIST(2),
		0x01, 0x42, 'h', 'i',
		END
	};
	struct UnionInt2 input;
	size_t num_encode;
	uint8_t output[60];

	input.Union_choice = union_uint5_l_c;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionInt2(output, sizeof(output),
			&input, &num_encode), NULL);
	zassert_equal(sizeof(exp_union_int_payload1), num_encode, NULL);
	zassert_mem_equal(exp_union_int_payload1, output, num_encode, NULL);

	input.Union_choice = union_uint1000_l_c;
	input.bstr.len = 16;
	input.bstr.value = (uint8_t *)&"This is thousand";
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionInt2(output, sizeof(output),
			&input, &num_encode), NULL);
	zassert_equal(sizeof(exp_union_int_payload2), num_encode, NULL);
	zassert_mem_equal(exp_union_int_payload2, output, num_encode, NULL);

	input.Union_choice = union_nint100000_l_c;
	input.number_m.number_choice = number_int_c;
	input.number_m.Int = 1;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionInt2(output, sizeof(output),
			&input, &num_encode), NULL);
	zassert_equal(sizeof(exp_union_int_payload3), num_encode, NULL);
	zassert_mem_equal(exp_union_int_payload3, output, num_encode, NULL);

	input.Union_choice = UnionInt2_union_Structure_One_m_c;
	input.Structure_One_m.some_array.value = (uint8_t *)&"hi";
	input.Structure_One_m.some_array.len = 2;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionInt2(output, sizeof(output),
			&input, &num_encode), NULL);
	zassert_equal(sizeof(exp_union_int_payload4), num_encode, NULL);
	zassert_mem_equal(exp_union_int_payload4, output, num_encode, NULL);
}


/* The union members start with integers, but the integers are optional or repeated. */
ZTEST(cbor_encode_test3, test_union_int_quantified)
{
	uint8_t exp_empty_map_payload[] = {MAP(0), END};
	uint8_t exp_union_int_opt1_payload[] = {MAP(1), 0x01, 0x05, END};
	uint8_t exp_union_int_rep_payload1[] = {MAP(1), 0x05, 0x05, END};
	uint8_t exp_union_int_rep_payload2[] = {MAP(2), 0x06, 0xF5, 0x06, 0xF4, END};
	uint8_t exp_union_int_opt_list_payload1[] = {LIST(0), END};
	uint8_t exp_union_int_opt_group_payload1[] = {MAP(2), 0x09, 0x05, 0x0A, 0x20, END};
	struct UnionIntOpt1 input_opt1 = {
		.Union_choice = union_uint1uint_c,
		.uint1uint_present = false,
	};
	struct UnionIntRep input_rep = {
		.Union_choice = union_uint5uint_c,
		.uint5uint = 5,
	};
	struct UnionIntOptList input_opt_list = {
		.Union_choice = union_uint7_l_c,
		.uint7_l_present = false,
	};
	struct UnionIntOptGroup input_opt_group = {
		.Union_choice = union_uint9uint_l_c,
		.uint9uint_present = false,
		.uint10int_present = false,
	};
	uint8_t output[10];
	size_t num_encode;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionIntOpt1(output, sizeof(output),
			&input_opt1, &num_encode));
	zassert_equal(sizeof(exp_empty_map_payload), num_encode);
	zassert_mem_equal(exp_empty_map_payload, output, num_encode);

	input_opt1.uint1uint_present = true;
	input_opt1.uint1uint = 5;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionIntOpt1(output, sizeof(output),
			&input_opt1, &num_encode));
	zassert_equal(sizeof(exp_union_int_opt1_payload), num_encode);
	zassert_mem_equal(exp_union_int_opt1_payload, output, num_encode);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionIntRep(output, sizeof(output),
			&input_rep, &num_encode));
	zassert_equal(sizeof(exp_union_int_rep_payload1), num_encode);
	zassert_mem_equal(exp_union_int_rep_payload1, output, num_encode);

	input_rep.Union_choice = union_uint6bool_c;
	input_rep.uint6bool_count = 0;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionIntRep(output, sizeof(output),
			&input_rep, &num_encode));
	zassert_equal(sizeof(exp_empty_map_payload), num_encode);
	zassert_mem_equal(exp_empty_map_payload, output, num_encode);

	input_rep.uint6bool_count = 2;
	input_rep.uint6bool[0] = true;
	input_rep.uint6bool[1] = false;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionIntRep(output, sizeof(output),
			&input_rep, &num_encode));
	zassert_equal(sizeof(exp_union_int_rep_payload2), num_encode);
	zassert_mem_equal(exp_union_int_rep_payload2, output, num_encode);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionIntOptList(output, sizeof(output),
			&input_opt_list, &num_encode));
	zassert_equal(sizeof(exp_union_int_opt_list_payload1), num_encode);
	zassert_mem_equal(exp_union_int_opt_list_payload1, output, num_encode);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionIntOptGroup(output, sizeof(output),
			&input_opt_group, &num_encode));
	zassert_equal(sizeof(exp_empty_map_payload), num_encode);
	zassert_mem_equal(exp_empty_map_payload, output, num_encode);

	input_opt_group.uint9uint_present = true;
	input_opt_group.uint9uint = 5;
	input_opt_group.uint10int_present = true;
	input_opt_group.uint10int = -1;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionIntOptGroup(output, sizeof(output),
			&input_opt_group, &num_encode));
	zassert_equal(sizeof(exp_union_int_opt_group_payload1), num_encode);
	zassert_mem_equal(exp_union_int_opt_group_payload1, output, num_encode);
}


ZTEST(cbor_encode_test3, test_intmax)
{
	uint8_t exp_intmax1_payload1[] = {LIST(C),
		0x38, 0x7F, 0x18, 0x7F, 0x18, 0xFF,
		0x39, 0x7F, 0xFF,
		0x19, 0x7F, 0xFF,
		0x19, 0xFF, 0xFF,
		0x3A, 0x7F, 0xFF, 0xFF, 0xFF,
		0x1A, 0x7F, 0xFF, 0xFF, 0xFF,
		0x1A, 0xFF, 0xFF, 0xFF, 0xFF,
		0x3B, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x1B, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x1B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		END
	};
	uint8_t exp_intmax2_payload1[] = {LIST(8),
		0x38, 0x7F, 0x0,
		0x39, 0x7F, 0xFF,
		0x0,
		0x3A, 0x7F, 0xFF, 0xFF, 0xFF,
		0x0,
		0x3B, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x0,
		END
	};
	uint8_t exp_intmax2_payload2[] = {LIST(8),
		0x18, 0x7F, 0x18, 0xFF,
		0x19, 0x7F, 0xFF,
		0x19, 0xFF, 0xFF,
		0x1A, 0x7F, 0xFF, 0xFF, 0xFF,
		0x1A, 0xFF, 0xFF, 0xFF, 0xFF,
		0x1B, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x1B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		END
	};
	struct Intmax2 intput2;
	struct Intmax4 intput4;
	size_t num_encode;
	uint8_t output[70];

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Intmax1(output,
		sizeof(output), NULL, &num_encode), NULL);
	zassert_equal(sizeof(exp_intmax1_payload1), num_encode, NULL);
	zassert_mem_equal(exp_intmax1_payload1, output, num_encode, NULL);

	intput2.INT_8 = INT8_MIN;
	intput2.UINT_8 = 0;
	intput2.INT_16 = INT16_MIN;
	intput2.UINT_16 = 0;
	intput2.INT_32 = INT32_MIN;
	intput2.UINT_32 = 0;
	intput2.INT_64 = INT64_MIN;
	intput2.UINT_64 = 0;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Intmax2(output,
		sizeof(output), &intput2, &num_encode), NULL);
	zassert_equal(sizeof(exp_intmax2_payload1), num_encode, NULL);
	zassert_mem_equal(exp_intmax2_payload1, output, num_encode, NULL);

	intput2.INT_8 = INT8_MAX;
	intput2.UINT_8 = UINT8_MAX;
	intput2.INT_16 = INT16_MAX;
	intput2.UINT_16 = UINT16_MAX;
	intput2.INT_32 = INT32_MAX;
	intput2.UINT_32 = UINT32_MAX;
	intput2.INT_64 = INT64_MAX;
	intput2.UINT_64 = UINT64_MAX;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Intmax2(output,
		sizeof(output), &intput2, &num_encode), NULL);
	zassert_equal(sizeof(exp_intmax2_payload2), num_encode, NULL);
	zassert_mem_equal(exp_intmax2_payload2, output, num_encode, NULL);

	intput4.INT_8_MIN_count = 1;
	intput4.INT_8_MAX_count = 1;
	intput4.UINT_8_MAX_count = 1;
	intput4.INT_16_MIN_count = 1;
	intput4.INT_16_MAX_count = 1;
	intput4.UINT_16_MAX_count = 1;
	intput4.INT_32_MIN_count = 1;
	intput4.INT_32_MAX_count = 1;
	intput4.UINT_32_MAX_count = 1;
	intput4.INT_64_MIN_count = 1;
	intput4.INT_64_MAX_count = 1;
	intput4.UINT_64_MAX_count = 1;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Intmax4(output,
		sizeof(output), &intput4, &num_encode), NULL);
	zassert_equal(sizeof(exp_intmax1_payload1), num_encode, NULL);
	zassert_mem_equal(exp_intmax1_payload1, output, num_encode, NULL);
}


/* Test that zcbor generates variable names that don't contain unsupported characters. */
ZTEST(cbor_encode_test3, test_invalid_identifiers)
{
	uint8_t exp_invalid_identifiers_payload1[] = {
		LIST(3),
		0x64, '1', 'o', 'n', 'e',
		0x02, /* Ø */
		0x67, '{', '[', 'a', '-', 'z', ']', '}',
		END
	};
	struct InvalidIdentifiers input;
	size_t num_encode;
	uint8_t output[100];

	input._1one_tstr_present = true;
	input.__present = true;
	input.a_z_tstr_present = true;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_InvalidIdentifiers(output,
		sizeof(output), &input, &num_encode), NULL);
	zassert_equal(sizeof(exp_invalid_identifiers_payload1), num_encode, NULL);
	zassert_mem_equal(exp_invalid_identifiers_payload1, output, num_encode, NULL);
}


/** Regression test for a bug where some decoding functions were not called
 *  because of the union_int optimization */
ZTEST(cbor_encode_test3, test_map_union_prim_alias)
{
	uint8_t exp_map_union_prim_alias_payload0[] = {MAP(1), 0, 0, END};
	uint8_t exp_map_union_prim_alias_payload1[] = {MAP(1), 1, 1, END};
	uint8_t exp_map_union_prim_alias_payload2[] = {MAP(1), 2, 0xf6, END};
	uint8_t exp_map_union_prim_alias_payload3[] = {MAP(1), 3, 0xf6, END};
	uint8_t exp_map_union_prim_alias_payload4[] = {MAP(1), 4, 0xf6, END};
	uint8_t exp_map_union_prim_alias_payload5[] = {MAP(1), 5, 5, END};
	uint8_t exp_map_union_prim_alias_payload6[] = {MAP(1), 6, 6, END};
	uint8_t payload[10];

	struct MapUnionPrimAlias input;
	size_t num_encode;

	input.Union_choice = union_uint0_c;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_MapUnionPrimAlias(payload,
		sizeof(payload), &input, &num_encode), NULL);
	zassert_equal(num_encode, sizeof(exp_map_union_prim_alias_payload0));
	zassert_mem_equal(exp_map_union_prim_alias_payload0, payload, num_encode);

	input.Union_choice = union_uint1int_c;
	input.uint1int = 1;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_MapUnionPrimAlias(payload,
		sizeof(payload), &input, &num_encode), NULL);
	zassert_equal(num_encode, sizeof(exp_map_union_prim_alias_payload1));
	zassert_mem_equal(exp_map_union_prim_alias_payload1, payload, num_encode);

	input.Union_choice = union_uint2nil_c;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_MapUnionPrimAlias(payload,
		sizeof(payload), &input, &num_encode), NULL);
	zassert_equal(num_encode, sizeof(exp_map_union_prim_alias_payload2));
	zassert_mem_equal(exp_map_union_prim_alias_payload2, payload, num_encode);

	input.Union_choice = union_m_nil_m_c;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_MapUnionPrimAlias(payload,
		sizeof(payload), &input, &num_encode), NULL);
	zassert_equal(num_encode, sizeof(exp_map_union_prim_alias_payload3));
	zassert_mem_equal(exp_map_union_prim_alias_payload3, payload, num_encode);

	input.Union_choice = union_mm_nil_m_c;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_MapUnionPrimAlias(payload,
		sizeof(payload), &input, &num_encode), NULL);
	zassert_equal(num_encode, sizeof(exp_map_union_prim_alias_payload4));
	zassert_mem_equal(exp_map_union_prim_alias_payload4, payload, num_encode);

	input.Union_choice = union_m_int_m_c;
	input.m_int_m = 5;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_MapUnionPrimAlias(payload,
		sizeof(payload), &input, &num_encode), NULL);
	zassert_equal(num_encode, sizeof(exp_map_union_prim_alias_payload5));
	zassert_mem_equal(exp_map_union_prim_alias_payload5, payload, num_encode);

	input.Union_choice = union_m_6_m_c;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_MapUnionPrimAlias(payload,
		sizeof(payload), &input, &num_encode), NULL);
	zassert_equal(num_encode, sizeof(exp_map_union_prim_alias_payload6));
	zassert_mem_equal(exp_map_union_prim_alias_payload6, payload, num_encode);
}


ZTEST(cbor_encode_test3, test_empty_group)
{
	uint8_t exp_empty_group_payload0[] = {LIST(3), 0, LIST(0), END MAP(0), END END};
	uint8_t payload[10];

	struct EmptyContainer input;
	size_t num_encode;

	input.Int = 0;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_EmptyContainer(payload,
		sizeof(payload), &input, &num_encode), NULL);
	zassert_equal(sizeof(exp_empty_group_payload0), num_encode, NULL);
	zassert_mem_equal(exp_empty_group_payload0, payload, num_encode);
}


ZTEST(cbor_encode_test3, test_single_elem_list)
{
	uint8_t exp_single_elem_list_payload1[] = {LIST(4),
		LIST(1), 0x61, 's', END
		LIST(1), LIST(1), 0, END END
		LIST(1), 0xf5, END
		MAP(1), 1, 0x41, 0x02, END
		END
	};

	struct SingleElemList input = {
		.tstr.len = 1,
		.tstr.value = &(uint8_t){'s'},
		.MyInt_m = 0,
		.uint1bstr.len = 1,
		.uint1bstr.value = &(uint8_t){2},
	};
	size_t num_encode;
	uint8_t payload[20];

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_SingleElemList(payload,
		sizeof(payload), &input, &num_encode), NULL);
	zassert_equal(num_encode, sizeof(exp_single_elem_list_payload1));
	zassert_mem_equal(exp_single_elem_list_payload1, payload, num_encode);
}


ZTEST(cbor_encode_test3, test_nested_choices)
{
	uint8_t nested_choices_exp_payload1[] = {0xf6};
	uint8_t nested_choices_exp_payload2[] = {LIST(2),
			LIST(1), 0x65, 'h', 'e', 'l', 'l', 'o', END
			LIST(1), 0x65, 'w', 'o', 'r', 'l', 'd', END
		END
	};
	uint8_t nested_choices_exp_payload3[] = {LIST(3),
			LIST(1), 0x65, 'h', 'e', 'l', 'l', 'o', END
			0xf6,
			LIST(1), 0x65, 'w', 'o', 'r', 'l', 'd', END
		END
	};
	uint8_t nested_choices_exp_payload4[] = {LIST(2),
			LIST(1), LIST(1),
				LIST(3),
					LIST(1), 0x65, 'h', 'e', 'l', 'l', 'o', END
					0xf6,
					LIST(1), 0x65, 'w', 'o', 'r', 'l', 'd', END
				END
			END END
			0xf6,
		END
	};
	uint8_t nested_choices_exp_payload5[] = {LIST(2),
			MAP(2),
				0, LIST(1), 0x65, 'h', 'e', 'l', 'l', 'o', END
				1, LIST(1), 0x65, 'w', 'o', 'r', 'l', 'd', END
			END
			0xf6,
		END
	};
	struct Choice1 input1 = {.Choice1_choice = Choice1_nil_c};
	struct Choice2 input2 = {.Choice2_choice = Choice2_nil_c};
	struct Choice3 input3 = {.Choice3_choice = Choice3_nil_c};
	struct Choice4 input4 = {.Choice4_choice = Choice4_nil_c};
	struct Choice5 input5 = {.Choice5_choice = Choice5_nil_c};

	uint8_t payload[50];

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Choice1(payload,
		sizeof(payload), &input1, NULL), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Choice2(payload,
		sizeof(payload), &input2, NULL), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Choice3(payload,
		sizeof(payload), &input3, NULL), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Choice4(payload,
		sizeof(payload), &input4, NULL), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Choice5(payload,
		sizeof(payload), &input5, NULL), NULL);
	zassert_mem_equal(payload, nested_choices_exp_payload1, sizeof(nested_choices_exp_payload1));
	zassert_mem_equal(payload, nested_choices_exp_payload1, sizeof(nested_choices_exp_payload1));
	zassert_mem_equal(payload, nested_choices_exp_payload1, sizeof(nested_choices_exp_payload1));
	zassert_mem_equal(payload, nested_choices_exp_payload1, sizeof(nested_choices_exp_payload1));
	zassert_mem_equal(payload, nested_choices_exp_payload1, sizeof(nested_choices_exp_payload1));

	input1.Choice1_choice = Choice1_tstr_l_l_c;
	input1.tstr_count = 2;
	input1.tstr[0].value = "hello";
	input1.tstr[0].len = 5;
	input1.tstr[1].value = "world";
	input1.tstr[1].len = 5;

	input2.Choice2_choice = Choice2_tstr_l_l_c;
	input2.tstr_count = 2;
	input2.tstr[0].value = "hello";
	input2.tstr[0].len = 5;
	input2.tstr[1].value = "world";
	input2.tstr[1].len = 5;

	input3.Choice3_choice = Choice3_union_l_c;
	input3.Union_count = 2;
	input3.Union[0].Union_choice = union_tstr_l_tstr_c;
	input3.Union[1].Union_choice = union_tstr_l_tstr_c;
	input3.Union[0].tstr.value = "hello";
	input3.Union[0].tstr.len = 5;
	input3.Union[1].tstr.value = "world";
	input3.Union[1].tstr.len = 5;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Choice1(payload,
		sizeof(payload), &input1, NULL), NULL);
	zassert_mem_equal(payload, nested_choices_exp_payload2, sizeof(nested_choices_exp_payload2));

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Choice2(payload,
		sizeof(payload), &input2, NULL), NULL);
	zassert_mem_equal(payload, nested_choices_exp_payload2, sizeof(nested_choices_exp_payload2));

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Choice3(payload,
		sizeof(payload), &input3, NULL), NULL);
	zassert_mem_equal(payload, nested_choices_exp_payload2, sizeof(nested_choices_exp_payload2));

	input3.Choice3_choice = Choice3_union_l_c;
	input3.Union_count = 3;
	input3.Union[0].Union_choice = union_tstr_l_tstr_c;
	input3.Union[1].Union_choice = Choice3_union_l_union_nil_c;
	input3.Union[2].Union_choice = union_tstr_l_tstr_c;
	input3.Union[0].tstr.value = "hello";
	input3.Union[0].tstr.len = 5;
	input3.Union[2].tstr.value = "world";
	input3.Union[2].tstr.len = 5;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Choice3(payload,
		sizeof(payload), &input3, NULL), NULL);
	zassert_mem_equal(payload, nested_choices_exp_payload3, sizeof(nested_choices_exp_payload3));

	input4.Choice4_choice = Choice4_union_l_c;
	input4.Union_count = 2;
	input4.Union[0].Union_choice = Choice3_m_l_l_Choice3_m_l_Choice3_m_c;

	input4.Union[0].Choice3_m.Choice3_choice = Choice3_union_l_c;
	input4.Union[0].Choice3_m.Union_count = 3;
	input4.Union[0].Choice3_m.Union[0].Union_choice = union_tstr_l_tstr_c;
	input4.Union[0].Choice3_m.Union[1].Union_choice = Choice3_union_l_union_nil_c;
	input4.Union[0].Choice3_m.Union[2].Union_choice = union_tstr_l_tstr_c;
	input4.Union[0].Choice3_m.Union[0].tstr.value = "hello";
	input4.Union[0].Choice3_m.Union[0].tstr.len = 5;
	input4.Union[0].Choice3_m.Union[2].tstr.value = "world";
	input4.Union[0].Choice3_m.Union[2].tstr.len = 5;

	input4.Union[1].Union_choice = Choice4_union_l_union_nil_c;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Choice4(payload,
		sizeof(payload), &input4, NULL), NULL);

	zassert_mem_equal(payload, nested_choices_exp_payload4, sizeof(nested_choices_exp_payload4));

	input5.Choice5_choice = Choice5_union_l_c;
	input5.Union_count = 2;
	input5.Union[0].Union_choice = union_map_c;
	input5.Union[0].tstr_l_count = 2;
	input5.Union[0].tstr_l[0].tstr_l_key = 0;
	input5.Union[0].tstr_l[0].tstr.value = "hello";
	input5.Union[0].tstr_l[0].tstr.len = 5;
	input5.Union[0].tstr_l[1].tstr_l_key = 1;
	input5.Union[0].tstr_l[1].tstr.value = "world";
	input5.Union[0].tstr_l[1].tstr.len = 5;
	input5.Union[1].Union_choice = Choice5_union_l_union_nil_c;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Choice5(payload,
		sizeof(payload), &input5, NULL), NULL);
	zassert_mem_equal(payload, nested_choices_exp_payload5, sizeof(nested_choices_exp_payload5));
}


ZTEST(cbor_encode_test3, test_optlist)
{
	uint8_t optlist_exp_payload1[] = {LIST(1),
			10,
		END
	};
	uint8_t optlist_exp_payload2[] = {LIST(2),
			11,
			LIST(1), 0x18, 100, END
		END
	};
	struct OptList input;
	uint8_t payload[20];

	input.uint_present = false;
	input.optlist_int= 10;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_OptList(payload,
		sizeof(payload), &input, NULL));
	zassert_mem_equal(payload, optlist_exp_payload1, sizeof(optlist_exp_payload1));

	input.uint_present = true;
	input.optlist_int = 11;
	input.uint = 100;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_OptList(payload,
		sizeof(payload), &input, NULL));
	zassert_mem_equal(payload, optlist_exp_payload2, sizeof(optlist_exp_payload2));
}


ZTEST(cbor_encode_test3, test_union_default)
{
	uint8_t union_default_exp_payload1[] = {LIST(2), 1, 0xF6, END};
	uint8_t union_default_exp_payload2[] = {LIST(4), 2, 2, 0xF6, LIST(0), END END};
	struct UnionDefault input;
	uint8_t payload[20];

	input.Int = 1;
	input.foo_present = false;
	input.bar_present = true;
	input.baz_present = false;
	input.bar.bar_choice = UnionDefault_bar_noBar_c;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionDefault(payload,
		sizeof(payload), &input, NULL));
	zassert_mem_equal(payload, union_default_exp_payload1, sizeof(union_default_exp_payload1));

	input.Int = 2;
	input.foo_present = true;
	input.bar_present = true;
	input.baz_present = true;
	input.foo.foo_choice = UnionDefault_foo_FooB_c;
	input.bar.bar_choice = UnionDefault_bar_noBar_c;
	input.baz.baz_choice = baz_BazA_c;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionDefault(payload,
		sizeof(payload), &input, NULL));
	zassert_mem_equal(payload, union_default_exp_payload2, sizeof(union_default_exp_payload2));
}


/* Test encoding EmptyFirst with and without the empty lists/maps, and that encoding into a payload
 * that is too small fails with ZCBOR_ERR_NO_PAYLOAD no matter where the payload runs out, e.g. in
 * the header of one of the empty lists/maps. */
ZTEST(cbor_encode_test3, test_empty_first)
{
	uint8_t exp_empty_first_payload1[] = {LIST(9),
		LIST(2), 1, 2, END
		MAP(1), 3, 4, END
		LIST(1), 0x61, 'a', END
		MAP(1), 0x61, 'b', 5, END
		LIST(0), END
		LIST(0), END
		LIST(1), 0xF5, END
		MAP(0), END
		MAP(1), 0xF4, 6, END
		END
	};
	uint8_t exp_empty_first_payload2[] = {LIST(8),
		LIST(0), END
		MAP(0), END
		LIST(0), END
		LIST(1), 0x61, 'a', END
		MAP(0), END
		MAP(1), 0x61, 'b', 5, END
		LIST(1), 0xF5, END
		MAP(1), 0xF4, 6, END
		END
	};
	struct EmptyFirst input;
	uint8_t payload[50];
	size_t num_encode;
	int err;

	input.ef_union_list_choice = ef_union_list_EFIntList_c;
	input.Int[0] = 1;
	input.Int[1] = 2;
	input.Int_count = 2;
	input.ef_union_map_choice = ef_union_map_EFIntMap_c;
	input.intint[0].intint_key = 3;
	input.intint[0].intint = 4;
	input.intint_count = 1;
	input.ef_opt_list_present = false;
	input.tstr[0].value = (const uint8_t *)"a";
	input.tstr[0].len = 1;
	input.tstr_count = 1;
	input.ef_opt_map_present = false;
	input.tstrint[0].tstrint_key.value = (const uint8_t *)"b";
	input.tstrint[0].tstrint_key.len = 1;
	input.tstrint[0].tstrint = 5;
	input.tstrint_count = 1;
	input.ef_rep_list_count = 2;
	input.Bool[0] = true;
	input.Bool_count = 1;
	input.ef_rep_map_count = 1;
	input.boolint[0].boolint_key = false;
	input.boolint[0].boolint = 6;
	input.boolint_count = 1;

	err = cbor_encode_EmptyFirst(payload, sizeof(payload), &input, &num_encode);
	zassert_equal(ZCBOR_SUCCESS, err, "%s\n", zcbor_error_str(err));
	zassert_equal(sizeof(exp_empty_first_payload1), num_encode, NULL);
	zassert_mem_equal(exp_empty_first_payload1, payload, num_encode, NULL);

	for (size_t i = 0; i < sizeof(exp_empty_first_payload1); i++) {
		err = cbor_encode_EmptyFirst(payload, i, &input, &num_encode);
		zassert_equal(ZCBOR_ERR_NO_PAYLOAD, err, "len %zu: %s\n", i, zcbor_error_str(err));
	}

	input.ef_union_list_choice = ef_union_list_EFEmptyList_c;
	input.ef_union_map_choice = ef_union_map_EFEmptyMap_c;
	input.ef_opt_list_present = true;
	input.ef_opt_map_present = true;
	input.ef_rep_list_count = 0;
	input.ef_rep_map_count = 0;

	err = cbor_encode_EmptyFirst(payload, sizeof(payload), &input, &num_encode);
	zassert_equal(ZCBOR_SUCCESS, err, "%s\n", zcbor_error_str(err));
	zassert_equal(sizeof(exp_empty_first_payload2), num_encode, NULL);
	zassert_mem_equal(exp_empty_first_payload2, payload, num_encode, NULL);

	for (size_t i = 0; i < sizeof(exp_empty_first_payload2); i++) {
		err = cbor_encode_EmptyFirst(payload, i, &input, &num_encode);
		zassert_equal(ZCBOR_ERR_NO_PAYLOAD, err, "len %zu: %s\n", i, zcbor_error_str(err));
	}
}


ZTEST(cbor_encode_test3, test_count_union)
{
	uint8_t count_union_exp_payload1[] = {0xf6};
	uint8_t count_union_exp_payload2[] = {1, 1};

	struct CountUnion input;
	uint8_t payload[6];

	input.CountUnion_choice = CountUnion_nil_c;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_CountUnion(payload,
		sizeof(payload), &input, NULL));
	zassert_mem_equal(payload, count_union_exp_payload1, sizeof(count_union_exp_payload1));

	input.CountUnion_choice = CountUnion_uint1_c;
	input.uint1_count = 2;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_CountUnion(payload,
		sizeof(payload), &input, NULL));
	zassert_mem_equal(payload, count_union_exp_payload2, sizeof(count_union_exp_payload2));
}


ZTEST(cbor_encode_test3, test_tags)
{
	uint8_t tags_exp_payload1[] = {LIST(2),
		0xD9, 0x04, 0xd2, 0x01, /* #6.1234(1) */
		0xD9, 0x0d, 0x80, 0xf5, /* #6.3456(true) */
		END
	};
	uint8_t tags_exp_payload2[] = {LIST(2),
		0xD9, 0x09, 0x29, 0x10, /* #6.2345(16) */
		0xD9, 0x0d, 0x80, 0xf4, /* #6.3456(false) */
		END
	};
	uint8_t tags_exp_payload3[] = {LIST(3),
		0xD9, 0x04, 0xd2, 0x01, /* #6.1234(1) */
		0xD9, 0x0d, 0x80, 0xf5, /* #6.3456(true) */
		0xD9, 0x04, 0xd2, 0x18, 0x63, /* #6.1234(99) */
		END
	};
	uint8_t tags_exp_payload4[] = {LIST(3),
		0xD9, 0x09, 0x29, 0x24, /* #6.2345(-5) */
		0xD9, 0x0d, 0x80, 0xf4, /* #6.3456(false) */
		0xD9, 0x04, 0xd2, 0x07, /* #6.1234(7) */
		END
	};
	struct Tags input;
	uint8_t payload[30];

	input.tag1_choice = Tags_tag1_tag1_alt1_c;
	input.tag1_alt1 = 1;
	input.tag2_choice = Tags_tag2_tag2_alt2_c;
	input.tag2_alt2 = true;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Tags(payload,
		sizeof(payload), &input, NULL));
	zassert_mem_equal(payload, tags_exp_payload1, sizeof(tags_exp_payload1));

	input.tag1_choice = Tags_tag1_tag1_alt2_c;
	input.tag1_alt2 = 16;
	input.tag2_choice = Tags_tag2_tag2_alt2_c;
	input.tag2_alt2 = false;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Tags(payload,
		sizeof(payload), &input, NULL));
	zassert_mem_equal(payload, tags_exp_payload2, sizeof(tags_exp_payload2));

	input.tag1_choice = Tags_tag1_tag1_alt1_c;
	input.tag1_alt1 = 1;
	input.tag2_choice = tag2_t3456bool_l_c;
	input.t3456bool = true;
	input.t1234int = 99;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Tags(payload,
		sizeof(payload), &input, NULL));
	zassert_mem_equal(payload, tags_exp_payload3, sizeof(tags_exp_payload3));

	input.tag1_choice = Tags_tag1_tag1_alt2_c;
	input.tag1_alt2 = -5;
	input.tag2_choice = tag2_t3456bool_l_c;
	input.t3456bool = false;
	input.t1234int = 7;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Tags(payload,
		sizeof(payload), &input, NULL));
	zassert_mem_equal(payload, tags_exp_payload4, sizeof(tags_exp_payload4));
}


ZTEST(cbor_encode_test3, test_quant_tags)
{
	uint8_t quant_tags_exp_payload1[] = {LIST(4),
		0xD8, 0xFF, 0x40, /* #6.255(h'') */
		0xD9, 0x01, 0x00, 0xF5, /* #6.256(true) */
		0xD9, 0x01, 0x00, 0xF4, /* #6.256(false) */
		MAP(0), END
		END
	};
	uint8_t quant_tags_exp_payload2[] = {LIST(D),
		0xD7, 0x05, /* #6.23(5) */
		0xD8, 0x18, 0x61, 'a', /* #6.24("a") */
		0xD8, 0x18, 0x62, 'b', 'c', /* #6.24("bc") */
		0xD8, 0xFF, 0x41, 0x01, /* #6.255(h'01') */
		0xD8, 0xFF, 0x40, /* #6.255(h'') */
		0xD9, 0x01, 0x00, 0xF5, /* #6.256(true) */
		0xD9, 0x01, 0x00, 0xF4, /* #6.256(false) */
		0xD9, 0x01, 0x00, 0xF5, /* #6.256(true) */
		0xD9, 0xFF, 0xFF, 0xFB, 0x3F, 0xF8, 0, 0, 0, 0, 0, 0, /* #6.65535(1.5) */
		0xDA, 0x00, 0x01, 0x00, 0x00, 0xF6, /* #6.65536(nil) */
		0xDA, 0xFF, 0xFF, 0xFF, 0xFF, 0x05, /* #6.4294967295(5) */
		0xDA, 0xFF, 0xFF, 0xFF, 0xFF, 0x05, /* #6.4294967295(5) */
		MAP(1), 0x01, 0xDA, 0x12, 0x34, 0x56, 0x78, 0x07, END /* {1: #6.305419896(7)} */
		END
	};
	struct QuantTags input = {
		.qt_bstr = {{.value = NULL, .len = 0}},
		.qt_bstr_count = 1,
		.qt_bool = {true, false},
		.qt_bool_count = 2,
	};
	uint8_t payload[100];
	size_t num_encode;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_QuantTags(payload,
		sizeof(payload), &input, &num_encode));
	zassert_equal(sizeof(quant_tags_exp_payload1), num_encode, NULL);
	zassert_mem_equal(payload, quant_tags_exp_payload1, sizeof(quant_tags_exp_payload1));

	input = (struct QuantTags){
		.qt_uint = 5,
		.qt_uint_present = true,
		.qt_tstr = {{.value = "a", .len = 1}, {.value = "bc", .len = 2}},
		.qt_tstr_count = 2,
		.qt_bstr = {{.value = (uint8_t []){1}, .len = 1}, {.value = NULL, .len = 0}},
		.qt_bstr_count = 2,
		.qt_bool = {true, false, true},
		.qt_bool_count = 3,
		.qt_float = 1.5,
		.qt_float_present = true,
		.qt_nil_present = true,
		.qt_five_count = 2,
		.qt_map_val = 7,
		.qt_map_val_present = true,
	};
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_QuantTags(payload,
		sizeof(payload), &input, &num_encode));
	zassert_equal(sizeof(quant_tags_exp_payload2), num_encode, NULL);
	zassert_mem_equal(payload, quant_tags_exp_payload2, sizeof(quant_tags_exp_payload2));
}


ZTEST(cbor_encode_test3, test_tagged_int_union)
{
	uint8_t tagged_int_union_exp_payload1[] = {0xC5, 0x01}; /* #6.5(1) */
	uint8_t tagged_int_union_exp_payload2[] = {0xC6, 0x02}; /* #6.6(2) */
	struct TaggedIntUnion input;
	uint8_t payload[10];
	size_t out_len;

	input.TaggedIntUnion_choice = TaggedIntUnion_tagged_one_c;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_TaggedIntUnion(payload,
		sizeof(payload), &input, &out_len));
	zassert_equal(sizeof(tagged_int_union_exp_payload1), out_len);
	zassert_mem_equal(payload, tagged_int_union_exp_payload1, sizeof(tagged_int_union_exp_payload1));

	input.TaggedIntUnion_choice = TaggedIntUnion_tagged_two_c;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_TaggedIntUnion(payload,
		sizeof(payload), &input, &out_len));
	zassert_equal(sizeof(tagged_int_union_exp_payload2), out_len);
	zassert_mem_equal(payload, tagged_int_union_exp_payload2, sizeof(tagged_int_union_exp_payload2));
}


ZTEST(cbor_encode_test3, test_tagged_member_union)
{
	uint8_t tagged_member_union_exp_payload1[] = {LIST(4),
		0xCB, 0x05, 0xCC, 0x06, 0xCD, 0x07, 0xCF, 0x08, END
	};
	uint8_t tagged_member_union_exp_payload2[] = {LIST(3), 0xF5, 0x61, 'a', 0xCE, 0xF4, END};
	uint8_t tagged_member_union_exp_payload3[] = {LIST(4), 0xF4, 0xF5, 0xCD, 0x01, 0xF5, END};
	struct TaggedMemberUnion input1 = {
		.tmu1_choice = TaggedMemberUnion_tmu1_tmu1_tagged_c,
		.tmu1_tagged = 5,
		.tmu2_choice = TaggedMemberUnion_tmu2_union_c,
		.Union_choice = TaggedMemberUnion_tmu2_union_tmu2_tagged_c,
		.tmu2_tagged = 6,
		.tmu3_choice = TaggedMemberUnion_tmu3_tmu3_tagged_c,
		.tmu3_tagged = 7,
		.tmu4_present = true,
		.tmu4 = {
			.tmu4_choice = TaggedMemberUnion_tmu4_tmu4_tagged_c,
			.tmu4_tagged = 8,
		},
	};
	struct TaggedMemberUnion input2 = {
		.tmu1_choice = TaggedMemberUnion_tmu1_tmu1_bool_c,
		.tmu1_bool = true,
		.tmu2_choice = TaggedMemberUnion_tmu2_union_c,
		.Union_choice = TaggedMemberUnion_tmu2_union_tmu2_tstr_c,
		.tmu2_tstr = {.value = (const uint8_t *)"a", .len = 1},
		.tmu3_choice = TaggedMemberUnion_tmu3_tmu3_tagged_bool_c,
		.tmu3_tagged_bool = false,
		.tmu4_present = false,
	};
	struct TaggedMemberUnion input3 = {
		.tmu1_choice = TaggedMemberUnion_tmu1_tmu1_bool_c,
		.tmu1_bool = false,
		.tmu2_choice = TaggedMemberUnion_tmu2_tmu2_bool_c,
		.tmu2_bool = true,
		.tmu3_choice = TaggedMemberUnion_tmu3_tmu3_tagged_c,
		.tmu3_tagged = 1,
		.tmu4_present = true,
		.tmu4 = {
			.tmu4_choice = TaggedMemberUnion_tmu4_tmu4_bool_c,
			.tmu4_bool = true,
		},
	};
	uint8_t payload[20];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_TaggedMemberUnion(payload,
		sizeof(payload), &input1, &out_len));
	zassert_equal(sizeof(tagged_member_union_exp_payload1), out_len);
	zassert_mem_equal(payload, tagged_member_union_exp_payload1, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_TaggedMemberUnion(payload,
		sizeof(payload), &input2, &out_len));
	zassert_equal(sizeof(tagged_member_union_exp_payload2), out_len);
	zassert_mem_equal(payload, tagged_member_union_exp_payload2, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_TaggedMemberUnion(payload,
		sizeof(payload), &input3, &out_len));
	zassert_equal(sizeof(tagged_member_union_exp_payload3), out_len);
	zassert_mem_equal(payload, tagged_member_union_exp_payload3, out_len);
}


ZTEST(cbor_encode_test3, test_opt_float_then_int)
{
	struct OptFloatThenInt input_invalid = {
		.optfloat_present = true,
		.optfloat = 15.0,
		.mandint = 7,
	};
	uint8_t output[20];
	size_t out_len;

	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_OptFloatThenInt(output,
		sizeof(output), &input_invalid, &out_len));
}


ZTEST(cbor_encode_test3, test_opt_float_union)
{
	uint8_t exp_payload_branch1[] = {LIST(1),
		0xFB, 0x40, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 3.0 */
		END
	};
	uint8_t exp_payload_branch2[] = {LIST(1),
		0xFB, 0x40, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 6.5 */
		END
	};
	struct OptFloatUnion input1 = {
		.foo_present = true,
		.foo = {
			.foo_choice = OptFloatUnion_foo_oneToFive_c,
			.oneToFive = 3.0,
		},
	};
	struct OptFloatUnion input2 = {
		.foo_present = true,
		.foo = {
			.foo_choice = OptFloatUnion_foo_sixToTen_c,
			.sixToTen = 6.5,
		},
	};
	struct OptFloatUnion input3_inv = {
		.foo_present = true,
		.foo = {
			.foo_choice = OptFloatUnion_foo_sixToTen_c,
			.sixToTen = 3.0,
		},
	};
	uint8_t output[20];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_OptFloatUnion(output,
		sizeof(output), &input1, &out_len));
	zassert_equal(sizeof(exp_payload_branch1), out_len);
	zassert_mem_equal(exp_payload_branch1, output, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_OptFloatUnion(output,
		sizeof(output), &input2, &out_len));
	zassert_equal(sizeof(exp_payload_branch2), out_len);
	zassert_mem_equal(exp_payload_branch2, output, out_len);

	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_encode_OptFloatUnion(output,
		sizeof(output), &input3_inv, &out_len));
}


ZTEST(cbor_encode_test3, test_wrap_list_group)
{
	const uint8_t exp_payload[] = {LIST(2), 0x01, 0x02, END};
	struct InnerPair input = {.left = 1, .right = 2};
	uint8_t output[10];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_WrapListGroup(output,
		sizeof(output), &input, &out_len), NULL);
	zassert_equal(sizeof(exp_payload), out_len, NULL);
	zassert_mem_equal(exp_payload, output, out_len, NULL);
}


ZTEST(cbor_encode_test3, test_wrap_list_group_opt)
{
	const uint8_t exp_present[] = {LIST(2), 0x01, 0x02, END};
	const uint8_t exp_absent[] = {LIST(0), END};
	struct WrapListGroupOpt1 input1 = {
		.inner1_present = true,
		.inner1 = {.left = 1, .right = 2},
	};
	struct InnerPairOpt input2 = {
		.InnerPairOpt_present = true,
		.InnerPairOpt = {.left = 1, .right = 2},
	};
	uint8_t output[10];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_WrapListGroupOpt1(output,
		sizeof(output), &input1, &out_len), NULL);
	zassert_equal(sizeof(exp_present), out_len, NULL);
	zassert_mem_equal(exp_present, output, out_len, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_WrapListGroupOpt2(output,
		sizeof(output), &input2, &out_len), NULL);
	zassert_equal(sizeof(exp_present), out_len, NULL);
	zassert_mem_equal(exp_present, output, out_len, NULL);

	input1.inner1_present = false;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_WrapListGroupOpt1(output,
		sizeof(output), &input1, &out_len), NULL);
	zassert_equal(sizeof(exp_absent), out_len, NULL);
	zassert_mem_equal(exp_absent, output, out_len, NULL);

	input2.InnerPairOpt_present = false;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_WrapListGroupOpt2(output,
		sizeof(output), &input2, &out_len), NULL);
	zassert_equal(sizeof(exp_absent), out_len, NULL);
	zassert_mem_equal(exp_absent, output, out_len, NULL);
}


ZTEST(cbor_encode_test3, test_wrap_list_group_opt_mult)
{
	const uint8_t exp_present[] = {LIST(4), 0x01, 0x02, 0x03, 0x04, END};
	struct WrapListGroupOptMult input = {
		.WrapListGroupOptMult_present = true,
		.WrapListGroupOptMult = {
			.inner_count = 2,
			.inner = {{.left = 1, .right = 2}, {.left = 3, .right = 4}},
		},
	};
	uint8_t output[12];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_WrapListGroupOptMult(output,
		sizeof(output), &input, &out_len), NULL);
	zassert_equal(sizeof(exp_present), out_len, NULL);
	zassert_mem_equal(exp_present, output, out_len, NULL);

	input.WrapListGroupOptMult_present = false;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_WrapListGroupOptMult(output,
		sizeof(output), &input, &out_len), NULL);
	zassert_equal(0, out_len, NULL);
}


ZTEST(cbor_encode_test3, test_opt_cbor)
{
	const uint8_t exp_present[] = {LIST(1), 0x42, 0x18, 0x2A, END};
	const uint8_t exp_absent[] = {LIST(0), END};
	struct OptCbor input = {
		.cbor_present = true,
		.cbor = {
			.cbor_cbor = 42,
		},
	};
	uint8_t output[10];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_OptCbor(output,
		sizeof(output), &input, &out_len), NULL);
	zassert_equal(sizeof(exp_present), out_len, NULL);
	zassert_mem_equal(exp_present, output, out_len, NULL);

	input.cbor_present = false;
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_OptCbor(output,
		sizeof(output), &input, &out_len), NULL);
	zassert_equal(sizeof(exp_absent), out_len, NULL);
	zassert_mem_equal(exp_absent, output, out_len, NULL);
}


ZTEST(cbor_encode_test3, test_opt_type_union)
{
	uint8_t exp_none_payload[] = {LIST(1), 0xF6, END};
	uint8_t exp_all_payload1[] = {LIST(4),
		0x05, 0x62, 'h', 'i',
		0xFB, 0x3F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 1.5 */
		0xF6, END
	};
	uint8_t exp_all_payload2[] = {LIST(4), 0xF5, 0x22, 0x42, 0x01, 0x02, 0xF6, END};
	uint8_t exp_second_payload[] = {LIST(2), 0x22, 0xF6, END};
	struct OptTypeUnion input_none = {
		.uint_or_bool_present = false,
		.tstr_or_nint_present = false,
		.named_present = false,
	};
	struct OptTypeUnion input_all1 = {
		.uint_or_bool_present = true,
		.uint_or_bool = {
			.uint_or_bool_choice = OptTypeUnion_uint_or_bool_uint_c,
			.uint = 5,
		},
		.tstr_or_nint_present = true,
		.tstr_or_nint = {
			.tstr_or_nint_choice = OptTypeUnion_tstr_or_nint_tstr_c,
			.tstr = {.value = "hi", .len = 2},
		},
		.named_present = true,
		.named = {
			.named_choice = OptTypeUnion_named_OptTypeUnionFloat_m_c,
			.OptTypeUnionFloat_m = 1.5,
		},
	};
	struct OptTypeUnion input_all2 = {
		.uint_or_bool_present = true,
		.uint_or_bool = {
			.uint_or_bool_choice = OptTypeUnion_uint_or_bool_bool_c,
			.Bool = true,
		},
		.tstr_or_nint_present = true,
		.tstr_or_nint = {
			.tstr_or_nint_choice = OptTypeUnion_tstr_or_nint_nint_c,
			.nint = -3,
		},
		.named_present = true,
		.named = {
			.named_choice = OptTypeUnion_named_OptTypeUnionBstr_m_c,
			.OptTypeUnionBstr_m = {.value = (const uint8_t *)"\x01\x02", .len = 2},
		},
	};
	struct OptTypeUnion input_second = {
		.uint_or_bool_present = false,
		.tstr_or_nint_present = true,
		.tstr_or_nint = {
			.tstr_or_nint_choice = OptTypeUnion_tstr_or_nint_nint_c,
			.nint = -3,
		},
		.named_present = false,
	};
	uint8_t output[20];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_OptTypeUnion(output,
		sizeof(output), &input_none, &out_len));
	zassert_equal(sizeof(exp_none_payload), out_len);
	zassert_mem_equal(exp_none_payload, output, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_OptTypeUnion(output,
		sizeof(output), &input_all1, &out_len));
	zassert_equal(sizeof(exp_all_payload1), out_len);
	zassert_mem_equal(exp_all_payload1, output, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_OptTypeUnion(output,
		sizeof(output), &input_all2, &out_len));
	zassert_equal(sizeof(exp_all_payload2), out_len);
	zassert_mem_equal(exp_all_payload2, output, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_OptTypeUnion(output,
		sizeof(output), &input_second, &out_len));
	zassert_equal(sizeof(exp_second_payload), out_len);
	zassert_mem_equal(exp_second_payload, output, out_len);
}


ZTEST(cbor_encode_test3, test_cbor_bstr_list)
{
	uint8_t cbor_bstr_list_exp_payload1[] = {LIST(1),
		STR_LEN(0x49, 1), LIST(2), 0x18, 42, 0x65, 'h', 'e', 'l', 'l', 'o', END
		END
	};
	uint8_t cbor_bstr_list_exp_payload2[] = {LIST(0), END};

	struct CborBstrList input1 = {
		.bstr_present = true,
		.bstr.Int = 42,
		.bstr.tstr.value = "hello",
		.bstr.tstr.len = 5
	};
	struct CborBstrList input2 = {
		.bstr_present = false,
	};
	uint8_t payload[20];
	size_t num_decode;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_CborBstrList(payload,
		sizeof(payload), &input1, &num_decode));
	zassert_mem_equal(cbor_bstr_list_exp_payload1, payload, sizeof(cbor_bstr_list_exp_payload1));
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_CborBstrList(payload,
		sizeof(payload), &input2, &num_decode));
	zassert_mem_equal(cbor_bstr_list_exp_payload2, payload, sizeof(cbor_bstr_list_exp_payload2));
}


ZTEST_SUITE(cbor_encode_test3, NULL, NULL, NULL, NULL, NULL);
