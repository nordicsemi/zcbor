/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The values of the #defines (including those used by union enums) in defines_types.h are changed
 * before this is compiled, see CMakeLists.txt. The tests check that both the encoding and the
 * decoding code follow the new values.
 */

#include <zephyr/ztest.h>
#include <zcbor_encode.h>
#include "defines_decode.h"
#include "defines_encode.h"

#ifndef ZCBOR_CANONICAL
#define TEST_INDEFINITE_LENGTH_ARRAYS
#endif
#include <common_test.h>

#define ZSTR(lit) ((struct zcbor_string){.value = (const uint8_t *)(lit), .len = sizeof(lit) - 1})

#define zassert_zstr_equal(exp, actual) \
	do { \
		zassert_equal((exp).len, (actual).len); \
		zassert_mem_equal((exp).value, (actual).value, (exp).len); \
	} while (0)

#define PAYLOAD_LEN(state, payload) ((size_t)((state)->payload - (payload)))


ZTEST(cbor_encode_test5, test_val)
{
	/* 5 -> 7, -5 -> -7, "hello" -> "hello world", 'abc' -> 'abcdef', 1.5 -> 2.5, true -> false */
	const uint8_t exp_payload[] = {
		LIST(6),
			0x07,
			0x26,
			0x6B, 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd',
			0x46, 'a', 'b', 'c', 'd', 'e', 'f',
			0xFB, 0x40, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xF4,
		END
	};
	uint8_t output[40];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Values(output, sizeof(output), NULL, &out_len));
	zassert_equal(sizeof(exp_payload), out_len);
	zassert_mem_equal(exp_payload, output, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Values(exp_payload, sizeof(exp_payload), NULL, &out_len));
	zassert_equal(sizeof(exp_payload), out_len);
}


ZTEST(cbor_encode_test5, test_key_val)
{
	/* 1 => uint -> 100 => uint, "key" => uint -> "other key" => uint */
	const uint8_t exp_payload[] = {
		MAP(2),
			0x18, 100, 0x01,
			0x69, 'o', 't', 'h', 'e', 'r', ' ', 'k', 'e', 'y', 0x02,
		END
	};
	const struct KeyValues input = {.KeyValues_uint1uint = 1, .KeyValues_key = 2};
	struct KeyValues result;
	uint8_t output[20];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_KeyValues(output, sizeof(output), &input, &out_len));
	zassert_equal(sizeof(exp_payload), out_len);
	zassert_mem_equal(exp_payload, output, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_KeyValues(exp_payload, sizeof(exp_payload), &result, NULL));
	zassert_equal(1, result.KeyValues_uint1uint);
	zassert_equal(2, result.KeyValues_key);
}


ZTEST(cbor_encode_test5, test_union_val)
{
	/* The VAL defines, and therefore the enum values, for 1 in value_union and 2 => tstr in
	 * key_union were changed to 300 and 70000.
	 * The members of tagged_union start with tags, so their values are #defines instead
	 * (2 -> 7 in #6.2(2)). */
	const uint8_t exp_payload1[] = {
		LIST(3),
			0x19, 0x01, 0x2C,
			MAP(1), 0x1A, 0x00, 0x01, 0x11, 0x70, 0x61, 'x', END
			0xC1, 0x01,
		END
	};
	const uint8_t exp_payload2[] = {
		LIST(3),
			0x02,
			MAP(1), 0x03, 0x41, 'y', END
			0xC2, 0x07,
		END
	};
	const uint8_t old_payload1[] = {
		LIST(3),
			0x01,
			MAP(1), 0x1A, 0x00, 0x01, 0x11, 0x70, 0x61, 'x', END
			0xC1, 0x01,
		END
	};
	const uint8_t old_payload2[] = {
		LIST(3),
			0x19, 0x01, 0x2C,
			MAP(1), 0x02, 0x61, 'x', END
			0xC1, 0x01,
		END
	};
	const uint8_t old_payload3[] = {
		LIST(3),
			0x19, 0x01, 0x2C,
			MAP(1), 0x1A, 0x00, 0x01, 0x11, 0x70, 0x61, 'x', END
			0xC2, 0x02,
		END
	};
	const struct UnionValues input1 = {
		.UnionValues_value_union_choice = UnionValues_value_union_uint1_c,
		.union_uint2tstr = ZSTR("x"),
		.UnionValues_key_union_union_choice = union_uint2tstr_c,
		.UnionValues_tagged_union_choice = UnionValues_tagged_union_uint1_c,
	};
	const struct UnionValues input2 = {
		.UnionValues_value_union_choice = UnionValues_value_union_uint2_c,
		.union_uint3bstr = ZSTR("y"),
		.UnionValues_key_union_union_choice = union_uint3bstr_c,
		.UnionValues_tagged_union_choice = UnionValues_tagged_union_uint2_c,
	};
	struct UnionValues result;
	uint8_t output[20];
	size_t out_len;

	zassert_equal(300, UnionValues_value_union_uint1_c);
	zassert_equal(70000, union_uint2tstr_c);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionValues(output, sizeof(output), &input1, &out_len));
	zassert_equal(sizeof(exp_payload1), out_len);
	zassert_mem_equal(exp_payload1, output, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_UnionValues(output, sizeof(output), &input2, &out_len));
	zassert_equal(sizeof(exp_payload2), out_len);
	zassert_mem_equal(exp_payload2, output, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_UnionValues(exp_payload1, sizeof(exp_payload1), &result, NULL));
	zassert_equal(UnionValues_value_union_uint1_c, result.UnionValues_value_union_choice);
	zassert_equal(union_uint2tstr_c, result.UnionValues_key_union_union_choice);
	zassert_zstr_equal(ZSTR("x"), result.union_uint2tstr);
	zassert_equal(UnionValues_tagged_union_uint1_c, result.UnionValues_tagged_union_choice);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_UnionValues(exp_payload2, sizeof(exp_payload2), &result, NULL));
	zassert_equal(UnionValues_value_union_uint2_c, result.UnionValues_value_union_choice);
	zassert_equal(union_uint3bstr_c, result.UnionValues_key_union_union_choice);
	zassert_zstr_equal(ZSTR("y"), result.union_uint3bstr);
	zassert_equal(UnionValues_tagged_union_uint2_c, result.UnionValues_tagged_union_choice);

	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_UnionValues(old_payload1, sizeof(old_payload1), &result, NULL));
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_UnionValues(old_payload2, sizeof(old_payload2), &result, NULL));
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_UnionValues(old_payload3, sizeof(old_payload3), &result, NULL));
}


/* Check that both the encoder and the decoder return exp_err for the given values. */
static void check_ranges(uint8_t uint_range, int8_t int_range, double float_range, int exp_err)
{
	const struct Ranges input = {
		.Ranges_uint_range = uint_range,
		.Ranges_int_range = int_range,
		.Ranges_float_range = float_range,
	};
	struct Ranges result;
	uint8_t payload[30];
	uint8_t output[30];
	size_t out_len;
	ZCBOR_STATE_E(state, 1, payload, sizeof(payload), 1);

	zassert_true(zcbor_list_start_encode(state, 3));
	zassert_true(zcbor_uint32_put(state, uint_range));
	zassert_true(zcbor_int32_put(state, int_range));
	zassert_true(zcbor_float64_put(state, float_range));
	zassert_true(zcbor_list_end_encode(state, 3));

	zassert_equal(exp_err, cbor_encode_Ranges(output, sizeof(output), &input, &out_len));
	zassert_equal(exp_err, cbor_decode_Ranges(payload, PAYLOAD_LEN(state, payload), &result, NULL));

	if (exp_err == ZCBOR_SUCCESS) {
		zassert_equal(PAYLOAD_LEN(state, payload), out_len);
		zassert_mem_equal(payload, output, out_len);
		zassert_equal(uint_range, result.Ranges_uint_range);
		zassert_equal(int_range, result.Ranges_int_range);
		zassert_equal(float_range, result.Ranges_float_range);
	}
}


ZTEST(cbor_encode_test5, test_min_max_val)
{
	/* uint_range: [10, 20] -> [5, 200]
	 * int_range: (-20, -10) -> (-100, -5)
	 * float_range: [1.5, 2.5] -> [0.5, 3.5]
	 */
	const uint8_t exp_payload_max[] = {
		LIST(3),
			0x18, 200,
			0x25,
			0xFB, 0x40, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		END
	};
	const struct Ranges input_max = {
		.Ranges_uint_range = 200,
		.Ranges_int_range = -6,
		.Ranges_float_range = 3.5,
	};
	uint8_t output[30];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Ranges(output, sizeof(output), &input_max, &out_len));
	zassert_equal(sizeof(exp_payload_max), out_len);
	zassert_mem_equal(exp_payload_max, output, out_len);

	check_ranges(5, -99, 0.5, ZCBOR_SUCCESS);
	check_ranges(200, -6, 3.5, ZCBOR_SUCCESS);
	check_ranges(4, -99, 0.5, ZCBOR_ERR_WRONG_RANGE);
	check_ranges(201, -99, 0.5, ZCBOR_ERR_WRONG_RANGE);
	check_ranges(5, -100, 0.5, ZCBOR_ERR_WRONG_RANGE);
	check_ranges(5, -5, 0.5, ZCBOR_ERR_WRONG_RANGE);
	check_ranges(5, -99, 0.25, ZCBOR_ERR_WRONG_RANGE);
	check_ranges(5, -99, 3.75, ZCBOR_ERR_WRONG_RANGE);
}


#ifdef CONFIG_64BIT
typedef uint64_t bits_t;
#else
typedef uint32_t bits_t;
#endif


/* Check that both the encoder and the decoder return exp_err for the given value. */
static void check_bits(bits_t bits, int exp_err)
{
	bits_t result;
	uint8_t payload[10];
	uint8_t output[10];
	size_t out_len;
	ZCBOR_STATE_E(state, 0, payload, sizeof(payload), 1);

	zassert_true(zcbor_uint32_put(state, (uint32_t)bits));

	zassert_equal(exp_err, cbor_encode_Bits(output, sizeof(output), &bits, &out_len));
	zassert_equal(exp_err, cbor_decode_Bits(payload, PAYLOAD_LEN(state, payload), &result, NULL));

	if (exp_err == ZCBOR_SUCCESS) {
		zassert_equal(PAYLOAD_LEN(state, payload), out_len);
		zassert_mem_equal(payload, output, out_len);
		zassert_equal(bits, result);
	}
}


ZTEST(cbor_encode_test5, test_msk)
{
	/* The allowed bits were changed from 0, 1, and 3 to 0, 3, and 5. */
	const uint8_t exp_payload[] = {0x18, 0x29};
	const bits_t input = 0x29;
	uint8_t output[10];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Bits(output, sizeof(output), &input, &out_len));
	zassert_equal(sizeof(exp_payload), out_len);
	zassert_mem_equal(exp_payload, output, out_len);

	check_bits(0x29, ZCBOR_SUCCESS);
	check_bits(0x20, ZCBOR_SUCCESS);
	check_bits(0x09, ZCBOR_SUCCESS);
	check_bits(0x02, ZCBOR_ERR_WRONG_RANGE);
	check_bits(0x04, ZCBOR_ERR_WRONG_RANGE);
}


/* Check that both the encoder and the decoder return exp_err for the given values. */
static void check_sizes(struct zcbor_string fixed_bstr, struct zcbor_string ranged_tstr, int exp_err)
{
	const struct Sizes input = {
		.Sizes_fixed_bstr = fixed_bstr,
		.Sizes_ranged_tstr = ranged_tstr,
	};
	struct Sizes result;
	uint8_t payload[30];
	uint8_t output[30];
	size_t out_len;
	ZCBOR_STATE_E(state, 1, payload, sizeof(payload), 1);

	zassert_true(zcbor_list_start_encode(state, 2));
	zassert_true(zcbor_bstr_encode(state, &fixed_bstr));
	zassert_true(zcbor_tstr_encode(state, &ranged_tstr));
	zassert_true(zcbor_list_end_encode(state, 2));

	zassert_equal(exp_err, cbor_encode_Sizes(output, sizeof(output), &input, &out_len));
	zassert_equal(exp_err, cbor_decode_Sizes(payload, PAYLOAD_LEN(state, payload), &result, NULL));

	if (exp_err == ZCBOR_SUCCESS) {
		zassert_equal(PAYLOAD_LEN(state, payload), out_len);
		zassert_mem_equal(payload, output, out_len);
		zassert_zstr_equal(fixed_bstr, result.Sizes_fixed_bstr);
		zassert_zstr_equal(ranged_tstr, result.Sizes_ranged_tstr);
	}
}


ZTEST(cbor_encode_test5, test_size)
{
	/* fixed_bstr: size 3 -> 5, ranged_tstr: size 2..4 -> 1..8 */
	const uint8_t exp_payload[] = {
		LIST(2),
			0x45, '1', '2', '3', '4', '5',
			0x61, 'a',
		END
	};
	const struct Sizes input = {
		.Sizes_fixed_bstr = ZSTR("12345"),
		.Sizes_ranged_tstr = ZSTR("a"),
	};
	uint8_t output[30];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Sizes(output, sizeof(output), &input, &out_len));
	zassert_equal(sizeof(exp_payload), out_len);
	zassert_mem_equal(exp_payload, output, out_len);

	check_sizes(ZSTR("12345"), ZSTR("a"), ZCBOR_SUCCESS);
	check_sizes(ZSTR("12345"), ZSTR("abcdefgh"), ZCBOR_SUCCESS);
	check_sizes(ZSTR("123"), ZSTR("abc"), ZCBOR_ERR_WRONG_RANGE);
	check_sizes(ZSTR("123456"), ZSTR("abc"), ZCBOR_ERR_WRONG_RANGE);
	check_sizes(ZSTR("12345"), ZSTR(""), ZCBOR_ERR_WRONG_RANGE);
	check_sizes(ZSTR("12345"), ZSTR("abcdefghi"), ZCBOR_ERR_WRONG_RANGE);
}


ZTEST(cbor_encode_test5, test_default_val)
{
	/* 5 -> 7, "abc" -> "default" */
	const uint8_t payload_absent[] = {LIST(0), END};
	const uint8_t payload_present[] = {LIST(2), 0x01, 0x61, 'x', END};
	struct Defaults input = {0};
	struct Defaults result;
	uint8_t output[10];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Defaults(payload_absent, sizeof(payload_absent), &result, NULL));
	zassert_false(result.Defaults_uint_default_present);
	zassert_equal(7, result.Defaults_uint_default);
	zassert_false(result.Defaults_tstr_default_present);
	zassert_zstr_equal(ZSTR("default"), result.Defaults_tstr_default);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Defaults(payload_present, sizeof(payload_present), &result, NULL));
	zassert_true(result.Defaults_uint_default_present);
	zassert_equal(1, result.Defaults_uint_default);
	zassert_true(result.Defaults_tstr_default_present);
	zassert_zstr_equal(ZSTR("x"), result.Defaults_tstr_default);

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Defaults(output, sizeof(output), &input, &out_len));
	zassert_equal(sizeof(payload_absent), out_len);
	zassert_mem_equal(payload_absent, output, out_len);
}


/* Check that the encoder returns exp_err_enc and the decoder returns exp_err_dec when
 * encoding/decoding the given number of elements. */
static void check_quantities(size_t range_qty, size_t exact_qty, int exp_err_enc, int exp_err_dec)
{
	struct Quantities input = {
		.Quantities_range_qty_uint_count = range_qty,
		.Quantities_exact_qty_uint_count = exact_qty,
	};
	struct Quantities result;
	uint8_t payload[40];
	uint8_t output[40];
	size_t out_len;
	ZCBOR_STATE_E(state, 2, payload, sizeof(payload), 1);

	zassert_true(zcbor_list_start_encode(state, 2));
	zassert_true(zcbor_list_start_encode(state, range_qty));
	for (uint32_t i = 0; i < range_qty; i++) {
		zassert_true(zcbor_uint32_put(state, i));
		if (i < ARRAY_SIZE(input.Quantities_range_qty_uint)) {
			input.Quantities_range_qty_uint[i] = i;
		}
	}
	zassert_true(zcbor_list_end_encode(state, range_qty));
	zassert_true(zcbor_list_start_encode(state, exact_qty));
	for (uint32_t i = 0; i < exact_qty; i++) {
		zassert_true(zcbor_uint32_put(state, i));
		if (i < ARRAY_SIZE(input.Quantities_exact_qty_uint)) {
			input.Quantities_exact_qty_uint[i] = i;
		}
	}
	zassert_true(zcbor_list_end_encode(state, exact_qty));
	zassert_true(zcbor_list_end_encode(state, 2));

	zassert_equal(exp_err_enc, cbor_encode_Quantities(output, sizeof(output), &input, &out_len));
	zassert_equal(exp_err_dec, cbor_decode_Quantities(payload, PAYLOAD_LEN(state, payload), &result, NULL));

	if (exp_err_enc == ZCBOR_SUCCESS) {
		zassert_equal(PAYLOAD_LEN(state, payload), out_len);
		zassert_mem_equal(payload, output, out_len);
	}
	if (exp_err_dec == ZCBOR_SUCCESS) {
		zassert_equal(range_qty, result.Quantities_range_qty_uint_count);
		zassert_equal(exact_qty, result.Quantities_exact_qty_uint_count);
		zassert_mem_equal(input.Quantities_range_qty_uint, result.Quantities_range_qty_uint,
			range_qty * sizeof(input.Quantities_range_qty_uint[0]));
		zassert_mem_equal(input.Quantities_exact_qty_uint, result.Quantities_exact_qty_uint,
			exact_qty * sizeof(input.Quantities_exact_qty_uint[0]));
	}
}


ZTEST(cbor_encode_test5, test_qty)
{
	/* range_qty: 2*5 -> 1*10, exact_qty: 3*3 -> 4*4 */
	const uint8_t exp_payload[] = {
		LIST(2),
			LIST(1), 0x00, END
			LIST(4), 0x00, 0x01, 0x02, 0x03, END
		END
	};
	struct Quantities input = {
		.Quantities_range_qty_uint = {0},
		.Quantities_range_qty_uint_count = 1,
		.Quantities_exact_qty_uint = {0, 1, 2, 3},
		.Quantities_exact_qty_uint_count = 4,
	};
	uint8_t output[40];
	size_t out_len;

	zassert_equal(10, ARRAY_SIZE(input.Quantities_range_qty_uint));
	zassert_equal(4, ARRAY_SIZE(input.Quantities_exact_qty_uint));

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Quantities(output, sizeof(output), &input, &out_len));
	zassert_equal(sizeof(exp_payload), out_len);
	zassert_mem_equal(exp_payload, output, out_len);

	check_quantities(1, 4, ZCBOR_SUCCESS, ZCBOR_SUCCESS);
	check_quantities(10, 4, ZCBOR_SUCCESS, ZCBOR_SUCCESS);
	check_quantities(0, 4, ZCBOR_ERR_ITERATIONS, ZCBOR_ERR_ITERATIONS);
	check_quantities(11, 4, ZCBOR_ERR_ITERATIONS, ARR_ERR1);
	check_quantities(1, 3, ZCBOR_ERR_ITERATIONS, ZCBOR_ERR_ITERATIONS);
	check_quantities(1, 5, ZCBOR_ERR_ITERATIONS, ARR_ERR1);
}


#define NEW_DEFAULT_MAX_QTY 30

static struct DefaultQty default_qty_input;
static struct DefaultQty default_qty_result;


/* Check that the encoder returns exp_err_enc and the decoder returns exp_err_dec when
 * encoding/decoding the given number of elements in each list/map. */
static void check_default_qty(size_t qty, int exp_err_enc, int exp_err_dec)
{
	struct DefaultQty *input = &default_qty_input;
	struct DefaultQty *result = &default_qty_result;
	static uint8_t payload[250];
	static uint8_t output[250];
	size_t out_len;
	ZCBOR_STATE_E(state, 2, payload, sizeof(payload), 1);

	memset(input, 0, sizeof(*input));
	input->DefaultQty_uints_uint_count = qty;
	input->groups_a_l_count = qty;
	input->map_uintbool_count = qty;

	zassert_true(zcbor_list_start_encode(state, 3));

	zassert_true(zcbor_list_start_encode(state, qty));
	for (uint32_t i = 0; i < qty; i++) {
		zassert_true(zcbor_uint32_put(state, i));
		if (i < ARRAY_SIZE(input->DefaultQty_uints_uint)) {
			input->DefaultQty_uints_uint[i] = i;
		}
	}
	zassert_true(zcbor_list_end_encode(state, qty));

	zassert_true(zcbor_list_start_encode(state, qty * 2));
	for (uint32_t i = 0; i < qty; i++) {
		zassert_true(zcbor_uint32_put(state, i));
		zassert_true(zcbor_tstr_put_lit(state, "x"));
		if (i < ARRAY_SIZE(input->groups_a_l)) {
			input->groups_a_l[i].groups_a_l_a = i;
			input->groups_a_l[i].groups_a_l_b = ZSTR("x");
		}
	}
	zassert_true(zcbor_list_end_encode(state, qty * 2));

	zassert_true(zcbor_map_start_encode(state, qty));
	for (uint32_t i = 0; i < qty; i++) {
		zassert_true(zcbor_uint32_put(state, i));
		zassert_true(zcbor_bool_put(state, i % 2));
		if (i < ARRAY_SIZE(input->map_uintbool)) {
			input->map_uintbool[i].DefaultQty_map_uintbool_key = i;
			input->map_uintbool[i].map_uintbool = i % 2;
		}
	}
	zassert_true(zcbor_map_end_encode(state, qty));

	zassert_true(zcbor_list_end_encode(state, 3));

	zassert_equal(exp_err_enc, cbor_encode_DefaultQty(output, sizeof(output), input, &out_len));
	zassert_equal(exp_err_dec, cbor_decode_DefaultQty(payload, PAYLOAD_LEN(state, payload), result, NULL));

	if (exp_err_enc == ZCBOR_SUCCESS) {
		zassert_equal(PAYLOAD_LEN(state, payload), out_len);
		zassert_mem_equal(payload, output, out_len);
	}
	if (exp_err_dec == ZCBOR_SUCCESS) {
		zassert_equal(qty, result->DefaultQty_uints_uint_count);
		zassert_equal(qty, result->groups_a_l_count);
		zassert_equal(qty, result->map_uintbool_count);
		for (size_t i = 0; i < qty; i++) {
			zassert_equal(i, result->DefaultQty_uints_uint[i]);
			zassert_equal(i, result->groups_a_l[i].groups_a_l_a);
			zassert_zstr_equal(ZSTR("x"), result->groups_a_l[i].groups_a_l_b);
			zassert_equal(i, result->map_uintbool[i].DefaultQty_map_uintbool_key);
			zassert_equal(i % 2, result->map_uintbool[i].map_uintbool);
		}
	}
}


ZTEST(cbor_encode_test5, test_default_max_qty)
{
	/* ZCBOR_DEFINES_DEFAULT_MAX_QTY: 3 -> 30 */
	zassert_equal(NEW_DEFAULT_MAX_QTY, ZCBOR_DEFINES_DEFAULT_MAX_QTY);
	zassert_equal(NEW_DEFAULT_MAX_QTY, ARRAY_SIZE(default_qty_input.DefaultQty_uints_uint));
	zassert_equal(NEW_DEFAULT_MAX_QTY, ARRAY_SIZE(default_qty_input.groups_a_l));
	zassert_equal(NEW_DEFAULT_MAX_QTY, ARRAY_SIZE(default_qty_input.map_uintbool));

	check_default_qty(1, ZCBOR_SUCCESS, ZCBOR_SUCCESS);
	check_default_qty(NEW_DEFAULT_MAX_QTY, ZCBOR_SUCCESS, ZCBOR_SUCCESS);
	check_default_qty(NEW_DEFAULT_MAX_QTY + 1, ZCBOR_ERR_ITERATIONS, ARR_ERR1);
}


ZTEST(cbor_encode_test5, test_tag)
{
	/* #6.1234 -> #6.4321, #6.5678 -> #6.100000 */
	const uint8_t exp_payload[] = {
		LIST(2),
			0xD9, 0x10, 0xE1, 0x01,
			0xDA, 0x00, 0x01, 0x86, 0xA0, 0x61, 'x',
		END
	};
	const uint8_t old_payload1[] = {
		LIST(2),
			0xD9, 0x04, 0xD2, 0x01,
			0xDA, 0x00, 0x01, 0x86, 0xA0, 0x61, 'x',
		END
	};
	const uint8_t old_payload2[] = {
		LIST(2),
			0xD9, 0x10, 0xE1, 0x01,
			0xD9, 0x16, 0x2E, 0x61, 'x',
		END
	};
	const struct Tags input = {
		.Tags_tagged_uint = 1,
		.Tags_tagged_tstr = ZSTR("x"),
	};
	struct Tags result;
	uint8_t output[20];
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Tags(output, sizeof(output), &input, &out_len));
	zassert_equal(sizeof(exp_payload), out_len);
	zassert_mem_equal(exp_payload, output, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Tags(exp_payload, sizeof(exp_payload), &result, NULL));
	zassert_equal(1, result.Tags_tagged_uint);
	zassert_zstr_equal(ZSTR("x"), result.Tags_tagged_tstr);

	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Tags(old_payload1, sizeof(old_payload1), &result, NULL));
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Tags(old_payload2, sizeof(old_payload2), &result, NULL));
}


ZTEST(cbor_encode_test5, test_num_backups)
{
	/* NESTED_NUM_BACKUPS: 5 -> 2
	 * This is enough for decoding an empty list, but not a list containing a list. Since the
	 * inner lists are optional, running out of backups while decoding one is not an error in
	 * itself, but the outer list is then left with an unexpected element.
	 * When encoding, backups are only needed with ZCBOR_CANONICAL, and only for lists, so 2 is
	 * enough for 2 nested lists, but not 3. */
	const uint8_t payload1[] = {LIST(0), END};
	const uint8_t payload2[] = {LIST(1), LIST(0), END END};
	const uint8_t payload3[] = {LIST(1), LIST(1), LIST(0), END END END};
	struct Nested input2 = {.Nested_uint_l_l_count = 1};
	struct Nested input3 = {.Nested_uint_l_l_count = 1};
	struct Nested result;
	uint8_t output[10];
	size_t out_len;

	input3.Nested_uint_l_l[0].uint_l_l_uint_l_count = 1;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Nested(payload1, sizeof(payload1), &result, NULL));
	zassert_equal(0, result.Nested_uint_l_l_count);
	zassert_equal(ARR_ERR1, cbor_decode_Nested(payload2, sizeof(payload2), &result, NULL));
	zassert_equal(ARR_ERR1, cbor_decode_Nested(payload3, sizeof(payload3), &result, NULL));

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Nested(output, sizeof(output), &input2, &out_len));
	zassert_equal(sizeof(payload2), out_len);
	zassert_mem_equal(payload2, output, out_len);

#ifdef ZCBOR_CANONICAL
	zassert_equal(ZCBOR_ERR_NO_BACKUP_MEM, cbor_encode_Nested(output, sizeof(output), &input3, &out_len));
#else
	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Nested(output, sizeof(output), &input3, &out_len));
	zassert_equal(sizeof(payload3), out_len);
	zassert_mem_equal(payload3, output, out_len);
#endif
}


ZTEST(cbor_encode_test5, test_max_count)
{
	/* COUNTS_MAX_COUNT: 2 -> 300, B_C_MAX_COUNT: 1 -> 300
	 * These are size hints for the list/map headers, which are only used with ZCBOR_CANONICAL.
	 * The headers are then first encoded with room for 300 elements (3 bytes each), and shrunk
	 * afterwards, so the payload buffer must fit the larger headers. */
	const uint8_t exp_payload[] = {
		LIST(2),
			0x01,
			MAP(1), 0x61, 'c', 0x02, END
		END
	};
	const struct Counts input = {.Counts_a = 1, .b_c = 2};
	struct Counts result;
	uint8_t output[sizeof(exp_payload) + 4];
	size_t out_len;

#ifdef ZCBOR_CANONICAL
	const size_t needed_len = sizeof(exp_payload) + 4;

	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, cbor_encode_Counts(output, needed_len - 1, &input, &out_len));
#else
	const size_t needed_len = sizeof(exp_payload);
#endif

	zassert_equal(ZCBOR_SUCCESS, cbor_encode_Counts(output, needed_len, &input, &out_len));
	zassert_equal(sizeof(exp_payload), out_len);
	zassert_mem_equal(exp_payload, output, out_len);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Counts(exp_payload, sizeof(exp_payload), &result, NULL));
	zassert_equal(1, result.Counts_a);
	zassert_equal(2, result.b_c);
}


ZTEST_SUITE(cbor_encode_test5, NULL, NULL, NULL, NULL, NULL);
