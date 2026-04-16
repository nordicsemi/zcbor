/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zcbor_common.h>
#include <zcbor_encode.h>
#include <zcbor_print.h>
#include "unordered_map_decode.h"
#include <common_test.h>

ZTEST(cbor_decode_testA, test_unordered_map1)
{
	const uint8_t payload_unordered_map1_1[] = {
		MAP(7),
		3, 0x40,
		0x41, 0, LIST(3), 1, 2, 3, END
		2, 0x63, 't', 'w', 'o',
		0x63, 'b', 'a', 'z', 0x63, 'b', 'o', 'z',
		0x41, 2, LIST(0), END
		1, 0x63, 'o', 'n', 'e',
		0x63, 'f', 'o', 'o', 0x63, 'b', 'a', 'r',
		END
	};

	const uint8_t payload_unordered_map1_2[] = {
		MAP(5),
		2, 0x63, 't', 'w', 'o',
		3, 0x40,
		1, 0x63, 'o', 'n', 'e',
		0x63, 'b', 'a', 'z', 0x63, 'b', 'o', 'z',
		0x63, 'f', 'o', 'o', 0x63, 'b', 'a', 'r',
		END
	};

	const uint8_t payload_unordered_map1_inv3[] = {
		MAP(5),
		2, 0x63, 't', 'w', 'o',
		3, 0x40,
		1, 0x63, 'o', 'n', 'f' /* here */,
		0x63, 'b', 'a', 'z', 0x63, 'b', 'o', 'z',
		0x63, 'f', 'o', 'o', 0x63, 'b', 'a', 'r',
		END
	};

	const uint8_t payload_unordered_map1_inv4[] = {
		MAP(6),
		3, 0x40,
		0x63, 'b', 'a', 'z', 0x63, 'b', 'o', 'z',
		0x41, 0, LIST(3), 1, 2, 0x40 /* here */, END
		0x63, 'f', 'o', 'o', 0x63, 'b', 'a', 'r',
		1, 0x63, 'o', 'n', 'e',
		2, 0x63, 't', 'w', 'o',
		END
	};

	struct UnorderedMap1 unordered_map1;

	int err = cbor_decode_UnorderedMap1(payload_unordered_map1_1, sizeof(payload_unordered_map1_1), &unordered_map1, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(3, unordered_map1.UnorderedMap1_intbstr_key);
	zassert_equal(0, unordered_map1.UnorderedMap1_intbstr.len);
	zassert_equal(2, unordered_map1.UnorderedMap1_intlist_m_count);
	zassert_equal(1, unordered_map1.UnorderedMap1_intlist_m[0].UnorderedMap1_intlist_m_key.len);
	zassert_equal(2, unordered_map1.UnorderedMap1_intlist_m[0].UnorderedMap1_intlist_m_key.value[0]);
	zassert_equal(0, unordered_map1.UnorderedMap1_intlist_m[0].UnorderedMap1_intlist_m.intlist_int_count);
	zassert_equal(1, unordered_map1.UnorderedMap1_intlist_m[1].UnorderedMap1_intlist_m_key.len);
	zassert_equal(0, unordered_map1.UnorderedMap1_intlist_m[1].UnorderedMap1_intlist_m_key.value[0]);
	zassert_equal(3, unordered_map1.UnorderedMap1_intlist_m[1].UnorderedMap1_intlist_m.intlist_int_count);
	zassert_equal(1, unordered_map1.UnorderedMap1_intlist_m[1].UnorderedMap1_intlist_m.intlist_int[0]);
	zassert_equal(2, unordered_map1.UnorderedMap1_intlist_m[1].UnorderedMap1_intlist_m.intlist_int[1]);
	zassert_equal(3, unordered_map1.UnorderedMap1_intlist_m[1].UnorderedMap1_intlist_m.intlist_int[2]);

	err = cbor_decode_UnorderedMap1(payload_unordered_map1_2, sizeof(payload_unordered_map1_2), &unordered_map1, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(3, unordered_map1.UnorderedMap1_intbstr_key);
	zassert_equal(0, unordered_map1.UnorderedMap1_intbstr.len);
	zassert_equal(0, unordered_map1.UnorderedMap1_intlist_m_count);

	err = cbor_decode_UnorderedMap1(payload_unordered_map1_inv3, sizeof(payload_unordered_map1_inv3), &unordered_map1, NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, err, "%s\n", zcbor_error_str(err));

	err = cbor_decode_UnorderedMap1(payload_unordered_map1_inv4, sizeof(payload_unordered_map1_inv4), &unordered_map1, NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, err, "%s\n", zcbor_error_str(err));
}

ZTEST(cbor_decode_testA, test_unordered_map2)
{
	const uint8_t payload_unordered_map2_1[] = {
		MAP(4),
		1, 0x61, 'a',
		0x63, 'm', 'a', 'p', MAP(3),
			0x20, 0x40,
			0x21, 0x40,
			0xF4, 0x18, 100,
			END
		0x21, 0xF6,
		2, 0x61, 'b',
		END
	};

	const uint8_t payload_unordered_map2_2[] = {
		MAP(2),
		0x63, 'm', 'a', 'p', MAP(1),
			0xF5, 0x39, 0x12, 0x34,
			END
		0x22, 0xF6,
		END
	};

	const uint8_t payload_unordered_map2_3[] = {
		MAP(3),
		2, 0x66, 'f', 'o', 'o', 'b', 'a', 'r',
		0x63, 'm', 'a', 'p', MAP(2),
			0x21, 0x40,
			0x20, 0x40,
			END
		1, 0x60,
		END
	};

	const uint8_t payload_unordered_map2_inv4[] = {
		MAP(4),
		1, 0x61, 'a',
		0x63, 'm', 'a', 'p', MAP(3),
			0x20, 0x40,
			0x21, 0x40,
			0xF4, 0xF6 /* here */,
			END
		0x21, 0xF6,
		2, 0x61, 'b',
		END
	};

	const uint8_t payload_unordered_map2_inv5[] = {
		MAP(4),
		1, 0x61, 'a',
		0x62, 'm', 'a', /* here */ MAP(3),
			0x20, 0x40,
			0x21, 0x40,
			0xF4, 0x18, 100,
			END
		0x21, 0xF6,
		2, 0x61, 'b',
		END
	};

	struct UnorderedMap2 unordered_map2;

	int err = cbor_decode_UnorderedMap2(payload_unordered_map2_1, sizeof(payload_unordered_map2_1), &unordered_map2, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(3, unordered_map2.UnorderedMap2_typeUnion_m_count);
	zassert_equal(typeUnion_type1_m_c, unordered_map2.UnorderedMap2_typeUnion_m[0].typeUnion_choice);
	zassert_equal(typeUnion_type2_m_c, unordered_map2.UnorderedMap2_typeUnion_m[1].typeUnion_choice);
	zassert_equal(typeUnion_typeDefault_m_c, unordered_map2.UnorderedMap2_typeUnion_m[2].typeUnion_choice);
	zassert_equal(1, unordered_map2.UnorderedMap2_typeUnion_m[0].typeUnion_type1_m.type1.len);
	zassert_equal('a', unordered_map2.UnorderedMap2_typeUnion_m[0].typeUnion_type1_m.type1.value[0]);
	zassert_equal(1, unordered_map2.UnorderedMap2_typeUnion_m[1].typeUnion_type2_m.type2.len);
	zassert_equal('b', unordered_map2.UnorderedMap2_typeUnion_m[1].typeUnion_type2_m.type2.value[0]);
	zassert_equal(-2, unordered_map2.UnorderedMap2_typeUnion_m[2].typeUnion_typeDefault_m.typeDefault_key);
	zassert_true(unordered_map2.map_nint1bstr_present);
	zassert_true(unordered_map2.map_nint2bstr_present);
	zassert_true(unordered_map2.map_boolint_present);
	zassert_equal(0, unordered_map2.map_nint1bstr.map_nint1bstr.len);
	zassert_equal(0, unordered_map2.map_nint2bstr.map_nint2bstr.len);
	zassert_false(unordered_map2.map_boolint.UnorderedMap2_map_boolint_key);
	zassert_equal(100, unordered_map2.map_boolint.map_boolint);

	err = cbor_decode_UnorderedMap2(payload_unordered_map2_2, sizeof(payload_unordered_map2_2), &unordered_map2, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(1, unordered_map2.UnorderedMap2_typeUnion_m_count);
	zassert_equal(typeUnion_typeDefault_m_c, unordered_map2.UnorderedMap2_typeUnion_m[0].typeUnion_choice);
	zassert_equal(-3, unordered_map2.UnorderedMap2_typeUnion_m[0].typeUnion_typeDefault_m.typeDefault_key);
	zassert_true(unordered_map2.map_boolint_present);
	zassert_true(unordered_map2.map_boolint.UnorderedMap2_map_boolint_key);
	zassert_equal(-0x1235, unordered_map2.map_boolint.map_boolint);

	err = cbor_decode_UnorderedMap2(payload_unordered_map2_3, sizeof(payload_unordered_map2_3), &unordered_map2, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(2, unordered_map2.UnorderedMap2_typeUnion_m_count);
	zassert_equal(typeUnion_type1_m_c, unordered_map2.UnorderedMap2_typeUnion_m[0].typeUnion_choice);
	zassert_equal(typeUnion_type2_m_c, unordered_map2.UnorderedMap2_typeUnion_m[1].typeUnion_choice);
	zassert_equal(0, unordered_map2.UnorderedMap2_typeUnion_m[0].typeUnion_type1_m.type1.len);
	zassert_equal(6, unordered_map2.UnorderedMap2_typeUnion_m[1].typeUnion_type2_m.type2.len);
	zassert_mem_equal("foobar", unordered_map2.UnorderedMap2_typeUnion_m[1].typeUnion_type2_m.type2.value, 6);
	zassert_true(unordered_map2.map_nint1bstr_present);
	zassert_true(unordered_map2.map_nint2bstr_present);
	zassert_equal(0, unordered_map2.map_nint1bstr.map_nint1bstr.len);
	zassert_equal(0, unordered_map2.map_nint2bstr.map_nint2bstr.len);

	err = cbor_decode_UnorderedMap2(payload_unordered_map2_inv4, sizeof(payload_unordered_map2_inv4), &unordered_map2, NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, err, "%s %d\n", zcbor_error_str(err), err);

	err = cbor_decode_UnorderedMap2(payload_unordered_map2_inv5, sizeof(payload_unordered_map2_inv5), &unordered_map2, NULL);
	zassert_equal(ZCBOR_ERR_ELEM_NOT_FOUND, err, "%s %d\n", zcbor_error_str(err), err);
}


ZTEST(cbor_decode_testA, test_unordered_map3)
{
	uint8_t payload_unordered_map3_1[21000];

	ZCBOR_STATE_E(state_e, 1, payload_unordered_map3_1, sizeof(payload_unordered_map3_1), 0);

	zassert_true(zcbor_map_start_encode(state_e, 2000));
	for (uint32_t i = 1; i <= 1000; i++) {
		zassert_true(zcbor_uint32_put(state_e, i), "%d\n", i);
		zassert_true(zcbor_uint32_put(state_e, i), "%d\n", i);
		zassert_true(zcbor_int32_put(state_e, -i), "%d\n", -i);
		zassert_true(zcbor_int32_put(state_e, -i), "%d\n", -i);
	}
	zassert_true(zcbor_map_end_encode(state_e, 2000));

	struct UnorderedMap3 unordered_map3;

	int err = cbor_decode_UnorderedMap3(payload_unordered_map3_1, sizeof(payload_unordered_map3_1), &unordered_map3, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);

	zassert_equal(1000, unordered_map3.UnorderedMap3_uintuint_count, "%d != %d", 1000, unordered_map3.UnorderedMap3_uintuint_count);
	zassert_equal(1000, unordered_map3.UnorderedMap3_nintnint_count, "%d != %d", 1000, unordered_map3.UnorderedMap3_nintnint_count);
}


ZTEST(cbor_decode_testA, test_unordered_map4)
{
	uint8_t payload_unordered_map4_1[100];
	struct UnorderedMap4 unordered_map4;
	int err;

	/* Valid case */
	ZCBOR_STATE_E(state_e, 1, payload_unordered_map4_1, sizeof(payload_unordered_map4_1), 0);

	zassert_true(zcbor_map_start_encode(state_e, 10));
	for (int32_t i = -1; i >= -6; i--) {
		zassert_true(zcbor_int32_put(state_e, i), "%d\n", i);
		zassert_true(zcbor_bstr_put_lit(state_e, "world"), NULL);
	}
	for (uint32_t i = 1; i <= 6; i++) {
		zassert_true(zcbor_uint32_put(state_e, i), "%d\n", i);
		zassert_true(zcbor_tstr_put_lit(state_e, "hello"), NULL);
	}
	zassert_true(zcbor_map_end_encode(state_e, 6));

	err = cbor_decode_UnorderedMap4(payload_unordered_map4_1, sizeof(payload_unordered_map4_1), &unordered_map4, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);

	zassert_equal(2, unordered_map4.UnorderedMap4_group1_m_count, "%d != %d", 2, unordered_map4.UnorderedMap4_group1_m_count);
	zassert_equal(3, unordered_map4.UnorderedMap4_group1_m[0].UnorderedMap4_group1_m.group1_count, "%d != %d", 3, unordered_map4.UnorderedMap4_group1_m[0].UnorderedMap4_group1_m.group1_count);
	zassert_false(unordered_map4.UnorderedMap4_intunion_present);
	for (int32_t i = 0; i < 2; i++) {
		for (int32_t j = 1; j <= 3; j++) {
			zassert_equal(j + (i * 3), unordered_map4.UnorderedMap4_group1_m[i].UnorderedMap4_group1_m.group1[j - 1].group1_uinttstr_key);
			zassert_equal(-j - (i * 3), unordered_map4.UnorderedMap4_group1_m[i].UnorderedMap4_group1_m.group1[j - 1].group1_nintbstr_key);
		}
	}

	/* Valid case (tests map elem state backup) */
	ZCBOR_STATE_E(state_e2, 1, payload_unordered_map4_1, sizeof(payload_unordered_map4_1), 0);

	zassert_true(zcbor_map_start_encode(state_e2, 10));
	for (int32_t i = -1; i >= -3; i--) {
		zassert_true(zcbor_int32_put(state_e2, i), "%d\n", i);
		zassert_true(zcbor_bstr_put_lit(state_e2, "world"), NULL);
	}

	/* Put an extra int that will first be attempted to fit into a third iteration of
	   group1, then the backup will clear it so it can be consumed as part of the next
	   line instead. */
	for (uint32_t i = 1; i <= 4; i++) {
		zassert_true(zcbor_uint32_put(state_e2, i), "%d\n", i);
		zassert_true(zcbor_tstr_put_lit(state_e2, "hello"), NULL);
	}
	zassert_true(zcbor_map_end_encode(state_e2, 6));

	err = cbor_decode_UnorderedMap4(payload_unordered_map4_1, sizeof(payload_unordered_map4_1), &unordered_map4, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);

	zassert_equal(1, unordered_map4.UnorderedMap4_group1_m_count, "%d != %d", 1, unordered_map4.UnorderedMap4_group1_m_count);
	zassert_equal(3, unordered_map4.UnorderedMap4_group1_m[0].UnorderedMap4_group1_m.group1_count, "%d != %d", 3, unordered_map4.UnorderedMap4_group1_m[0].UnorderedMap4_group1_m.group1_count);
	zassert_true(unordered_map4.UnorderedMap4_intunion_present);
	for (int32_t i = 0; i < 1; i++) {
		for (int32_t j = 1; j <= 3; j++) {
			zassert_equal(j + (i * 3), unordered_map4.UnorderedMap4_group1_m[i].UnorderedMap4_group1_m.group1[j - 1].group1_uinttstr_key);
			zassert_equal(-j - (i * 3), unordered_map4.UnorderedMap4_group1_m[i].UnorderedMap4_group1_m.group1[j - 1].group1_nintbstr_key);
		}
	}
	zassert_equal(4, unordered_map4.UnorderedMap4_intunion.UnorderedMap4_intunion_key);

	/* Invalid case */
	ZCBOR_STATE_E(state_e_inv1, 1, payload_unordered_map4_1, sizeof(payload_unordered_map4_1), 0);

	zassert_true(zcbor_map_start_encode(state_e_inv1, 10));
	for (int32_t i = -1; i >= -5; i--) { /* <- Here (Not a multiple of 3 )*/
		zassert_true(zcbor_int32_put(state_e_inv1, i), "%d\n", i);
		zassert_true(zcbor_bstr_put_lit(state_e_inv1, "world"), NULL);
	}
	for (uint32_t i = 1; i <= 6; i++) {
		zassert_true(zcbor_uint32_put(state_e_inv1, i), "%d\n", i);
		zassert_true(zcbor_tstr_put_lit(state_e_inv1, "hello"), NULL);
	}
	zassert_true(zcbor_map_end_encode(state_e_inv1, 6));

	err = cbor_decode_UnorderedMap4(payload_unordered_map4_1, sizeof(payload_unordered_map4_1), &unordered_map4, NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, err, "%s %d\n", zcbor_error_str(err), err);


	/* Invalid case */
	ZCBOR_STATE_E(state_e_inv2, 1, payload_unordered_map4_1, sizeof(payload_unordered_map4_1), 0);

	zassert_true(zcbor_map_start_encode(state_e_inv2, 10));
	for (int32_t i = -1; i >= -3; i--) {
		zassert_true(zcbor_int32_put(state_e_inv2, i), "%d\n", i);
		zassert_true(zcbor_bstr_put_lit(state_e_inv2, "world"), NULL);
	}
	for (uint32_t i = 1; i <= 2; i++) { /* <- Here (Not a multiple of 3 )*/
		zassert_true(zcbor_uint32_put(state_e_inv2, i), "%d\n", i);
		zassert_true(zcbor_tstr_put_lit(state_e_inv2, "hello"), NULL);
	}
	zassert_true(zcbor_map_end_encode(state_e_inv2, 6));

	err = cbor_decode_UnorderedMap4(payload_unordered_map4_1, sizeof(payload_unordered_map4_1), &unordered_map4, NULL);
	zassert_equal(ZCBOR_ERR_ITERATIONS, err, "%s %d\n", zcbor_error_str(err), err);
}


ZTEST(cbor_decode_testA, test_unordered_map5)
{
	uint8_t payload_unordered_map5_1[] = {
		MAP(2),
			0x01, 0xF5,
			0x02, MAP(6),
				0x10, 0x40,
				0x11, 0x41, 0x01,
				0x12, 0x42, 0x01, 0x02,
				0x22, MAP(2),
					0x63, 'm', 'a', 'p', MAP(1),
						0xF5, 0x39, 0x12, 0x34,
						END
					0x22, 0xF6,
				END
				0x21, 0x12,
				0x20, 0x02,
			END
		END

	};

	struct UnorderedMap5 unordered_map5;

	int err = cbor_decode_UnorderedMap5(payload_unordered_map5_1, sizeof(payload_unordered_map5_1), &unordered_map5, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
}


ZTEST(cbor_decode_testA, test_unordered_map6)
{
	uint8_t payload_unordered_map6_1[] = {
		MAP(2),
			0x01, 0x10,
			0x03, 0x61, 'a',
		END
	};
	uint8_t payload_unordered_map6_2[] = {
		MAP(2),
			0x04, 0x61, 'b',
			0x01, 0x11,
		END
	};
	uint8_t payload_unordered_map6_3[] = {
		MAP(2),
			0x02, 0x61, 'c',
			0x01, 0x12,
		END
	};

	struct UnorderedMap6 unordered_map6;
	int err;

	err = cbor_decode_UnorderedMap6(payload_unordered_map6_1, sizeof(payload_unordered_map6_1), &unordered_map6, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(union_typechoice3_l_c, unordered_map6.UnorderedMap6_union_choice);
	zassert_equal(0x10, unordered_map6.typechoice3_l_typechoice3);
	zassert_equal(1, unordered_map6.typechoice3_l_uint3tstr.len);
	zassert_equal('a', unordered_map6.typechoice3_l_uint3tstr.value[0]);

	err = cbor_decode_UnorderedMap6(payload_unordered_map6_2, sizeof(payload_unordered_map6_2), &unordered_map6, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(union_typechoice4_l_c, unordered_map6.UnorderedMap6_union_choice);
	zassert_equal(0x11, unordered_map6.typechoice4_l_typechoice4);
	zassert_equal(1, unordered_map6.typechoice4_l_uint4tstr.len);
	zassert_equal('b', unordered_map6.typechoice4_l_uint4tstr.value[0]);

	err = cbor_decode_UnorderedMap6(payload_unordered_map6_3, sizeof(payload_unordered_map6_3), &unordered_map6, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(union_typechoice2_l_c, unordered_map6.UnorderedMap6_union_choice);
	zassert_equal(0x12, unordered_map6.typechoice2_l_typechoice2);
	zassert_equal(1, unordered_map6.typechoice2_l_uint2tstr.len);
	zassert_equal('c', unordered_map6.typechoice2_l_uint2tstr.value[0]);
}


// static void zassert_zcbor_string_equal_lit(const char *expected, const struct zcbor_string *str)
#define zassert_zcbor_string_equal_lit(expected, str) \
do { \
	zassert_equal(strlen(expected), (str)->len, "%d != %d\n", strlen(expected), (str)->len); \
	zassert_mem_equal(expected, (str)->value, (str)->len, "%s != %s\n", expected, (str)->value); \
} while(0)


ZTEST(cbor_decode_testA, test_unordered_map7)
{
	uint8_t payload_unordered_map7_1[500];

	ZCBOR_STATE_E(state_e, 1, payload_unordered_map7_1, sizeof(payload_unordered_map7_1), 0);
	zassert_true(zcbor_map_start_encode(state_e, 300));
	for (int32_t i = 1; i <= 100; i++) {
		zassert_true(zcbor_uint32_put(state_e, i), "%d\n", i);
		zassert_true(zcbor_int32_put(state_e, -i), NULL);
	}
	zassert_true(zcbor_int32_put(state_e, -1), NULL);
	zassert_true(zcbor_tstr_put_lit(state_e, "hello"), NULL);
	zassert_true(zcbor_int32_put(state_e, -2), NULL);
	zassert_true(zcbor_bstr_put_lit(state_e, "world"), NULL);
	zassert_true(zcbor_int32_put(state_e, -10), NULL);
	zassert_true(zcbor_bool_put(state_e, true), NULL);
	zassert_true(zcbor_int32_put(state_e, -11), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_int32_put(state_e, -12), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_int32_put(state_e, -13), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_int32_put(state_e, -14), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_int32_put(state_e, -15), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_int32_put(state_e, -16), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_int32_put(state_e, -17), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 300));

	struct UnorderedMap7 unordered_map7;
	int err;

	err = cbor_decode_UnorderedMap7(payload_unordered_map7_1, sizeof(payload_unordered_map7_1), &unordered_map7, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(100, unordered_map7.UnorderedMap7_uintint_count, "%d != %d", 100, unordered_map7.UnorderedMap7_uintint_count);
	for (int32_t i = 1; i <= 100; i++) {
		zassert_equal(i, unordered_map7.UnorderedMap7_uintint[i - 1].UnorderedMap7_uintint_key);
		zassert_equal(-i, unordered_map7.UnorderedMap7_uintint[i - 1].UnorderedMap7_uintint);
	}
	zassert_equal(5, unordered_map7.UnorderedMap7_union_count, "%d != %d", 5, unordered_map7.UnorderedMap7_union_count);

	zassert_equal(union_nint1tstr_c, unordered_map7.UnorderedMap7_union[0].UnorderedMap7_union_choice);
	zassert_zcbor_string_equal_lit("hello", &unordered_map7.UnorderedMap7_union[0].union_nint1tstr);

	zassert_equal(union_nint2bstr_c, unordered_map7.UnorderedMap7_union[1].UnorderedMap7_union_choice);
	zassert_zcbor_string_equal_lit("world", &unordered_map7.UnorderedMap7_union[1].union_nint2bstr);

	zassert_equal(union_bool_or_nil_m_c, unordered_map7.UnorderedMap7_union[2].UnorderedMap7_union_choice);
	zassert_equal(3, unordered_map7.UnorderedMap7_union[2].union_bool_or_nil_m_count);
	zassert_equal(-10, unordered_map7.UnorderedMap7_union[2].union_bool_or_nil_m[0].UnorderedMap7_union_bool_or_nil_m_key);
	zassert_equal(bool_or_nil_bool_c, unordered_map7.UnorderedMap7_union[2].union_bool_or_nil_m[0].union_bool_or_nil_m.bool_or_nil_choice);
	zassert_equal(true, unordered_map7.UnorderedMap7_union[2].union_bool_or_nil_m[0].union_bool_or_nil_m.bool_or_nil_bool);
	zassert_equal(-11, unordered_map7.UnorderedMap7_union[2].union_bool_or_nil_m[1].UnorderedMap7_union_bool_or_nil_m_key);
	zassert_equal(bool_or_nil_nil_c, unordered_map7.UnorderedMap7_union[2].union_bool_or_nil_m[1].union_bool_or_nil_m.bool_or_nil_choice);
	zassert_equal(-12, unordered_map7.UnorderedMap7_union[2].union_bool_or_nil_m[2].UnorderedMap7_union_bool_or_nil_m_key);
	zassert_equal(bool_or_nil_nil_c, unordered_map7.UnorderedMap7_union[2].union_bool_or_nil_m[2].union_bool_or_nil_m.bool_or_nil_choice);

	zassert_equal(union_bool_or_nil_m_c, unordered_map7.UnorderedMap7_union[3].UnorderedMap7_union_choice);
	zassert_equal(3, unordered_map7.UnorderedMap7_union[3].union_bool_or_nil_m_count);
	zassert_equal(-13, unordered_map7.UnorderedMap7_union[3].union_bool_or_nil_m[0].UnorderedMap7_union_bool_or_nil_m_key);
	zassert_equal(bool_or_nil_nil_c, unordered_map7.UnorderedMap7_union[3].union_bool_or_nil_m[0].union_bool_or_nil_m.bool_or_nil_choice);
	zassert_equal(-14, unordered_map7.UnorderedMap7_union[3].union_bool_or_nil_m[1].UnorderedMap7_union_bool_or_nil_m_key);
	zassert_equal(bool_or_nil_nil_c, unordered_map7.UnorderedMap7_union[3].union_bool_or_nil_m[1].union_bool_or_nil_m.bool_or_nil_choice);
	zassert_equal(-15, unordered_map7.UnorderedMap7_union[3].union_bool_or_nil_m[2].UnorderedMap7_union_bool_or_nil_m_key);
	zassert_equal(bool_or_nil_nil_c, unordered_map7.UnorderedMap7_union[3].union_bool_or_nil_m[2].union_bool_or_nil_m.bool_or_nil_choice);

	zassert_equal(union_bool_or_nil_m_c, unordered_map7.UnorderedMap7_union[4].UnorderedMap7_union_choice);
	zassert_equal(2, unordered_map7.UnorderedMap7_union[4].union_bool_or_nil_m_count);
	zassert_equal(-16, unordered_map7.UnorderedMap7_union[4].union_bool_or_nil_m[0].UnorderedMap7_union_bool_or_nil_m_key);
	zassert_equal(bool_or_nil_nil_c, unordered_map7.UnorderedMap7_union[4].union_bool_or_nil_m[0].union_bool_or_nil_m.bool_or_nil_choice);
	zassert_equal(-17, unordered_map7.UnorderedMap7_union[4].union_bool_or_nil_m[1].UnorderedMap7_union_bool_or_nil_m_key);
	zassert_equal(bool_or_nil_nil_c, unordered_map7.UnorderedMap7_union[4].union_bool_or_nil_m[1].union_bool_or_nil_m.bool_or_nil_choice);
}


ZTEST(cbor_decode_testA, test_unordered_map8)
{
	uint8_t payload_unordered_map8_1[] = {MAP(2), 0x04, 0x23, 0x01, 0x20, END};
	uint8_t payload_unordered_map8_2[] = {MAP(3), 0x05, 0x24, 0x09, 0x28, 0x03, 0x22, END};
	uint8_t payload_unordered_map8_3[] = {MAP(3), 0x08, 0x27, 0x02, 0x21, 0x06, 0x25, END};

	struct UnorderedMap8 unordered_map8;

	int err = cbor_decode_UnorderedMap8(payload_unordered_map8_1, sizeof(payload_unordered_map8_1), &unordered_map8, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(union2_uint4int_c, unordered_map8.UnorderedMap8_union2_choice);
	zassert_equal(-4, unordered_map8.union2_uint4int);
	zassert_equal(union1_uint1int_c, unordered_map8.UnorderedMap8_union1_choice);
	zassert_equal(-1, unordered_map8.union1_uint1int);
	zassert_equal(false, unordered_map8.UnorderedMap8_union3_present);
	zassert_equal(UnorderedMap8_union3_uint42_c, unordered_map8.UnorderedMap8_union3.UnorderedMap8_union3_choice); // Default value reported

	err = cbor_decode_UnorderedMap8(payload_unordered_map8_2, sizeof(payload_unordered_map8_2), &unordered_map8, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(union2_uint5int_c, unordered_map8.UnorderedMap8_union2_choice);
	zassert_equal(-5, unordered_map8.union2_uint4int);
	zassert_equal(union1_uint3int_c, unordered_map8.UnorderedMap8_union1_choice);
	zassert_equal(-3, unordered_map8.union1_uint1int);
	zassert_equal(true, unordered_map8.UnorderedMap8_union3_present);
	zassert_equal(union3_uint9int_c, unordered_map8.UnorderedMap8_union3.UnorderedMap8_union3_choice);
	zassert_equal(-9, unordered_map8.UnorderedMap8_union3.union3_uint9int);

	err = cbor_decode_UnorderedMap8(payload_unordered_map8_3, sizeof(payload_unordered_map8_3), &unordered_map8, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_equal(union1_uint2int_c, unordered_map8.UnorderedMap8_union1_choice);
	zassert_equal(-2, unordered_map8.union1_uint1int);
	zassert_equal(union2_uint6int_c, unordered_map8.UnorderedMap8_union2_choice);
	zassert_equal(-6, unordered_map8.union2_uint4int);
	zassert_equal(true, unordered_map8.UnorderedMap8_union3_present);
	zassert_equal(union3_uint8int_c, unordered_map8.UnorderedMap8_union3.UnorderedMap8_union3_choice);
	zassert_equal(-8, unordered_map8.UnorderedMap8_union3.union3_uint8int);
}


ZTEST_SUITE(cbor_decode_testA, NULL, NULL, NULL, NULL, NULL);
