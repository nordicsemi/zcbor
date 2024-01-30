/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <math.h>
#include <corner_cases.h>
#include <zcbor_decode.h>
#include <zcbor_print.h>

#define CONCAT_BYTE(a,b) a ## b

#ifdef TEST_INDEFINITE_LENGTH_ARRAYS
#define LIST(num) 0x9F /* Use short count 31 (indefinite-length). Note that the 'num' argument is ignored */
#define LIST2(num) 0x9F /* Use short count 31 (indefinite-length). Note that the 'num' argument is ignored */
#define LIST3(num) 0x9F /* Use short count 31 (indefinite-length). Note that the 'num' argument is ignored */
#define MAP(num) 0xBF  /* Use short count 31 (indefinite-length). Note that the 'num' argument is ignored */
#define ARR_ERR1 ZCBOR_ERR_WRONG_TYPE
#define ARR_ERR2 ZCBOR_ERR_NO_PAYLOAD
#define ARR_ERR3 ZCBOR_ERR_WRONG_TYPE
#define ARR_ERR4 ZCBOR_ERR_PAYLOAD_NOT_CONSUMED
#define END 0xFF,
#define STR_LEN(len, lists) (len + lists)
#else
#define LIST(num) CONCAT_BYTE(0x8, num)
#define LIST2(num) CONCAT_BYTE(0x9, num)
#define LIST3(num) 0x98, num
#define MAP(num) CONCAT_BYTE(0xA, num)
#define ARR_ERR1 ZCBOR_ERR_HIGH_ELEM_COUNT
#define ARR_ERR2 ZCBOR_ERR_HIGH_ELEM_COUNT
#define ARR_ERR3 ZCBOR_ERR_NO_PAYLOAD
#define ARR_ERR4 ZCBOR_ERR_NO_PAYLOAD
#define END
#define STR_LEN(len, lists) (len)
#endif


ZTEST(cbor_decode_test5, test_numbers)
{
	size_t decode_len = 0xFFFFFFFF;
	const uint8_t payload_numbers1[] = {
		LIST(A),
			0x01, // 1
			0x21, // -2
			0x05, // 5
			0x19, 0x01, 0x00, // 256
			0x1A, 0x01, 0x02, 0x03, 0x04, // 0x01020304
			0x39, 0x13, 0x87, // -5000
			0x1A, 0xEE, 0x6B, 0x28, 0x00, // 4000000000
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -2^31
			0x00, // 0
			0xD9, 0xFF, 0xFF, 0x01, // 1 tagged (0xFFFF)
		END
	};
	int ret;

	struct Numbers numbers;
	ret = cbor_decode_Numbers(payload_numbers1, sizeof(payload_numbers1) - 1, &numbers, &decode_len);
	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, ret, "%d\r\n", ret); // Payload too small.
	zassert_equal(0xFFFFFFFF, decode_len, NULL); // Should be untouched
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Numbers(payload_numbers1,
		sizeof(payload_numbers1) + 1, &numbers, &decode_len), NULL); // Payload too large
	zassert_equal(sizeof(payload_numbers1), decode_len, NULL);

	zassert_equal(5, numbers.fourtoten, NULL);
	zassert_equal(256, numbers.twobytes, NULL);
	zassert_equal(0x01020304, numbers.onetofourbytes, NULL);
	zassert_equal(-5000, numbers.minusfivektoplustwohundred, "%d", numbers.minusfivektoplustwohundred);
	zassert_equal(-2147483648, numbers.negint, NULL);
	zassert_equal(0, numbers.posint, NULL);
	zassert_equal(1, numbers.tagged_int, NULL);
}


ZTEST(cbor_decode_test5, test_numbers2)
{
	size_t decode_len = 0xFFFFFFFF;
	const uint8_t payload_numbers2[] = {
		LIST(8),
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x80000000
			0x1B, 0x01, 2, 3, 4, 5, 6, 7, 8, // 0x0102030405060708
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x00, // 0
			0x3A, 0x80, 0x00, 0x00, 0x00, // -0x8000_0001
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000
			0xD9, 0x04, 0xD2, 0x03, // #6.1234(3)
		END
	};
	const uint8_t payload_numbers2_1[] = {
		LIST(8),
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456
			0x1A, 0x7F, 0xFF, 0xFF, 0xFF, // 0x7FFFFFFF
			0x3B, 0x01, 2, 3, 4, 5, 6, 7, 8, // -0x0102030405060709
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x1B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0xFFFFFFFFFFFFFFFF
			0x3A, 0x80, 0x00, 0x00, 0x00, // -0x8000_0001
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000
			0xD9, 0x04, 0xD2, 0x03, // #6.1234(3)
		END
	};
	const uint8_t payload_numbers2_inv2[] = {
		LIST(8),
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456
			0x1A, 0x7F, 0xFF, 0xFF, 0xFF, // 0x7FFFFFFF
			0x1B, 0x01, 2, 3, 4, 5, 6, 7, 8, // 0x0102030405060708
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x3A, 0x80, 0x00, 0x00, 0x01, // -0x8000_0002 INV
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000
			0xD9, 0x04, 0xD2, 0x03, // #6.1234(3)
		END
	};
	const uint8_t payload_numbers2_inv3[] = {
		LIST(8),
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456
			0x1A, 0x7F, 0xFF, 0xFF, 0xFF, // 0x7FFFFFFF
			0x1B, 0x01, 2, 3, 4, 5, 6, 7, 8, // 0x0102030405060708
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000 INV
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000
			0xD9, 0x04, 0xD2, 0x03, // #6.1234(3)
		END
	};
	const uint8_t payload_numbers2_inv4[] = {
		LIST(8),
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456
			0x1A, 0x7F, 0xFF, 0xFF, 0xFF, // 0x7FFFFFFF
			0x1B, 0x01, 2, 3, 4, 5, 6, 7, 8, // 0x0102030405060708
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456 INV
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x3A, 0x80, 0x00, 0x00, 0x00, // -0x8000_0001
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000
			0xD9, 0x04, 0xD2, 0x03, // #6.1234(3)
		END
	};
	const uint8_t payload_numbers2_inv5[] = {
		LIST(8),
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456
			0x1A, 0x7F, 0xFF, 0xFF, 0xFF, // 0x7FFFFFFF
			0x1B, 0x01, 2, 3, 4, 5, 6, 7, 8, // 0x0102030405060708
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x3A, 0x80, 0x00, 0x00, 0x00, // -0x8000_0001
			0x3A, 0x80, 0x00, 0x00, 0x00, // -0x8000_0001 INV
			0xD9, 0x04, 0xD2, 0x03, // #6.1234(3)
		END
	};
	const uint8_t payload_numbers2_inv6[] = {
		LIST(8),
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456
			0x1A, 0x7F, 0xFF, 0xFF, 0xFF, // 0x7FFFFFFF
			0x3B, 0x01, 2, 3, 4, 5, 6, 7, 8, // -0x0102030405060709
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x20, // -1 INV
			0x3A, 0x80, 0x00, 0x00, 0x00, // -0x8000_0001
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000
			0xD9, 0x04, 0xD2, 0x03, // #6.1234(3)
		END
	};
	const uint8_t payload_numbers2_inv7[] = {
		LIST(8),
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456
			0x1A, 0x7F, 0xFF, 0xFF, 0xFF, // 0x7FFFFFFF
			0x1B, 0x01, 2, 3, 4, 5, 6, 7, 8, // 0x0102030405060708
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x00, // 0
			0x3A, 0x80, 0x00, 0x00, 0x00, // -0x8000_0001
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000
			0xD9, 0x04, 0xD3, 0x03, // #6.1235(3) INV
		END
	};
	const uint8_t payload_numbers2_inv8[] = {
		LIST(8),
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456
			0x3A, 0x80, 0x00, 0x00, 0x00, // -0x80000001 INV
			0x1B, 0x01, 2, 3, 4, 5, 6, 7, 8, // 0x0102030405060708
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x00, // 0
			0x3A, 0x80, 0x00, 0x00, 0x00, // -0x8000_0001
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000
			0xD9, 0x04, 0xD2, 0x03, // #6.1234(3)
		END
	};
	const uint8_t payload_numbers2_inv9[] = {
		LIST(8),
			0x1A, 0x00, 0x12, 0x34, 0x56, // 0x123456
			0x1A, 0x80, 0x00, 0x00, 0x00, // 0x80000000 INV
			0x1B, 0x01, 2, 3, 4, 5, 6, 7, 8, // 0x0102030405060708
			0x1B, 0x11, 2, 3, 4, 5, 6, 7, 9, // 0x1102030405060709
			0x00, // 0
			0x3A, 0x80, 0x00, 0x00, 0x00, // -0x8000_0001
			0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -0x8000_0000
			0xD9, 0x04, 0xD2, 0x03, // #6.1234(3)
		END
	};
	struct Numbers2 numbers2;
	int ret;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Numbers2(payload_numbers2,
		sizeof(payload_numbers2), &numbers2, &decode_len), NULL);

	zassert_equal(0x123456, numbers2.threebytes, NULL);
	zassert_equal(-0x80000000L, numbers2.int32, NULL);
	zassert_equal(0x0102030405060708, numbers2.big_int, NULL);
	zassert_equal(0x1102030405060709, numbers2.big_uint, NULL);
	zassert_equal(0, numbers2.big_uint2, NULL);
	zassert_equal(3, numbers2.tagged_int, NULL);

	ret = cbor_decode_Numbers2(payload_numbers2_1,
		sizeof(payload_numbers2_1), &numbers2, &decode_len);
	zassert_equal(ZCBOR_SUCCESS, ret, "%d\r\n", ret);

	zassert_equal(0x123456, numbers2.threebytes, NULL);
	zassert_equal(0x7FFFFFFFL, numbers2.int32, NULL);
	zassert_equal(-0x0102030405060709, numbers2.big_int, NULL);
	zassert_equal(0x1102030405060709, numbers2.big_uint, NULL);
	zassert_equal(0xFFFFFFFFFFFFFFFF, numbers2.big_uint2, NULL);
	zassert_equal(3, numbers2.tagged_int, NULL);

	ret = cbor_decode_Numbers2(payload_numbers2_inv2,
		sizeof(payload_numbers2_inv2), &numbers2, &decode_len);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, ret, "%d\r\n", ret);

	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Numbers2(payload_numbers2_inv3,
		sizeof(payload_numbers2_inv3), &numbers2, &decode_len), NULL);

	ret = cbor_decode_Numbers2(payload_numbers2_inv4,
		sizeof(payload_numbers2_inv4), &numbers2, &decode_len);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, ret, "%d\r\n", ret);

	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Numbers2(payload_numbers2_inv5,
		sizeof(payload_numbers2_inv5), &numbers2, &decode_len), NULL);

	zassert_equal(ZCBOR_ERR_WRONG_TYPE, cbor_decode_Numbers2(payload_numbers2_inv6,
		sizeof(payload_numbers2_inv6), &numbers2, &decode_len), NULL);

	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Numbers2(payload_numbers2_inv7,
		sizeof(payload_numbers2_inv7), &numbers2, &decode_len), NULL);

	ret = cbor_decode_Numbers2(payload_numbers2_inv8,
		sizeof(payload_numbers2_inv8), &numbers2, &decode_len);
	zassert_equal(sizeof(numbers2.int32) == 4 ? ZCBOR_ERR_INT_SIZE : ZCBOR_ERR_WRONG_RANGE,
		ret, "%d\r\n", ret);

	zassert_equal(sizeof(numbers2.int32) == 4 ? ZCBOR_ERR_INT_SIZE : ZCBOR_ERR_WRONG_RANGE,
		cbor_decode_Numbers2(payload_numbers2_inv9,
		sizeof(payload_numbers2_inv9), &numbers2, &decode_len), NULL);
}


/** Test that when unions contain tagged elements (with #6.x), without
 *  indirection, the tags are enforced correctly.
*/
ZTEST(cbor_decode_test5, test_tagged_union)
{
	size_t decode_len;
	const uint8_t payload_tagged_union1[] = {0xD9, 0x10, 0xE1, 0xF5};
	const uint8_t payload_tagged_union2[] = {0xD9, 0x09, 0x29, 0x10};
	const uint8_t payload_tagged_union3_inv[] = {0xD9, 0x10, 0xE1, 0x10};

	struct TaggedUnion_r result;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_TaggedUnion(payload_tagged_union1,
		sizeof(payload_tagged_union1), &result, &decode_len), "%d\r\n");

	zassert_equal(sizeof(payload_tagged_union1), decode_len, NULL);
	zassert_equal(TaggedUnion_bool_c, result.TaggedUnion_choice, NULL);
	zassert_true(result.Bool, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_TaggedUnion(payload_tagged_union2,
		sizeof(payload_tagged_union2), &result, &decode_len), NULL);

	zassert_equal(TaggedUnion_uint_c, result.TaggedUnion_choice, NULL);
	zassert_equal(0x10, result.uint, NULL);

	zassert_equal(ZCBOR_ERR_WRONG_TYPE, cbor_decode_TaggedUnion(payload_tagged_union3_inv,
		sizeof(payload_tagged_union3_inv), &result, &decode_len), NULL);
}

ZTEST(cbor_decode_test5, test_number_map)
{
	size_t decode_len = 0xFFFFFFFF;
	const uint8_t payload_number_map1[] = {
		MAP(3),
			0x64, 'b', 'y', 't', 'e',
			0x18, 42,
			0x69, 'o', 'p', 't', '_', 's', 'h', 'o', 'r', 't',
			0x19, 0x12, 0x34,
			0x68, 'o', 'p', 't', '_', 'c', 'b', 'o', 'r',
			0x45, 0x1A, 0x12, 0x34, 0x56, 0x78,
		END
	};
	const uint8_t payload_number_map2[] = {
		MAP(1),
			0x64, 'b', 'y', 't', 'e',
			0x04,
		END
	};
	const uint8_t payload_number_map3[] = {
		MAP(2),
			0x64, 'b', 'y', 't', 'e',
			0x18, 42,
			0x69, 'o', 'p', 't', '_', 's', 'h', 'o', 'r', 't',
			0x12,
		END
	};
	const uint8_t payload_number_map4_inv[] = {
		MAP(2),
			0x64, 'b', 'y', 't', 'e',
			0x19, 42, 42,
		END
	};
	const uint8_t payload_number_map5_inv[] = {
		MAP(2),
			0x64, 'b', 'y', 't', 'e',
			0x18, 42,
			0x69, 'o', 'p', 't', '_', 's', 'h', 'o', 'r', 't',
			0x1A, 0x12, 0x34, 0x56, 0x78,
		END
	};
	const uint8_t payload_number_map6_inv[] = {
		MAP(2),
			0x64, 'b', 'y', 't', 'e',
			0x18, 42,
			0x68, 'o', 'p', 't', '_', 'c', 'b', 'o', 'r',
			0x43, 0x19, 0x12, 0x34,
		END
	};
	const uint8_t payload_number_map7_inv[] = {
		MAP(1),
			0x64, 'B', 'y', 't', 'e',
			0x04,
		END
	};

	struct NumberMap number_map;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NumberMap(payload_number_map1,
		sizeof(payload_number_map1), &number_map, &decode_len), NULL);
	zassert_equal(42, number_map.byte, NULL);
	zassert_true(number_map.opt_short_present, NULL);
	zassert_equal(0x1234, number_map.opt_short.opt_short, NULL);
	zassert_true(number_map.opt_cbor_present, NULL);
	zassert_equal(0x12345678, number_map.opt_cbor.opt_cbor_cbor, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NumberMap(payload_number_map2,
		sizeof(payload_number_map2), &number_map, &decode_len), NULL);
	zassert_equal(4, number_map.byte, NULL);
	zassert_false(number_map.opt_short_present, NULL);
	zassert_false(number_map.opt_cbor_present, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NumberMap(payload_number_map3,
		sizeof(payload_number_map3), &number_map, &decode_len), NULL);
	zassert_equal(42, number_map.byte, NULL);
	zassert_true(number_map.opt_short_present, NULL);
	zassert_equal(0x12, number_map.opt_short.opt_short, NULL);
	zassert_false(number_map.opt_cbor_present, NULL);

	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_NumberMap(payload_number_map4_inv,
		sizeof(payload_number_map4_inv), &number_map, &decode_len), NULL);

	int res = cbor_decode_NumberMap(payload_number_map5_inv,
		sizeof(payload_number_map5_inv), &number_map, &decode_len);
	zassert_equal(ARR_ERR1, res, "%d\r\n", res);

	zassert_equal(ARR_ERR1, cbor_decode_NumberMap(payload_number_map6_inv,
		sizeof(payload_number_map6_inv), &number_map, &decode_len), NULL);

	res = cbor_decode_NumberMap(payload_number_map7_inv,
		sizeof(payload_number_map7_inv), &number_map, &decode_len);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, res, "%d\r\n", res);
}

ZTEST(cbor_decode_test5, test_strings)
{
	const uint8_t payload_strings1[] = {
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
		0x58, STR_LEN(29, 1), // Numbers (len: 29)
			LIST(A),
				0x01, // 1
				0x21, // -2
				0x05, // 5
				0x19, 0xFF, 0xFF, // 0xFFFF
				0x18, 0x18, // 24
				0x00, // 0
				0x1A, 0xEE, 0x6B, 0x28, 0x00, // 4000000000
				0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -2^31
				0x1A, 0xFF, 0xFF, 0xFF, 0xFF, // 0xFFFFFFFF
				0xD9, 0xFF, 0xFF, 0x09, // 9, tagged (0xFFFF)
			END
		STR_LEN(0x52, 3), // Simples (len: 18)
			LIST(5),
				0xF5, // True
				0xF4, // False
				0xF4, // False
				0xF6, // Nil
				0xF7, // Undefined
			END
			LIST(5),
				0xF5, // True
				0xF4, // False
				0xF5, // True
				0xF6, // Nil
				0xF7, // Undefined
			END
			LIST(5),
				0xF5, // True
				0xF4, // False
				0xF4, // False
				0xF6, // Nil
				0xF7, // Undefined
			END
		0x59, 0x01, STR_LEN(0x68, 3), // Strings (len: 360)
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
			0x58, STR_LEN(29, 1), // Numbers (len: 29)
				LIST(A),
					0x01, // 1
					0x21, // -2
					0x05, // 5
					0x19, 0xFF, 0xFF, // 0xFFFF
					0x18, 0x18, // 24
					0x00, // 0
					0x1A, 0xEE, 0x6B, 0x28, 0x00, // 4000000000
					0x3A, 0x7F, 0xFF, 0xFF, 0xFF, // -2^31
					0x1A, 0xFF, 0xFF, 0xFF, 0xFF, // 0xFFFFFFFF
					0xD9, 0xFF, 0xFF, 0x29, // -10, tagged (0xFFFF)
				END
			STR_LEN(0x46, 1), // Simples (len: 6)
				LIST(5),
					0xF5, // True
					0xF4, // False
					0xF4, // False
					0xF6, // Nil
					0xF7, // Undefined
				END
			END
		END
	};

	struct Strings strings1;
	struct Strings strings2;
	struct Numbers numbers1;
	struct Numbers numbers2;
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Strings(payload_strings1,
		sizeof(payload_strings1), &strings1, NULL), NULL);
	zassert_true(strings1.optCborStrings_present, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Strings(strings1.optCborStrings.value,
		strings1.optCborStrings.len, &strings2, NULL), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Numbers(strings1.cborNumbers.value,
		strings1.cborNumbers.len, &numbers1, NULL), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Numbers(strings2.cborNumbers.value,
		strings2.cborNumbers.len, &numbers2, NULL), NULL);

	zassert_equal(300, strings1.threehundrebytebstr.len, NULL);
	zassert_equal(0, strings1.threehundrebytebstr.value[0], NULL);
	zassert_equal(9, strings1.threehundrebytebstr.value[299], NULL);
	zassert_equal(30, strings1.tentothirtybytetstr.len, NULL);
	zassert_equal(STR_LEN(29, 1), strings1.cborNumbers.len, NULL);
	zassert_equal(3, strings1.cborseqSimples_cbor_count, NULL);
	zassert_false(strings1.cborseqSimples_cbor[0].boolval, NULL);
	zassert_true(strings1.cborseqSimples_cbor[1].boolval, NULL);
	zassert_false(strings1.cborseqSimples_cbor[2].boolval, NULL);

	zassert_equal(300, strings2.threehundrebytebstr.len, NULL);
	zassert_equal(10, strings2.tentothirtybytetstr.len, NULL);
	zassert_equal(5, numbers2.fourtoten, NULL);
	zassert_equal(0xFFFF, numbers2.twobytes, NULL);
	zassert_equal(24, numbers2.onetofourbytes, NULL);
	zassert_equal(0, numbers2.minusfivektoplustwohundred, NULL);
	zassert_equal(-2147483648, numbers2.negint, NULL);
	zassert_equal(0xFFFFFFFF, numbers2.posint, NULL);
	zassert_equal(-10, numbers2.tagged_int, NULL);
	zassert_equal(1, strings2.cborseqSimples_cbor_count, NULL);
	zassert_false(strings2.cborseqSimples_cbor[0].boolval, NULL);
}

ZTEST(cbor_decode_test5, test_simples)
{
	uint8_t payload_simple1[] = {LIST(5), 0xF5, 0xF4, 0xF4, 0xF6, 0xF7, END};
	uint8_t payload_simple2[] = {LIST(5), 0xF5, 0xF4, 0xF5, 0xF6, 0xF7, END};
	uint8_t payload_simple_inv3[] = {LIST(5), 0xF7, 0xF4, 0xF4, 0xF6, 0xF7, END};
	uint8_t payload_simple_inv4[] = {LIST(5), 0xF5, 0xF7, 0xF4, 0xF6, 0xF7, END};
	uint8_t payload_simple_inv5[] = {LIST(5), 0xF5, 0xF4, 0xF7, 0xF6, 0xF7, END};
	uint8_t payload_simple_inv6[] = {LIST(5), 0xF5, 0xF4, 0xF5, 0xF7, 0xF7, END};
	uint8_t payload_simple_inv7[] = {LIST(5), 0xF5, 0xF4, 0xF5, 0xF6, 0xF6, END};
	uint8_t payload_simple_inv8[] = {LIST(5), 0xF5, 0xF4, 0xF5, 0xF6, 0xF5, END};
	uint8_t payload_simple_inv9[] = {LIST(5), 0xF5, 0xF4, 0xF6, 0xF6, 0xF7, END};
	uint8_t payload_simple_inv10[] = {LIST(5), 0xF5, 0xF5, 0xF5, 0xF6, 0xF7, END};
	uint8_t payload_simple_inv11[] = {LIST(5), 0xF4, 0xF4, 0xF5, 0xF6, 0xF7, END};

	struct Simples result;
	size_t len_decode;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Simple2(payload_simple1, sizeof(payload_simple1), &result, &len_decode), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Simple2(payload_simple2, sizeof(payload_simple2), &result, &len_decode), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Simple2(payload_simple_inv3, sizeof(payload_simple_inv3), &result, &len_decode), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Simple2(payload_simple_inv4, sizeof(payload_simple_inv4), &result, &len_decode), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, cbor_decode_Simple2(payload_simple_inv5, sizeof(payload_simple_inv5), &result, &len_decode), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Simple2(payload_simple_inv6, sizeof(payload_simple_inv6), &result, &len_decode), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Simple2(payload_simple_inv7, sizeof(payload_simple_inv7), &result, &len_decode), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Simple2(payload_simple_inv8, sizeof(payload_simple_inv8), &result, &len_decode), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, cbor_decode_Simple2(payload_simple_inv9, sizeof(payload_simple_inv9), &result, &len_decode), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Simple2(payload_simple_inv10, sizeof(payload_simple_inv10), &result, &len_decode), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Simple2(payload_simple_inv11, sizeof(payload_simple_inv11), &result, &len_decode), NULL);
}

ZTEST(cbor_decode_test5, test_string_overflow)
{
	const uint8_t payload_overflow[] = {
		0x5A, 0xFF, 0xFF, 0xF0, 0x00, /* overflows to before this string. */
	};

	struct zcbor_string result_overflow;
	size_t out_len;

	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, cbor_decode_SingleBstr(payload_overflow, sizeof(payload_overflow), &result_overflow, &out_len), NULL);
}

ZTEST(cbor_decode_test5, test_optional)
{
	const uint8_t payload_optional1[] = {
		LIST(3), 0xCA /* tag */, 0xF4 /* False */, 0x02, 0x03,
		END
	};
	const uint8_t payload_optional2[] = {
		LIST(2), 0xCA /* tag */, 0xF4 /* False */, 0x03,
		END
	};
	const uint8_t payload_optional3_inv[] = {
		LIST(2), 0xCA /* tag */, 0xF4 /* False */, 0x02,
		END
	};
	const uint8_t payload_optional4[] = {
		LIST(3), 0xCA /* tag */, 0xF4 /* False */, 0x02, 0x01,
		END
	};
	const uint8_t payload_optional5[] = {
		LIST(3), 0xCA /* tag */, 0xF5 /* True */, 0x02, 0x02,
		END
	};
	const uint8_t payload_optional6[] = {
		LIST(4), 0xCA /* tag */, 0xF5 /* True */, 0xF4 /* False */, 0x02, 0x02,
		END
	};
	const uint8_t payload_optional7_inv[] = {
		LIST(2), 0xCB /* wrong tag */, 0xF4 /* False */, 0x03,
		END
	};
	const uint8_t payload_optional8_inv[] = {
		LIST(2), 0xF4 /* False (no tag first) */, 0x03,
		END
	};
	const uint8_t payload_optional9[] = {
		LIST(4), 0xCA /* tag */, 0xF4 /* False */, 0x02, 0x03, 0x08,
		END
	};
	const uint8_t payload_optional10[] = {
		LIST(6), 0xCA /* tag */, 0xF4 /* False */, 0x02, 0x03, 0x08, 0x08, 0x08,
		END
	};
	const uint8_t payload_optional11_inv[] = {
		LIST(6), 0xCA /* tag */, 0xF4 /* False */, 0x02, 0x03, 0x08, 0x08, 0x09,
		END
	};
	const uint8_t payload_optional12[] = {
		LIST(3), 0xCA /* tag */, 0xF4 /* False */, 0x02, 0x08,
		END
	};

	struct Optional optional;
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Optional(payload_optional1,
			sizeof(payload_optional1), &optional, NULL), NULL);
	zassert_false(optional.boolval, NULL);
	zassert_false(optional.optbool_present, NULL);
	zassert_true(optional.opttwo_present, NULL);
	zassert_equal(3, optional.manduint, NULL);
	zassert_equal(0, optional.multi8_count, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Optional(payload_optional2,
			sizeof(payload_optional2), &optional, NULL), NULL);
	zassert_false(optional.boolval, NULL);
	zassert_false(optional.optbool_present, NULL);
	zassert_false(optional.opttwo_present, NULL);
	zassert_equal(3, optional.manduint, NULL);
	zassert_equal(0, optional.multi8_count, NULL);

	int ret = cbor_decode_Optional(payload_optional3_inv,
			sizeof(payload_optional3_inv), &optional, NULL);
	zassert_equal(ARR_ERR3, ret, "%d\r\n", ret);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Optional(payload_optional4,
			sizeof(payload_optional4), &optional, NULL), NULL);
	zassert_false(optional.boolval, NULL);
	zassert_false(optional.optbool_present, NULL);
	zassert_true(optional.opttwo_present, NULL);
	zassert_equal(1, optional.manduint, NULL);
	zassert_equal(0, optional.multi8_count, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Optional(payload_optional5,
			sizeof(payload_optional5), &optional, NULL), NULL);
	zassert_true(optional.boolval, NULL);
	zassert_false(optional.optbool_present, NULL);
	zassert_true(optional.opttwo_present, NULL);
	zassert_equal(2, optional.manduint, NULL);
	zassert_equal(0, optional.multi8_count, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Optional(payload_optional6,
			sizeof(payload_optional6), &optional, NULL), NULL);
	zassert_true(optional.boolval, NULL);
	zassert_true(optional.optbool_present, NULL);
	zassert_false(optional.optbool, NULL);
	zassert_true(optional.opttwo_present, NULL);
	zassert_equal(2, optional.manduint, NULL);
	zassert_equal(0, optional.multi8_count, NULL);

	ret = cbor_decode_Optional(payload_optional7_inv,
			sizeof(payload_optional7_inv), &optional, NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, ret, "%d\r\n", ret);

	zassert_equal(ZCBOR_ERR_WRONG_TYPE, cbor_decode_Optional(payload_optional8_inv,
			sizeof(payload_optional8_inv), &optional, NULL), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Optional(payload_optional9,
			sizeof(payload_optional9), &optional, NULL), NULL);
	zassert_false(optional.boolval, NULL);
	zassert_false(optional.optbool_present, NULL);
	zassert_true(optional.opttwo_present, NULL);
	zassert_equal(3, optional.manduint, NULL);
	zassert_equal(1, optional.multi8_count, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Optional(payload_optional10,
			sizeof(payload_optional10), &optional, NULL), NULL);
	zassert_false(optional.boolval, NULL);
	zassert_false(optional.optbool_present, NULL);
	zassert_true(optional.opttwo_present, NULL);
	zassert_equal(3, optional.manduint, NULL);
	zassert_equal(3, optional.multi8_count, NULL);

	ret = cbor_decode_Optional(payload_optional11_inv,
			sizeof(payload_optional11_inv), &optional, NULL);
	zassert_equal(ARR_ERR1, ret, "%d\r\n", ret);

	ret = cbor_decode_Optional(payload_optional12,
			sizeof(payload_optional12), &optional, NULL);
	zassert_equal(ZCBOR_SUCCESS, ret, "%d\r\n", ret);
	zassert_false(optional.boolval, NULL);
	zassert_false(optional.optbool_present, NULL);
	zassert_true(optional.opttwo_present, NULL);
	zassert_equal(8, optional.manduint, NULL);
	zassert_equal(0, optional.multi8_count, NULL);

}

ZTEST(cbor_decode_test5, test_union)
{
	const uint8_t payload_union1[] = {0x01, 0x21};
	const uint8_t payload_union2[] = {0x03, 0x23};
	const uint8_t payload_union3[] = {0x03, 0x04};
	const uint8_t payload_union4[] = {
		0x67, 0x22, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x22 // "hello"
	};
	const uint8_t payload_union6[] = {
		0x03, 0x23, 0x03, 0x23, 0x03, 0x23,
		0x03, 0x23, 0x03, 0x23, 0x03, 0x23
	};
	const uint8_t payload_union7_long[] = {0x03, 0x23, 0x03, 0x04};
	const uint8_t payload_union8_long[] = {0x03, 0x23, 0x03};
	const uint8_t payload_union9_long[] = {0x03, 0x04, 0x03, 0x04};
	const uint8_t payload_union10_inv[] = {
		0x03, 0x23, 0x03, 0x23, 0x03, 0x23, 0x03, 0x23,
		0x03, 0x23, 0x03, 0x23, 0x03, 0x23}; /* Too many */
	size_t decode_len;

	struct Union_r union_r;
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Union(payload_union1, sizeof(payload_union1),
				&union_r, NULL), NULL);
	zassert_equal(Union_Group_m_c, union_r.Union_choice, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Union(payload_union2, sizeof(payload_union2),
				&union_r, NULL), NULL);
	zassert_equal(Union_MultiGroup_m_c, union_r.Union_choice, NULL);
	zassert_equal(1, union_r.MultiGroup_m.MultiGroup_count, "was %d\r\n", union_r.MultiGroup_m.MultiGroup_count);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Union(payload_union3, sizeof(payload_union3),
				&union_r, NULL), NULL);
	zassert_equal(Union_uint3_l_c, union_r.Union_choice, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Union(payload_union4, sizeof(payload_union4),
				&union_r, NULL), NULL);
	zassert_equal(Union_hello_tstr_c, union_r.Union_choice, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Union(payload_union6, sizeof(payload_union6),
				&union_r, &decode_len), NULL);
	zassert_equal(Union_MultiGroup_m_c, union_r.Union_choice, NULL);
	zassert_equal(6, union_r.MultiGroup_m.MultiGroup_count, NULL);
	zassert_equal(12, decode_len, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Union(payload_union7_long, sizeof(payload_union7_long),
				&union_r, &decode_len), NULL);
	zassert_equal(2, decode_len, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Union(payload_union8_long, sizeof(payload_union8_long),
				&union_r, &decode_len), NULL);
	zassert_equal(2, decode_len, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Union(payload_union9_long, sizeof(payload_union9_long),
				&union_r, &decode_len), NULL);
	zassert_equal(2, decode_len, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Union(payload_union10_inv, sizeof(payload_union10_inv),
				&union_r, &decode_len), NULL);
	zassert_equal(6, union_r.MultiGroup_m.MultiGroup_count, NULL);
	zassert_equal(12, decode_len, NULL);
}

ZTEST(cbor_decode_test5, test_levels)
{
	const uint8_t payload_levels1[] = {
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

	struct Level2 level1;
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Level1(payload_levels1,
		sizeof(payload_levels1), &level1, NULL), NULL);

	zassert_equal(2, level1.Level3_m_count, NULL);
	zassert_equal(4, level1.Level3_m[0].Level4_m_count, NULL);
	zassert_equal(4, level1.Level3_m[1].Level4_m_count, NULL);
}


ZTEST(cbor_decode_test5, test_map)
{
	const uint8_t payload_map1[] = {
		MAP(4), LIST(2), 0x05, 0x06, END 0xF4, // [5,6] => false
		0x07, 0x01, // 7 => 1
		0xf6, 0x45, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // nil => "hello"
		0xf6, 0x40, // nil => ""
		END
	};
	const uint8_t payload_map2_inv[] = {
		MAP(4), LIST(2), 0x05, 0x06, END 0xF4, // [5,6] => false
		0x07, 0x01, // 7 => 1
		0xf6, 0x45, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // nil => "hello"
		END
	};
	const uint8_t payload_map3[] = {
		MAP(5), LIST(2), 0x05, 0x06, END 0xF5, // [5,6] => true
		0x07, 0x01, // 7 => 1
		0xf6, 0x45, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // nil => "hello"
		0xf6, 0x40, // nil => ""
		0xf6, 0x40, // nil => ""
		END
	};

	/* Wrong list length. Will not fail for indefinite length arrays. */
	const uint8_t payload_map4_inv[] = {
		MAP(6), LIST(2), 0x05, 0x06, END 0xF4, // [5,6] => false
		0x07, 0x01, // 7 => 1
		0xf6, 0x45, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // nil => "hello"
		0xf6, 0x40, // nil => ""
		0xf6, 0x40, // nil => ""
		END
	};
	const uint8_t payload_map5[] = {
		MAP(4), LIST(2), 0x05, 0x06, END 0xF4, // [5,6] => false
		0x27, 0x01, // -8 => 1
		0xf6, 0x45, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // nil => "hello"
		0xf6, 0x40, // nil => ""
		END
	};

	struct Map map;

	zassert_equal(union_nintuint_c, -8,
		"The union_int optimization seems to not be working.\r\n");

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Map(payload_map1, sizeof(payload_map1),
			&map, NULL), NULL);
	zassert_false(map.listkey, NULL);
	zassert_equal(union_uint7uint_c, map.union_choice, NULL);
	zassert_equal(1, map.uint7uint, NULL);
	zassert_equal(2, map.twotothree_count, NULL);
	zassert_equal(5, map.twotothree[0].twotothree.len, NULL);
	zassert_mem_equal("hello", map.twotothree[0].twotothree.value,
			5, NULL);
	zassert_equal(0, map.twotothree[1].twotothree.len, NULL);

	zassert_equal(ZCBOR_ERR_ITERATIONS, cbor_decode_Map(payload_map2_inv, sizeof(payload_map2_inv),
			&map, NULL), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Map(payload_map3, sizeof(payload_map3),
			&map, NULL), NULL);
	zassert_true(map.listkey, NULL);
	zassert_equal(union_uint7uint_c, map.union_choice, NULL);
	zassert_equal(1, map.uint7uint, NULL);
	zassert_equal(3, map.twotothree_count, NULL);
	zassert_equal(5, map.twotothree[0].twotothree.len, NULL);
	zassert_mem_equal("hello", map.twotothree[0].twotothree.value,
			5, NULL);
	zassert_equal(0, map.twotothree[1].twotothree.len, NULL);
	zassert_equal(0, map.twotothree[2].twotothree.len, NULL);

#ifdef TEST_INDEFINITE_LENGTH_ARRAYS
	zassert_equal(ZCBOR_SUCCESS,
#else
	zassert_equal(ARR_ERR1,
#endif
		cbor_decode_Map(payload_map4_inv, sizeof(payload_map4_inv),
			&map, NULL), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Map(payload_map5, sizeof(payload_map5),
			&map, NULL), NULL);
	zassert_false(map.listkey, NULL);
	zassert_equal(union_nintuint_c, map.union_choice, NULL);
	zassert_equal(1, map.nintuint, NULL);
	zassert_equal(2, map.twotothree_count, NULL);
}


static bool my_decode_EmptyMap(zcbor_state_t *state, void *unused)
{
	size_t payload_len_out;
	int res = cbor_decode_EmptyMap(state->payload,
		state->payload_end - state->payload, NULL, &payload_len_out);

	if (!res) {
		state->payload += payload_len_out;
	}
	return !res;
}


ZTEST(cbor_decode_test5, test_empty_map)
{
	const uint8_t payload1[] = {MAP(0), END};
	const uint8_t payload2_inv[] = {MAP(1), END};
	const uint8_t payload3_inv[] = {MAP(1), 0, END};
	const uint8_t payload4[] = {MAP(0), END MAP(0), END MAP(0), END};
	size_t num_decode;

	ZCBOR_STATE_D(state, 0, payload4, sizeof(payload4), 3, 0);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_EmptyMap(payload1, sizeof(payload1), NULL, NULL), NULL);
#ifdef TEST_INDEFINITE_LENGTH_ARRAYS
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_EmptyMap(payload2_inv, sizeof(payload2_inv), NULL, NULL), NULL);
#else
	zassert_equal(ARR_ERR1, cbor_decode_EmptyMap(payload2_inv, sizeof(payload2_inv), NULL, NULL), NULL);
#endif
	zassert_equal(ARR_ERR1, cbor_decode_EmptyMap(payload3_inv, sizeof(payload3_inv), NULL, NULL), NULL);
	zassert_true(zcbor_multi_decode(3, 3, &num_decode, my_decode_EmptyMap, state, NULL, 0), NULL);
	zassert_equal(3, num_decode, NULL);
}


ZTEST(cbor_decode_test5, test_nested_list_map)
{
	const uint8_t payload_nested_lm1[] = {LIST(0), END};
	const uint8_t payload_nested_lm2[] = {LIST(1), MAP(0), END END};
	const uint8_t payload_nested_lm3[] = {LIST(1), MAP(1), 0x01, 0x04, END END};
	const uint8_t payload_nested_lm4_inv[] = {LIST(1), MAP(2), 0x01, 0x04, 0x1, 0x4, END END};
	const uint8_t payload_nested_lm5[] = {LIST(2), MAP(0), END MAP(1), 0x01, 0x04, END END};
	const uint8_t payload_nested_lm6_inv[] = {LIST(2), MAP(0), END MAP(1), 0x04, END END};
	const uint8_t payload_nested_lm7[] = {LIST(3), MAP(0), END MAP(0), END MAP(0), END END};
	struct NestedListMap listmap;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NestedListMap(payload_nested_lm1,
			sizeof(payload_nested_lm1), &listmap, NULL), NULL);
	zassert_equal(0, listmap.map_count, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NestedListMap(payload_nested_lm2,
			sizeof(payload_nested_lm2), &listmap, NULL), NULL);
	zassert_equal(1, listmap.map_count, NULL);
	zassert_false(listmap.map[0].uint4_present, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NestedListMap(payload_nested_lm3,
			sizeof(payload_nested_lm3), &listmap, NULL), NULL);
	zassert_equal(1, listmap.map_count, NULL);
	zassert_true(listmap.map[0].uint4_present, NULL);
	zassert_equal(1, listmap.map[0].uint4_present, NULL);
	zassert_equal(ARR_ERR1, cbor_decode_NestedListMap(payload_nested_lm4_inv,
			sizeof(payload_nested_lm4_inv), &listmap, NULL), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NestedListMap(payload_nested_lm5,
			sizeof(payload_nested_lm5), &listmap, NULL), NULL);
	zassert_equal(2, listmap.map_count, NULL);
	zassert_false(listmap.map[0].uint4_present, NULL);
	zassert_true(listmap.map[1].uint4_present, NULL);
	zassert_equal(ARR_ERR1, cbor_decode_NestedListMap(payload_nested_lm6_inv,
			sizeof(payload_nested_lm6_inv), &listmap, NULL), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NestedListMap(payload_nested_lm7,
			sizeof(payload_nested_lm7), &listmap, NULL), NULL);
	zassert_equal(3, listmap.map_count, NULL);
	zassert_false(listmap.map[0].uint4_present, NULL);
	zassert_false(listmap.map[1].uint4_present, NULL);
	zassert_false(listmap.map[2].uint4_present, NULL);
}

ZTEST(cbor_decode_test5, test_nested_map_list_map)
{
	const uint8_t payload_nested_mlm1[] = {MAP(1), LIST(0), END LIST(0), END END};
	const uint8_t payload_nested_mlm2[] = {MAP(1), LIST(0), END LIST(1), MAP(0), END END END};
	const uint8_t payload_nested_mlm3[] = {MAP(1), LIST(0), END LIST(2), MAP(0), END MAP(0), END END END};
	const uint8_t payload_nested_mlm4_inv[] = {MAP(1), LIST(0), END LIST(1), MAP(1), MAP(0), END END END END};
	const uint8_t payload_nested_mlm5[] = {MAP(2), LIST(0), END LIST(0), END LIST(0), END LIST(0), END END};
	const uint8_t payload_nested_mlm6[] = {MAP(3), LIST(0), END LIST(0), END LIST(0), END LIST(0), END LIST(0), END LIST(2), MAP(0), END MAP(0), END END END};
	struct NestedMapListMap maplistmap;

	int ret = cbor_decode_NestedMapListMap(payload_nested_mlm1,
			sizeof(payload_nested_mlm1), &maplistmap, NULL);
	zassert_equal(ZCBOR_SUCCESS, ret, "%d\r\n", ret);
	zassert_equal(1, maplistmap.map_l_count, NULL);
	zassert_equal(0, maplistmap.map_l[0].map_count, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NestedMapListMap(payload_nested_mlm2,
			sizeof(payload_nested_mlm2), &maplistmap, NULL), NULL);
	zassert_equal(1, maplistmap.map_l_count, NULL);
	zassert_equal(1, maplistmap.map_l[0].map_count, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NestedMapListMap(payload_nested_mlm3,
			sizeof(payload_nested_mlm3), &maplistmap, NULL), NULL);
	zassert_equal(1, maplistmap.map_l_count, NULL);
	zassert_equal(2, maplistmap.map_l[0].map_count, NULL);
	ret = cbor_decode_NestedMapListMap(payload_nested_mlm4_inv,
			sizeof(payload_nested_mlm4_inv), &maplistmap, NULL);
	zassert_equal(ZCBOR_ERR_ITERATIONS, ret, "%d\r\n", ret);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NestedMapListMap(payload_nested_mlm5,
			sizeof(payload_nested_mlm5), &maplistmap, NULL), NULL);
	zassert_equal(2, maplistmap.map_l_count, NULL);
	zassert_equal(0, maplistmap.map_l[0].map_count, NULL);
	zassert_equal(0, maplistmap.map_l[1].map_count, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_NestedMapListMap(payload_nested_mlm6,
			sizeof(payload_nested_mlm6), &maplistmap, NULL), NULL);
	zassert_equal(3, maplistmap.map_l_count, NULL);
	zassert_equal(0, maplistmap.map_l[0].map_count, NULL);
	zassert_equal(0, maplistmap.map_l[1].map_count, NULL);
	zassert_equal(2, maplistmap.map_l[2].map_count, NULL);
}


ZTEST(cbor_decode_test5, test_range)
{
	const uint8_t payload_range1[] = {LIST(3),
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x00,
		END
	};

	const uint8_t payload_range2[] = {LIST(6),
		0x05,
		0x08, 0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x00, 0x0A,
		END
	};

	const uint8_t payload_range3_inv[] = {LIST(4),
		0x06, // outside range
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x00,
		END
	};

	const uint8_t payload_range4_inv[] = {LIST(4),
		0x00,
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x0B, //outside range
		END
	};

	const uint8_t payload_range5[] = {LIST(5),
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x07,
		END
	};

	const uint8_t payload_range6_inv[] = {LIST(4),
		0x67, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x6f, 0x6f, // "hellooo" -> too long
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x07,
		END
	};

	const uint8_t payload_range7_inv[] = {LIST(5),
		0x22,
		0x62, 0x68, 0x65, // "he" -> too short
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x07,
		END
	};

	const uint8_t payload_range8_inv[] = {LIST(5),
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x07, 0x08, 0x18, // Last too large
		END
	};

	const uint8_t payload_range9[] = {0x84,
		0x28,
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x0
	};

	const uint8_t payload_range10_inv[] = {0x84,
		0x25,
		0x08,
		0x65, 0x68, 0x65, 0x6c, 0x6c, 0x6f, // "hello"
		0x0
	};

	struct Range output;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Range(payload_range1, sizeof(payload_range1),
				&output, NULL), NULL);
	zassert_false(output.optMinus5to5_present, NULL);
	zassert_false(output.optStr3to6_present, NULL);
	zassert_false(output.optMinus9toMinus6excl_present, NULL);
	zassert_equal(1, output.multi8_count, NULL);
	zassert_equal(1, output.multiHello_count, NULL);
	zassert_equal(1, output.multi0to10_count, NULL);
	zassert_equal(0, output.multi0to10[0], NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Range(payload_range2, sizeof(payload_range2),
				&output, NULL), NULL);
	zassert_true(output.optMinus5to5_present, NULL);
	zassert_equal(5, output.optMinus5to5, "was %d", output.optMinus5to5);
	zassert_false(output.optStr3to6_present, NULL);
	zassert_false(output.optMinus9toMinus6excl_present, NULL);
	zassert_equal(2, output.multi8_count, NULL);
	zassert_equal(1, output.multiHello_count, NULL);
	zassert_equal(2, output.multi0to10_count, NULL);
	zassert_equal(0, output.multi0to10[0], NULL);
	zassert_equal(10, output.multi0to10[1], NULL);

	int ret = cbor_decode_Range(payload_range3_inv, sizeof(payload_range3_inv),
				&output, NULL);
	zassert_equal(ZCBOR_ERR_ITERATIONS, ret, "%d\r\n", ret);

	zassert_equal(ZCBOR_ERR_ITERATIONS, cbor_decode_Range(payload_range4_inv, sizeof(payload_range4_inv),
				&output, NULL), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Range(payload_range5, sizeof(payload_range5),
				&output, NULL), NULL);
	zassert_false(output.optMinus5to5_present, NULL);
	zassert_true(output.optStr3to6_present, NULL);
	zassert_equal(5, output.optStr3to6.len, NULL);
	zassert_mem_equal("hello", output.optStr3to6.value, 5, NULL);
	zassert_false(output.optMinus9toMinus6excl_present, NULL);
	zassert_equal(1, output.multi8_count, NULL);
	zassert_equal(2, output.multiHello_count, NULL);
	zassert_equal(1, output.multi0to10_count, NULL);
	zassert_equal(7, output.multi0to10[0], NULL);

	zassert_equal(ZCBOR_ERR_ITERATIONS, cbor_decode_Range(payload_range6_inv, sizeof(payload_range6_inv),
				&output, NULL), NULL);

	zassert_equal(ZCBOR_ERR_ITERATIONS, cbor_decode_Range(payload_range7_inv, sizeof(payload_range7_inv),
				&output, NULL), NULL);

	ret = cbor_decode_Range(payload_range8_inv, sizeof(payload_range8_inv),
				&output, NULL);
	zassert_equal(ARR_ERR1, ret, "%d\r\n", ret);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Range(payload_range9, sizeof(payload_range9),
				&output, NULL), NULL);
	zassert_false(output.optMinus5to5_present, NULL);
	zassert_false(output.optStr3to6_present, NULL);
	zassert_true(output.optMinus9toMinus6excl_present, NULL);
	zassert_equal(-9, output.optMinus9toMinus6excl, NULL);
	zassert_equal(1, output.multi8_count, NULL);
	zassert_equal(1, output.multiHello_count, NULL);
	zassert_equal(1, output.multi0to10_count, NULL);
	zassert_equal(0, output.multi0to10[0], NULL);

	ret = cbor_decode_Range(payload_range10_inv, sizeof(payload_range10_inv),
				&output, NULL);
	zassert_equal(ZCBOR_ERR_ITERATIONS, ret, "%d\r\n", ret);
}

ZTEST(cbor_decode_test5, test_value_range)
{
	const uint8_t payload_value_range1[] = {LIST(6),
		11,
		0x19, 0x03, 0xe7, // 999
		0x29, // -10
		1,
		0x18, 42, // 42
		0x65, 'w', 'o', 'r', 'l', 'd', // "world"
		END
	};

	const uint8_t payload_value_range2[] = {LIST(6),
		0x18, 100, // 100
		0x39, 0x03, 0xe8, // -1001
		0x18, 100, // 100
		0,
		0x18, 42, // 42
		0x65, 'w', 'o', 'r', 'l', 'd', // "world"
		END
	};

	const uint8_t payload_value_range3_inv[] = {LIST(6),
		10,
		0x19, 0x03, 0xe7, // 999
		0x29, // -10
		1,
		0x18, 42, // 42
		0x65, 'w', 'o', 'r', 'l', 'd', // "world"
		END
	};

	const uint8_t payload_value_range4_inv[] = {LIST(6),
		11,
		0x19, 0x03, 0xe8, // 1000
		0x29, // -10
		1,
		0x18, 42, // 42
		0x65, 'w', 'o', 'r', 'l', 'd', // "world"
		END
	};

	const uint8_t payload_value_range5_inv[] = {LIST(6),
		11,
		0x19, 0x03, 0xe7, // 999
		0x2a, // -11
		1,
		0x18, 42, // 42
		0x65, 'w', 'o', 'r', 'l', 'd', // "world"
		END
	};

	const uint8_t payload_value_range6_inv[] = {LIST(6),
		11,
		0x19, 0x03, 0xe7, // 999
		0x29, // -10
		2,
		0x18, 42, // 42
		0x65, 'w', 'o', 'r', 'l', 'd', // "world"
		END
	};

	const uint8_t payload_value_range7_inv[] = {LIST(6),
		11,
		0x19, 0x03, 0xe7, // 999
		0x29, // -10
		1,
		0x18, 43, // 42
		0x65, 'w', 'o', 'r', 'l', 'e', // "worle"
		END
	};

	const uint8_t payload_value_range8_inv[] = {LIST(6),
		11,
		0x19, 0x03, 0xe7, // 999
		0x29, // -10
		1,
		0x18, 42, // 42
		0x66, 'w', 'o', 'r', 'l', 'd', 'd', // "worldd"
		END
	};

	const uint8_t payload_value_range9_inv[] = {LIST(6),
		11,
		0x19, 0x03, 0xe7, // 999
		0x29, // -10
		1,
		0x18, 42, // 42
		0x64, 'w', 'o', 'r', 'l', // "worl"
		END
	};

	const uint8_t payload_value_range10_inv[] = {LIST(6),
		11,
		0x39, 0x03, 0xe6, // -999
		0x39, // -10
		1,
		0x18, 42, // 42
		0x65, 'w', 'o', 'r', 'l', 'd', // "world"
		END
	};

	const uint8_t payload_value_range11_inv[] = {LIST(6),
		11,
		0x1a, 0x10, 0x00, 0x00, 0x00, // 0x10000000
		0x39, 0x03, 0xe8, // -1001
		1,
		0x18, 42, // 42
		0x65, 'w', 'o', 'r', 'l', 'd', // "world"
		END
	};

	struct ValueRange exp_output_value_range1 = {
		.greater10 = 11,
		.less1000 = 999,
		.greatereqmin10 = -10,
		.lesseq1 = 1,
	};
	struct ValueRange exp_output_value_range2 = {
		.greater10 = 100,
		.less1000 = -1001,
		.greatereqmin10 = 100,
		.lesseq1 = 0,
	};

	struct ValueRange output;
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_ValueRange(payload_value_range1, sizeof(payload_value_range1),
					&output, &out_len), NULL);
	zassert_equal(sizeof(payload_value_range1), out_len, NULL);
	zassert_equal(exp_output_value_range1.greater10,
			output.greater10, NULL);
	zassert_equal(exp_output_value_range1.less1000,
			output.less1000, NULL);
	zassert_equal(exp_output_value_range1.greatereqmin10,
			output.greatereqmin10, NULL);
	zassert_equal(exp_output_value_range1.lesseq1,
			output.lesseq1, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_ValueRange(payload_value_range2, sizeof(payload_value_range2),
					&output, &out_len), NULL);
	zassert_equal(sizeof(payload_value_range2), out_len, NULL);
	zassert_equal(exp_output_value_range2.greater10,
			output.greater10, NULL);
	zassert_equal(exp_output_value_range2.less1000,
			output.less1000, NULL);
	zassert_equal(exp_output_value_range2.greatereqmin10,
			output.greatereqmin10, NULL);
	zassert_equal(exp_output_value_range2.lesseq1,
			output.lesseq1, NULL);

	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_ValueRange(payload_value_range3_inv,
				sizeof(payload_value_range3_inv), &output, &out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_ValueRange(payload_value_range4_inv,
				sizeof(payload_value_range4_inv), &output, &out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_ValueRange(payload_value_range5_inv,
				sizeof(payload_value_range5_inv), &output, &out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_ValueRange(payload_value_range6_inv,
				sizeof(payload_value_range6_inv), &output, &out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_ValueRange(payload_value_range7_inv,
				sizeof(payload_value_range7_inv), &output, &out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_ValueRange(payload_value_range8_inv,
				sizeof(payload_value_range8_inv), &output, &out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_ValueRange(payload_value_range9_inv,
				sizeof(payload_value_range9_inv), &output, &out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_ValueRange(payload_value_range10_inv,
				sizeof(payload_value_range10_inv), &output, &out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_ValueRange(payload_value_range11_inv,
				sizeof(payload_value_range11_inv), &output, &out_len), NULL);
}

ZTEST(cbor_decode_test5, test_single)
{
	uint8_t payload_single0[] = {0x45, 'h', 'e', 'l', 'l', 'o'};
	uint8_t payload_single1[] = {0x18, 52};
	uint8_t payload_single2_inv[] = {0x18, 53};
	uint8_t payload_single3[] = {9};
	uint8_t payload_single4_inv[] = {10};
	struct zcbor_string result_bstr;
	size_t result_int;
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SingleBstr(payload_single0, sizeof(payload_single0), &result_bstr, &out_len), NULL);
	zassert_equal(sizeof(payload_single0), out_len, NULL);
	zassert_equal(5, result_bstr.len, NULL);
	zassert_mem_equal(result_bstr.value, "hello", result_bstr.len, NULL);
	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, cbor_decode_SingleBstr(payload_single0, 5, &result_bstr, &out_len), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SingleInt_uint52(payload_single1, sizeof(payload_single1), NULL, &out_len), NULL); // Result pointer not needed.
	zassert_equal(sizeof(payload_single1), out_len, NULL);
	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, cbor_decode_SingleInt_uint52(payload_single1, 1, NULL, &out_len), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_SingleInt_uint52(payload_single2_inv, sizeof(payload_single2_inv), NULL, &out_len), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SingleInt2(payload_single3, sizeof(payload_single3), &result_int, &out_len), NULL);
	zassert_equal(sizeof(payload_single3), out_len, NULL);
	zassert_equal(9, result_int, NULL);
	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_SingleInt2(payload_single4_inv, sizeof(payload_single4_inv), &result_int, &out_len), NULL);
}

ZTEST(cbor_decode_test5, test_unabstracted)
{
	uint8_t payload_unabstracted0[] = {LIST(2), 0x01, 0x03, END};
	uint8_t payload_unabstracted1[] = {LIST(2), 0x02, 0x04, END};
	uint8_t payload_unabstracted2_inv[] = {LIST(2), 0x03, 0x03, END};
	uint8_t payload_unabstracted3_inv[] = {LIST(2), 0x01, 0x01, END};
	struct Unabstracted result_unabstracted;
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Unabstracted(payload_unabstracted0,
					sizeof(payload_unabstracted0),
					&result_unabstracted, &out_len), NULL);
	zassert_equal(result_unabstracted.unabstractedunion1_choice,
			Unabstracted_unabstractedunion1_choice1_c, NULL);
	zassert_equal(result_unabstracted.unabstractedunion2_choice,
			Unabstracted_unabstractedunion2_uint3_c, NULL);
	zassert_equal(sizeof(payload_unabstracted0), out_len, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Unabstracted(payload_unabstracted1,
					sizeof(payload_unabstracted1),
					&result_unabstracted, &out_len), NULL);
	zassert_equal(result_unabstracted.unabstractedunion1_choice,
			Unabstracted_unabstractedunion1_choice2_c, NULL);
	zassert_equal(result_unabstracted.unabstractedunion2_choice,
			Unabstracted_unabstractedunion2_choice4_c, NULL);
	zassert_equal(sizeof(payload_unabstracted1), out_len, NULL);

	int ret = cbor_decode_Unabstracted(payload_unabstracted2_inv,
					sizeof(payload_unabstracted2_inv),
					&result_unabstracted, &out_len);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, ret, "%d\r\n", ret);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Unabstracted(payload_unabstracted3_inv,
					sizeof(payload_unabstracted3_inv),
					&result_unabstracted, &out_len), NULL);
}

ZTEST(cbor_decode_test5, test_quantity_range)
{
	uint8_t payload_qty_range1[] = {0xF5, 0xF5, 0xF5};
	uint8_t payload_qty_range2_inv[] = {0xF5, 0xF5};
	uint8_t payload_qty_range3[] = {0xF6, 0xF6, 0xF6, 0xF6, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5};
	uint8_t payload_qty_range4_inv[] = {0xF6, 0xF6, 0xF6, 0xF6, 0xF6, 0xF5, 0xF5, 0xF5};
	struct QuantityRange result_qty_range;
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_QuantityRange(payload_qty_range1,
					sizeof(payload_qty_range1),
					&result_qty_range, &out_len), NULL);
	zassert_equal(0, result_qty_range.upto4nils_count, NULL);
	zassert_equal(3, result_qty_range.from3true_count, NULL);
	zassert_equal(sizeof(payload_qty_range1), out_len, NULL);

	zassert_equal(ZCBOR_ERR_ITERATIONS, cbor_decode_QuantityRange(payload_qty_range2_inv,
					sizeof(payload_qty_range2_inv),
					&result_qty_range, &out_len), NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_QuantityRange(payload_qty_range3,
					sizeof(payload_qty_range3),
					&result_qty_range, &out_len), NULL);
	zassert_equal(4, result_qty_range.upto4nils_count, NULL);
	zassert_equal(6, result_qty_range.from3true_count, NULL);
	zassert_equal(sizeof(payload_qty_range3), out_len, NULL);

	zassert_equal(ZCBOR_ERR_ITERATIONS, cbor_decode_QuantityRange(payload_qty_range4_inv,
					sizeof(payload_qty_range4_inv),
					&result_qty_range, &out_len), NULL);
}

ZTEST(cbor_decode_test5, test_doublemap)
{
	uint8_t payload_doublemap0[] = {MAP(2), 0x01, 0xA1, 0x01, 0x01, 0x02, 0xA1, 0x02, 0x02, END};
	uint8_t payload_doublemap1_inv[] = {MAP(2), 0x01, 0xA1, 0x01, 0x01, 0x02, 0xA1, 0x03, 0x02, END};
	struct DoubleMap result_doublemap;
	size_t out_len;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_DoubleMap(payload_doublemap0,
					sizeof(payload_doublemap0),
					&result_doublemap, &out_len), NULL);
	zassert_equal(result_doublemap.uintmap_count, 2, NULL);
	zassert_equal(result_doublemap.uintmap[0].uintmap_key, 1, NULL);
	zassert_true(result_doublemap.uintmap[0].MyKeys_m.uint1int_present, NULL);
	zassert_equal(result_doublemap.uintmap[0].MyKeys_m.uint1int.uint1int, 1, NULL);
	zassert_false(result_doublemap.uintmap[0].MyKeys_m.uint2int_present, NULL);
	zassert_equal(result_doublemap.uintmap[1].uintmap_key, 2, NULL);
	zassert_false(result_doublemap.uintmap[1].MyKeys_m.uint1int_present, NULL);
	zassert_true(result_doublemap.uintmap[1].MyKeys_m.uint2int_present, NULL);
	zassert_equal(result_doublemap.uintmap[1].MyKeys_m.uint2int.uint2int, 2, NULL);

	int ret = cbor_decode_DoubleMap(payload_doublemap1_inv,
					sizeof(payload_doublemap1_inv),
					&result_doublemap, &out_len);
	zassert_equal(ARR_ERR1, ret, "%d\r\n", ret);
}


#ifndef INFINITY
#define INFINITY (__builtin_inff())
#endif


ZTEST(cbor_decode_test5, test_floats)
{

	uint8_t floats_payload1[] = {LIST(7), 0xF9, 0x42, 0x48, /* 3.1415 */
			0xFA, 0x49, 0x96, 0xb4, 0x3f /* 1234567.89 */,
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			0xFA, 0x40, 0x49, 0xe, 0x56 /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			0xFA, 0x39, 0x8d, 0x2c, 0xef /* 123/456789 */,
			0xFB, 0xbd, 0x50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 /* -2^(-42) */,
			END
	};

	uint8_t floats_payload2[] = {LIST(5), 0xF9, 0xfc, 0x0, /* -infinity */
			0xFA, 0xc7, 0xc0, 0xe6, 0xb7 /* -98765.4321 */,
			0xFB, 0x41, 0x32, 0xd6, 0x87, 0xe3, 0xd7, 0xa, 0x3d /* 1234567.89 */,
			0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			END
	};

	uint8_t floats_payload3[] = {LIST(5), 0xF9, 0x0, 0x1, /* 0.000000059604645 */
			0xFA, 0, 0, 0, 0 /* 0.0 */,
			0xFB, 0, 0, 0, 0, 0, 0, 0, 0 /* 0.0 */,
			0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			END
	};

	uint8_t floats_payload4[] = {LIST(6), 0xF9, 0x42, 0x48, /* 3.1415 */
			0xFA, 0x49, 0x96, 0xb4, 0x3f /* 1234567.89 */,
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			0xFB, 0x3f, 0x31, 0xa5, 0x9d, 0xe0, 0x0, 0x0, 0x0 /* 123/456789 */,
			END
	};

	uint8_t floats_payload5[] = {LIST(6), 0xF9, 0x42, 0x48, /* 3.1415 */
			0xFA, 0x49, 0x96, 0xb4, 0x3f /* 1234567.89 */,
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			0xF9, 0xc, 0x69 /* 123/456789 UNSUPPORTED SIZE */,
			END
	};

	uint8_t floats_payload6_inv[] = {LIST(5), 0xF9, 0x42, 0x48, /* 3.1415 */
			0xFB, 0x41, 0x32, 0xd6, 0x87, 0xe0, 0x0, 0x0, 0x0 /* 1234567.89 WRONG SIZE */,
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			END
	};

	uint8_t floats_payload7_inv[] = {LIST(5), 0xF9, 0x42, 0x48, /* 3.1415 */
			0xFA, 0x49, 0x96, 0xb4, 0x3f /* 1234567.89 */,
			0xFA, 0xc7, 0xc0, 0xe6, 0xb7 /* -98765.4321 WRONG SIZE */,
			0xFA, 0x40, 0x49, 0xe, 0x56 /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			END
	};

	uint8_t floats_payload8_inv[] = {LIST(5), 0xF9, 0x42, 0x48, /* 3.1415 */
			0xFA, 0x49, 0x96, 0xb4, 0x3f /* 1234567.89 */,
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			0xFA, 0x40, 0x49, 0xe, 0x56 /* 3.1415 */,
			0xFA, 0x40, 0x2d, 0xf8, 0x4d /* 2.71828 WRONG SIZE */,
			END
	};

	uint8_t floats_payload9_inv[] = {LIST(5), 0xF9, 0x42, 0x48, /* 3.1415 */
			0xFA, 0x49, 0x96, 0xb4, 0x3f /* 1234567.89 */,
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			0xFB, 0x40, 0x9, 0x21, 0xca, 0, 0, 0, 0 /* 3.1415 */,
			0xFB, 0x40, 0x5, 0xbf, 0x9, 0x95, 0xaa, 0xf7, 0x90 /* 2.71828 */,
			END
	};

	size_t num_decode;
	struct Floats result;
	int ret;
	float expected;

	ret = cbor_decode_Floats(floats_payload1, sizeof(floats_payload1), &result, &num_decode);
	zassert_equal(ZCBOR_SUCCESS, ret, "%d\n", ret);
	zassert_equal(sizeof(floats_payload1), num_decode, NULL);
	zassert_equal((float)3.140625, result.float_16, NULL);
	zassert_equal((float)1234567.89, result.float_32, NULL);
	zassert_equal((double)-98765.4321, result.float_64, NULL);
	zassert_equal(2, result.floats_count, NULL);
	zassert_equal((double)(float)(123.0/456789.0), result.floats[0], NULL);
	zassert_equal((double)(-1.0/(1LL << 42)), result.floats[1], NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Floats(
		floats_payload2, sizeof(floats_payload2), &result, &num_decode), NULL);
	zassert_equal(sizeof(floats_payload2), num_decode, NULL);
	zassert_equal((float)-INFINITY, result.float_16, NULL);
	zassert_equal((float)-98765.4321, result.float_32, NULL);
	zassert_equal((double)1234567.89, result.float_64, NULL);
	zassert_equal(0, result.floats_count, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Floats(
		floats_payload3, sizeof(floats_payload3), &result, &num_decode), NULL);
	zassert_equal(sizeof(floats_payload3), num_decode, NULL);
	zassert_equal((float)0.000000059604645, result.float_16, NULL);
	zassert_equal((float)0.0, result.float_32, NULL);
	zassert_equal((double)0.0, result.float_64, NULL);
	zassert_equal(0, result.floats_count, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Floats(
		floats_payload4, sizeof(floats_payload4), &result, &num_decode), NULL);
	zassert_equal(sizeof(floats_payload4), num_decode, NULL);
	zassert_equal((float)3.140625, result.float_16, NULL);
	zassert_equal((float)1234567.89, result.float_32, NULL);
	zassert_equal((double)-98765.4321, result.float_64, NULL);
	zassert_equal(1, result.floats_count, NULL);
	zassert_false((double)(123.0/456789.0) == result.floats[0], NULL);
	zassert_equal((double)(float)(123.0/456789.0), result.floats[0], NULL);

	ret = cbor_decode_Floats(
		floats_payload5, sizeof(floats_payload5), &result, &num_decode);
	zassert_equal(ZCBOR_SUCCESS, ret, "%d\r\n", ret);
	zassert_equal(sizeof(floats_payload5), num_decode, NULL);
	zassert_equal((float)3.140625, result.float_16, NULL);
	zassert_equal((float)1234567.89, result.float_32, NULL);
	zassert_equal((double)-98765.4321, result.float_64, NULL);
	zassert_equal(1, result.floats_count, NULL);
	zassert_false((double)(123.0/456789.0) == result.floats[0], NULL);
	expected = 0.00026917458f; /* 0x398d2000 */
	zassert_equal((double)expected, result.floats[0], NULL);

	zassert_equal(ZCBOR_ERR_FLOAT_SIZE, cbor_decode_Floats(
		floats_payload6_inv, sizeof(floats_payload6_inv), &result, &num_decode), NULL);

	zassert_equal(ZCBOR_ERR_FLOAT_SIZE, cbor_decode_Floats(
		floats_payload7_inv, sizeof(floats_payload7_inv), &result, &num_decode), NULL);

	ret = cbor_decode_Floats(
		floats_payload8_inv, sizeof(floats_payload8_inv), &result, &num_decode);
	zassert_equal(ZCBOR_ERR_FLOAT_SIZE, ret, "%d\r\n", ret);

	zassert_equal(ZCBOR_ERR_WRONG_VALUE, cbor_decode_Floats(
		floats_payload9_inv, sizeof(floats_payload9_inv), &result, &num_decode), NULL);
}


/* Test using ranges (greater/less than) on floats. */
ZTEST(cbor_decode_test5, test_floats2)
{
	uint8_t floats2_payload1[] = {LIST(2),
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			0xFB, 0xbd, 0x50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 /* -2^(-42) */,
			END
	};
	uint8_t floats2_payload2[] = {LIST(2),
			0xFB, 0xbd, 0x50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 /* -2^(-42) */,
			0xFB, 0xc0, 0xc3, 0x88, 0x0, 0x0, 0x0, 0x0, 0x0 /* -10000 */,
			END
	};
	uint8_t floats2_payload3[] = {LIST(2),
			0xF9, 0xf0, 0xe2 /* -10000 */,
			0xFA, 0x39, 0x8d, 0x2c, 0xef /* 123/456789 */,
			END
	};
	uint8_t floats2_payload4_inv[] = {LIST(2),
			0xFA, 0x3f, 0x80, 0x0, 0x0 /* 1.0 */,
			0xFB, 0xc0, 0xc3, 0x88, 0x0, 0x0, 0x0, 0x0, 0x0 /* -10000 */,
			END
	};
	uint8_t floats2_payload5_inv[] = {LIST(2),
			0xF9, 0x3c, 0x0 /* 1.0 */,
			0xF9, 0xf0, 0xe2 /* -10000 */,
			END
	};
	uint8_t floats2_payload6_inv[] = {LIST(2),
			0xFB, 0xbd, 0x50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 /* -2^(-42) */,
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			END
	};

	size_t num_decode;
	struct Floats2 result;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Floats2(
		floats2_payload1, sizeof(floats2_payload1), &result, &num_decode), NULL);
	zassert_equal(sizeof(floats2_payload1), num_decode, NULL);
	zassert_equal((double)-98765.4321, result.float_lt_1, NULL);
	zassert_equal((double)(-1.0/(1LL << 42)), result.float_ge_min_10000, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Floats2(
		floats2_payload2, sizeof(floats2_payload2), &result, &num_decode), NULL);
	zassert_equal(sizeof(floats2_payload2), num_decode, NULL);
	zassert_equal((double)(-1.0/(1LL << 42)), result.float_lt_1, NULL);
	zassert_equal((double)-10000, result.float_ge_min_10000, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Floats2(
		floats2_payload3, sizeof(floats2_payload3), &result, &num_decode), NULL);
	zassert_equal(sizeof(floats2_payload3), num_decode, NULL);
	zassert_equal((double)(-10000), result.float_lt_1, NULL);
	zassert_equal((double)(float)(123.0/456789.0), result.float_ge_min_10000, NULL);

	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_Floats2(
		floats2_payload4_inv, sizeof(floats2_payload4_inv), &result, &num_decode), NULL);

	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_Floats2(
		floats2_payload5_inv, sizeof(floats2_payload5_inv), &result, &num_decode), NULL);

	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_Floats2(
		floats2_payload6_inv, sizeof(floats2_payload6_inv), &result, &num_decode), NULL);
}


/* Test float16-32 and float32-64 */
ZTEST(cbor_decode_test5, test_floats3)
{
	uint8_t floats3_payload1[] = {LIST(2),
			0xF9, 0xf0, 0xe2 /* -10000 */,
			0xFA, 0x39, 0x8d, 0x2c, 0xef /* 123/456789 */,
			END
	};
	uint8_t floats3_payload2[] = {LIST(2),
			0xFA, 0x3f, 0x80, 0x0, 0x0 /* 1.0 */,
			0xFB, 0xc0, 0xc3, 0x88, 0x0, 0x0, 0x0, 0x0, 0x0 /* -10000 */,
			END
	};
	uint8_t floats3_payload3_inv[] = {LIST(2),
			0xF9, 0x3c, 0x0 /* 1.0 */,
			0xF9, 0xf0, 0xe2 /* -10000 */,
			END
	};
	uint8_t floats3_payload4_inv[] = {LIST(2),
			0xFB, 0xbd, 0x50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 /* -2^(-42) */,
			0xFB, 0xc0, 0xf8, 0x1c, 0xd6, 0xe9, 0xe1, 0xb0, 0x8a /* -98765.4321 */,
			END
	};

	size_t num_decode;
	struct Floats3 result;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Floats3(
		floats3_payload1, sizeof(floats3_payload1), &result, &num_decode), NULL);
	zassert_equal(sizeof(floats3_payload1), num_decode, NULL);
	zassert_equal((float)(-10000), result.float_16_32, NULL);
	zassert_equal((double)(float)(123.0/456789.0), result.float_32_64, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Floats3(
		floats3_payload2, sizeof(floats3_payload2), &result, &num_decode), NULL);
	zassert_equal(sizeof(floats3_payload2), num_decode, NULL);
	zassert_equal((float)(1.0), result.float_16_32, NULL);
	zassert_equal((double)(-10000), result.float_32_64, NULL);

	zassert_equal(ZCBOR_ERR_FLOAT_SIZE, cbor_decode_Floats3(
		floats3_payload3_inv, sizeof(floats3_payload3_inv), &result, &num_decode), NULL);

	zassert_equal(ZCBOR_ERR_FLOAT_SIZE, cbor_decode_Floats3(
		floats3_payload4_inv, sizeof(floats3_payload4_inv), &result, &num_decode), NULL);
}

ZTEST(cbor_decode_test5, test_prelude)
{
	uint8_t prelude_payload1[] = {
		LIST3(25), 0x40 /* empty bstr */, 0x60 /* empty tstr */,
		0xC0, 0x6A, '2', '0', '2', '2', '-', '0', '2', '-', '2', '2' /* tdate */,
		0xC1, 0x1A, 0x62, 0x15, 0x5d, 0x6c /* 1645567340 */,
		0xFA, 0x40, 0x49, 0xe, 0x56 /* 3.1415 */,
		0xC2, 0x4A, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 /* 0x0102030405060708090A */,
		0xC3, 0x4A, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9 /* -0x0102030405060708090A */,
		0xC3, 0x4A, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9 /* -0x0102030405060708090A */,
		0 /* 0 (integer) */, 0,
		0xC4, LIST(2), 0x01, 0xC2, 0x4C, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, END /* decfrac: [1, 0x010000000000000000000001] */
		0xC5, LIST(2), 0x04, 0xC3, 0x4C, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, END /* bigfloat: [4, -0x020000000000000000000001] */
		0xD5, LIST(2), LIST(1), 0x41, 'a', END LIST(1), 0x41, 'b', END END /* eb64url: [['a']['b']] */
		0xD6, 0xF6, 0xD7, 0xF6,
		0xD8, 24, 0x41, 0x0 /* encoded-cbor: 0x0 */,
		0xD8, 32, 0x71,  'f', 't', 'p', ':', '/', '/', 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm',
		0xD8, 33, 0x60, 0xD8, 34, 0x60,
		0xD8, 35, 0x62, '.', '*' /* regexp: ".*" */,
		0xD8, 36, 0x60, 0xD9, 0xd9, 0xf7, 0xF6,
		0xFA, 0x40, 0x49, 0xe, 0x56 /* 3.1415 */,
		0xFA, 0x40, 0x49, 0xe, 0x56 /* 3.1415 */,
		0xF6,
		END
	};

	struct Prelude result;
	size_t num_decode;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Prelude(prelude_payload1, sizeof(prelude_payload1), &result, &num_decode), NULL);
}

/** Testing that the generated code correctly enforces the restrictions that are
 *  specified inside the .cbor statements. I.e. that it checks that the string and
 *  float values are correct.
*/
ZTEST(cbor_decode_test5, test_cbor_bstr)
{

#ifdef TEST_INDEFINITE_LENGTH_ARRAYS
#define CBOR_BSTR_LEN(len) (len + 1)
#else
#define CBOR_BSTR_LEN(len) len
#endif
	uint8_t cbor_bstr_payload1[] = {
		0x58, CBOR_BSTR_LEN(33),
			LIST(4),
				0x46, 0x65, 'H', 'e', 'l', 'l', 'o',
				0x49, 0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
				0x4C, 0xC2, 0x4A, 0x42, 2, 3, 4, 5, 6, 7, 8, 9, 10 /* 0x4202030405060708090A */,
				0x41, 0xF6, /* nil */
			END
	};

	uint8_t cbor_bstr_payload2_inv[] = {
		0x58, CBOR_BSTR_LEN(33),
			LIST(4),
				0x46, 0x65, 'Y', 'e', 'l', 'l', 'o',
				0x49, 0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
				0x4C, 0xC2, 0x4A, 0x42, 2, 3, 4, 5, 6, 7, 8, 9, 10 /* 0x4202030405060708090A */,
				0x41, 0xF6, /* nil */
			END
	};

	uint8_t cbor_bstr_payload3_inv[] = {
		0x58, CBOR_BSTR_LEN(33),
			LIST(4),
				0x46, 0x65, 'H', 'e', 'l', 'l', 'o',
				0x49, 0xFB, 0x40, 0x9, 0x21, 0xca, 0, 0, 0, 0 /* 3.1415 */,
				0x4C, 0xC2, 0x4A, 0x42, 2, 3, 4, 5, 6, 7, 8, 9, 10 /* 0x4202030405060708090A */,
				0x41, 0xF6, /* nil */
			END
	};

	uint8_t cbor_bstr_payload4_inv[] = {
		0x58, CBOR_BSTR_LEN(18),
			LIST(3),
				0x46, 0x65, 'H', 'e', 'l', 'l', 'o',
				0x49, 0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
			END
	};

	uint8_t cbor_bstr_payload5_inv[] = {
		0x58, CBOR_BSTR_LEN(18),
			LIST(2),
				0x46, 0x65, 'H', 'e', 'l', 'l', 'o',
				0x49, 0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
			END
	};

	uint8_t cbor_bstr_payload6_inv[] = {
		0x58, CBOR_BSTR_LEN(33),
			LIST(4),
				0x46, 0x65, 'H', 'e', 'l', 'l', 'o',
				0x49, 0xFB, 0x40, 0x9, 0x21, 0xca, 0xc0, 0x83, 0x12, 0x6f /* 3.1415 */,
				0x4C, 0xC2, 0x4A, 0x42, 2, 3, 4, 5, 6, 7, 8, 9, 10 /* 0x4202030405060708090A */,
				0x41, 0xF5, /* true (wrong) */
			END
	};

	struct CBORBstr result;
	size_t num_decode;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_CBORBstr(cbor_bstr_payload1, sizeof(cbor_bstr_payload1), &result, &num_decode), NULL);
	zassert_equal(&cbor_bstr_payload1[2], result.CBORBstr.value, NULL);
	zassert_equal(&cbor_bstr_payload1[21], result.big_uint_bstr.value, NULL);
	zassert_equal(&cbor_bstr_payload1[23], result.big_uint_bstr_cbor.value, NULL);

	int res = cbor_decode_CBORBstr(cbor_bstr_payload2_inv, sizeof(cbor_bstr_payload2_inv), &result, &num_decode);
	zassert_equal(ZCBOR_ERR_PAYLOAD_NOT_CONSUMED, res, "%d\r\n", res);

	zassert_equal(ZCBOR_ERR_PAYLOAD_NOT_CONSUMED, cbor_decode_CBORBstr(cbor_bstr_payload3_inv, sizeof(cbor_bstr_payload3_inv), &result, &num_decode), NULL);

	res = cbor_decode_CBORBstr(cbor_bstr_payload4_inv, sizeof(cbor_bstr_payload4_inv), &result, &num_decode);
	zassert_equal(ARR_ERR4, res, "%d\r\n", res);

	res = cbor_decode_CBORBstr(cbor_bstr_payload5_inv, sizeof(cbor_bstr_payload5_inv), &result, &num_decode);
	zassert_equal(ARR_ERR4, res, "%d\r\n", res);

	res = cbor_decode_CBORBstr(cbor_bstr_payload6_inv, sizeof(cbor_bstr_payload6_inv), &result, &num_decode);
	zassert_equal(ZCBOR_ERR_PAYLOAD_NOT_CONSUMED, res, "%d\r\n", res);
}


ZTEST(cbor_decode_test5, test_map_length)
{
	uint8_t map_length_payload1[] = {MAP(2),
		0x61, 'r', 0x01,
		0x61, 'm', 0x46, 1, 2, 3, 4, 5, 6, END
	};
	uint8_t map_length_payload2[] = {MAP(3),
		0x61, 'r', 0x01,
		0x61, 'm', 0x46, 1, 2, 3, 4, 5, 6,
		0x61, 'e', LIST(0), END END
	};
	uint8_t map_length_payload3[] = {MAP(3),
		0x61, 'r', 0x01,
		0x61, 'm', 0x46, 1, 2, 3, 4, 5, 6,
		0x61, 'e', LIST(1), 0x50, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, END END
	};
	uint8_t map_length_payload4[] = {MAP(3),
		0x61, 'r', 0x01,
		0x61, 'm', 0x46, 1, 2, 3, 4, 5, 6,
		0x61, 'e', LIST(2), 0x50, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1,
			0x50, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, END END
	};
	uint8_t map_length_payload5_inv[] = {MAP(2),
		0x61, 'r', 0x01,
		0x61, 'm', 0x45, 1, 2, 3, 4, 5, END
	};
	uint8_t map_length_payload6_inv[] = {MAP(3),
		0x61, 'r', 0x01,
		0x61, 'm', 0x46, 1, 2, 3, 4, 5, 6,
		0x61, 'e', LIST(2), 0x50, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1,
			0x49, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, END END
	};
	uint8_t map_length_payload7_inv[] = {MAP(3),
		0x61, 'r', 0x01,
		0x61, 'm', 0x46, 1, 2, 3, 4, 5, 6,
		0x61, 'e', LIST(2), 0x51, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, 0,
			0x50, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, END END
	};

	struct MapLength result;
	size_t num_decode;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_MapLength(map_length_payload1,
		sizeof(map_length_payload1), &result, &num_decode), NULL);
	zassert_equal(sizeof(map_length_payload1), num_decode, NULL);
	zassert_false(result.end_device_array_present, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_MapLength(map_length_payload2,
		sizeof(map_length_payload2), &result, &num_decode), NULL);
	zassert_equal(sizeof(map_length_payload2), num_decode, NULL);
	zassert_true(result.end_device_array_present, NULL);
	zassert_equal(0, result.end_device_array.uuid_m_count, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_MapLength(map_length_payload3,
		sizeof(map_length_payload3), &result, &num_decode), NULL);
	zassert_equal(sizeof(map_length_payload3), num_decode, NULL);
	zassert_true(result.end_device_array_present, NULL);
	zassert_equal(1, result.end_device_array.uuid_m_count, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_MapLength(map_length_payload4,
		sizeof(map_length_payload4), &result, &num_decode), NULL);
	zassert_equal(sizeof(map_length_payload4), num_decode, NULL);
	zassert_true(result.end_device_array_present, NULL);
	zassert_equal(2, result.end_device_array.uuid_m_count, NULL);
	zassert_equal(16, result.end_device_array.uuid_m[0].len, NULL);
	zassert_equal(16, result.end_device_array.uuid_m[1].len, NULL);
	zassert_equal(8, result.end_device_array.uuid_m[1].value[8], NULL);

	zassert_equal(ZCBOR_ERR_WRONG_RANGE, cbor_decode_MapLength(map_length_payload5_inv,
		sizeof(map_length_payload5_inv), &result, &num_decode), NULL);

	int err = cbor_decode_MapLength(map_length_payload6_inv,
		sizeof(map_length_payload6_inv), &result, &num_decode);
	zassert_equal(ARR_ERR1, err, "%d\r\n", err);

	err = cbor_decode_MapLength(map_length_payload7_inv,
		sizeof(map_length_payload7_inv), &result, &num_decode);
	zassert_equal(ARR_ERR1, err, "%d\r\n", err);
}


/* UnionInt1 will be optimized, while UnionInt2 won't because it contains a separate type
   (Structure_One) which prevents it from being optimized. */
ZTEST(cbor_decode_test5, test_union_int)
{
	uint8_t union_int_payload1[] = {LIST(2),
		0x05, 0x6E, 'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 'a', ' ', 'f', 'i', 'v', 'e',
		END
	};
	uint8_t union_int_payload2[] = {LIST(2),
		0x19, 0x03, 0xE8, 0x50, 'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 't', 'h', 'o', 'u', 's', 'a', 'n', 'd',
		END
	};
	uint8_t union_int_payload3[] = {LIST(3),
		0x3A, 0x00, 0x01, 0x86, 0x9F, 0xF6, 0x01,
		END
	};
	uint8_t union_int_payload4_inv[] = {LIST(3),
		0x3A, 0x00, 0x01, 0x86, 0x9F, 0xF7, 0x01,
		END
	};
	uint8_t union_int_payload5_inv[] = {LIST(2),
		0x24, 0x6E, 'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 'a', ' ', 'f', 'i', 'v', 'e',
		END
	};
	uint8_t union_int_payload6[] = {LIST(2),
		0x01, 0x42, 'h', 'i',
		END
	};
	struct UnionInt1 result1;
	struct UnionInt2 result2;
	size_t num_decode;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_UnionInt1(union_int_payload1,
		sizeof(union_int_payload1), &result1, &num_decode), NULL);
	zassert_equal(sizeof(union_int_payload1), num_decode, NULL);
	zassert_equal(result1.union_choice, UnionInt1_union_uint1_l_c, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_UnionInt2(union_int_payload1,
		sizeof(union_int_payload1), &result2, &num_decode), NULL);
	zassert_equal(sizeof(union_int_payload1), num_decode, NULL);
	zassert_equal(result2.union_choice, union_uint5_l_c, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_UnionInt1(union_int_payload2,
		sizeof(union_int_payload2), &result1, &num_decode), NULL);
	zassert_equal(sizeof(union_int_payload2), num_decode, NULL);
	zassert_equal(result1.union_choice, UnionInt1_union_uint2_l_c, NULL);
	zassert_equal(result1.bstr.len, 16, NULL);
	zassert_mem_equal(result1.bstr.value, "This is thousand", 16, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_UnionInt2(union_int_payload2,
		sizeof(union_int_payload2), &result2, &num_decode), NULL);
	zassert_equal(sizeof(union_int_payload2), num_decode, NULL);
	zassert_equal(result2.union_choice, union_uint1000_l_c, NULL);
	zassert_equal(result2.bstr.len, 16, NULL);
	zassert_mem_equal(result2.bstr.value, "This is thousand", 16, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_UnionInt1(union_int_payload3,
		sizeof(union_int_payload3), &result1, &num_decode), NULL);
	zassert_equal(sizeof(union_int_payload3), num_decode, NULL);
	zassert_equal(result1.union_choice, UnionInt1_union_uint3_l_c, NULL);
	zassert_equal(result1.number_m.number_choice, number_int_c, NULL);
	zassert_equal(result1.number_m.Int, 1, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_UnionInt2(union_int_payload3,
		sizeof(union_int_payload3), &result2, &num_decode), NULL);
	zassert_equal(sizeof(union_int_payload3), num_decode, NULL);
	zassert_equal(result2.union_choice, union_nint_l_c, NULL);
	zassert_equal(result2.number_m.number_choice, number_int_c, NULL);
	zassert_equal(result2.number_m.Int, 1, NULL);

	int err = cbor_decode_UnionInt2(union_int_payload6,
		sizeof(union_int_payload6), &result2, &num_decode);
	zassert_equal(ZCBOR_SUCCESS, err, "%d\r\n", err);
	zassert_equal(sizeof(union_int_payload6), num_decode, NULL);
	zassert_equal(result2.union_choice, UnionInt2_union_Structure_One_m_c, NULL);
	zassert_equal(result2.Structure_One_m.some_array.len, 2, NULL);
	zassert_mem_equal(result2.Structure_One_m.some_array.value, "hi", 2, NULL);

	err = cbor_decode_UnionInt1(union_int_payload4_inv,
		sizeof(union_int_payload4_inv), &result1, &num_decode);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, err, "%d\r\n", err);

	err = cbor_decode_UnionInt2(union_int_payload4_inv,
		sizeof(union_int_payload4_inv), &result2, &num_decode);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, err, "%d\r\n", err);

	err = cbor_decode_UnionInt1(union_int_payload5_inv,
		sizeof(union_int_payload5_inv), &result1, &num_decode);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, err, "%d\r\n", err);

	err = cbor_decode_UnionInt2(union_int_payload5_inv,
		sizeof(union_int_payload5_inv), &result2, &num_decode);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, err, "%d\r\n", err);
}


ZTEST(cbor_decode_test5, test_intmax)
{
	uint8_t intmax1_payload1[] = {LIST(C),
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
	uint8_t intmax2_payload1[] = {LIST(8),
		0x38, 0x7F, 0x0,
		0x39, 0x7F, 0xFF,
		0x0,
		0x3A, 0x7F, 0xFF, 0xFF, 0xFF,
		0x0,
		0x3B, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x0,
		END
	};
	uint8_t intmax2_payload2[] = {LIST(8),
		0x18, 0x7F, 0x18, 0xFF,
		0x19, 0x7F, 0xFF,
		0x19, 0xFF, 0xFF,
		0x1A, 0x7F, 0xFF, 0xFF, 0xFF,
		0x1A, 0xFF, 0xFF, 0xFF, 0xFF,
		0x1B, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x1B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		END
	};
	uint8_t intmax3_payload1[] = {LIST(3),
		0x18, 254,
		0x19, 0xFF, 0xFE,
		MAP(1),
			0x38, 127,
			0x58, 127,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		END
		END
	};


	struct Intmax2 result2;
	struct Intmax3 result3;
	size_t num_decode;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Intmax1(intmax1_payload1,
		sizeof(intmax1_payload1), NULL, &num_decode), NULL);
	zassert_equal(sizeof(intmax1_payload1), num_decode, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Intmax2(intmax2_payload1,
		sizeof(intmax2_payload1), &result2, &num_decode), NULL);
	zassert_equal(sizeof(intmax2_payload1), num_decode, NULL);
	zassert_equal(result2.INT_8, INT8_MIN, NULL);
	zassert_equal(result2.UINT_8, 0, NULL);
	zassert_equal(result2.INT_16, INT16_MIN, NULL);
	zassert_equal(result2.UINT_16, 0, NULL);
	zassert_equal(result2.INT_32, INT32_MIN, NULL);
	zassert_equal(result2.UINT_32, 0, NULL);
	zassert_equal(result2.INT_64, INT64_MIN, NULL);
	zassert_equal(result2.UINT_64, 0, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Intmax2(intmax2_payload2,
		sizeof(intmax2_payload2), &result2, &num_decode), NULL);
	zassert_equal(sizeof(intmax2_payload2), num_decode, NULL);
	zassert_equal(result2.INT_8, INT8_MAX, NULL);
	zassert_equal(result2.UINT_8, UINT8_MAX, NULL);
	zassert_equal(result2.INT_16, INT16_MAX, NULL);
	zassert_equal(result2.UINT_16, UINT16_MAX, NULL);
	zassert_equal(result2.INT_32, INT32_MAX, NULL);
	zassert_equal(result2.UINT_32, UINT32_MAX, NULL);
	zassert_equal(result2.INT_64, INT64_MAX, NULL);
	zassert_equal(result2.UINT_64, UINT64_MAX, NULL);

	int res = cbor_decode_Intmax3(intmax3_payload1,
		sizeof(intmax3_payload1), &result3, &num_decode);
	zassert_equal(ZCBOR_SUCCESS, res, "%s\n", zcbor_error_str(res));
	zassert_equal(sizeof(intmax3_payload1), num_decode, NULL);
	zassert_equal(result3.union_choice, Intmax3_union_uint254_c, NULL);
	zassert_equal(result3.Int, 65534, NULL);
	zassert_equal(result3.nintbstr.len, 127, NULL);
}


/* Test that zcbor generates variable names that don't contain unsupported characters. */
ZTEST(cbor_decode_test5, test_invalid_identifiers)
{
	struct InvalidIdentifiers result;
	uint8_t invalid_identifiers_payload1[] = {
		LIST(3),
		0x64, '1', 'o', 'n', 'e',
		0x02, /* Ø */
		0x67, '{', '[', 'a', '-', 'z', ']', '}',
		END
	};

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_InvalidIdentifiers(invalid_identifiers_payload1,
		sizeof(invalid_identifiers_payload1), &result, NULL), NULL);
	zassert_true(result._1one_tstr_present, NULL);
	zassert_true(result.__present, NULL);
	zassert_true(result.a_z_tstr_present, NULL);
}


ZTEST(cbor_decode_test5, test_uint64_list)
{
	uint8_t uint64_list_payload1[] = {LIST(8),
		0x10,
		0x18, 0x20,
		0x19, 0x12, 0x34,
		0x1a, 0x12, 0x34, 0x56, 0x78,
		0x1b, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
		0x3b, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
		0x1b, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
		0x3b, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xee,
		END
	};

	struct Uint64List result;
	size_t num_decode;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_Uint64List(uint64_list_payload1,
		sizeof(uint64_list_payload1), &result, &num_decode), NULL);
	zassert_equal(sizeof(uint64_list_payload1), num_decode, NULL);

	zassert_equal(5, result.uint64_count, NULL);
	zassert_equal(0x10, result.uint64[0], NULL);
	zassert_equal(0x20, result.uint64[1], NULL);
	zassert_equal(0x1234, result.uint64[2], NULL);
	zassert_equal(0x12345678, result.uint64[3], NULL);
	zassert_equal(0x123456789abcdef0, result.uint64[4], NULL);
}


ZTEST(cbor_decode_test5, test_bstr_size)
{
	uint8_t bstr_size_payload1[] = {MAP(1),
		0x61, 's',
		0x4c, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
		0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,
		END
	};

	struct BstrSize result;
	size_t num_decode;

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_BstrSize(bstr_size_payload1,
		sizeof(bstr_size_payload1), &result, &num_decode), NULL);
	zassert_equal(sizeof(bstr_size_payload1), num_decode, NULL);

	zassert_equal(12, result.bstr12_m.s.len, NULL);
}


ZTEST_SUITE(cbor_decode_test5, NULL, NULL, NULL, NULL, NULL);
