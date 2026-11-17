/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zcbor_common.h>
#include <zcbor_print.h>
#include "unordered_map_counter_decode.h"
#include <common_test.h>

#ifdef ZCBOR_MAP_SMART_SEARCH
#error "This test is for unordered maps without ZCBOR_MAP_SMART_SEARCH."
#endif

/* The failed map15_bad finds key 1 before failing on key 3. The count of processed elements must
 * be restored, or it no longer matches the number of elements in the map. */
ZTEST(cbor_decode_testB, test_unordered_map16)
{
	const uint8_t payload_unordered_map16[] = {
		MAP(2),
		0x01, 0x05,
		0x04, 0xF5,
		END
	};
	const uint8_t payload_unordered_map16_inv1[] = {
		MAP(3),
		0x01, 0x05,
		0x03, 0x61, 'a',
		0x04, 0xF5,
		END
	};
	/* Key 9 is not in the CDDL. */
	const uint8_t payload_unordered_map16_inv2[] = {
		MAP(3),
		0x01, 0x05,
		0x04, 0xF5,
		0x09, 0x00,
		END
	};

	struct UnorderedMap16 unordered_map16;

	int err = cbor_decode_UnorderedMap16(payload_unordered_map16,
			sizeof(payload_unordered_map16), &unordered_map16, NULL);
	zassert_equal(ZCBOR_SUCCESS, err, "%s %d\n", zcbor_error_str(err), err);
	zassert_false(unordered_map16.UnorderedMap16_map15_bad_m_present);
	zassert_equal(5, unordered_map16.UnorderedMap16_uint1uint);
	zassert_true(unordered_map16.UnorderedMap16_uint4bool);

	err = cbor_decode_UnorderedMap16(payload_unordered_map16_inv1,
			sizeof(payload_unordered_map16_inv1), &unordered_map16, NULL);
	zassert_not_equal(ZCBOR_SUCCESS, err);

	err = cbor_decode_UnorderedMap16(payload_unordered_map16_inv2,
			sizeof(payload_unordered_map16_inv2), &unordered_map16, NULL);
	zassert_not_equal(ZCBOR_SUCCESS, err);
}

ZTEST_SUITE(cbor_decode_testB, NULL, NULL, NULL, NULL, NULL);
