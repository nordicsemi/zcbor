/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/sys/util.h>
#include "zcbor_decode.h"
#include "zcbor_encode.h"
#include "zcbor_print.h"
#include <math.h>

/* The error from ending a list/map before all its elements have been decoded. It depends
 * on how the list/map header was encoded: a definite length container is ended by its
 * element count reaching 0, while an indefinite length one is ended by a 0xFF break byte,
 * which is what the decoder fails to find here. */
#ifdef ZCBOR_CANONICAL
#define UNIT_ARR_ERR1 ZCBOR_ERR_HIGH_ELEM_COUNT
#else
#define UNIT_ARR_ERR1 ZCBOR_ERR_WRONG_TYPE
#endif

/* Helpers for checking that a failed call left the state completely untouched, i.e. that
 * decoding can continue as if the call had never happened. Only the members that the
 * start/end functions can modify are covered. */
struct zcbor_dec_state_snapshot {
	const uint8_t *payload;
	const uint8_t *payload_end;
	size_t elem_count;
	size_t current_backup;
	bool inside_cbor_bstr;
};

static void dec_snapshot(zcbor_state_t *state, struct zcbor_dec_state_snapshot *snap)
{
	snap->payload = state->payload;
	snap->payload_end = state->payload_end;
	snap->elem_count = state->elem_count;
	snap->current_backup = state->constant_state->current_backup;
	snap->inside_cbor_bstr = state->inside_cbor_bstr;
}

static void dec_assert_unchanged(zcbor_state_t *state, const struct zcbor_dec_state_snapshot *snap,
				 const char *msg)
{
	zassert_equal(snap->payload, state->payload, "%s: payload", msg);
	zassert_equal(snap->payload_end, state->payload_end, "%s: payload_end", msg);
	zassert_equal(snap->elem_count, state->elem_count, "%s: elem_count", msg);
	zassert_equal(snap->current_backup, state->constant_state->current_backup, "%s: backup", msg);
	zassert_equal(snap->inside_cbor_bstr, state->inside_cbor_bstr, "%s: inside_cbor_bstr", msg);
}

/* The same, for encoding states. Only needed when ZCBOR_CANONICAL is defined, since
 * that is the only case where list/map encoding takes backups. */
#ifdef ZCBOR_CANONICAL
struct zcbor_enc_state_snapshot {
	const uint8_t *payload;
	size_t elem_count;
	size_t current_backup;
};

static void enc_snapshot(zcbor_state_t *state, struct zcbor_enc_state_snapshot *snap)
{
	snap->payload = state->payload;
	snap->elem_count = state->elem_count;
	snap->current_backup = state->constant_state->current_backup;
}

static void enc_assert_unchanged(zcbor_state_t *state, const struct zcbor_enc_state_snapshot *snap,
				 const char *msg)
{
	zassert_equal(snap->payload, state->payload, "%s: payload", msg);
	zassert_equal(snap->elem_count, state->elem_count, "%s: elem_count", msg);
	zassert_equal(snap->current_backup, state->constant_state->current_backup, "%s: backup", msg);
}
#endif


ZTEST(zcbor_unit_tests, test_int64)
{
	uint8_t payload[100] = {0};
	int64_t int64;
	int32_t int32;

	ZCBOR_STATE_E(state_e, 0, payload, sizeof(payload), 0);
	ZCBOR_STATE_D(state_d, 0, payload, sizeof(payload), 10, 0);

	zassert_true(zcbor_int64_put(state_e, 5), NULL);
	zassert_false(zcbor_int64_expect(state_d, 4), NULL);
	zassert_false(zcbor_int64_expect(state_d, 6), NULL);
	zassert_false(zcbor_int64_expect(state_d, -5), NULL);
	zassert_false(zcbor_int64_expect(state_d, -6), NULL);
	zassert_true(zcbor_int64_expect(state_d, 5), NULL);

	zassert_true(zcbor_int32_put(state_e, 5), NULL);
	zassert_true(zcbor_int64_expect(state_d, 5), NULL);

	zassert_true(zcbor_int64_put(state_e, 5), NULL);
	zassert_true(zcbor_int32_expect(state_d, 5), NULL);

	zassert_true(zcbor_int64_put(state_e, 5000000000), NULL);
	zassert_false(zcbor_int32_decode(state_d, &int32), NULL);
	zassert_true(zcbor_int64_decode(state_d, &int64), NULL);
	zassert_equal(int64, 5000000000, NULL);

	zassert_true(zcbor_int64_encode(state_e, &int64), NULL);
	zassert_false(zcbor_int64_expect(state_d, 5000000001), NULL);
	zassert_true(zcbor_int64_expect(state_d, 5000000000), NULL);

	zassert_true(zcbor_int64_put(state_e, 0x80000000), NULL);
	zassert_false(zcbor_int32_decode(state_d, &int32), NULL);
	zassert_true(zcbor_uint32_expect(state_d, 0x80000000), NULL);

	zassert_true(zcbor_int32_put(state_e, -505), NULL);
	zassert_true(zcbor_int64_expect(state_d, -505), NULL);

	zassert_true(zcbor_int64_put(state_e, -5000000000000), NULL);
	zassert_false(zcbor_int64_expect(state_d, -5000000000001), NULL);
	zassert_false(zcbor_int64_expect(state_d, -4999999999999), NULL);
	zassert_false(zcbor_int64_expect(state_d, 5000000000000), NULL);
	zassert_false(zcbor_int64_expect(state_d, 4999999999999), NULL);
	zassert_true(zcbor_int64_expect(state_d, -5000000000000), NULL);

	zassert_true(zcbor_uint64_put(state_e, ((uint64_t)INT64_MAX + 1)), NULL);
	zassert_false(zcbor_int64_decode(state_d, &int64), NULL);
	zassert_true(zcbor_uint64_expect(state_d, ((uint64_t)INT64_MAX + 1)), NULL);

}


ZTEST(zcbor_unit_tests, test_uint64)
{
	uint8_t payload[100] = {0};
	uint64_t uint64;
	uint32_t uint32;

	zcbor_state_t state_e;
	zcbor_new_state(&state_e, 1, payload, sizeof(payload), 0, NULL, 0);
	zcbor_state_t state_d;
	zcbor_new_state(&state_d, 1, payload, sizeof(payload), 10, NULL, 0);

	zassert_true(zcbor_uint64_put(&state_e, 5), NULL);
	zassert_false(zcbor_uint64_expect(&state_d, 4), NULL);
	zassert_false(zcbor_uint64_expect(&state_d, 6), NULL);
	zassert_false(zcbor_uint64_expect(&state_d, -5), NULL);
	zassert_false(zcbor_uint64_expect(&state_d, -6), NULL);
	zassert_true(zcbor_uint64_expect(&state_d, 5), NULL);

	zassert_true(zcbor_uint32_put(&state_e, 5), NULL);
	zassert_true(zcbor_uint64_expect(&state_d, 5), NULL);

	zassert_true(zcbor_uint64_put(&state_e, 5), NULL);
	zassert_true(zcbor_uint32_expect(&state_d, 5), NULL);

	zassert_true(zcbor_uint64_put(&state_e, UINT64_MAX), NULL);
	zassert_false(zcbor_uint32_decode(&state_d, &uint32), NULL);
	zassert_true(zcbor_uint64_decode(&state_d, &uint64), NULL);
	zassert_equal(uint64, UINT64_MAX, NULL);

	zassert_true(zcbor_uint64_encode(&state_e, &uint64), NULL);
	zassert_false(zcbor_uint64_expect(&state_d, (UINT64_MAX - 1)), NULL);
	zassert_true(zcbor_uint64_expect(&state_d, UINT64_MAX), NULL);
}

ZTEST(zcbor_unit_tests, test_size)
{
	uint8_t payload[100] = {0};
	size_t read;

	zcbor_state_t state_e;
	zcbor_new_state(&state_e, 1, payload, sizeof(payload), 0, NULL, 0);
	zcbor_state_t state_d;
	zcbor_new_state(&state_d, 1, payload, sizeof(payload), 10, NULL, 0);

	zassert_true(zcbor_size_put(&state_e, 5), NULL);
	zassert_false(zcbor_size_expect(&state_d, 4), NULL);
	zassert_false(zcbor_size_expect(&state_d, 6), NULL);
	zassert_false(zcbor_size_expect(&state_d, -5), NULL);
	zassert_false(zcbor_size_expect(&state_d, -6), NULL);
	zassert_true(zcbor_size_expect(&state_d, 5), NULL);

	zassert_true(zcbor_int32_put(&state_e, -7), NULL);
	zassert_false(zcbor_size_expect(&state_d, -7), NULL);
	zassert_false(zcbor_size_decode(&state_d, &read), NULL); // Negative number not supported.
	zassert_true(zcbor_int32_expect(&state_d, -7), NULL);

	zassert_true(zcbor_uint32_put(&state_e, 5), NULL);
	zassert_true(zcbor_size_expect(&state_d, 5), NULL);

	zassert_true(zcbor_uint64_put(&state_e, 5), NULL);
	zassert_true(zcbor_size_expect(&state_d, 5), NULL);

	zassert_true(zcbor_uint32_put(&state_e, UINT32_MAX), NULL);
	zassert_true(zcbor_size_decode(&state_d, &read), NULL);
	zassert_equal(read, UINT32_MAX, NULL);

#if SIZE_MAX == UINT64_MAX
	zassert_true(zcbor_uint64_put(&state_e, UINT64_MAX), NULL);
	zassert_true(zcbor_size_decode(&state_d, &read), NULL);
	zassert_equal(read, UINT64_MAX, NULL);
#endif

#if SIZE_MAX == UINT32_MAX
	zassert_true(zcbor_uint64_put(&state_e, UINT64_MAX), NULL);
	zassert_false(zcbor_size_decode(&state_d, &read), NULL);
#endif
}

#if SIZE_MAX == UINT64_MAX
/* Only runs on 64-bit builds. */
#include <stdlib.h>

#define PAYL_SIZE 0x100000100
#define STR_SIZE 0x100000010

ZTEST(zcbor_unit_tests, test_size64)
{
	uint8_t *large_payload = malloc(PAYL_SIZE);
	uint8_t *large_string = malloc(STR_SIZE);
	struct zcbor_string tstr = {.value = large_string, .len = STR_SIZE};
	struct zcbor_string tstr_res;

	for (int i = 0; i < 1000; i++) {
		large_string[i] = i % 256;
		large_payload[i + 9] = 0;
	}
	for (size_t i = STR_SIZE - 1001; i < STR_SIZE; i++) {
		large_string[i] = i % 256;
		large_payload[i + 9] = 0;
	}

	ZCBOR_STATE_E(state_e, 0, large_payload, PAYL_SIZE, 0);
	ZCBOR_STATE_D(state_d, 0, large_payload, PAYL_SIZE, 10, 0);

	zassert_true(zcbor_tstr_encode(state_e, &tstr), NULL);
	zassert_false(zcbor_bstr_decode(state_d, &tstr_res), NULL);
	zassert_true(zcbor_tstr_decode(state_d, &tstr_res), NULL);
	zassert_equal(tstr_res.len, tstr.len, NULL);
	zassert_equal_ptr(tstr_res.value, &large_payload[9], NULL);
	zassert_mem_equal(tstr_res.value, large_string, tstr.len, NULL);
}


#else
ZTEST(zcbor_unit_tests, test_size64)
{
	printf("Skip on non-64-bit builds.\n");
}
#endif


ZTEST(zcbor_unit_tests, test_string_macros)
{
	uint8_t payload[100];
	ZCBOR_STATE_E(state_e, 0, payload, sizeof(payload), 0);
	char world[] = {'w', 'o', 'r', 'l', 'd'};

	zassert_true(zcbor_bstr_put_lit(state_e, "Hello"), NULL);
	zassert_true(zcbor_bstr_put_term(state_e, "Hello world", 5), NULL); /* Check that strnlen cuts off at 5 */
	zassert_true(zcbor_bstr_put_arr(state_e, world), NULL);
	zassert_true(zcbor_tstr_put_lit(state_e, "Hello"), NULL);
	zassert_true(zcbor_tstr_put_term(state_e, "Hellooooo", 5), NULL); /* Check that strnlen cuts off at 5 */
	zassert_true(zcbor_tstr_put_arr(state_e, world), NULL);

	ZCBOR_STATE_D(state_d, 0, payload, sizeof(payload), 6, 0);

	zassert_false(zcbor_bstr_expect_lit(state_d, "Yello"), NULL);
	zassert_false(zcbor_tstr_expect_lit(state_d, "Hello"), NULL);
	zassert_true(zcbor_bstr_expect_lit(state_d, "Hello"), NULL);
	zassert_false(zcbor_bstr_expect_term(state_d, "Hello!", 10), NULL);
	zassert_false(zcbor_bstr_expect_term(state_d, "Hello", 4), NULL); /* Check that strnlen cuts off at 4 */
	zassert_true(zcbor_bstr_expect_term(state_d, "Hello", 5), NULL);
	world[3]++;
	zassert_false(zcbor_bstr_expect_arr(state_d, world), NULL);
	world[3]--;
	zassert_true(zcbor_bstr_expect_arr(state_d, world), NULL);
	zassert_false(zcbor_tstr_expect_lit(state_d, "hello"), NULL);
	zassert_true(zcbor_tstr_expect_lit(state_d, "Hello"), NULL);
	zassert_false(zcbor_tstr_expect_term(state_d, "Helo", 10), NULL);
	zassert_false(zcbor_tstr_expect_term(state_d, "Hello", 0), NULL);
	zassert_true(zcbor_tstr_expect_term(state_d, "Hello", 10), NULL);
	world[2]++;
	zassert_false(zcbor_tstr_expect_arr(state_d, world), NULL);
	world[2]--;
	zassert_false(zcbor_bstr_expect_arr(state_d, world), NULL);
	zassert_true(zcbor_tstr_expect_arr(state_d, world), NULL);
}


ZTEST(zcbor_unit_tests, test_string_overflow)
{
	uint8_t payload[50];
	ZCBOR_STATE_E(state_e1, 0, payload, 6, 0);
	ZCBOR_STATE_E(state_e2, 0, payload, 32, 0);

	bool ret = zcbor_tstr_put_lit(state_e1, "hello!");
	zassert_false(ret, NULL);
	zassert_true(zcbor_tstr_put_lit(state_e1, "hello"), NULL);
	zassert_true(state_e1->payload_end == state_e1->payload, NULL);
	zassert_false(zcbor_bstr_put_lit(state_e2, "hello world and everyone in it!"), NULL);
	zassert_true(zcbor_bstr_put_lit(state_e2, "hello world and everyone in it"), NULL);
	zassert_true(state_e2->payload_end == state_e2->payload, NULL);
}


ZTEST(zcbor_unit_tests, test_stop_on_error)
{
	uint8_t payload[100];
	ZCBOR_STATE_E(state_e, 3, payload, sizeof(payload), 0);
	struct zcbor_string failing_string = {.value = NULL, .len = 1000};
	struct zcbor_string dummy_string;
	zcbor_state_t state_backup;
	struct zcbor_state_constant constant_state_backup;

	state_e->constant_state->stop_on_error = true;

	zassert_false(zcbor_tstr_encode(state_e, &failing_string), NULL);
	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, state_e->constant_state->error, "%d\r\n", state_e->constant_state->error);
	memcpy(&state_backup, state_e, sizeof(state_backup));
	memcpy(&constant_state_backup, state_e->constant_state, sizeof(constant_state_backup));

	/* All fail because of ZCBOR_ERR_NO_PAYLOAD */
	zassert_false(zcbor_int8_put(state_e, 16), NULL);
	zassert_false(zcbor_int16_put(state_e, 17), NULL);
	zassert_false(zcbor_int32_put(state_e, 1), NULL);
	zassert_false(zcbor_int64_put(state_e, 2), NULL);
	zassert_false(zcbor_uint8_put(state_e, 18), NULL);
	zassert_false(zcbor_uint16_put(state_e, 19), NULL);
	zassert_false(zcbor_uint32_put(state_e, 3), NULL);
	zassert_false(zcbor_uint64_put(state_e, 4), NULL);
	zassert_false(zcbor_size_put(state_e, 10), NULL);
	zassert_false(zcbor_int8_put(state_e, 20), NULL);
	zassert_false(zcbor_int16_put(state_e, 21), NULL);
	zassert_false(zcbor_int32_put(state_e, 11), NULL);
	zassert_false(zcbor_int64_put(state_e, 12), NULL);
	zassert_false(zcbor_uint8_put(state_e, 22), NULL);
	zassert_false(zcbor_uint16_put(state_e, 23), NULL);
	zassert_false(zcbor_uint32_put(state_e, 13), NULL);
	zassert_false(zcbor_uint64_put(state_e, 14), NULL);
	zassert_false(zcbor_size_put(state_e, 15), NULL);
	zassert_false(zcbor_int8_encode(state_e, &(int8_t){24}), NULL);
	zassert_false(zcbor_int16_encode(state_e, &(int16_t){25}), NULL);
	zassert_false(zcbor_int32_encode(state_e, &(int32_t){5}), NULL);
	zassert_false(zcbor_int64_encode(state_e, &(int64_t){6}), NULL);
	zassert_false(zcbor_uint8_encode(state_e, &(uint8_t){26}), NULL);
	zassert_false(zcbor_uint16_encode(state_e, &(uint16_t){27}), NULL);
	zassert_false(zcbor_uint32_encode(state_e, &(uint32_t){7}), NULL);
	zassert_false(zcbor_uint64_encode(state_e, &(uint64_t){8}), NULL);
	zassert_false(zcbor_size_encode(state_e, &(size_t){9}), NULL);
	zassert_false(zcbor_bstr_put_lit(state_e, "Hello"), NULL);
	zassert_false(zcbor_tstr_put_lit(state_e, "World"), NULL);
	zassert_false(zcbor_tag_put(state_e, 9), NULL);
	zassert_false(zcbor_tag_encode(state_e, &(uint32_t){10}));
	zassert_false(zcbor_tag_put(state_e, 11), NULL);
	zassert_false(zcbor_bool_put(state_e, true), NULL);
	zassert_false(zcbor_bool_put(state_e, false), NULL);
	zassert_false(zcbor_bool_encode(state_e, &(bool){false}), NULL);
	zassert_false(zcbor_float32_put(state_e, 10.1), NULL);
	zassert_false(zcbor_float32_put(state_e, 11.2), NULL);
	zassert_false(zcbor_float32_encode(state_e, &(float){12.3}), NULL);
	zassert_false(zcbor_float64_put(state_e, 13.4), NULL);
	zassert_false(zcbor_float64_put(state_e, 14.5), NULL);
	zassert_false(zcbor_float64_encode(state_e, &(double){15.6}), NULL);
	zassert_false(zcbor_nil_put(state_e, NULL), NULL);
	zassert_false(zcbor_undefined_put(state_e, NULL), NULL);
	zassert_false(zcbor_bstr_start_encode(state_e), NULL);
	zassert_false(zcbor_bstr_end_encode(state_e, NULL), NULL);
	zassert_false(zcbor_list_start_encode(state_e, 1), NULL);
	zassert_false(zcbor_map_start_encode(state_e, 0), NULL);
	zassert_false(zcbor_map_end_encode(state_e, 0), NULL);
	zassert_false(zcbor_list_end_encode(state_e, 1), NULL);
	zassert_false(zcbor_multi_encode(1, ZCBOR_CAST_FP(zcbor_int32_encode), state_e, &(int32_t){14}, 0), NULL);
	zassert_false(zcbor_multi_encode_minmax(1, 1, &(size_t){1}, ZCBOR_CAST_FP(zcbor_int32_encode), state_e, &(int32_t){15}, 0), NULL);


	zassert_mem_equal(&state_backup, state_e, sizeof(state_backup), NULL);
	zassert_mem_equal(&constant_state_backup, state_e->constant_state, sizeof(constant_state_backup), NULL);

	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, zcbor_peek_error(state_e), NULL);
	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, zcbor_pop_error(state_e), NULL);
	zassert_equal(ZCBOR_SUCCESS, zcbor_peek_error(state_e), NULL);

	/* All succeed since the error has been popped. */
	zassert_true(zcbor_int8_put(state_e, 16), NULL);
	zassert_true(zcbor_int16_put(state_e, 17), NULL);
	zassert_true(zcbor_int32_put(state_e, 1), NULL);
	zassert_true(zcbor_int64_put(state_e, 2), NULL);
	zassert_true(zcbor_uint8_put(state_e, 18), NULL);
	zassert_true(zcbor_uint16_put(state_e, 19), NULL);
	zassert_true(zcbor_uint32_put(state_e, 3), NULL);
	zassert_true(zcbor_uint64_put(state_e, 4), NULL);
	zassert_true(zcbor_size_put(state_e, 10), NULL);
	zassert_true(zcbor_int8_put(state_e, 20), NULL);
	zassert_true(zcbor_int16_put(state_e, 21), NULL);
	zassert_true(zcbor_int32_put(state_e, 11), NULL);
	zassert_true(zcbor_int64_put(state_e, 12), NULL);
	zassert_true(zcbor_uint8_put(state_e, 22), NULL);
	zassert_true(zcbor_uint16_put(state_e, 23), NULL);
	zassert_true(zcbor_uint32_put(state_e, 13), NULL);
	zassert_true(zcbor_uint64_put(state_e, 14), NULL);
	zassert_true(zcbor_size_put(state_e, 15), NULL);
	zassert_true(zcbor_int8_encode(state_e, &(int8_t){24}), NULL);
	zassert_true(zcbor_int16_encode(state_e, &(int16_t){25}), NULL);
	zassert_true(zcbor_int32_encode(state_e, &(int32_t){5}), NULL);
	zassert_true(zcbor_int64_encode(state_e, &(int64_t){6}), NULL);
	zassert_true(zcbor_uint8_encode(state_e, &(uint8_t){26}), NULL);
	zassert_true(zcbor_uint16_encode(state_e, &(uint16_t){27}), NULL);
	zassert_true(zcbor_uint32_encode(state_e, &(uint32_t){7}), NULL);
	zassert_true(zcbor_uint64_encode(state_e, &(uint64_t){8}), NULL);
	zassert_true(zcbor_size_encode(state_e, &(size_t){9}), NULL);
	zassert_true(zcbor_bstr_put_lit(state_e, "Hello"), NULL);
	zassert_true(zcbor_tstr_put_lit(state_e, "World"), NULL);
	zassert_true(zcbor_tag_put(state_e, 9), NULL);
	zassert_true(zcbor_tag_encode(state_e, &(uint32_t){10}));
	zassert_true(zcbor_tag_put(state_e, 11), NULL);
	zassert_true(zcbor_bool_put(state_e, true), NULL);
	zassert_true(zcbor_bool_put(state_e, false), NULL);
	zassert_true(zcbor_bool_encode(state_e, &(bool){false}), NULL);
	zassert_true(zcbor_float32_put(state_e, 10.1), NULL);
	zassert_true(zcbor_float32_put(state_e, 11.2), NULL);
	zassert_true(zcbor_float32_encode(state_e, &(float){12.3}), NULL);
	zassert_true(zcbor_float64_put(state_e, 13.4), NULL);
	zassert_true(zcbor_float64_put(state_e, 14.5), NULL);
	zassert_true(zcbor_float64_encode(state_e, &(double){15.6}), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_undefined_put(state_e, NULL), NULL);
	zassert_true(zcbor_bstr_start_encode(state_e), NULL);
	zassert_true(zcbor_bstr_end_encode(state_e, NULL), NULL);
	zassert_true(zcbor_list_start_encode(state_e, 1), NULL);
	zassert_true(zcbor_map_start_encode(state_e, 0), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 0), NULL);
	zassert_true(zcbor_list_end_encode(state_e, 1), NULL);
	zassert_true(zcbor_multi_encode(1, ZCBOR_CAST_FP(zcbor_int32_encode), state_e, &(int32_t){14}, 0), NULL);
	zassert_true(zcbor_multi_encode_minmax(1, 1, &(size_t){1}, ZCBOR_CAST_FP(zcbor_int32_encode), state_e, &(int32_t){15}, 0), NULL);

	ZCBOR_STATE_D(state_d, 3, payload, sizeof(payload), 50, 0);
	state_d->constant_state->stop_on_error = true;

	zassert_false(zcbor_int32_expect(state_d, 2), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, state_d->constant_state->error, "%d\r\n", state_d->constant_state->error);
	memcpy(&state_backup, state_d, sizeof(state_backup));
	memcpy(&constant_state_backup, state_d->constant_state, sizeof(constant_state_backup));

	/* All fail because of ZCBOR_ERR_WRONG_VALUE */
	zassert_false(zcbor_int8_expect(state_d, 16), NULL);
	zassert_false(zcbor_int16_expect(state_d, 17), NULL);
	zassert_false(zcbor_int32_expect(state_d, 1), NULL);
	zassert_false(zcbor_int64_expect(state_d, 2), NULL);
	zassert_false(zcbor_uint8_expect(state_d, 18), NULL);
	zassert_false(zcbor_uint16_expect(state_d, 19), NULL);
	zassert_false(zcbor_uint32_expect(state_d, 3), NULL);
	zassert_false(zcbor_uint64_expect(state_d, 4), NULL);
	zassert_false(zcbor_size_expect(state_d, 10), NULL);
	zassert_false(zcbor_int8_pexpect(state_d, &(int8_t){20}), NULL);
	zassert_false(zcbor_int16_pexpect(state_d, &(int16_t){21}), NULL);
	zassert_false(zcbor_int32_pexpect(state_d, &(int32_t){11}), NULL);
	zassert_false(zcbor_int64_pexpect(state_d, &(int64_t){12}), NULL);
	zassert_false(zcbor_uint8_pexpect(state_d, &(uint8_t){22}), NULL);
	zassert_false(zcbor_uint16_pexpect(state_d, &(uint16_t){23}), NULL);
	zassert_false(zcbor_uint32_pexpect(state_d, &(uint32_t){13}), NULL);
	zassert_false(zcbor_uint64_pexpect(state_d, &(uint64_t){14}), NULL);
	zassert_false(zcbor_size_pexpect(state_d, &(size_t){15}), NULL);
	zassert_false(zcbor_int8_decode(state_d, &(int8_t){24}), NULL);
	zassert_false(zcbor_int16_decode(state_d, &(int16_t){25}), NULL);
	zassert_false(zcbor_int32_decode(state_d, &(int32_t){5}), NULL);
	zassert_false(zcbor_int64_decode(state_d, &(int64_t){6}), NULL);
	zassert_false(zcbor_uint8_decode(state_d, &(uint8_t){26}), NULL);
	zassert_false(zcbor_uint16_decode(state_d, &(uint16_t){27}), NULL);
	zassert_false(zcbor_uint32_decode(state_d, &(uint32_t){7}), NULL);
	zassert_false(zcbor_uint64_decode(state_d, &(uint64_t){8}), NULL);
	zassert_false(zcbor_size_decode(state_d, &(size_t){9}), NULL);
	zassert_false(zcbor_bstr_expect_lit(state_d, "Hello"), NULL);
	zassert_false(zcbor_tstr_expect_lit(state_d, "World"), NULL);
	zassert_false(zcbor_tag_decode(state_d, &(uint32_t){9}), NULL);
	zassert_false(zcbor_tag_expect(state_d, 10), NULL);
	zassert_false(zcbor_tag_pexpect(state_d, &(uint32_t){11}), NULL);
	zassert_false(zcbor_bool_expect(state_d, true), NULL);
	zassert_false(zcbor_bool_pexpect(state_d, &(bool){false}), NULL);
	zassert_false(zcbor_bool_decode(state_d, &(bool){false}), NULL);
	zassert_false(zcbor_float32_expect(state_d, 10.1), NULL);
	zassert_false(zcbor_float32_pexpect(state_d, &(float){11.2}), NULL);
	zassert_false(zcbor_float32_decode(state_d, &(float){12.3}), NULL);
	zassert_false(zcbor_float64_expect(state_d, 13.4), NULL);
	zassert_false(zcbor_float64_pexpect(state_d, &(double){14.5}), NULL);
	zassert_false(zcbor_float64_decode(state_d, &(double){15.6}), NULL);
	zassert_false(zcbor_nil_expect(state_d, NULL), NULL);
	zassert_false(zcbor_undefined_expect(state_d, NULL), NULL);
	zassert_false(zcbor_bstr_start_decode(state_d, &dummy_string), NULL);
	zassert_false(zcbor_bstr_end_decode(state_d, false), NULL);
	zassert_false(zcbor_list_start_decode(state_d), NULL);
	zassert_false(zcbor_map_start_decode(state_d), NULL);
	zassert_false(zcbor_map_end_decode(state_d, true), NULL);
	zassert_false(zcbor_list_end_decode(state_d, true), NULL);
	zassert_false(zcbor_multi_decode(1, 1, &(size_t){1}, ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){14}, 0), NULL);
	zassert_false(zcbor_present_decode(&(bool){true}, ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){15}), NULL);

	zassert_mem_equal(&state_backup, state_d, sizeof(state_backup), NULL);
	zassert_mem_equal(&constant_state_backup, state_d->constant_state, sizeof(constant_state_backup), NULL);

	zassert_equal(ZCBOR_ERR_WRONG_VALUE, zcbor_pop_error(state_d), NULL);

	/* All succeed since the error has been popped. */
	zassert_true(zcbor_int8_expect(state_d, 16), NULL);
	zassert_true(zcbor_int16_expect(state_d, 17), NULL);
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);
	zassert_true(zcbor_int64_expect(state_d, 2), NULL);
	zassert_true(zcbor_uint8_expect(state_d, 18), NULL);
	zassert_true(zcbor_uint16_expect(state_d, 19), NULL);
	zassert_true(zcbor_uint32_expect(state_d, 3), NULL);
	zassert_true(zcbor_uint64_expect(state_d, 4), NULL);
	zassert_true(zcbor_size_expect(state_d, 10), NULL);
	zassert_true(zcbor_int8_pexpect(state_d, &(int8_t){20}), NULL);
	zassert_true(zcbor_int16_pexpect(state_d, &(int16_t){21}), NULL);
	zassert_true(zcbor_int32_pexpect(state_d, &(int32_t){11}), NULL);
	zassert_true(zcbor_int64_pexpect(state_d, &(int64_t){12}), NULL);
	zassert_true(zcbor_uint8_pexpect(state_d, &(uint8_t){22}), NULL);
	zassert_true(zcbor_uint16_pexpect(state_d, &(uint16_t){23}), NULL);
	zassert_true(zcbor_uint32_pexpect(state_d, &(uint32_t){13}), NULL);
	zassert_true(zcbor_uint64_pexpect(state_d, &(uint64_t){14}), NULL);
	zassert_true(zcbor_size_pexpect(state_d, &(size_t){15}), NULL);
	zassert_true(zcbor_int8_decode(state_d, &(int8_t){24}), NULL);
	zassert_true(zcbor_int16_decode(state_d, &(int16_t){25}), NULL);
	zassert_true(zcbor_int32_decode(state_d, &(int32_t){5}), NULL);
	zassert_true(zcbor_int64_decode(state_d, &(int64_t){6}), NULL);
	zassert_true(zcbor_uint8_decode(state_d, &(uint8_t){26}), NULL);
	zassert_true(zcbor_uint16_decode(state_d, &(uint16_t){27}), NULL);
	zassert_true(zcbor_uint32_decode(state_d, &(uint32_t){7}), NULL);
	zassert_true(zcbor_uint64_decode(state_d, &(uint64_t){8}), NULL);
	zassert_true(zcbor_size_decode(state_d, &(size_t){9}), NULL);
	zassert_true(zcbor_bstr_expect_lit(state_d, "Hello"), NULL);
	zassert_true(zcbor_tstr_expect_lit(state_d, "World"), NULL);
	zassert_true(zcbor_tag_decode(state_d, &(uint32_t){9}), NULL);
	zassert_true(zcbor_tag_expect(state_d, 10), NULL);
	zassert_true(zcbor_tag_pexpect(state_d, &(uint32_t){11}), NULL);
	zassert_true(zcbor_bool_expect(state_d, true), NULL);
	zassert_true(zcbor_bool_pexpect(state_d, &(bool){false}), NULL);
	zassert_true(zcbor_bool_decode(state_d, &(bool){false}), NULL);
	zassert_true(zcbor_float32_expect(state_d, 10.1), NULL);
	zassert_true(zcbor_float32_pexpect(state_d, &(float){11.2}), NULL);
	zassert_true(zcbor_float32_decode(state_d, &(float){12.3}), NULL);
	zassert_true(zcbor_float64_expect(state_d, 13.4), NULL);
	zassert_true(zcbor_float64_pexpect(state_d, &(double){14.5}), NULL);
	zassert_true(zcbor_float64_decode(state_d, &(double){15.6}), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	zassert_true(zcbor_undefined_expect(state_d, NULL), NULL);
	zassert_true(zcbor_bstr_start_decode(state_d, &dummy_string), NULL);
	zassert_true(zcbor_bstr_end_decode(state_d, false), NULL);
	zassert_true(zcbor_list_start_decode(state_d), NULL);
	zassert_true(zcbor_map_start_decode(state_d), NULL);
	zassert_true(zcbor_map_end_decode(state_d, true), NULL);
	zassert_true(zcbor_list_end_decode(state_d, true), NULL);
	zassert_true(zcbor_multi_decode(1, 1, &(size_t){1}, ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){14}, 0), NULL);
	zassert_true(zcbor_present_decode(&(bool){1}, ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){15}), NULL);

	/* Everything has been decoded. */
	zassert_equal(state_e->payload, state_d->payload, NULL);
}


/* Test that functions fail early when state is NULL. */
ZTEST(zcbor_unit_tests, test_null_state)
{
	struct zcbor_string dummy_string;
	uint8_t dummy_payload[10] = {0};

	zassert_false(zcbor_int8_put(NULL, 16), NULL);
	zassert_false(zcbor_int16_put(NULL, 17), NULL);
	zassert_false(zcbor_int32_put(NULL, 1), NULL);
	zassert_false(zcbor_int64_put(NULL, 2), NULL);
	zassert_false(zcbor_uint8_put(NULL, 18), NULL);
	zassert_false(zcbor_uint16_put(NULL, 19), NULL);
	zassert_false(zcbor_uint32_put(NULL, 3), NULL);
	zassert_false(zcbor_uint64_put(NULL, 4), NULL);
	zassert_false(zcbor_size_put(NULL, 10), NULL);
	zassert_false(zcbor_int8_put(NULL, 20), NULL);
	zassert_false(zcbor_int16_put(NULL, 21), NULL);
	zassert_false(zcbor_int32_put(NULL, 11), NULL);
	zassert_false(zcbor_int64_put(NULL, 12), NULL);
	zassert_false(zcbor_uint8_put(NULL, 22), NULL);
	zassert_false(zcbor_uint16_put(NULL, 23), NULL);
	zassert_false(zcbor_uint32_put(NULL, 13), NULL);
	zassert_false(zcbor_uint64_put(NULL, 14), NULL);
	zassert_false(zcbor_size_put(NULL, 15), NULL);
	zassert_false(zcbor_int8_encode(NULL, &(int8_t){24}), NULL);
	zassert_false(zcbor_int16_encode(NULL, &(int16_t){25}), NULL);
	zassert_false(zcbor_int32_encode(NULL, &(int32_t){5}), NULL);
	zassert_false(zcbor_int64_encode(NULL, &(int64_t){6}), NULL);
	zassert_false(zcbor_uint8_encode(NULL, &(uint8_t){26}), NULL);
	zassert_false(zcbor_uint16_encode(NULL, &(uint16_t){27}), NULL);
	zassert_false(zcbor_uint32_encode(NULL, &(uint32_t){7}), NULL);
	zassert_false(zcbor_uint64_encode(NULL, &(uint64_t){8}), NULL);
	zassert_false(zcbor_size_encode(NULL, &(size_t){9}), NULL);
	zassert_false(zcbor_bstr_put_lit(NULL, "Hello"), NULL);
	zassert_false(zcbor_tstr_put_lit(NULL, "World"), NULL);
	zassert_false(zcbor_tag_put(NULL, 9), NULL);
	zassert_false(zcbor_tag_encode(NULL, &(uint32_t){10}));
	zassert_false(zcbor_tag_put(NULL, 11), NULL);
	zassert_false(zcbor_bool_put(NULL, true), NULL);
	zassert_false(zcbor_bool_put(NULL, false), NULL);
	zassert_false(zcbor_bool_encode(NULL, &(bool){false}), NULL);
	zassert_false(zcbor_float32_put(NULL, 10.1), NULL);
	zassert_false(zcbor_float32_put(NULL, 11.2), NULL);
	zassert_false(zcbor_float32_encode(NULL, &(float){12.3}), NULL);
	zassert_false(zcbor_float64_put(NULL, 13.4), NULL);
	zassert_false(zcbor_float64_put(NULL, 14.5), NULL);
	zassert_false(zcbor_float64_encode(NULL, &(double){15.6}), NULL);
	zassert_false(zcbor_nil_put(NULL, NULL), NULL);
	zassert_false(zcbor_undefined_put(NULL, NULL), NULL);
	zassert_false(zcbor_bstr_start_encode(NULL), NULL);
	zassert_false(zcbor_bstr_end_encode(NULL, NULL), NULL);
	zassert_false(zcbor_list_start_encode(NULL, 1), NULL);
	zassert_false(zcbor_map_start_encode(NULL, 0), NULL);
	zassert_false(zcbor_map_end_encode(NULL, 0), NULL);
	zassert_false(zcbor_list_end_encode(NULL, 1), NULL);
	zassert_false(zcbor_multi_encode(1, ZCBOR_CAST_FP(zcbor_int32_encode), NULL, &(int32_t){14}, 0), NULL);
	zassert_false(zcbor_multi_encode_minmax(1, 1, &(size_t){1}, ZCBOR_CAST_FP(zcbor_int32_encode), NULL, &(int32_t){15}, 0), NULL);

	zassert_false(zcbor_int8_expect(NULL, 16), NULL);
	zassert_false(zcbor_int16_expect(NULL, 17), NULL);
	zassert_false(zcbor_int32_expect(NULL, 1), NULL);
	zassert_false(zcbor_int64_expect(NULL, 2), NULL);
	zassert_false(zcbor_uint8_expect(NULL, 18), NULL);
	zassert_false(zcbor_uint16_expect(NULL, 19), NULL);
	zassert_false(zcbor_uint32_expect(NULL, 3), NULL);
	zassert_false(zcbor_uint64_expect(NULL, 4), NULL);
	zassert_false(zcbor_size_expect(NULL, 10), NULL);
	zassert_false(zcbor_int8_pexpect(NULL, &(int8_t){20}), NULL);
	zassert_false(zcbor_int16_pexpect(NULL, &(int16_t){21}), NULL);
	zassert_false(zcbor_int32_pexpect(NULL, &(int32_t){11}), NULL);
	zassert_false(zcbor_int64_pexpect(NULL, &(int64_t){12}), NULL);
	zassert_false(zcbor_uint8_pexpect(NULL, &(uint8_t){22}), NULL);
	zassert_false(zcbor_uint16_pexpect(NULL, &(uint16_t){23}), NULL);
	zassert_false(zcbor_uint32_pexpect(NULL, &(uint32_t){13}), NULL);
	zassert_false(zcbor_uint64_pexpect(NULL, &(uint64_t){14}), NULL);
	zassert_false(zcbor_size_pexpect(NULL, &(size_t){15}), NULL);
	zassert_false(zcbor_int8_decode(NULL, &(int8_t){24}), NULL);
	zassert_false(zcbor_int16_decode(NULL, &(int16_t){25}), NULL);
	zassert_false(zcbor_int32_decode(NULL, &(int32_t){5}), NULL);
	zassert_false(zcbor_int64_decode(NULL, &(int64_t){6}), NULL);
	zassert_false(zcbor_uint8_decode(NULL, &(uint8_t){26}), NULL);
	zassert_false(zcbor_uint16_decode(NULL, &(uint16_t){27}), NULL);
	zassert_false(zcbor_uint32_decode(NULL, &(uint32_t){7}), NULL);
	zassert_false(zcbor_uint64_decode(NULL, &(uint64_t){8}), NULL);
	zassert_false(zcbor_size_decode(NULL, &(size_t){9}), NULL);
	zassert_false(zcbor_bstr_expect_lit(NULL, "Hello"), NULL);
	zassert_false(zcbor_tstr_expect_lit(NULL, "World"), NULL);
	zassert_false(zcbor_tag_decode(NULL, &(uint32_t){9}), NULL);
	zassert_false(zcbor_tag_expect(NULL, 10), NULL);
	zassert_false(zcbor_tag_pexpect(NULL, &(uint32_t){11}), NULL);
	zassert_false(zcbor_bool_expect(NULL, true), NULL);
	zassert_false(zcbor_bool_pexpect(NULL, &(bool){false}), NULL);
	zassert_false(zcbor_bool_decode(NULL, &(bool){false}), NULL);
	zassert_false(zcbor_float32_expect(NULL, 10.1), NULL);
	zassert_false(zcbor_float32_pexpect(NULL, &(float){11.2}), NULL);
	zassert_false(zcbor_float32_decode(NULL, &(float){12.3}), NULL);
	zassert_false(zcbor_float64_expect(NULL, 13.4), NULL);
	zassert_false(zcbor_float64_pexpect(NULL, &(double){14.5}), NULL);
	zassert_false(zcbor_float64_decode(NULL, &(double){15.6}), NULL);
	zassert_false(zcbor_nil_expect(NULL, NULL), NULL);
	zassert_false(zcbor_undefined_expect(NULL, NULL), NULL);
	zassert_false(zcbor_bstr_start_decode(NULL, &dummy_string), NULL);
	zassert_false(zcbor_bstr_end_decode(NULL, false), NULL);
	zassert_false(zcbor_list_start_decode(NULL), NULL);
	zassert_false(zcbor_map_start_decode(NULL), NULL);
	zassert_false(zcbor_map_end_decode(NULL, true), NULL);
	zassert_false(zcbor_list_end_decode(NULL, true), NULL);
	zassert_false(zcbor_multi_decode(1, 1, &(size_t){1}, ZCBOR_CAST_FP(zcbor_int32_pexpect), NULL, &(int32_t){14}, 0), NULL);
	zassert_false(zcbor_present_decode(&(bool){true}, ZCBOR_CAST_FP(zcbor_int32_pexpect), NULL, &(int32_t){15}), NULL);
	zassert_false(zcbor_any_skip(NULL, NULL), NULL);


	zassert_false(zcbor_new_backup(NULL, 0), NULL);
	zassert_false(zcbor_process_backup(NULL, 0, 0), NULL);
	zassert_false(zcbor_process_backup_num(NULL, 0, 0, 0), NULL);
	zassert_false(zcbor_union_start_code(NULL), NULL);
	zassert_false(zcbor_union_elem_code(NULL), NULL);
	zassert_false(zcbor_union_end_code(NULL), NULL);
	zcbor_new_state(NULL, 2, dummy_payload, sizeof(dummy_payload), 0, NULL, 0);
	zassert_false(zcbor_entry_function_with_elem_states(dummy_payload, sizeof(dummy_payload), NULL, NULL, NULL, ZCBOR_CAST_FP(zcbor_int32_pexpect), 2, 0, 0), NULL);
	zcbor_update_state(NULL, dummy_payload, sizeof(dummy_payload));
}


ZTEST(zcbor_unit_tests, test_null_params)
{
	uint8_t payload[10];
	ZCBOR_STATE_E(state_e, 0, payload, sizeof(payload), 0);
	struct zcbor_string dummy_string = {0};
	uint8_t hello[] = "Hello";

	zassert_true(zcbor_bstr_encode(state_e, &dummy_string), NULL);
	dummy_string.len = 5;
	zassert_false(zcbor_bstr_encode(state_e, &dummy_string), NULL);
	zassert_equal(ZCBOR_ERR_BAD_ARG, zcbor_peek_error(state_e), NULL);
	dummy_string.value = hello;
	zassert_true(zcbor_bstr_encode(state_e, &dummy_string), NULL);

	uint8_t exp_payload[] = {0x40, 0x45, 0x48, 0x65, 0x6c, 0x6c, 0x6f};
	zassert_equal(sizeof(exp_payload), state_e->payload - payload, NULL);
	zassert_mem_equal(exp_payload, payload, sizeof(exp_payload), NULL);

	/* A NULL value must be rejected before it reaches the byte copying. */
	zassert_false(zcbor_uint_encode(state_e, NULL, sizeof(uint32_t)), NULL);
	zassert_equal(ZCBOR_ERR_BAD_ARG, zcbor_peek_error(state_e), NULL);
	zassert_false(zcbor_int_encode(state_e, NULL, sizeof(int32_t)), NULL);
	zassert_equal(ZCBOR_ERR_BAD_ARG, zcbor_peek_error(state_e), NULL);
	zassert_false(zcbor_float64_encode(state_e, NULL), NULL);
	zassert_equal(ZCBOR_ERR_BAD_ARG, zcbor_peek_error(state_e), NULL);
	zassert_false(zcbor_float32_encode(state_e, NULL), NULL);
	zassert_equal(ZCBOR_ERR_BAD_ARG, zcbor_peek_error(state_e), NULL);
	zassert_false(zcbor_float16_encode(state_e, NULL), NULL);
	zassert_equal(ZCBOR_ERR_BAD_ARG, zcbor_peek_error(state_e), NULL);

	/* None of the failures should have consumed payload. */
	zassert_equal(sizeof(exp_payload), state_e->payload - payload, NULL);
}


/** The string "HelloWorld" is split into 2 fragments, and a number of different
 * functions are called to test that they respond correctly to the situation.
**/
ZTEST(zcbor_unit_tests, test_fragments)
{
	uint8_t payload[100];
	ZCBOR_STATE_E(state_e, 0, payload, sizeof(payload), 0);
	struct zcbor_string output;
	struct zcbor_string_fragment output_frags[2];
	size_t offset;

	zassert_true(zcbor_bstr_put_lit(state_e, "HelloWorld"), NULL);

	ZCBOR_STATE_D(state_d, 0, payload, 8, 1, 0);
	ZCBOR_STATE_D(state_d2, 0, payload, sizeof(payload), 1, 0);

	zassert_false(zcbor_current_string_offset(state_e, &offset), NULL);
	zassert_false(zcbor_current_string_offset(state_d, &offset), NULL);

	zassert_true(zcbor_bstr_decode(state_d2, &output), NULL);
	zassert_false(zcbor_payload_at_end(state_d2), NULL);
	zassert_false(zcbor_bstr_decode(state_d, &output), NULL);
	zassert_false(zcbor_payload_at_end(state_d), NULL);
	zassert_true(zcbor_bstr_fragments_start_decode(state_d), NULL);
	zcbor_str_fragment_decode(state_d, &output_frags[0]);
	zassert_equal_ptr(&payload[1], output_frags[0].fragment.value, NULL);
	zassert_equal(7, output_frags[0].fragment.len, NULL);
	zassert_equal(10, output_frags[0].total_len, "%d != %d\r\n", 10, output_frags[0].total_len);
	zassert_equal(0, output_frags[0].offset, NULL);
	zassert_false(zcbor_is_last_fragment(&output_frags[0]), NULL);

	zassert_true(zcbor_payload_at_end(state_d), NULL);
	zcbor_update_state(state_d, &payload[8], sizeof(payload) - 8);
	zassert_false(zcbor_bstr_fragments_start_decode(state_d), NULL);
	zcbor_str_fragment_decode(state_d, &output_frags[1]);
	zassert_equal_ptr(&payload[8], output_frags[1].fragment.value, NULL);
	zassert_equal(3, output_frags[1].fragment.len, "%d != %d\r\n", 3, output_frags[1].fragment.len);
	zassert_equal(10, output_frags[1].total_len, NULL);
	zassert_equal(7, output_frags[1].offset, NULL);
	zassert_true(zcbor_is_last_fragment(&output_frags[1]), NULL);

	uint8_t spliced[11];
	output.value = spliced;
	output.len = sizeof(spliced);

	zassert_true(zcbor_validate_string_fragments(output_frags, 2), NULL);
	zassert_true(zcbor_splice_string_fragments(output_frags, 2, spliced, &output.len), NULL);

	zassert_equal(10, output.len, NULL);
	zassert_mem_equal(output.value, "HelloWorld", 10, NULL);
}

/** The long string "HelloWorld1HelloWorld2..." is split into 18 fragments.
 *
 * First, zcbor_validate_string_fragments() is checked to be true, then various
 * errors are introduced one by one and zcbor_validate_string_fragments() is
 * checked to be false for all of them in turn.
 */
ZTEST(zcbor_unit_tests, test_validate_fragments)
{
	uint8_t payload[200];

	ZCBOR_STATE_E(state_e, 0, payload, sizeof(payload), 0);
	struct zcbor_string output;
	struct zcbor_string_fragment output_frags[18];
	struct zcbor_string_fragment output_frags_backup;
	size_t remainder;
	size_t offset;

	// Start positive test

	zassert_true(zcbor_bstr_put_lit(state_e,
		"HelloWorld1HelloWorld2HelloWorld3HelloWorld4HelloWorld5HelloWorld6" \
		"HelloWorld7HelloWorld8HelloWorld9HelloWorldAHelloWorldBHelloWorldC" \
		"HelloWorldDHelloWorldEHelloWorldFHelloWorldGHelloWorldHHelloWorldI"),
		NULL);

	uint8_t fragmented_payload[300];
	memcpy(&fragmented_payload[0], &payload[0], 13); // + 2 because of the CBOR header

	for (int i = 1; i < 18; i++) {
		memcpy(&fragmented_payload[15 * i + 2], &payload[11 * i + 2], 11); // + 2 because of the CBOR header, 15 to physically separate fragments
	}

	ZCBOR_STATE_D(state_d, 0, fragmented_payload, 13, 1, 0);
	ZCBOR_STATE_D(state_d2, 0, payload, sizeof(payload), 1, 0);

	zassert_true(zcbor_bstr_decode(state_d2, &output), NULL);
	zassert_true(zcbor_bstr_fragments_start_decode(state_d), NULL);
	zassert_true(zcbor_str_fragment_decode(state_d, &output_frags[0]));

	for (int i = 1; i < 18; i++) {
		zassert_true(zcbor_payload_at_end(state_d), NULL);
		zassert_false(zcbor_is_last_fragment(&output_frags[i - 1]), NULL);
		zcbor_update_state(state_d, &fragmented_payload[15 * i + 2], 11);
		zassert_true(zcbor_str_fragment_decode(state_d, &output_frags[i]));
	}
	zassert_true(zcbor_payload_at_end(state_d), NULL);
	zassert_true(zcbor_is_last_fragment(&output_frags[17]), NULL);
	state_d->payload++; // Move past last fragment to simulate end of payload
	zassert_false(zcbor_current_string_offset(state_d, NULL));
	zassert_false(zcbor_current_string_offset(state_d, &offset));
	zassert_false(zcbor_current_string_remainder(state_d, NULL));
	zassert_false(zcbor_current_string_remainder(state_d, &remainder));

	uint8_t spliced[200];
	size_t out_len = sizeof(spliced);

	zassert_true(zcbor_validate_string_fragments(output_frags, 18), NULL);
	zassert_true(zcbor_splice_string_fragments(output_frags, 18, spliced,
			&out_len), NULL);

	zassert_equal(198, output.len, NULL);
	zassert_mem_equal(output.value,
		"HelloWorld1HelloWorld2HelloWorld3HelloWorld4HelloWorld5HelloWorld6" \
		"HelloWorld7HelloWorld8HelloWorld9HelloWorldAHelloWorldBHelloWorldC" \
		"HelloWorldDHelloWorldEHelloWorldFHelloWorldGHelloWorldHHelloWorldI",
		198, NULL);


	zassert_equal(198, out_len, NULL);
	zassert_mem_equal(spliced,
		"HelloWorld1HelloWorld2HelloWorld3HelloWorld4HelloWorld5HelloWorld6" \
		"HelloWorld7HelloWorld8HelloWorld9HelloWorldAHelloWorldBHelloWorldC" \
		"HelloWorldDHelloWorldEHelloWorldFHelloWorldGHelloWorldHHelloWorldI",
		198, NULL);

	// Start negative tests

	zassert_false(zcbor_validate_string_fragments(output_frags, 17), NULL); // Last fragment missing

	output_frags[2].total_len--;
	zassert_false(zcbor_validate_string_fragments(output_frags, 18), NULL); // Mismatch total_len
	output_frags[2].total_len += 2;
	zassert_false(zcbor_validate_string_fragments(output_frags, 18), NULL); // Mismatch total_len
	output_frags[2].total_len--;

	memcpy(&output_frags_backup, &output_frags[16], sizeof(output_frags[0]));
	memcpy(&output_frags[16], &output_frags[17], sizeof(output_frags[0])); // Move last fragment

	zassert_false(zcbor_validate_string_fragments(output_frags, 17), NULL); // Second-to-last fragment missing
	memcpy(&output_frags[16], &output_frags_backup, sizeof(output_frags[0])); // Restore

	output_frags[6].offset++;
	output_frags[7].offset--;
	zassert_false(zcbor_validate_string_fragments(output_frags, 18), NULL); // Slight error in offset vs. fragment lengths.
	output_frags[6].offset--;
	output_frags[7].offset++;

	for (int i = 0; i < 18; i++) {
		output_frags[i].total_len -= output_frags[17].fragment.len;
	}

	zassert_true(zcbor_validate_string_fragments(output_frags, 17), NULL); // Should work with 17 fragments now.
	zassert_false(zcbor_validate_string_fragments(output_frags, 18), NULL); // Last fragment will extend past total_len.

	for (int i = 0; i < 18; i++) {
		output_frags[i].total_len += (output_frags[17].fragment.len - 1);
	}
	zassert_false(zcbor_validate_string_fragments(output_frags, 18), NULL); // Last fragment will extend past total_len by a single byte.
	output_frags[17].fragment.len--;
	zassert_true(zcbor_validate_string_fragments(output_frags, 18), NULL); // Should work with shorter last fragment
	output_frags[17].fragment.len++; // Restore
	for (int i = 0; i < 18; i++) {
		output_frags[i].total_len += 1; // Restore
	}

	zassert_true(zcbor_validate_string_fragments(output_frags, 18), NULL); // Check that all errors were restored correctly.
}


/** This test creates the following structure, wrapped in a BSTR:
 *
 *  (
 *      42,
 *      "Hello World",
 *      [
 *          true,
 *          nil,
 *      ]
 *  )
 *
 *  This structures is then split in three fragments like so:
 *  1st fragment (8 bytes):
 *  (
 *      42,
 *      "Hell
 *
 *  2nd fragment (8 bytes):
 *           o World",
 *      [
 *
 *  3rd fragment (3 bytes or 2 bytes if ZCBOR_CANONICAL is defined):
 *          true,
 *          nil,
 *      ]
 *  )
 *
 *  This means that the TSTR has to be handled using fragments (tstr_frags)
 *  as well.
 */
ZTEST(zcbor_unit_tests, test_bstr_cbor_fragments)
{
	uint8_t payload[100];
	uint8_t payload_sections[4][20];
	ZCBOR_STATE_E(state_e, 2, payload, sizeof(payload), 0);
	struct zcbor_string output;
	struct zcbor_string_fragment tstr_frags[2];
	size_t remainder;

	zassert_true(zcbor_bstr_start_encode(state_e), NULL); // 1 B
	zassert_true(zcbor_uint32_put(state_e, 42), NULL); // 2 B
	zassert_true(zcbor_tstr_put_lit(state_e, "Hello World"), NULL); // 12 B
	zassert_true(zcbor_list_start_encode(state_e, 2), NULL); // 1 B
	zassert_true(zcbor_bool_put(state_e, true), NULL); // 1 B
	zassert_true(zcbor_nil_put(state_e, NULL), NULL); // 1 B
	zassert_true(zcbor_list_end_encode(state_e, 2), NULL); // 1 B
	zassert_true(zcbor_bstr_end_encode(state_e, NULL), NULL); // 0 B

	memcpy(payload_sections[0], &payload[0], 8);
	memcpy(payload_sections[1], &payload[8], 8);
	memcpy(payload_sections[2], &payload[16], 8);

	ZCBOR_STATE_D(state_d, 2, payload_sections[0], 8, 1, 0);
	ZCBOR_STATE_D(state_d2, 0, payload, sizeof(payload), 1, 0);

#ifdef ZCBOR_CANONICAL
	#define EXP_TOTAL_LEN 17
#else
	#define EXP_TOTAL_LEN 18
#endif

	zassert_true(zcbor_bstr_decode(state_d2, &output), NULL);
	zassert_false(zcbor_bstr_start_decode(state_d, &output), NULL);
	zassert_true(zcbor_cbor_bstr_fragments_start_decode(state_d), NULL);
	zassert_equal_ptr(&payload_sections[0][0], state_d->constant_state->curr_payload_section, "%p, %p\n",&payload_sections[0][0], state_d->constant_state->curr_payload_section);
	zassert_equal(EXP_TOTAL_LEN, state_d->str_total_len_cbor, "%d != %d\r\n", EXP_TOTAL_LEN, state_d->str_total_len_cbor);
	zassert_equal(-1, state_d->frag_offset_cbor, NULL);
	zassert_true(zcbor_uint32_expect(state_d, 42), NULL);
	zassert_false(zcbor_tstr_expect_lit(state_d, "Hello World"), NULL);
	zassert_true(zcbor_tstr_fragments_start_decode(state_d), NULL);
	zassert_true(zcbor_str_fragment_decode(state_d, &tstr_frags[0]));
	zassert_equal_ptr(&payload_sections[0][4], tstr_frags[0].fragment.value, NULL);
	zassert_equal(4, tstr_frags[0].fragment.len, NULL);
	zassert_equal(11, tstr_frags[0].total_len, NULL);
	zassert_equal(0, tstr_frags[0].offset, NULL);

	zassert_true(zcbor_payload_at_end(state_d), NULL);
	zcbor_update_state(state_d, payload_sections[1], 8);
	zassert_true(zcbor_str_fragment_decode(state_d, &tstr_frags[1]));
	zassert_true(zcbor_str_fragments_end_decode(state_d));
	zassert_false(zcbor_cbor_bstr_fragments_start_decode(state_d), NULL);
	zassert_equal_ptr(&payload_sections[1][0], tstr_frags[1].fragment.value, NULL);
	zassert_equal(7, tstr_frags[1].fragment.len, "%d != %d\r\n", 7, tstr_frags[1].fragment.len);
	zassert_equal(11, tstr_frags[1].total_len, "%d != %d\r\n", 11, tstr_frags[1].total_len);
	zassert_equal(EXP_TOTAL_LEN, state_d->str_total_len_cbor, "%d != %d\r\n", EXP_TOTAL_LEN, state_d->str_total_len_cbor);
	zassert_equal(7, state_d->frag_offset_cbor, NULL);
	zassert_true(zcbor_is_last_fragment(&tstr_frags[1]), NULL);
	bool ret = zcbor_list_start_decode(state_d);
	zassert_true(ret, "%s\n", zcbor_error_str(zcbor_peek_error(state_d)));

	zassert_true(zcbor_current_string_remainder(state_d, &remainder));
	zassert_equal(EXP_TOTAL_LEN - 15, remainder); // 15 because 16 minus header.
	zassert_true(zcbor_payload_at_end(state_d), NULL);
	zcbor_update_state(state_d, payload_sections[2], 8);
	zassert_false(zcbor_bstr_fragments_start_decode(state_d), NULL);
	zassert_equal_ptr(&payload_sections[2][0], state_d->constant_state->curr_payload_section, NULL);
	zassert_equal(EXP_TOTAL_LEN, state_d->str_total_len_cbor, NULL);
	zassert_equal(15, state_d->frag_offset_cbor, NULL);
	zassert_true(zcbor_bool_expect(state_d, true), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	zassert_true(zcbor_list_end_decode(state_d, true), NULL);
	zassert_true(zcbor_payload_at_end(state_d), NULL);

	// "Erroneous" update_state.
	zcbor_update_state(state_d, payload_sections[3], 8);
	zassert_true(zcbor_payload_at_end(state_d), NULL);
	zassert_equal_ptr(&payload_sections[3][0], state_d->constant_state->curr_payload_section, NULL);
	zassert_equal(EXP_TOTAL_LEN, state_d->str_total_len_cbor, NULL);
	zassert_equal(EXP_TOTAL_LEN, state_d->frag_offset_cbor, NULL);

	uint8_t spliced[19];
	output.value = spliced;
	output.len = sizeof(spliced);

	zassert_true(zcbor_validate_string_fragments(tstr_frags, 2), NULL);
	zassert_true(zcbor_splice_string_fragments(tstr_frags, 2, spliced, &output.len), NULL);

	zassert_equal(11, output.len, NULL);
	zassert_mem_equal(output.value, &payload[4], 11, NULL);
}

#define zassert_error(err, state) zassert_equal(err, zcbor_peek_error(state), #err " != %s\n", zcbor_error_str(zcbor_peek_error(state)))

#define test_offset_and_remainder(state, total_len) \
do { \
	size_t offset_, remainder_; \
	zassert_true(zcbor_current_string_offset(state, &offset_) && zcbor_current_string_remainder(state, &remainder_)); \
	zassert_equal(total_len, offset_ + remainder_); \
} while (0)


ZTEST(zcbor_unit_tests, test_nested_fragments)
{
	uint8_t lorem[] = "Lorem ipsum dolor sit amet";
	struct zcbor_string lorem_str = {.value = lorem, .len = sizeof(lorem) - 1};
	struct zcbor_string lorem_str_exp = {.value = lorem, .len = sizeof(lorem) - 1};
	struct zcbor_string_fragment output_frags[3];
	struct zcbor_string_fragment output_frags_bstr;
	uint8_t output_string[30];
	size_t output_str_len = sizeof(output_string);
	struct zcbor_string res_str;
	size_t enc_len;
	uint8_t payload_frag1[4];
	int dummy_sep1; // To separate payload fragments
	uint8_t payload_frag2[18];
	int dummy_sep2; // To separate payload fragments
	uint8_t payload_frag3[10];
	int dummy_sep3; // To separate payload fragments
	uint8_t payload_frag4[25];

	uint8_t payload1[100];
	size_t remainder;

	(void)dummy_sep1;
	(void)dummy_sep2;
	(void)dummy_sep3;

	ZCBOR_STATE_E(state_e, 4, payload_frag1, sizeof(payload_frag1), 0);

	ZCBOR_STATE_D(state_d, 4, payload_frag1, sizeof(payload_frag1) - 1, 1, 0);
	ZCBOR_STATE_D(state_d2, 4, payload1, sizeof(payload1), 1, 0);

	/* Start encode tests, negative tests are indented. */

	/* payload_frag1 */
	zassert_true(zcbor_list_start_encode(state_e, 2));
		zassert_false(zcbor_str_fragments_end_encode(state_e));
		zassert_error(ZCBOR_ERR_NOT_IN_FRAGMENT, state_e);
		zassert_false(zcbor_str_fragment_decode(state_d, &output_frags_bstr));
		zassert_equal(ZCBOR_ERR_NOT_IN_FRAGMENT, zcbor_peek_error(state_d));
	zassert_true(zcbor_uint32_put(state_e, 42));
		zassert_false(zcbor_cbor_bstr_fragments_start_encode(state_e, 38));
		zassert_error(ZCBOR_ERR_NO_PAYLOAD, state_e);
	zcbor_update_state(state_e, payload_frag2, sizeof(payload_frag2)); /* Abandon 1 byte of the fragment. */

#ifdef ZCBOR_CANONICAL
	#define LEN_OFFS 0
#else
	#define LEN_OFFS 1
#endif

	/* payload_frag2 */
		zassert_false(zcbor_current_string_remainder(state_e, &remainder));
		zassert_error(ZCBOR_ERR_NOT_IN_FRAGMENT, state_e);
	zassert_true(zcbor_cbor_bstr_fragments_start_encode(state_e, 37 + LEN_OFFS));
	test_offset_and_remainder(state_e, state_e->str_total_len_cbor);
	zassert_true(zcbor_uint32_put(state_e, 43));
	zassert_true(zcbor_list_start_encode(state_e, 2));
	zassert_true(zcbor_uint32_put(state_e, 44));
		zassert_false(zcbor_cbor_bstr_fragments_start_encode(state_e, lorem_str.len + 5 + LEN_OFFS));
		zassert_error(ZCBOR_ERR_TOO_LARGE_FOR_STRING, state_e);
	const size_t inner_cbor_str_len = lorem_str.len + 4;
	zassert_true(zcbor_cbor_bstr_fragments_start_encode(state_e, inner_cbor_str_len));
		zassert_false(zcbor_str_fragment_encode(state_e, &lorem_str, &enc_len));
		zassert_error(ZCBOR_ERR_NOT_IN_FRAGMENT, state_e);
	zassert_true(zcbor_uint32_put(state_e, 45));
		zassert_false(zcbor_tstr_fragments_start_encode(state_e, lorem_str.len + 1));
		zassert_error(ZCBOR_ERR_TOO_LARGE_FOR_STRING, state_e);
	bool ret = zcbor_tstr_fragments_start_encode(state_e, lorem_str.len);
	zassert_true(ret, "err %s\n", zcbor_error_str(zcbor_peek_error(state_e)));

	ret = zcbor_current_string_remainder(state_e, &remainder);
	zassert_true(ret, "err %s\n", zcbor_error_str(zcbor_peek_error(state_e)));
	zassert_equal(lorem_str.len, remainder);

		zassert_false(zcbor_uint32_put(state_e, 46));
		zassert_error(ZCBOR_ERR_INSIDE_STRING, state_e);
		zassert_false(zcbor_tstr_fragments_start_encode(state_e, 1));
		zassert_error(ZCBOR_ERR_INSIDE_STRING, state_e);
	ret = zcbor_str_fragment_encode(state_e, &lorem_str, &enc_len);
	test_offset_and_remainder(state_e, state_e->str_total_len);
	zassert_true(ret, "err %s\n", zcbor_error_str(zcbor_peek_error(state_e)));
	zassert_equal(sizeof(payload_frag2) - 13, enc_len);
		zassert_false(zcbor_str_fragment_encode(state_e, &lorem_str, NULL));
		zassert_error(ZCBOR_ERR_NO_PAYLOAD, state_e);
	zcbor_update_state(state_e, payload_frag3, sizeof(payload_frag3));

	/* payload_frag3 */
	lorem_str.value += enc_len;
	lorem_str.len -= enc_len;
	zassert_true(zcbor_str_fragment_encode(state_e, &lorem_str, &enc_len));
	zassert_equal(sizeof(payload_frag3), enc_len);
	zcbor_update_state(state_e, payload_frag4, sizeof(payload_frag4));

	/* payload_frag4 */
	lorem_str.value += enc_len;
	lorem_str.len -= enc_len;
	zassert_true(zcbor_str_fragment_encode(state_e, &lorem_str, &enc_len));
	zassert_equal(lorem_str.len, enc_len, "%d != %d\n", lorem_str.len, enc_len);
	test_offset_and_remainder(state_e, state_e->str_total_len);
	state_e->payload++; /* Create bad state. */
		zassert_false(zcbor_current_string_remainder(state_e, &remainder));
		zassert_error(ZCBOR_ERR_BAD_STATE, state_e);
	state_e->payload--; /* Revert bad state. */
	zassert_true(zcbor_str_fragments_end_encode(state_e));
	test_offset_and_remainder(state_e, state_e->str_total_len_cbor);
	zassert_true(zcbor_str_fragments_end_encode(state_e));
	ret = zcbor_list_end_encode(state_e, 2);
	zassert_true(ret, "err %s\n", zcbor_error_str(zcbor_peek_error(state_e)));

	ret = zcbor_str_fragments_end_encode(state_e);
	zassert_true(ret, "err %s\n", zcbor_error_str(zcbor_peek_error(state_e)));

		zassert_false(zcbor_str_fragments_end_encode(state_e));
		zassert_error(ZCBOR_ERR_NOT_IN_FRAGMENT, state_e);
	zassert_true(zcbor_list_end_encode(state_e, 2));
	size_t offs = 0;
	memcpy(payload1, payload_frag1, sizeof(payload_frag1) - 1);
	offs += sizeof(payload_frag1) - 1; /* 1 abandoned byte */
	memcpy(&payload1[offs], payload_frag2, sizeof(payload_frag2));
	offs += sizeof(payload_frag2);
	memcpy(&payload1[offs], payload_frag3, sizeof(payload_frag3));
	offs += sizeof(payload_frag3);
	memcpy(&payload1[offs], payload_frag4, sizeof(payload_frag4));

	/* Check */
	zassert_true(zcbor_list_start_decode(state_d2));
	zassert_true(zcbor_uint32_expect(state_d2, 42));
	zassert_true(zcbor_bstr_start_decode(state_d2, &res_str));
	zassert_true(zcbor_uint32_expect(state_d2, 43));
	zassert_true(zcbor_list_start_decode(state_d2));
	zassert_true(zcbor_uint32_expect(state_d2, 44));
	zassert_true(zcbor_bstr_start_decode(state_d2, &res_str));
	zassert_true(zcbor_uint32_expect(state_d2, 45));
	zassert_true(zcbor_tstr_expect(state_d2, &lorem_str_exp));
	zassert_true(zcbor_bstr_end_decode(state_d2, false));
	zassert_true(zcbor_list_end_decode(state_d2, true));
	zassert_true(zcbor_bstr_end_decode(state_d2, false));
	zassert_true(zcbor_list_end_decode(state_d2, true));

	/* Start decode tests, negative tests are indented. */

	/* payload_frag1 */
	zassert_true(zcbor_list_start_decode(state_d));
		zassert_false(zcbor_str_fragments_end_decode(state_d));
		zassert_error(ZCBOR_ERR_NOT_IN_FRAGMENT, state_d);
	zassert_true(zcbor_uint32_expect(state_d, 42));
		zassert_false(zcbor_cbor_bstr_fragments_start_decode(state_d));
		zassert_error(ZCBOR_ERR_NO_PAYLOAD, state_d);
	zcbor_update_state(state_d, payload_frag2, sizeof(payload_frag2));

	/* payload_frag2 */
	zassert_true(zcbor_cbor_bstr_fragments_start_decode(state_d));
	test_offset_and_remainder(state_d, state_d->str_total_len_cbor);
	zassert_true(zcbor_uint32_expect(state_d, 43));
	zassert_true(zcbor_list_start_decode(state_d));
		zassert_false(zcbor_cbor_bstr_fragments_start_decode(state_d));
		zassert_error(ZCBOR_ERR_WRONG_TYPE, state_d);
	zassert_true(zcbor_uint32_expect(state_d, 44));
		state_d->payload_mut[1] += 2; /* induce an error */
		zassert_false(zcbor_cbor_bstr_fragments_start_decode(state_d));
		zassert_error(ZCBOR_ERR_TOO_LARGE_FOR_STRING, state_d);
		state_d->payload_mut[1] -= 2;
	zassert_true(zcbor_str_fragment_decode(state_d, &output_frags_bstr));
	zassert_equal_ptr(output_frags_bstr.fragment.value, &payload_frag2[2]);
	zassert_equal_ptr(output_frags_bstr.fragment.len, state_d->payload - &payload_frag2[2]);
	zassert_equal(37 + LEN_OFFS, output_frags_bstr.total_len);
	zassert_equal(0, output_frags_bstr.offset);
	zassert_true(zcbor_cbor_bstr_fragments_start_decode(state_d));
	ret = zcbor_current_string_remainder(state_d, &remainder);
	zassert_true(ret, "err %s\n", zcbor_error_str(zcbor_peek_error(state_d)));
	zassert_equal(inner_cbor_str_len, remainder, "%d != %d\r\n", inner_cbor_str_len, remainder);
	zassert_true(zcbor_uint32_expect(state_d, 45));
	zassert_true(zcbor_str_fragment_decode(state_d, &output_frags_bstr));
	zassert_equal_ptr(output_frags_bstr.fragment.value, state_d->payload - 2);
	zassert_equal_ptr(output_frags_bstr.fragment.len, 2);
	zassert_equal(sizeof(lorem) + 3, output_frags_bstr.total_len, "%d != %d\r\n", sizeof(lorem) + 3, output_frags_bstr.total_len);
	zassert_equal(0, output_frags_bstr.offset);
	zassert_true(zcbor_tstr_fragments_start_decode(state_d));
	test_offset_and_remainder(state_d, state_d->str_total_len);
		zassert_false(zcbor_uint32_expect(state_d, 46));
		zassert_error(ZCBOR_ERR_INSIDE_STRING, state_d);
		zassert_false(zcbor_tstr_fragments_start_decode(state_d));
		zassert_error(ZCBOR_ERR_INSIDE_STRING, state_d);
	zassert_true(zcbor_str_fragment_decode(state_d, &output_frags[0]));
		zassert_false(zcbor_str_fragment_decode(state_d, &output_frags[1]));
		zassert_error(ZCBOR_ERR_NO_PAYLOAD, state_d);
	test_offset_and_remainder(state_d, state_d->str_total_len);
	zcbor_update_state(state_d, payload_frag3, sizeof(payload_frag3));

	/* payload_frag3 */
	zassert_true(zcbor_str_fragment_decode(state_d, &output_frags[1]));
	zcbor_update_state(state_d, payload_frag4, sizeof(payload_frag4));

	/* payload_frag4 */
	zassert_true(zcbor_str_fragment_decode(state_d, &output_frags[2]));
	zassert_true(zcbor_validate_string_fragments(output_frags, 3));
	zassert_true(zcbor_splice_string_fragments(output_frags, 3, output_string, &output_str_len));
	zassert_mem_equal(output_string, lorem, sizeof(lorem) - 1);
	zassert_true(zcbor_str_fragments_end_decode(state_d));
	zassert_true(zcbor_str_fragments_end_decode(state_d));
		zassert_false(zcbor_str_fragment_decode(state_d, &output_frags_bstr));
	zassert_true(zcbor_list_end_decode(state_d, true), NULL);
	test_offset_and_remainder(state_d, state_d->str_total_len_cbor);
	zassert_true(zcbor_str_fragments_end_decode(state_d));
}


/** A fragment with a NULL value must be treated like zcbor_bstr_encode() treats a
 * string with a NULL value: accepted (and ignored) when empty, rejected otherwise.
 * Neither case may reach memcpy() with a NULL source.
**/
ZTEST(zcbor_unit_tests, test_fragment_encode_null_value)
{
	uint8_t payload[64];
	ZCBOR_STATE_E(state_e, 2, payload, sizeof(payload), 0);
	struct zcbor_string fragment = {.value = NULL, .len = 0};
	size_t enc_len = SIZE_MAX;

	zassert_true(zcbor_bstr_fragments_start_encode(state_e, 5), NULL);

	zassert_true(zcbor_str_fragment_encode(state_e, &fragment, &enc_len), NULL);
	zassert_equal(0, enc_len, NULL);
	zassert_error(ZCBOR_SUCCESS, state_e);

	fragment.len = 5;
	zassert_false(zcbor_str_fragment_encode(state_e, &fragment, &enc_len), NULL);
	zassert_error(ZCBOR_ERR_BAD_ARG, state_e);
	zassert_equal(0, enc_len, NULL);

	/* The string can still be encoded after the rejected fragment. */
	fragment.value = (const uint8_t *)"Hello";
	zassert_true(zcbor_str_fragment_encode(state_e, &fragment, &enc_len), NULL);
	zassert_equal(5, enc_len, NULL);
	zassert_true(zcbor_str_fragments_end_encode(state_e), NULL);

	uint8_t exp_payload[] = {0x45, 0x48, 0x65, 0x6c, 0x6c, 0x6f};

	zassert_equal(sizeof(exp_payload), state_e->payload - payload, NULL);
	zassert_mem_equal(exp_payload, payload, sizeof(exp_payload), NULL);
}


/** Same contract as above, for the splicing of already-decoded fragments. **/
ZTEST(zcbor_unit_tests, test_splice_fragments_null_value)
{
	uint8_t result[16];
	size_t result_len = sizeof(result);
	struct zcbor_string_fragment fragments[] = {
		{.fragment = {.value = NULL, .len = 0}, .offset = 0, .total_len = 5},
		{.fragment = {.value = (const uint8_t *)"Hello", .len = 5}, .offset = 0,
			.total_len = 5},
	};

	zassert_true(zcbor_validate_string_fragments(fragments, 2), NULL);
	zassert_true(zcbor_splice_string_fragments(fragments, 2, result, &result_len), NULL);
	zassert_equal(5, result_len, NULL);
	zassert_mem_equal("Hello", result, 5, NULL);

	fragments[0].fragment.len = 1;
	result_len = sizeof(result);
	zassert_false(zcbor_validate_string_fragments(fragments, 2), NULL);
	zassert_false(zcbor_splice_string_fragments(fragments, 2, result, &result_len), NULL);
}


ZTEST(zcbor_unit_tests, test_canonical_list)
{
#ifndef ZCBOR_CANONICAL
	printf("Skip on non-canonical builds.\n");
#else
	uint8_t payload1[100];
	uint8_t payload2[100];
	uint8_t exp_payload[] = {0x8A, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	ZCBOR_STATE_E(state_e1, 1, payload1, sizeof(payload1), 0);
	ZCBOR_STATE_E(state_e2, 1, payload2, sizeof(payload2), 0);

	zassert_true(zcbor_list_start_encode(state_e1, 10), "%d\r\n", state_e1->constant_state->error);
	for (int i = 0; i < 30; i++) {
		zassert_true(zcbor_uint32_put(state_e1, i), NULL);
	}

	zassert_true(zcbor_list_start_encode(state_e2, 1000), NULL);
	for (int i = 0; i < 10; i++) {
		zassert_true(zcbor_uint32_put(state_e2, i), NULL);
	}
	zassert_true(zcbor_list_end_encode(state_e2, 1000), NULL);
	zassert_equal(sizeof(exp_payload), state_e2->payload - payload2, NULL);
	zassert_mem_equal(exp_payload, payload2, sizeof(exp_payload), NULL);
#endif
}


ZTEST(zcbor_unit_tests, test_int)
{
	uint8_t payload1[100];
	ZCBOR_STATE_E(state_e, 1, payload1, sizeof(payload1), 0);
	ZCBOR_STATE_D(state_d, 1, payload1, sizeof(payload1), 16, 0);

	/* Encode all numbers */
	/* Arbitrary positive numbers in each size */
	int8_t int8 = 12;
	int16_t int16 = 1234;
	int32_t int32 = 12345678;
	int64_t int64 = 1234567812345678;

	zassert_true(zcbor_int_encode(state_e, &int8, sizeof(int8)), NULL);
	zassert_true(zcbor_int_encode(state_e, &int16, sizeof(int16)), NULL);
	zassert_true(zcbor_int_encode(state_e, &int32, sizeof(int32)), NULL);
	zassert_true(zcbor_int_encode(state_e, &int64, sizeof(int64)), NULL);

	/* Arbitrary negative numbers in each size */
	int8 = -12;
	int16 = -1234;
	int32 = -12345678;
	int64 = -1234567812345678;

	zassert_true(zcbor_int_encode(state_e, &int8, sizeof(int8)), NULL);
	zassert_true(zcbor_int_encode(state_e, &int16, sizeof(int16)), NULL);
	zassert_true(zcbor_int_encode(state_e, &int32, sizeof(int32)), NULL);
	zassert_true(zcbor_int_encode(state_e, &int64, sizeof(int64)), NULL);

	/* Check against overflow (negative). */
	zassert_true(zcbor_int32_put(state_e, INT8_MIN - 1), NULL);
	zassert_true(zcbor_int32_put(state_e, INT16_MIN - 1), NULL);
	zassert_true(zcbor_int64_put(state_e, (int64_t)INT32_MIN - 1), NULL);
	/* Check absolute minimum number. */
	zassert_true(zcbor_int64_put(state_e, INT64_MIN), NULL);

	/* Check against overflow (positive). */
	zassert_true(zcbor_int32_put(state_e, INT8_MAX + 1), NULL);
	zassert_true(zcbor_int32_put(state_e, INT16_MAX + 1), NULL);
	zassert_true(zcbor_int64_put(state_e, (int64_t)INT32_MAX + 1), NULL);
	/* Check absolute maximum number. */
	zassert_true(zcbor_int64_put(state_e, INT64_MAX), NULL);

	/* Decode all numbers */
	/* Arbitrary positive numbers in each size */
	zassert_true(zcbor_int_decode(state_d, &int8, sizeof(int8)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int16, sizeof(int8)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int16, sizeof(int16)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int32, sizeof(int8)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int32, sizeof(int16)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int32, sizeof(int32)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int8)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int16)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int32)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int64, sizeof(int64)), NULL);

	zassert_equal(int8, 12, "%d\r\n", int8);
	zassert_equal(int16, 1234, "%d\r\n", int16);
	zassert_equal(int32, 12345678, "%d\r\n", int32);
	zassert_equal(int64, 1234567812345678, "%lld\r\n", int64);

	/* Arbitrary negative numbers in each size */
	zassert_true(zcbor_int_decode(state_d, &int8, sizeof(int8)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int16, sizeof(int8)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int16, sizeof(int16)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int32, sizeof(int8)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int32, sizeof(int16)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int32, sizeof(int32)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int8)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int16)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int32)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int64, sizeof(int64)), NULL);

	zassert_equal(int8, -12, NULL);
	zassert_equal(int16, -1234, NULL);
	zassert_equal(int32, -12345678, NULL);
	zassert_equal(int64, -1234567812345678, NULL);

	/* Check against overflow (negative). */
	zassert_false(zcbor_int_decode(state_d, &int16, sizeof(int8)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int16, sizeof(int16)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int32, sizeof(int8)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int32, sizeof(int16)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int32, sizeof(int32)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int8)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int16)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int32)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int64, sizeof(int64)), NULL);

	zassert_equal(int16, INT8_MIN - 1, NULL);
	zassert_equal(int32, INT16_MIN - 1, NULL);
	zassert_equal(int64, (int64_t)INT32_MIN - 1, NULL);

	/* Check absolute minimum number. */
	zassert_true(zcbor_int_decode(state_d, &int64, sizeof(int64)), NULL);

	zassert_equal(int64, INT64_MIN, NULL);

	/* Check against overflow (positive). */
	zassert_false(zcbor_int_decode(state_d, &int16, sizeof(int8)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int16, sizeof(int16)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int32, sizeof(int8)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int32, sizeof(int16)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int32, sizeof(int32)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int8)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int16)), NULL);
	zassert_false(zcbor_int_decode(state_d, &int64, sizeof(int32)), NULL);
	zassert_true(zcbor_int_decode(state_d, &int64, sizeof(int64)), NULL);

	zassert_equal(int16, INT8_MAX + 1, NULL);
	zassert_equal(int32, INT16_MAX + 1, NULL);
	zassert_equal(int64, (int64_t)INT32_MAX + 1, NULL);

	/* Check absolute maximum number. */
	zassert_true(zcbor_int_decode(state_d, &int64, sizeof(int64)), NULL);

	zassert_equal(int64, INT64_MAX, NULL);
}


ZTEST(zcbor_unit_tests, test_uint)
{
	uint8_t payload1[100];
	ZCBOR_STATE_E(state_e, 1, payload1, sizeof(payload1), 0);
	ZCBOR_STATE_D(state_d, 1, payload1, sizeof(payload1), 16, 0);

	/* Encode all numbers */
	/* Arbitrary positive numbers in each size */
	uint8_t uint8 = 12;
	uint16_t uint16 = 1234;
	uint32_t uint32 = 12345678;
	uint64_t uint64 = 1234567812345678;

	zassert_true(zcbor_uint_encode(state_e, &uint8, sizeof(uint8)), NULL);
	zassert_true(zcbor_uint_encode(state_e, &uint16, sizeof(uint16)), NULL);
	zassert_true(zcbor_uint_encode(state_e, &uint32, sizeof(uint32)), NULL);
	zassert_true(zcbor_uint_encode(state_e, &uint64, sizeof(uint64)), NULL);

	/* Check absolute maximum number. */
	zassert_true(zcbor_uint64_put(state_e, UINT64_MAX), NULL);

	/* Decode all numbers */
	/* Arbitrary positive numbers in each size */
	zassert_true(zcbor_uint_decode(state_d, &uint8, sizeof(uint8)), NULL);
	zassert_false(zcbor_uint_decode(state_d, &uint16, sizeof(uint8)), NULL);
	zassert_true(zcbor_uint_decode(state_d, &uint16, sizeof(uint16)), NULL);
	zassert_false(zcbor_uint_decode(state_d, &uint32, sizeof(uint8)), NULL);
	zassert_false(zcbor_uint_decode(state_d, &uint32, sizeof(uint16)), NULL);
	zassert_true(zcbor_uint_decode(state_d, &uint32, sizeof(uint32)), NULL);
	zassert_false(zcbor_uint_decode(state_d, &uint64, sizeof(uint8)), NULL);
	zassert_false(zcbor_uint_decode(state_d, &uint64, sizeof(uint16)), NULL);
	zassert_false(zcbor_uint_decode(state_d, &uint64, sizeof(uint32)), NULL);
	zassert_true(zcbor_uint_decode(state_d, &uint64, sizeof(uint64)), NULL);

	zassert_equal(uint8, 12, "%d\r\n", uint8);
	zassert_equal(uint16, 1234, "%d\r\n", uint16);
	zassert_equal(uint32, 12345678, "%d\r\n", uint32);
	zassert_equal(uint64, 1234567812345678, "%llu\r\n", uint64);

	/* Check absolute maximum number. */
	zassert_true(zcbor_uint_decode(state_d, &uint64, sizeof(uint64)), NULL);

	zassert_equal(uint64, UINT64_MAX, NULL);
}


/** This tests a regression in big-endian encoding, where small numbers (like 0)
  * where handled incorrectly (1-off), because of an error in get_result().
  */
ZTEST(zcbor_unit_tests, test_encode_int_0)
{
	uint8_t payload1[100];
	uint32_t values_32[2] = {0, UINT32_MAX};
	uint64_t values_64[2] = {0, UINT64_MAX};
	ZCBOR_STATE_E(state_e, 1, payload1, sizeof(payload1), 0);
	ZCBOR_STATE_D(state_d, 1, payload1, sizeof(payload1), 16, 0);

	zassert_true(zcbor_uint32_put(state_e, values_32[0]));
	zassert_true(zcbor_uint64_put(state_e, values_64[0]));
	zassert_true(zcbor_uint32_expect(state_d, 0));
	zassert_true(zcbor_uint64_expect(state_d, 0));
}


bool zcbor_simple_put(zcbor_state_t *state, uint8_t input);
bool zcbor_simple_encode(zcbor_state_t *state, uint8_t *input);
bool zcbor_simple_expect(zcbor_state_t *state, uint8_t expected);
bool zcbor_simple_pexpect(zcbor_state_t *state, uint8_t *expected);
bool zcbor_simple_decode(zcbor_state_t *state, uint8_t *result);

ZTEST(zcbor_unit_tests, test_simple)
{
	uint8_t payload1[100];
	ZCBOR_STATE_E(state_e, 1, payload1, sizeof(payload1), 0);
	ZCBOR_STATE_D(state_d, 1, payload1, sizeof(payload1), 16, 0);
	uint8_t simple1 = 0;
	uint8_t simple2 = 2;
	uint8_t simple3 = 0;

	zassert_true(zcbor_simple_encode(state_e, &simple1), NULL);
	zassert_true(zcbor_simple_put(state_e, 14), NULL);
	zassert_true(zcbor_simple_put(state_e, 22), NULL);
	zassert_false(zcbor_simple_put(state_e, 24), NULL);
	zassert_equal(ZCBOR_ERR_INVALID_VALUE_ENCODING, zcbor_peek_error(state_e), NULL);
	zassert_true(zcbor_simple_put(state_e, 32), NULL);
	zassert_true(zcbor_simple_put(state_e, 255), NULL);
	state_e->payload_mut[0] = 0xF8;
	state_e->payload_mut[1] = 24; // Invalid value
	state_e->payload_end += 2;

	zassert_true(zcbor_simple_decode(state_d, &simple2), NULL);
	zassert_true(zcbor_simple_expect(state_d, 14), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	zassert_false(zcbor_undefined_expect(state_d, NULL), NULL);
	zassert_true(zcbor_simple_expect(state_d, 32), NULL);
	zassert_false(zcbor_simple_expect(state_d, 254), NULL);
	zassert_true(zcbor_simple_decode(state_d, &simple1), NULL);
	zassert_equal(0, simple2, NULL);
	zassert_equal(255, simple1, NULL);

	zassert_false(zcbor_simple_expect(state_d, 24), NULL);
	zassert_equal(ZCBOR_ERR_INVALID_VALUE_ENCODING, zcbor_peek_error(state_d), "%s\n", zcbor_error_str(zcbor_peek_error(state_d)));
	zassert_false(zcbor_simple_decode(state_d, &simple3), NULL);
	zassert_equal(ZCBOR_ERR_INVALID_VALUE_ENCODING, zcbor_peek_error(state_d), "%s\n", zcbor_error_str(zcbor_peek_error(state_d)));
}


ZTEST(zcbor_unit_tests, test_header_len)
{
	zassert_equal(1, zcbor_header_len(0), NULL);
	zassert_equal(1, zcbor_header_len_ptr((uint8_t*)&(uint8_t){0}, 1), NULL);
	zassert_equal(1, zcbor_header_len(23), NULL);
	zassert_equal(1, zcbor_header_len_ptr((uint8_t*)&(uint8_t){23}, 1), NULL);
	zassert_equal(2, zcbor_header_len(24), NULL);
	zassert_equal(2, zcbor_header_len_ptr((uint8_t*)&(uint8_t){24}, 1), NULL);
	zassert_equal(2, zcbor_header_len(0xFF), NULL);
	zassert_equal(2, zcbor_header_len_ptr((uint8_t*)&(uint8_t){0xFF}, 1), NULL);
	zassert_equal(3, zcbor_header_len(0x100), NULL);
	zassert_equal(3, zcbor_header_len_ptr((uint8_t*)&(uint16_t){0x100}, 2), NULL);
	zassert_equal(3, zcbor_header_len(0xFFFF), NULL);
	zassert_equal(3, zcbor_header_len_ptr((uint8_t*)&(uint16_t){0xFFFF}, 2), NULL);
	zassert_equal(5, zcbor_header_len(0x10000), NULL);
	zassert_equal(5, zcbor_header_len_ptr((uint8_t*)&(uint32_t){0x10000}, 4), NULL);
	zassert_equal(5, zcbor_header_len(0xFFFFFFFF), NULL);
	zassert_equal(5, zcbor_header_len_ptr((uint8_t*)&(uint32_t){0xFFFFFFFF}, 4), NULL);
#if SIZE_MAX >= 0x100000000ULL
	zassert_equal(9, zcbor_header_len(0x100000000), NULL);
	zassert_equal(9, zcbor_header_len_ptr((uint8_t*)&(uint64_t){0x100000000}, 8), NULL);
	zassert_equal(9, zcbor_header_len(0xFFFFFFFFFFFFFFFF), NULL);
	zassert_equal(9, zcbor_header_len_ptr((uint8_t*)&(uint64_t){0xFFFFFFFFFFFFFFFF}, 8), NULL);
#endif

	/* A 0-length value is all zeroes, i.e. the value 0. */
	zassert_equal(1, zcbor_header_len_ptr((uint8_t*)&(uint8_t){0xFF}, 0), NULL);

	/* Invalid arguments give 0. A NULL value must not be handed to memcpy(). */
	zassert_equal(0, zcbor_header_len_ptr(NULL, 0), NULL);
	zassert_equal(0, zcbor_header_len_ptr(NULL, 4), NULL);
	zassert_equal(0, zcbor_header_len_ptr((uint8_t*)&(uint64_t){0}, 9), NULL);
}


ZTEST(zcbor_unit_tests, test_compare_strings)
{
	const uint8_t hello[] = "hello";
	const uint8_t hello2[] = "hello";
	const uint8_t long_str[] = "This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. ";
	const uint8_t long_str2[] = "This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. "
		"This is a very long string. This is a very long string. This is a very long string. ";
	struct zcbor_string test_str1_hello = {hello, 5};
	struct zcbor_string test_str2_hello_short = {hello, 4};
	struct zcbor_string test_str3_hello_offset = {&hello[1], 4};
	struct zcbor_string test_str4_long = {long_str, sizeof(long_str)};
	struct zcbor_string test_str5_long2 = {long_str2, sizeof(long_str2)};
	struct zcbor_string test_str6_empty = {hello, 0};
	struct zcbor_string test_str7_hello2 = {hello2, 5};
	struct zcbor_string test_str8_empty = {hello2, 0};
	struct zcbor_string test_str9_NULL = {NULL, 0};

	zassert_true(zcbor_compare_strings(&test_str1_hello, &test_str1_hello), NULL);
	zassert_true(zcbor_compare_strings(&test_str1_hello, &test_str7_hello2), NULL);
	zassert_true(zcbor_compare_strings(&test_str4_long, &test_str5_long2), NULL);
	zassert_true(zcbor_compare_strings(&test_str6_empty, &test_str8_empty), NULL);

	zassert_false(zcbor_compare_strings(&test_str2_hello_short, &test_str7_hello2), NULL);
	zassert_false(zcbor_compare_strings(&test_str3_hello_offset, &test_str7_hello2), NULL);
	zassert_false(zcbor_compare_strings(&test_str1_hello, NULL), NULL);
	zassert_false(zcbor_compare_strings(NULL, &test_str5_long2), NULL);
	zassert_false(zcbor_compare_strings(&test_str6_empty, NULL), NULL);
	zassert_false(zcbor_compare_strings(&test_str1_hello, &test_str9_NULL), NULL);
	zassert_false(zcbor_compare_strings(&test_str9_NULL, &test_str5_long2), NULL);
}


ZTEST(zcbor_unit_tests, test_error_str)
{
#define test_str(err) zassert_mem_equal(zcbor_error_str(err), #err, sizeof(#err), NULL)

	test_str(ZCBOR_SUCCESS);
	test_str(ZCBOR_ERR_NO_BACKUP_MEM);
	test_str(ZCBOR_ERR_NO_BACKUP_ACTIVE);
	test_str(ZCBOR_ERR_LOW_ELEM_COUNT);
	test_str(ZCBOR_ERR_HIGH_ELEM_COUNT);
	test_str(ZCBOR_ERR_INT_SIZE);
	test_str(ZCBOR_ERR_FLOAT_SIZE);
	test_str(ZCBOR_ERR_ADDITIONAL_INVAL);
	test_str(ZCBOR_ERR_NO_PAYLOAD);
	test_str(ZCBOR_ERR_PAYLOAD_NOT_CONSUMED);
	test_str(ZCBOR_ERR_WRONG_TYPE);
	test_str(ZCBOR_ERR_WRONG_VALUE);
	test_str(ZCBOR_ERR_WRONG_RANGE);
	test_str(ZCBOR_ERR_ITERATIONS);
	test_str(ZCBOR_ERR_ASSERTION);
	test_str(ZCBOR_ERR_PAYLOAD_OUTDATED);
	test_str(ZCBOR_ERR_ELEM_NOT_FOUND);
	test_str(ZCBOR_ERR_MAP_MISALIGNED);
	test_str(ZCBOR_ERR_ELEMS_NOT_PROCESSED);
	test_str(ZCBOR_ERR_NOT_AT_END);
	test_str(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE);
	test_str(ZCBOR_ERR_INVALID_VALUE_ENCODING);
	test_str(ZCBOR_ERR_CONSTANT_STATE_MISSING);
	test_str(ZCBOR_ERR_BAD_ARG);
	test_str(ZCBOR_ERR_NO_FLAG_MEM);
	test_str(ZCBOR_ERR_BAD_STATE);
	test_str(ZCBOR_ERR_TOO_LARGE_FOR_STRING);
	test_str(ZCBOR_ERR_NOT_IN_FRAGMENT);
	test_str(ZCBOR_ERR_INSIDE_STRING);
	test_str(ZCBOR_ERR_BACKUP_MISMATCH);
	test_str(ZCBOR_ERR_UNKNOWN);
	zassert_mem_equal(zcbor_error_str(-1), "ZCBOR_ERR_UNKNOWN", sizeof("ZCBOR_ERR_UNKNOWN"), NULL);
	zassert_mem_equal(zcbor_error_str(-10), "ZCBOR_ERR_UNKNOWN", sizeof("ZCBOR_ERR_UNKNOWN"), NULL);
	zassert_mem_equal(zcbor_error_str(ZCBOR_ERR_BACKUP_MISMATCH + 1), "ZCBOR_ERR_UNKNOWN", sizeof("ZCBOR_ERR_UNKNOWN"), NULL);
	zassert_mem_equal(zcbor_error_str(100000), "ZCBOR_ERR_UNKNOWN", sizeof("ZCBOR_ERR_UNKNOWN"), NULL);
}


ZTEST(zcbor_unit_tests, test_any_skip)
{
	uint8_t payload[200];
	ZCBOR_STATE_E(state_e, 1, payload, sizeof(payload), 0);
	ZCBOR_STATE_D(state_d, 0, payload, sizeof(payload), 10, 0);
	size_t exp_elem_count = 10;

	zassert_true(zcbor_uint32_put(state_e, 10), NULL);
	bool ret = zcbor_any_skip(state_d, NULL);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_equal(state_d->payload, state_e->payload, NULL);
	zassert_equal(state_d->elem_count, --exp_elem_count, NULL);

	zassert_true(zcbor_int64_put(state_e, -10000000000000), NULL);
	ret = zcbor_any_skip(state_d, NULL);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_equal(state_d->payload, state_e->payload, NULL);
	zassert_equal(state_d->elem_count, --exp_elem_count, NULL);

	zassert_true(zcbor_bstr_put_term(state_e, "hello", 10), NULL);
	zassert_true(zcbor_any_skip(state_d, NULL));
	zassert_equal(state_d->payload, state_e->payload, NULL);
	zassert_equal(state_d->elem_count, --exp_elem_count, NULL);

	zassert_true(zcbor_tstr_put_term(state_e, "world", 5), NULL);
	zassert_true(zcbor_any_skip(state_d, NULL));
	zassert_equal(state_d->payload, state_e->payload, NULL);
	zassert_equal(state_d->elem_count, --exp_elem_count, NULL);

	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_any_skip(state_d, NULL));
	zassert_equal(state_d->payload, state_e->payload, NULL);
	zassert_equal(state_d->elem_count, --exp_elem_count, NULL);

	zassert_true(zcbor_float64_put(state_e, 3.14), NULL);
	zassert_true(zcbor_any_skip(state_d, NULL));
	zassert_equal(state_d->payload, state_e->payload, NULL);
	zassert_equal(state_d->elem_count, --exp_elem_count, NULL);

	zassert_true(zcbor_list_start_encode(state_e, 6), NULL);
	zassert_true(zcbor_uint32_put(state_e, 10), NULL);
	zassert_true(zcbor_int64_put(state_e, -10000000000000), NULL);
	zassert_true(zcbor_bstr_put_term(state_e, "hello", 10), NULL);
	zassert_true(zcbor_tstr_put_term(state_e, "world", 10), NULL);
	zassert_true(zcbor_bool_put(state_e, true), NULL);
	zassert_true(zcbor_float64_put(state_e, 3.14), NULL);
	zassert_true(zcbor_list_end_encode(state_e, 6), NULL);
	ret = zcbor_any_skip(state_d, NULL);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_equal(state_d->payload, state_e->payload, NULL);
	zassert_equal(state_d->elem_count, --exp_elem_count, NULL);

	zassert_true(zcbor_tag_put(state_e, 1), NULL);
	zassert_true(zcbor_tag_put(state_e, 200), NULL);
	zassert_true(zcbor_tag_put(state_e, 3000), NULL);
	zassert_true(zcbor_map_start_encode(state_e, 6), NULL);
	zassert_true(zcbor_uint32_put(state_e, 10), NULL);
	zassert_true(zcbor_int64_put(state_e, -10000000000000), NULL);
	zassert_true(zcbor_bstr_put_term(state_e, "hello", 10), NULL);
	zassert_true(zcbor_tstr_put_term(state_e, "world", 10), NULL);
	zassert_true(zcbor_undefined_put(state_e, NULL), NULL);
	zassert_true(zcbor_float64_put(state_e, 3.14), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 6), NULL);
	zassert_true(zcbor_any_skip(state_d, NULL));
	zassert_equal(state_d->payload, state_e->payload, "%p != %p\n",
		state_d->payload, state_e->payload);
	zassert_equal(state_d->elem_count, --exp_elem_count, NULL);
}


ZTEST(zcbor_unit_tests, test_any_skip_str_overflow)
{
	uint8_t payload[] = {0xd9, 0x12, 0x34, 0x45, 'h', 'e', 'l', 'l'}; // Indicates a string of length 5, but only 4 bytes of data.
	ZCBOR_STATE_D(state_d, 0, payload, sizeof(payload), 1, 0);

	bool ret = zcbor_any_skip(state_d, NULL);
	zassert_false(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, zcbor_peek_error(state_d), NULL);
}


ZTEST(zcbor_unit_tests, test_tag_expect_wrong_value)
{
	/* LIST(2): #6.1234(1), #6.3456(false) */
	uint8_t payload[] = {0x82, 0xD9, 0x04, 0xD2, 0x01, 0xD9, 0x0D, 0x80, 0xF4};
	ZCBOR_STATE_D(state_d, 3, payload, sizeof(payload), 2, 0);

	zassert_true(zcbor_list_start_decode(state_d), NULL);
	zassert_equal(2, state_d->elem_count, NULL);

	/* Wrong tag should fail without changing elem_count. */
	zassert_false(zcbor_tag_expect(state_d, 2345), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, zcbor_peek_error(state_d), NULL);
	zassert_equal(2, state_d->elem_count, NULL);
	zassert_equal(payload + 1, state_d->payload, NULL);

	/* Correct tag should succeed and allow decoding the value. */
	zassert_true(zcbor_tag_expect(state_d, 1234), NULL);
	zassert_equal(2, state_d->elem_count, NULL);
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);
	zassert_equal(1, state_d->elem_count, NULL);

	/* Wrong tag should fail without changing elem_count. */
	zassert_false(zcbor_tag_expect(state_d, 1234), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, zcbor_peek_error(state_d), NULL);
	zassert_equal(1, state_d->elem_count, NULL);
	zassert_equal(payload + 5, state_d->payload, NULL);

	/* Correct tag should succeed and allow decoding the rest of the list. */
	zassert_true(zcbor_tag_expect(state_d, 3456), NULL);
	zassert_equal(1, state_d->elem_count, NULL);
	zassert_true(zcbor_bool_expect(state_d, false), NULL);
	zassert_equal(0, state_d->elem_count, NULL);
	zassert_true(zcbor_list_end_decode(state_d, true), NULL);
}


ZTEST(zcbor_unit_tests, test_pexpect)
{
	uint8_t payload[100];
	ZCBOR_STATE_E(state_e, 2, payload, sizeof(payload), 0);
	ZCBOR_STATE_D(state_d, 2, payload, sizeof(payload), 20, 0);

	zassert_true(zcbor_int32_put(state_e, 1), NULL);
	zassert_true(zcbor_int32_put(state_e, 2), NULL);
	zassert_true(zcbor_int32_put(state_e, 3), NULL);
	zassert_true(zcbor_int32_put(state_e, 4), NULL);
	zassert_true(zcbor_int32_put(state_e, 5), NULL);
	zassert_true(zcbor_tag_put(state_e, 6), NULL);
	zassert_true(zcbor_simple_put(state_e, 7), NULL);
	zassert_true(zcbor_bool_put(state_e, true), NULL);
	zassert_true(zcbor_float16_bytes_put(state_e, 0x4800), NULL);
	zassert_true(zcbor_float16_bytes_put(state_e, 0x4880), NULL);
	zassert_true(zcbor_float32_put(state_e, 10.0), NULL);
	zassert_true(zcbor_float32_put(state_e, 11.0), NULL);
	zassert_true(zcbor_float32_put(state_e, 12.0), NULL);
	zassert_true(zcbor_float64_put(state_e, 13.0), NULL);
	zassert_true(zcbor_float64_put(state_e, 14.0), NULL);

	zassert_true(zcbor_int32_pexpect(state_d, &(int32_t){1}), NULL);
	zassert_true(zcbor_int64_pexpect(state_d, &(int64_t){2}), NULL);
	zassert_true(zcbor_uint32_pexpect(state_d, &(uint32_t){3}), NULL);
	zassert_true(zcbor_uint64_pexpect(state_d, &(uint64_t){4}), NULL);
	zassert_true(zcbor_size_pexpect(state_d, &(size_t){5}), NULL);
	zassert_true(zcbor_tag_pexpect(state_d, &(uint32_t){6}), NULL);
	zassert_true(zcbor_simple_pexpect(state_d, &(uint8_t){7}), NULL);
	zassert_true(zcbor_bool_pexpect(state_d, &(bool){true}), NULL);
	zassert_true(zcbor_float16_pexpect(state_d, &(float){8.0}), NULL);
	zassert_true(zcbor_float16_bytes_pexpect(state_d, &(uint16_t){0x4880}), NULL);
	zassert_true(zcbor_float16_32_pexpect(state_d, &(float){10.0}), NULL);
	zassert_true(zcbor_float32_pexpect(state_d, &(float){11.0}), NULL);
	zassert_true(zcbor_float32_64_pexpect(state_d, &(double){12.0}), NULL);
	zassert_true(zcbor_float64_pexpect(state_d, &(double){13.0}), NULL);
	zassert_true(zcbor_float_pexpect(state_d, &(double){14.0}), NULL);
}


ZTEST(zcbor_unit_tests, test_unordered_map)
{
	uint8_t payload[200];
	ZCBOR_STATE_E(state_e, 2, payload, sizeof(payload), 0);
	ZCBOR_STATE_D(state_d, 2, payload, sizeof(payload), 10, 40);
	struct zcbor_string str_result1;
	struct zcbor_string str_result2;
	struct zcbor_string str_result3;
	int32_t int_result1;
	uint8_t const *start2, *start3, *start4;
	bool ret;

	zassert_true(zcbor_map_start_encode(state_e, 0), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 0), NULL);
	start2 = state_e->payload;

	zassert_true(zcbor_map_start_encode(state_e, 0), NULL);
	zassert_true(zcbor_uint32_put(state_e, 1), NULL);
	zassert_true(zcbor_uint32_put(state_e, 1), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 0), NULL);
	start3 = state_e->payload;

	zassert_true(zcbor_map_start_encode(state_e, 0), NULL);
	zassert_true(zcbor_uint32_put(state_e, 1), NULL);
	zassert_true(zcbor_uint32_put(state_e, 1), NULL);
	zassert_true(zcbor_uint32_put(state_e, 2), NULL);
	zassert_true(zcbor_uint32_put(state_e, 2), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 0), NULL);
	start4 = state_e->payload;

	ZCBOR_STATE_D(state_d2, 2, start3, start4 - start3, 10, 2);
	ZCBOR_STATE_D(state_d3, 2, start3, start4 - start3, 10, 0); // No flags

	zassert_true(zcbor_map_start_encode(state_e, 43), NULL);
	zassert_true(zcbor_uint32_put(state_e, 1), NULL);
	zassert_true(zcbor_tstr_put_lit(state_e, "hello"), NULL);
	zassert_true(zcbor_int32_put(state_e, -1), NULL);
	zassert_true(zcbor_tstr_put_lit(state_e, "world"), NULL);
	zassert_true(zcbor_bool_put(state_e, true), NULL);
	zassert_true(zcbor_tstr_put_lit(state_e, "foo"), NULL);

	/* Nested map */
	zassert_true(zcbor_tstr_put_lit(state_e, "bar"), NULL);
	zassert_true(zcbor_map_start_encode(state_e, 0), NULL);
	zassert_true(zcbor_tstr_put_lit(state_e, "hello"), NULL);
	zassert_true(zcbor_tstr_put_lit(state_e, "world"), NULL);
	zassert_true(zcbor_bool_put(state_e, false), NULL);
	zassert_true(zcbor_undefined_put(state_e, NULL), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 0), NULL);
	/* Nested map end */

	for (int i = 2; i < 35; i++) {
		zassert_true(zcbor_uint32_put(state_e, i), NULL);
		zassert_true(zcbor_int32_put(state_e, -i), NULL);
	}

	zassert_true(zcbor_tstr_put_lit(state_e, "baz1"), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_tstr_put_lit(state_e, "baz2"), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_bstr_put_term(state_e, "baz3", 4), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_bstr_put_term(state_e, "baz4", 4), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_bstr_put_term(state_e, "baz5", 4), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 43), NULL);


	/* Test empty map */
	zassert_true(zcbor_unordered_map_start_decode(state_d), NULL);
	zassert_false(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){2}), NULL);
	zassert_equal(ZCBOR_ERR_ELEM_NOT_FOUND, zcbor_peek_error(state_d), "err: %d\n", zcbor_peek_error(state_d));
	ret = zcbor_unordered_map_end_decode(state_d, true);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_equal(start2, state_d->payload, NULL);

	/* Test single entry map */
	zassert_true(zcbor_unordered_map_start_decode(state_d), NULL);
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){1});
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);
	ret = zcbor_unordered_map_end_decode(state_d, true);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_equal(start3, state_d->payload, NULL);

	/* Test that looping stops both if it starts at the very start and very end of the map.
	 * Also test ZCBOR_ERR_MAP_MISALIGNED. */
	zassert_true(zcbor_unordered_map_start_decode(state_d2), NULL);
	zassert_false(zcbor_elem_processed(state_d2), NULL);
	int err = zcbor_pop_error(state_d2);
	zassert_equal(err, ZCBOR_ERR_MAP_MISALIGNED, "err: %d\n", err);
	zassert_false(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d2, &(int32_t){3}), NULL);
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d2, &(int32_t){2});
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d2));
	zassert_false(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d2, &(int32_t){1}), NULL);
	zassert_equal(zcbor_peek_error(state_d2), ZCBOR_ERR_MAP_MISALIGNED, NULL);
	zassert_true(zcbor_int32_expect(state_d2, 2), NULL);
	zassert_true(zcbor_array_at_end(state_d2), NULL);
	zassert_false(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d2, &(int32_t){3}), NULL);
	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d2, &(int32_t){1}), NULL);
	zassert_true(zcbor_int32_expect(state_d2, 1), NULL);
	ret = zcbor_unordered_map_end_decode(state_d2, true);
	zassert_true(ret, NULL);

	/* Test that state_d3 fails because of missing flags. */
#ifdef ZCBOR_MAP_SMART_SEARCH
#ifdef ZCBOR_CANONICAL
	zassert_false(zcbor_unordered_map_start_decode(state_d3), NULL);
#else
	ret = zcbor_unordered_map_start_decode(state_d3);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d3)));
	zassert_false(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d3, &(int32_t){2}), NULL);
#endif
	zassert_equal(zcbor_peek_error(state_d3), ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE, NULL);
#endif

	/* Test premature map end */
	zassert_true(zcbor_unordered_map_start_decode(state_d), NULL);
	ret = zcbor_unordered_map_end_decode(state_d, false);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d), NULL);
#ifndef ZCBOR_CANONICAL
	zcbor_elem_processed(state_d); // Should do nothing because no elements have been discovered.
#endif
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){1});
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);
	ret = zcbor_unordered_map_end_decode(state_d, false);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d), NULL);
	/* Cause a restart of the map */
	zassert_false(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){3}), NULL);
	ret = zcbor_unordered_map_end_decode(state_d, false);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d), NULL);
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){2});
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_true(zcbor_int32_expect(state_d, 2), NULL);
	ret = zcbor_unordered_map_end_decode(state_d, false);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_equal(start4, state_d->payload, NULL);

	/* Test a large map, including nesting. */
	state_d->constant_state->manually_process_elem = true;
	zassert_true(zcbor_unordered_map_start_decode(state_d), NULL);
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){-1});
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_true(zcbor_tstr_decode(state_d, &str_result1), NULL);
	zcbor_elem_processed(state_d);

	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_bool_pexpect), state_d, &(bool){true}), NULL);
	zassert_true(zcbor_tstr_decode(state_d, &str_result2), NULL);
	zcbor_elem_processed(state_d);
	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){2}), NULL);
	zassert_true(zcbor_int32_decode(state_d, &int_result1), NULL);
	zcbor_elem_processed(state_d);
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &(int32_t){1});
	zassert_true(ret, "%s\n", zcbor_error_str(zcbor_peek_error(state_d)));
	zassert_true(zcbor_tstr_decode(state_d, &str_result3), NULL);
	zcbor_elem_processed(state_d);

	char baz4[] = {'b', 'a', 'z', '4'};
	char baz3[] = "baz3";
	char baz2[] = {'b', 'a', 'z', '2'};
	char baz1[] = "baz1";
	ret = zcbor_search_key_bstr_lit(state_d, "baz5");
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	zcbor_elem_processed(state_d);
	zassert_true(zcbor_search_key_bstr_arr(state_d, baz4), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	zcbor_elem_processed(state_d);
	zassert_true(zcbor_search_key_bstr_term(state_d, baz3, 6), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	zcbor_elem_processed(state_d);
	zassert_true(zcbor_search_key_tstr_arr(state_d, baz2), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	zcbor_elem_processed(state_d);
	zassert_true(zcbor_search_key_tstr_term(state_d, baz1, 6), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	// Don't call zcbor_elem_processed() To check that we can find the element again.
	zassert_true(zcbor_search_key_tstr_term(state_d, baz1, 6), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	zcbor_elem_processed(state_d);
	// Check whether we can find the element again when it has been marked as processed.
#ifdef ZCBOR_MAP_SMART_SEARCH
	zassert_false(zcbor_search_key_tstr_term(state_d, baz1, 6), NULL);
#else
	zassert_true(zcbor_search_key_tstr_term(state_d, baz1, 6), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
#endif

	zassert_true(zcbor_search_key_tstr_lit(state_d, "bar"), NULL);
	zassert_true(zcbor_unordered_map_start_decode(state_d), NULL);
	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_bool_pexpect), state_d, &(bool){false}), NULL);
	zassert_true(zcbor_undefined_expect(state_d, NULL), NULL);
	zcbor_elem_processed(state_d);
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_tstr_expect), state_d, &(struct zcbor_string){"hello", 5});
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d)));
	zassert_true(zcbor_tstr_expect(state_d, &(struct zcbor_string){"world", 5}), NULL);
	zcbor_elem_processed(state_d);
	ret = zcbor_unordered_map_end_decode(state_d, true);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zcbor_elem_processed(state_d);

	for (size_t i = 34; i > 2; i--) {
		zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d, &i), "i: %d, err: %d\n", i, zcbor_peek_error(state_d));
		zassert_false(zcbor_int32_expect(state_d, i), NULL);
		zassert_true(zcbor_int32_expect(state_d, -1 * i), NULL);
		zcbor_elem_processed(state_d);
	}
	ret = zcbor_unordered_map_end_decode(state_d, true);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));

	zassert_equal(int_result1, -2, NULL);
	zassert_true(zcbor_compare_strings(&str_result1, &(struct zcbor_string){"world", 5}), "%s (len: %d)\n", (char*)&str_result1.value, str_result1.len);
	zassert_true(zcbor_compare_strings(&str_result2, &(struct zcbor_string){"foo", 3}), NULL);
	zassert_true(zcbor_compare_strings(&str_result3, &(struct zcbor_string){"hello", 5}), NULL);
}


ZTEST(zcbor_unit_tests, test_canonical_check)
{
	uint8_t payload[] = {
		0xBF, /* Invalid map start */
		0x9F, /* Invalid list start */
		0x78, 0x00, /* invalid 0 */
		0x78, 0x17, /* invalid 23 */
		0x59, 0x00, 0x18, /* invalid 24 */
		0x59, 0x00, 0xFF, /* invalid 255 */
		0x3A, 0x00, 0x00, 0x01, 0x00, /* invalid 256 */
		0x3A, 0x00, 0x00, 0xFF, 0xFF, /* invalid 65535 */
		0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, /* invalid 65536 */
		0x1B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, /* invalid 4294967295 */
		0xFF, 0xFF,
	};
	ZCBOR_STATE_D(state_d, 2, payload, sizeof(payload), 20, 0);
	uint64_t u64_result;
	int64_t i64_result;
	struct zcbor_string str_result;

#ifdef ZCBOR_CANONICAL
	#define CHECK_ERROR1(state) zassert_equal(ZCBOR_ERR_INVALID_VALUE_ENCODING, zcbor_pop_error(state), "err: %s\n", zcbor_error_str(zcbor_peek_error(state)))

	zassert_false(zcbor_map_start_decode(state_d), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 1;
	zassert_false(zcbor_list_start_decode(state_d), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 1;
	zassert_false(zcbor_tstr_decode(state_d, &str_result), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 2;
	zassert_false(zcbor_tstr_decode(state_d, &str_result), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 2;
	zassert_false(zcbor_bstr_decode(state_d, &str_result), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 3;
	zassert_false(zcbor_bstr_decode(state_d, &str_result), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 3;
	zassert_false(zcbor_int64_decode(state_d, &i64_result), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 5;
	zassert_false(zcbor_int64_decode(state_d, &i64_result), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 5;
	zassert_false(zcbor_uint64_decode(state_d, &u64_result), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 9;
	zassert_false(zcbor_uint64_decode(state_d, &u64_result), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 9;

#else
	#define CHECK_ERROR1(state) zassert_equal(ZCBOR_ERR_NO_PAYLOAD, zcbor_pop_error(state), "err: %s\n", zcbor_error_str(zcbor_peek_error(state)))

	zassert_true(zcbor_map_start_decode(state_d), NULL);
	zassert_true(zcbor_list_start_decode(state_d), NULL);
	zassert_true(zcbor_tstr_decode(state_d, &str_result), NULL);
	zassert_true(zcbor_tstr_decode(state_d, &str_result), NULL);
	state_d->payload = state_d->payload_bak + 2; /* Reset since test vector doesn't contain the string value, just the header. */
	zassert_true(zcbor_bstr_decode(state_d, &str_result), NULL);
	state_d->payload = state_d->payload_bak + 3; /* Reset since test vector doesn't contain the string value, just the header. */
	zassert_false(zcbor_bstr_decode(state_d, &str_result), NULL); /* Fails because payload isn't big enough. */
	CHECK_ERROR1(state_d);
	state_d->payload += 3;
	zassert_true(zcbor_int64_decode(state_d, &i64_result), NULL);
	zassert_true(zcbor_int64_decode(state_d, &i64_result), NULL);
	zassert_true(zcbor_uint64_decode(state_d, &u64_result), NULL);
	zassert_true(zcbor_uint64_decode(state_d, &u64_result), NULL);
	zassert_true(zcbor_list_end_decode(state_d, true), NULL);
	zassert_true(zcbor_map_end_decode(state_d, true), NULL);
#endif
}


ZTEST(zcbor_unit_tests, test_zcbor_version)
{
	const char zcbor_version_str[] = ZCBOR_VERSION_STR;
	const char zcbor_version_expected[] = TEST_ZCBOR_VERSION_STR;


	zassert_mem_equal(zcbor_version_expected, zcbor_version_str, sizeof(zcbor_version_expected));
	zassert_equal(TEST_ZCBOR_VERSION, ZCBOR_VERSION, NULL);
	zassert_equal(TEST_ZCBOR_VERSION_MAJOR, ZCBOR_VERSION_MAJOR, NULL);
	zassert_equal(TEST_ZCBOR_VERSION_MINOR, ZCBOR_VERSION_MINOR, NULL);
	zassert_equal(TEST_ZCBOR_VERSION_BUGFIX, ZCBOR_VERSION_BUGFIX, NULL);
}


/* Test that CBOR-encoded bstrs are encoded with the correct length. */
ZTEST(zcbor_unit_tests, test_cbor_encoded_bstr_len)
{
	uint8_t payload[50];

#ifdef ZCBOR_VERBOSE
	for (size_t len = 10; len < 0x108; len++)
#else
	for (size_t len = 10; len < 0x10010; len++)
#endif /* ZCBOR_VERBOSE */
	{
		ZCBOR_STATE_E(state_e, 1, payload, len, 0);
		ZCBOR_STATE_D(state_d, 1, payload, len, 1, 0);

		zassert_true(zcbor_bstr_start_encode(state_e), "len: %d\n", len);
		zassert_true(zcbor_size_put(state_e, len), "len: %d\n", len);
		zassert_true(zcbor_bstr_end_encode(state_e, NULL), "len: %d\n", len);

		zassert_true(zcbor_bstr_start_decode(state_d, NULL), "len: %d\n", len);
		zassert_false(zcbor_bstr_end_decode(state_d, false), NULL);
		zassert_true(zcbor_size_expect(state_d, len), "len: %d\n", len);
		zassert_true(zcbor_bstr_end_decode(state_d, false), "len: %d\n", len);
	}

#if SIZE_MAX == UINT64_MAX
	for (size_t len = 0xFFFFFF00; len <= 0x100000100; len++) {
		ZCBOR_STATE_E(state_e, 1, payload, len, 0);
		ZCBOR_STATE_D(state_d, 1, payload, len, 1, 0);

		zassert_true(zcbor_bstr_start_encode(state_e), "len: %d\n", len);
		zassert_true(zcbor_size_put(state_e, len), "len: %d\n", len);
		zassert_true(zcbor_bstr_end_encode(state_e, NULL), "len: %d\n", len);

		zassert_true(zcbor_bstr_start_decode(state_d, NULL), "len: %d\n", len);
		zassert_true(zcbor_size_expect(state_d, len), "len: %d\n", len);
		zassert_true(zcbor_bstr_end_decode(state_d, false), "len: %d\n", len);
	}
#endif /* SIZE_MAX == UINT64_MAX */
}


/* Test that zcbor_list_end_decode() with force=false keeps the backup when it fails, so
 * that the rest of the list can be decoded afterwards. The end is attempted after each
 * element, so it fails twice before the list is actually exhausted. */
ZTEST(zcbor_unit_tests, test_list_end_decode_force_false)
{
	uint8_t payload[50];
	ZCBOR_STATE_E(state_e, 2, payload, sizeof(payload), 0);
	ZCBOR_STATE_D(state_d, 2, payload, sizeof(payload), 10, 0);
	size_t backup_count;
	size_t elem_count;

	/* LIST(3): 1, 2, 3 */
	zassert_true(zcbor_list_start_encode(state_e, 3), NULL);
	zassert_true(zcbor_int32_put(state_e, 1), NULL);
	zassert_true(zcbor_int32_put(state_e, 2), NULL);
	zassert_true(zcbor_int32_put(state_e, 3), NULL);
	zassert_true(zcbor_list_end_encode(state_e, 3), NULL);

	zassert_true(zcbor_list_start_decode(state_d), NULL);
	backup_count = state_d->constant_state->current_backup;
	elem_count = state_d->elem_count;

	zassert_true(zcbor_int32_expect(state_d, 1), NULL);
	zassert_equal(elem_count - 1, state_d->elem_count, NULL);

	/* Premature end with force=false should fail without consuming the backup. */
	zassert_false(zcbor_list_end_decode(state_d, false), NULL);
	zassert_equal(UNIT_ARR_ERR1, zcbor_peek_error(state_d), NULL);
	zassert_equal(elem_count - 1, state_d->elem_count, NULL);
	zassert_equal(backup_count, state_d->constant_state->current_backup, NULL);

	/* Decoding can continue because the list backup is still active. */
	zassert_true(zcbor_int32_expect(state_d, 2), NULL);
	zassert_false(zcbor_list_end_decode(state_d, false), NULL);
	zassert_equal(UNIT_ARR_ERR1, zcbor_peek_error(state_d), NULL);

	zassert_true(zcbor_int32_expect(state_d, 3), NULL);
	zassert_equal(elem_count - 3, state_d->elem_count, NULL);
	zassert_true(zcbor_list_end_decode(state_d, false), NULL);
	zassert_equal(backup_count - 1, state_d->constant_state->current_backup, NULL);

}


/* The same as test_list_end_decode_force_false, but for maps. Note that a map counts both
 * keys and values as elements, so the element count drops by 2 per key-value-pair. */
ZTEST(zcbor_unit_tests, test_map_end_decode_force_false)
{
	uint8_t map_payload[50];
	size_t backup_count;
	size_t elem_count;

	ZCBOR_STATE_E(state_e, 2, map_payload, sizeof(map_payload), 0);
	ZCBOR_STATE_D(state_d, 2, map_payload, sizeof(map_payload), 10, 0);

	/* MAP(2): {1: 10, 2: 20} */
	zassert_true(zcbor_map_start_encode(state_e, 2), NULL);
	zassert_true(zcbor_int32_put(state_e, 1), NULL);
	zassert_true(zcbor_int32_put(state_e, 10), NULL);
	zassert_true(zcbor_int32_put(state_e, 2), NULL);
	zassert_true(zcbor_int32_put(state_e, 20), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 2), NULL);

	zassert_true(zcbor_map_start_decode(state_d), NULL);
	backup_count = state_d->constant_state->current_backup;
	elem_count = state_d->elem_count;

	zassert_true(zcbor_int32_expect(state_d, 1), NULL);
	zassert_true(zcbor_int32_expect(state_d, 10), NULL);
	zassert_equal(elem_count - 2, state_d->elem_count, NULL);

	zassert_false(zcbor_map_end_decode(state_d, false), NULL);
	zassert_equal(UNIT_ARR_ERR1, zcbor_peek_error(state_d), NULL);
	zassert_equal(elem_count - 2, state_d->elem_count, NULL);
	zassert_equal(backup_count, state_d->constant_state->current_backup, NULL);

	zassert_true(zcbor_int32_expect(state_d, 2), NULL);

	zassert_false(zcbor_map_end_decode(state_d, false), NULL);
	zassert_equal(UNIT_ARR_ERR1, zcbor_peek_error(state_d), NULL);
	zassert_equal(elem_count - 3, state_d->elem_count, NULL);
	zassert_equal(backup_count, state_d->constant_state->current_backup, NULL);

	zassert_true(zcbor_int32_expect(state_d, 20), NULL);
	zassert_equal(elem_count - 4, state_d->elem_count, NULL);
	zassert_true(zcbor_map_end_decode(state_d, false), NULL);
	zassert_equal(backup_count - 1, state_d->constant_state->current_backup, NULL);
}


/* Test that the *_start_decode() functions leave the state untouched when they fail, so
 * that decoding can continue afterwards. */
ZTEST(zcbor_unit_tests, test_decode_start_failure_state)
{
	uint8_t payload[30];
	struct zcbor_dec_state_snapshot snap;

	ZCBOR_STATE_E(state_e, 2, payload, sizeof(payload), 0);
	ZCBOR_STATE_D(state_d, 2, payload, sizeof(payload), 10, 0);

	/* The payload holds an int, so every container start fails the type check.
	 * Both states are rewound between the checks to reuse the same int. */
	zassert_true(zcbor_int32_put(state_e, 1), NULL);
	dec_snapshot(state_d, &snap);
	zassert_false(zcbor_list_start_decode(state_d), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, zcbor_peek_error(state_d), NULL);
	dec_assert_unchanged(state_d, &snap, "list_start wrong type");
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);

	zassert_true(zcbor_int32_put(state_e, 1), NULL);
	dec_snapshot(state_d, &snap);
	zassert_false(zcbor_map_start_decode(state_d), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, zcbor_peek_error(state_d), NULL);
	dec_assert_unchanged(state_d, &snap, "map_start wrong type");
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);

	zassert_true(zcbor_int32_put(state_e, 1), NULL);
	dec_snapshot(state_d, &snap);
	zassert_false(zcbor_bstr_start_decode(state_d, NULL), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, zcbor_peek_error(state_d), NULL);
	dec_assert_unchanged(state_d, &snap, "bstr_start wrong type");
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);

	/* A map header with an element count that cannot be doubled without overflowing
	 * elem_count. state_d2 has no backups, so the failure happens before the map is
	 * entered. state_d3 has one, so the failure happens after the backup has been made,
	 * which means the backup must be exited again on the way out. */
#if SIZE_MAX == UINT64_MAX
	uint8_t huge_map[] = {0xBB, 0x80, 0, 0, 0, 0, 0, 0, 0};
#elif SIZE_MAX == UINT32_MAX
	uint8_t huge_map[] = {0xBA, 0x80, 0, 0, 0};
#else
#error "Unsupported width of size_t."
#endif

	ZCBOR_STATE_D(state_d2, 0, huge_map, sizeof(huge_map), 1, 0);
	ZCBOR_STATE_D(state_d3, 1, huge_map, sizeof(huge_map), 1, 0);

	dec_snapshot(state_d2, &snap);
	zassert_false(zcbor_map_start_decode(state_d2), NULL);
	zassert_equal(ZCBOR_ERR_NO_BACKUP_MEM, zcbor_peek_error(state_d2), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d2)));
	dec_assert_unchanged(state_d2, &snap, "map_start no backup mem");

	dec_snapshot(state_d3, &snap);
	zassert_false(zcbor_map_start_decode(state_d3), NULL);
	zassert_equal(ZCBOR_ERR_INT_SIZE, zcbor_peek_error(state_d3), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d3)));
	dec_assert_unchanged(state_d3, &snap, "map_start elem_count too large");
}


#ifdef ZCBOR_MAP_SMART_SEARCH
/* Position the state on the value of key 1 of the unordered map at the start of the
 * payload, then take away the map flag storage that exit_map() needs to descend into that
 * value. allocate_map_flags() always reserves enough room for the maps that are actually
 * entered, so shrinking the buffer afterwards is the only way to reach the failure. */
static void exhaust_map_flags(zcbor_state_t *state)
{
	zassert_true(zcbor_unordered_map_start_decode(state), NULL);
	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state,
		&(int32_t){1}), NULL);

	/* exit_backup() only has to stash the error from exit_map() when stop_on_error is
	 * enabled, so that is where the interesting cleanup happens. */
	(void)zcbor_pop_error(state);
	state->constant_state->stop_on_error = true;

	state->constant_state->map_search_elem_state_end
		= state->decode_state.map_search_elem_state;
}


/* Test the exit_map() failure path in the *_start_decode() functions. All three take a
 * backup before calling exit_map(), so that backup has to be exited again on the way out,
 * and the payload and element count restored, leaving the map usable. */
ZTEST(zcbor_unit_tests, test_start_decode_exit_map_fail)
{
	/* MAP(1) with a bstr, a list and a map as the value of key 1. */
	uint8_t bstr_payload[] = {0xA1, 0x01, 0x41, 0x01};
	uint8_t list_payload[] = {0xA1, 0x01, 0x81, 0x01};
	uint8_t map_payload[] = {0xA1, 0x01, 0xA1, 0x01, 0x01};
	struct zcbor_dec_state_snapshot snap;

	ZCBOR_STATE_D(state_d, 2, bstr_payload, sizeof(bstr_payload), 10, 8);
	exhaust_map_flags(state_d);
	dec_snapshot(state_d, &snap);
	zassert_false(zcbor_bstr_start_decode(state_d, NULL), NULL);
	zassert_equal(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE, zcbor_peek_error(state_d), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d)));
	dec_assert_unchanged(state_d, &snap, "bstr_start exit_map fail");

	ZCBOR_STATE_D(state_d2, 2, list_payload, sizeof(list_payload), 10, 8);
	exhaust_map_flags(state_d2);
	dec_snapshot(state_d2, &snap);
	zassert_false(zcbor_list_start_decode(state_d2), NULL);
	zassert_equal(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE, zcbor_peek_error(state_d2), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d2)));
	dec_assert_unchanged(state_d2, &snap, "list_start exit_map fail");

	ZCBOR_STATE_D(state_d3, 2, map_payload, sizeof(map_payload), 10, 8);
	uint8_t *flags_end = state_d3->constant_state->map_search_elem_state_end;

	exhaust_map_flags(state_d3);
	dec_snapshot(state_d3, &snap);
	zassert_false(zcbor_map_start_decode(state_d3), NULL);
	zassert_equal(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE, zcbor_peek_error(state_d3), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d3)));
	dec_assert_unchanged(state_d3, &snap, "map_start exit_map fail");

	/* Give the flag storage back to check that the map really was left usable, i.e. that
	 * the failed call restored everything it had changed. */
	(void)zcbor_pop_error(state_d3);
	state_d3->constant_state->map_search_elem_state_end = flags_end;
	zassert_true(zcbor_map_start_decode(state_d3), NULL);
	zassert_true(zcbor_int32_expect(state_d3, 1), NULL);
	zassert_true(zcbor_int32_expect(state_d3, 1), NULL);
	zassert_true(zcbor_map_end_decode(state_d3, false), NULL);
}
#endif


/* Test the state left behind when the *_end_decode() functions fail because the container
 * has not been fully decoded, and how that differs with the force argument.
 * With force=false the state is untouched, so decoding can continue. With force=true the
 * backup is consumed anyway, which is what allows the caller to unwind to an outer backup
 * instead. Either way the caller is told about the error. */
ZTEST(zcbor_unit_tests, test_decode_end_failure_state)
{
	uint8_t bstr_payload[20];
	uint8_t list_payload[20];
	uint8_t map_payload[30];
	struct zcbor_dec_state_snapshot snap;

	/* bstr_end_decode(): force=false preserves state; force=true consumes backup. */
	ZCBOR_STATE_E(state_e, 1, bstr_payload, sizeof(bstr_payload), 0);
	ZCBOR_STATE_D(state_d, 1, bstr_payload, sizeof(bstr_payload), 10, 0);

	zassert_true(zcbor_bstr_start_encode(state_e), NULL);
	zassert_true(zcbor_int32_put(state_e, 1), NULL);
	zassert_true(zcbor_bstr_end_encode(state_e, NULL), NULL);

	zassert_true(zcbor_bstr_start_decode(state_d, NULL), NULL);
	dec_snapshot(state_d, &snap);
	zassert_false(zcbor_bstr_end_decode(state_d, false), NULL);
	zassert_equal(ZCBOR_ERR_PAYLOAD_NOT_CONSUMED, zcbor_peek_error(state_d), NULL);
	dec_assert_unchanged(state_d, &snap, "bstr_end force=false");
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);
	zassert_true(zcbor_bstr_end_decode(state_d, false), NULL);

	ZCBOR_STATE_D(state_d2, 1, bstr_payload, sizeof(bstr_payload), 10, 0);
	zassert_true(zcbor_bstr_start_decode(state_d2, NULL), NULL);
	size_t backup_before = state_d2->constant_state->current_backup;
	zassert_false(zcbor_bstr_end_decode(state_d2, true), NULL);
	zassert_equal(ZCBOR_ERR_PAYLOAD_NOT_CONSUMED, zcbor_peek_error(state_d2), NULL);
	zassert_equal(backup_before - 1, state_d2->constant_state->current_backup, NULL);
	zassert_false(state_d2->inside_cbor_bstr, NULL);
	zassert_false(zcbor_bstr_end_decode(state_d2, false), NULL);
	zassert_equal(ZCBOR_ERR_PAYLOAD_NOT_CONSUMED, zcbor_peek_error(state_d2), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d2)));

	/* list_end_decode(): force=true consumes backup and leaves state unusable. */
	ZCBOR_STATE_E(state_e2, 2, list_payload, sizeof(list_payload), 0);
	ZCBOR_STATE_D(state_d3, 2, list_payload, sizeof(list_payload), 10, 0);

	zassert_true(zcbor_list_start_encode(state_e2, 2), NULL);
	zassert_true(zcbor_int32_put(state_e2, 1), NULL);
	zassert_true(zcbor_int32_put(state_e2, 2), NULL);
	zassert_true(zcbor_list_end_encode(state_e2, 2), NULL);

	backup_before = state_d3->constant_state->current_backup;
	zassert_true(zcbor_list_start_decode(state_d3), NULL);
	zassert_false(zcbor_list_end_decode(state_d3, true), NULL);
	zassert_equal(UNIT_ARR_ERR1, zcbor_peek_error(state_d3), NULL);
	zassert_equal(backup_before, state_d3->constant_state->current_backup, NULL);

	/* map_end_decode(): force=true consumes backup and leaves state unusable. */
	ZCBOR_STATE_E(state_e3, 2, map_payload, sizeof(map_payload), 0);
	ZCBOR_STATE_D(state_d4, 2, map_payload, sizeof(map_payload), 10, 0);

	zassert_true(zcbor_map_start_encode(state_e3, 2), NULL);
	zassert_true(zcbor_int32_put(state_e3, 1), NULL);
	zassert_true(zcbor_int32_put(state_e3, 10), NULL);
	zassert_true(zcbor_int32_put(state_e3, 2), NULL);
	zassert_true(zcbor_int32_put(state_e3, 20), NULL);
	zassert_true(zcbor_map_end_encode(state_e3, 2), NULL);

	backup_before = state_d4->constant_state->current_backup;
	zassert_true(zcbor_map_start_decode(state_d4), NULL);
	zassert_false(zcbor_map_end_decode(state_d4, true), NULL);
	zassert_equal(UNIT_ARR_ERR1, zcbor_peek_error(state_d4), NULL);
	zassert_equal(backup_before, state_d4->constant_state->current_backup, NULL);
}


/* Test the *_end_decode() failures that happen inside the *_end_decode() call itself,
 * i.e. the cases where exit_backup() is called with skip_check_error=true.
 * These need special handling because zcbor_process_backup() refuses to do anything when
 * an error is already registered (when stop_on_error is enabled), so the error must be
 * popped before consuming the backup, and restored afterwards. */
ZTEST(zcbor_unit_tests, test_end_decode_force_stop_on_error)
{
	/* Indefinite length list: [1, 2] */
	uint8_t list_payload[] = {0x9F, 0x01, 0x02, 0xFF};
	/* Indefinite length map: {1: 2} */
	uint8_t map_payload[] = {0xBF, 0x01, 0x02, 0xFF};
	/* MAP(2): {1: 2, 3: 4} */
	uint8_t unordered_payload[] = {0xA2, 0x01, 0x02, 0x03, 0x04};
	size_t backup_before;

	/* zcbor_list_end_decode() with force=false: array_end_expect() fails because the
	 * list end (0xFF) has not been reached, but the backup is kept so decoding can
	 * continue after popping the error. */
	ZCBOR_STATE_D(state_d, 2, list_payload, sizeof(list_payload), 10, 0);
	state_d->constant_state->stop_on_error = true;
	state_d->constant_state->enforce_canonical = false;

	zassert_true(zcbor_list_start_decode(state_d), NULL);
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);

	backup_before = state_d->constant_state->current_backup;
	zassert_false(zcbor_list_end_decode(state_d, false), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, zcbor_peek_error(state_d), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d)));
	zassert_equal(backup_before, state_d->constant_state->current_backup, NULL);

	zassert_equal(ZCBOR_ERR_WRONG_TYPE, zcbor_pop_error(state_d), NULL);
	zassert_true(zcbor_int32_expect(state_d, 2), NULL);
	zassert_true(zcbor_list_end_decode(state_d, false), NULL);
	zassert_equal(backup_before - 1, state_d->constant_state->current_backup, NULL);

	/* Same failure with force=true: the backup must be consumed even though
	 * array_end_expect() has already registered an error, and that error must still be
	 * the one reported to the caller. */
	ZCBOR_STATE_D(state_d2, 2, list_payload, sizeof(list_payload), 10, 0);
	state_d2->constant_state->stop_on_error = true;
	state_d2->constant_state->enforce_canonical = false;

	backup_before = state_d2->constant_state->current_backup;
	zassert_true(zcbor_list_start_decode(state_d2), NULL);
	zassert_true(zcbor_int32_expect(state_d2, 1), NULL);

	zassert_false(zcbor_list_end_decode(state_d2, true), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, zcbor_peek_error(state_d2), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d2)));
	zassert_equal(backup_before, state_d2->constant_state->current_backup, NULL);

	/* zcbor_map_end_decode() takes the same path as zcbor_list_end_decode(). */
	ZCBOR_STATE_D(state_d3, 2, map_payload, sizeof(map_payload), 10, 0);
	state_d3->constant_state->stop_on_error = true;
	state_d3->constant_state->enforce_canonical = false;

	backup_before = state_d3->constant_state->current_backup;
	zassert_true(zcbor_map_start_decode(state_d3), NULL);
	zassert_true(zcbor_int32_expect(state_d3, 1), NULL);

	zassert_false(zcbor_map_end_decode(state_d3, true), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, zcbor_peek_error(state_d3), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d3)));
	zassert_equal(backup_before, state_d3->constant_state->current_backup, NULL);

	/* zcbor_unordered_map_end_decode() with force=true, failing in zcbor_any_skip()
	 * while moving to the end of the map. */
	ZCBOR_STATE_D(state_d4, 2, unordered_payload, sizeof(unordered_payload), 10, 10);

	backup_before = state_d4->constant_state->current_backup;
	zassert_true(zcbor_unordered_map_start_decode(state_d4), NULL);
	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d4,
		&(int32_t){3}), NULL);
	zassert_true(zcbor_int32_expect(state_d4, 4), NULL);

	/* The searches leave the errors from the non-matching keys behind, so enable
	 * stop_on_error only for the zcbor_unordered_map_end_decode() call. */
	(void)zcbor_pop_error(state_d4);
	state_d4->constant_state->stop_on_error = true;

	zassert_false(zcbor_unordered_map_end_decode(state_d4, true), NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d4), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d4)));
	zassert_equal(backup_before, state_d4->constant_state->current_backup, NULL);
}


/* Test zcbor_unordered_map_end_decode() on its own, i.e. what the force argument changes.
 * The two values only behave differently when the map has not been fully processed, which
 * is when the call fails with ZCBOR_ERR_ELEMS_NOT_PROCESSED: force=false leaves the state
 * untouched so the remaining elements can still be searched for, while force=true consumes
 * the map's backup so the caller can unwind to an outer backup instead. When every element
 * has been processed the call succeeds either way. */
ZTEST(zcbor_unit_tests, test_unordered_map_end_decode_force)
{
	/* MAP(3): {1: 2, 3: 4, 5: 6} */
	uint8_t payload[] = {0xA3, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
	/* MAP(0): {} */
	uint8_t empty_payload[] = {0xA0};
	struct zcbor_dec_state_snapshot snap;
	size_t backup_before;

	/* force=false: the end is attempted after each element, so it fails three times
	 * before the map has been fully processed. Every failure leaves the state as it was,
	 * so the next search picks up where the previous one left off. */
	ZCBOR_STATE_D(state_d, 2, payload, sizeof(payload), 10, 10);

	backup_before = state_d->constant_state->current_backup;
	zassert_true(zcbor_unordered_map_start_decode(state_d), NULL);
	zassert_equal(backup_before + 1, state_d->constant_state->current_backup, NULL);

	dec_snapshot(state_d, &snap);
	zassert_false(zcbor_unordered_map_end_decode(state_d, false), NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d)));
	dec_assert_unchanged(state_d, &snap, "unordered_map_end force=false, 0 of 3 processed");

	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d,
		&(int32_t){1}), NULL);

	dec_snapshot(state_d, &snap);
	zassert_false(zcbor_unordered_map_end_decode(state_d, false), NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d)));
	dec_assert_unchanged(state_d, &snap, "unordered_map_end force=false, 1 of 3 processed");
	zassert_true(zcbor_int32_expect(state_d, 2), NULL);

	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d,
		&(int32_t){3}), NULL);
	zassert_true(zcbor_int32_expect(state_d, 4), NULL);

	dec_snapshot(state_d, &snap);
	zassert_false(zcbor_unordered_map_end_decode(state_d, false), NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d)));
	dec_assert_unchanged(state_d, &snap, "unordered_map_end force=false, 2 of 3 processed");

	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d,
		&(int32_t){5}), NULL);
	zassert_true(zcbor_int32_expect(state_d, 6), NULL);

	/* All three elements have been processed, so the map ends normally. */
	zassert_true(zcbor_unordered_map_end_decode(state_d, false), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d)));
	zassert_equal(backup_before, state_d->constant_state->current_backup, NULL);
	zassert_equal(payload + sizeof(payload), state_d->payload, NULL);

	/* force=true fails in the same way, but consumes the map's backup, which is what
	 * lets the caller leave the map instead of continuing inside it. */
	ZCBOR_STATE_D(state_d2, 2, payload, sizeof(payload), 10, 10);

	backup_before = state_d2->constant_state->current_backup;
	zassert_true(zcbor_unordered_map_start_decode(state_d2), NULL);
	zassert_equal(backup_before + 1, state_d2->constant_state->current_backup, NULL);

	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d2,
		&(int32_t){1}), NULL);
	zassert_true(zcbor_int32_expect(state_d2, 2), NULL);

	zassert_false(zcbor_unordered_map_end_decode(state_d2, true), NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d2), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d2)));
	zassert_equal(backup_before, state_d2->constant_state->current_backup, NULL);

	/* force=true on a fully processed map succeeds just like force=false. The elements
	 * are searched for out of order here, so the last one found is not the last one in
	 * the payload, and the call has to skip the remainder to reach the end of the map. */
	ZCBOR_STATE_D(state_d3, 2, payload, sizeof(payload), 10, 10);

	backup_before = state_d3->constant_state->current_backup;
	zassert_true(zcbor_unordered_map_start_decode(state_d3), NULL);

	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d3,
		&(int32_t){1}), NULL);
	zassert_true(zcbor_int32_expect(state_d3, 2), NULL);
	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d3,
		&(int32_t){5}), NULL);
	zassert_true(zcbor_int32_expect(state_d3, 6), NULL);
	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_int32_pexpect), state_d3,
		&(int32_t){3}), NULL);
	zassert_true(zcbor_int32_expect(state_d3, 4), NULL);
	zassert_false(zcbor_array_at_end(state_d3), NULL);

	zassert_true(zcbor_unordered_map_end_decode(state_d3, true), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d3)));
	zassert_equal(backup_before, state_d3->constant_state->current_backup, NULL);
	zassert_equal(payload + sizeof(payload), state_d3->payload, NULL);

	/* An empty map has nothing to process, so it ends successfully either way. */
	ZCBOR_STATE_D(state_d4, 2, empty_payload, sizeof(empty_payload), 10, 10);
	zassert_true(zcbor_unordered_map_start_decode(state_d4), NULL);
	zassert_true(zcbor_unordered_map_end_decode(state_d4, false), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d4)));
	zassert_equal(empty_payload + sizeof(empty_payload), state_d4->payload, NULL);

	ZCBOR_STATE_D(state_d5, 2, empty_payload, sizeof(empty_payload), 10, 10);
	zassert_true(zcbor_unordered_map_start_decode(state_d5), NULL);
	zassert_true(zcbor_unordered_map_end_decode(state_d5, true), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d5)));
	zassert_equal(empty_payload + sizeof(empty_payload), state_d5->payload, NULL);
}


/* Test that the encoding start/end functions leave the state untouched when they fail.
 * Only relevant when ZCBOR_CANONICAL is defined, because that is the only case where
 * list/map encoding takes backups. Otherwise they are encoded with indefinite length
 * headers, which need no backup and cannot fail this way. */
#ifdef ZCBOR_CANONICAL
ZTEST(zcbor_unit_tests, test_encode_start_end_failure_state)
{
	uint8_t payload[30];
	struct zcbor_enc_state_snapshot snap;

	/* End without a matching start: no backup to consume. */
	ZCBOR_STATE_E(state_e, 1, payload, sizeof(payload), 0);
	enc_snapshot(state_e, &snap);
	zassert_false(zcbor_list_end_encode(state_e, 0), NULL);
	zassert_equal(ZCBOR_ERR_NO_BACKUP_ACTIVE, zcbor_peek_error(state_e), NULL);
	enc_assert_unchanged(state_e, &snap, "list_end no backup");

	zassert_false(zcbor_map_end_encode(state_e, 0), NULL);
	zassert_equal(ZCBOR_ERR_NO_BACKUP_ACTIVE, zcbor_peek_error(state_e), NULL);
	enc_assert_unchanged(state_e, &snap, "map_end no backup");

	zassert_false(zcbor_bstr_end_encode(state_e, NULL), NULL);
	zassert_equal(ZCBOR_ERR_NO_BACKUP_ACTIVE, zcbor_peek_error(state_e), NULL);
	enc_assert_unchanged(state_e, &snap, "bstr_end no backup");

	/* Start without backup memory. */
	ZCBOR_STATE_E(state_e2, 0, payload, sizeof(payload), 0);
	enc_snapshot(state_e2, &snap);
	zassert_false(zcbor_list_start_encode(state_e2, 1), NULL);
	zassert_equal(ZCBOR_ERR_NO_BACKUP_MEM, zcbor_peek_error(state_e2), NULL);
	enc_assert_unchanged(state_e2, &snap, "list_start no backup mem");

	enc_snapshot(state_e2, &snap);
	zassert_false(zcbor_bstr_start_encode(state_e2), NULL);
	zassert_equal(ZCBOR_ERR_NO_BACKUP_MEM, zcbor_peek_error(state_e2), NULL);
	enc_assert_unchanged(state_e2, &snap, "bstr_start no backup mem");
}
#endif /* ZCBOR_CANONICAL */


/* Test zcbor_remaining_str_len().
 * Some payload lengths are impossible to fill with a properly encoded string,
 * these have special cases. */
ZTEST(zcbor_unit_tests, test_remaining_str_len)
{
	uint8_t payload[8];
	ZCBOR_STATE_E(state_e, 1, payload, 0, 0);

	zassert_equal(zcbor_remaining_str_len(state_e), 0, "i: 0\n");

	for (uint64_t i = 1; i <= 0x20000; i++) {
		size_t offset;

		state_e->payload_end = payload + i;

		switch(i) {
		case 25:
		case 0x102:
			offset = 1;
			break;
		case 0x10003:
		case 0x10004:
			offset = 2;
			break;
		default:
			offset = 0;
			break;
		}

		size_t total_len = zcbor_remaining_str_len(state_e)
					+ zcbor_header_len(zcbor_remaining_str_len(state_e));
		zassert_equal(i - offset, total_len, "i: %llu, len: %zu\n", i, total_len);
	}

#if SIZE_MAX == UINT64_MAX
	for (uint64_t i = 0xFFFFFF00; i <= 0x100000100; i++) {
		size_t offset;

		state_e->payload_end = payload + i;

		switch(i) {
		case 0x100000005:
		case 0x100000006:
		case 0x100000007:
		case 0x100000008:
			offset = 4;
			break;
		default:
			offset = 0;
			break;
		}

		size_t total_len = zcbor_remaining_str_len(state_e)
					+ zcbor_header_len(zcbor_remaining_str_len(state_e));
		zassert_equal(i - offset, total_len, "i: %lx, len: %zx\n", i, total_len);
	}
#endif
}

/* Test that CBOR-encoded tstrs are encoded with the correct length, even if the first part of the
   number is 0s. */
ZTEST(zcbor_unit_tests, test_float_len)
{
	uint8_t payload[50];
	ZCBOR_STATE_E(state_e, 1, payload, sizeof(payload), 0);
	ZCBOR_STATE_D(state_d, 1, payload, sizeof(payload), 20, 0);

	zassert_true(zcbor_float16_put(state_e, 0.0));
	zassert_true(zcbor_float16_expect(state_d, 0.0));
	zassert_true(zcbor_float32_put(state_e, 0.0));
	zassert_true(zcbor_float32_expect(state_d, 0.0));
	zassert_true(zcbor_float64_put(state_e, 0.0));
	zassert_true(zcbor_float64_expect(state_d, 0.0));

	zassert_true(zcbor_float16_put(state_e, powf(2, -17)));
	zassert_true(zcbor_float16_expect(state_d, powf(2, -17)));
	zassert_true(zcbor_float32_put(state_e, 0.000000000000000000000000000000000000000001f));
	zassert_true(zcbor_float32_expect(state_d, 0.000000000000000000000000000000000000000001f));
	zassert_true(zcbor_float64_put(state_e, pow(10, -315)));
	zassert_true(zcbor_float64_expect(state_d, pow(10, -315)));
}

ZTEST(zcbor_unit_tests, test_simple_value_len)
{
#ifndef ZCBOR_CANONICAL
	printf("Skip on non-canonical builds.\n");
#else
	uint8_t payload[] = {0xe1, 0xf4, 0xf5, 0xf6, 0xf7};

	/* Simple values under 24 must be encoded as single bytes. */
	uint8_t payload_inv[] = {
		0xf8, 1,
		0xf8, ZCBOR_BOOL_TO_SIMPLE,
		0xf8, ZCBOR_BOOL_TO_SIMPLE + 1,
		0xf8, ZCBOR_NIL_VAL,
		0xf8, ZCBOR_UNDEF_VAL
	};
	ZCBOR_STATE_D(state_d, 1, payload, sizeof(payload), 20, 0);
	ZCBOR_STATE_D(state_d_inv, 1, payload_inv, sizeof(payload_inv), 20, 0);

	zassert_true(zcbor_simple_expect(state_d, 1));
	zassert_true(zcbor_bool_expect(state_d, false));
	zassert_true(zcbor_bool_expect(state_d, true));
	zassert_true(zcbor_nil_expect(state_d, NULL));
	zassert_true(zcbor_undefined_expect(state_d, NULL));

	zassert_false(zcbor_simple_expect(state_d_inv, 1));
	zassert_false(zcbor_bool_expect(state_d_inv, false));
	zassert_false(zcbor_bool_expect(state_d_inv, true));
	zassert_false(zcbor_nil_expect(state_d_inv, NULL));
	zassert_false(zcbor_undefined_expect(state_d_inv, NULL));
#endif
}

bool my_decode_func1(zcbor_state_t *state, void *out)
{
	ZCBOR_FAIL_IF(!zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_uint32_pexpect), state, &((uint32_t){1})));
	ZCBOR_FAIL_IF(!zcbor_int32_expect(state, -1));
	zcbor_elem_processed(state);
	ZCBOR_FAIL_IF(!zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_uint32_pexpect), state, &((uint32_t){2})));
	ZCBOR_FAIL_IF(!zcbor_int32_expect(state, -2));
	zcbor_elem_processed(state);
	return true;
}
bool my_decode_func2(zcbor_state_t *state, void *out)
{
	ZCBOR_FAIL_IF(!my_decode_func1(state, out));
	ZCBOR_FAIL_IF(!zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_uint32_pexpect), state, &((uint32_t){3})));
	ZCBOR_FAIL_IF(!zcbor_int32_expect(state, -3));
	zcbor_elem_processed(state);
	return true;
}

ZTEST(zcbor_unit_tests, test_elem_state_backup)
{
	uint8_t payload[50];
	ZCBOR_STATE_E(state_e, 1, payload, sizeof(payload), 0);
	ZCBOR_STATE_D(state_d, 3, payload, sizeof(payload), 20, 20);
	state_d->constant_state->manually_process_elem = true;

	zassert_true(zcbor_map_start_encode(state_e, 3), NULL);
	zassert_true(zcbor_uint32_put(state_e, 1), NULL);
	zassert_true(zcbor_int32_put(state_e, -1), NULL);
	zassert_true(zcbor_uint32_put(state_e, 2), NULL);
	zassert_true(zcbor_int32_put(state_e, -2), NULL);
	zassert_true(zcbor_uint32_put(state_e, 4), NULL);
	zassert_true(zcbor_int32_put(state_e, -4), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 3), NULL);

	zassert_true(zcbor_unordered_map_start_decode(state_d));
	zassert_true(zcbor_new_backup_w_elem_state(state_d, state_d->elem_count, true));
	zassert_false(my_decode_func2(state_d, NULL));
	zassert_equal(ZCBOR_ERR_ELEM_NOT_FOUND, zcbor_peek_error(state_d), "err: %d\n", zcbor_peek_error(state_d));
	zassert_false(my_decode_func1(state_d, NULL)); // Flags are taken erroneously from the first call.
	zassert_equal(ZCBOR_ERR_ELEM_NOT_FOUND, zcbor_peek_error(state_d), "err: %d\n", zcbor_peek_error(state_d));
	bool ret = zcbor_process_backup(state_d, ZCBOR_FLAG_RESTORE, ZCBOR_MAX_ELEM_COUNT);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_true(my_decode_func1(state_d, NULL));
	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_uint32_pexpect), state_d, &((uint32_t){4})));
	zassert_true(zcbor_int32_expect(state_d, -4), NULL);

	zassert_false(zcbor_unordered_map_end_decode(state_d, false), NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d), "err: %d\n", zcbor_peek_error(state_d));

	zcbor_elem_processed(state_d);

	ret = zcbor_unordered_map_end_decode(state_d, false);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));

#ifdef ZCBOR_MAP_SMART_SEARCH
	zassert_true(zcbor_map_start_encode(state_e, 6), NULL);

	/* Below are two sets, one of 1,2,3 and one of 1,2,4. */
	zassert_true(zcbor_uint32_put(state_e, 3), NULL);
	zassert_true(zcbor_int32_put(state_e, -3), NULL);
	zassert_true(zcbor_uint32_put(state_e, 2), NULL);
	zassert_true(zcbor_int32_put(state_e, -2), NULL);
	zassert_true(zcbor_uint32_put(state_e, 1), NULL);
	zassert_true(zcbor_int32_put(state_e, -1), NULL);
	zassert_true(zcbor_uint32_put(state_e, 2), NULL);
	zassert_true(zcbor_int32_put(state_e, -2), NULL);
	zassert_true(zcbor_uint32_put(state_e, 4), NULL);
	zassert_true(zcbor_int32_put(state_e, -4), NULL);
	zassert_true(zcbor_uint32_put(state_e, 1), NULL);
	zassert_true(zcbor_int32_put(state_e, -1), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 6), NULL);

	zassert_equal(sizeof(zcbor_state_t), state_d->constant_state->map_search_elem_state_end - state_d->decode_state.map_search_elem_state, "%d\n",
		      state_d->constant_state->map_search_elem_state_end - state_d->decode_state.map_search_elem_state);

	size_t num_decoded = 0;
	zassert_true(zcbor_unordered_map_start_decode(state_d));
	zcbor_multi_decode_w_backup(1, 2, &num_decoded, my_decode_func2, state_d, NULL, 0);
	zassert_equal(num_decoded, 1, "num_decoded: %zu\n", num_decoded);

	zcbor_union_start_code(state_d);
	zassert_false(my_decode_func2(state_d, NULL));
	zcbor_union_elem_code(state_d);
	ret = my_decode_func1(state_d, NULL);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zcbor_union_end_code(state_d);

	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_uint32_pexpect), state_d, &((uint32_t){4})));
	zassert_true(zcbor_int32_expect(state_d, -4), NULL);
	zcbor_elem_processed(state_d);

	ret = zcbor_unordered_map_end_decode(state_d, true);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
#endif
}

ZTEST(zcbor_unit_tests, test_elem_state_backup2)
{
#ifdef ZCBOR_MAP_SMART_SEARCH
	/* Test elem_state buffer overflow.
	 * The number of elements is tailored so that after decoding a number of elements, there's
	 * enough flags to do one backup, but barely not two. I.e. num_decode is just above 1/3 of
	 * the available flags.
	 * Likewise, the total number of encoded elements is barely too large to be able to
	 * allocate flags for all of them, after the successful backup. I.e. num_encode is just
	 * above 2/3 of the available flags. */

	static uint8_t payload_2[5000];
	ZCBOR_STATE_E(state_e2, 1, payload_2, sizeof(payload_2), 0);
	ZCBOR_STATE_D(state_d2, 5, payload_2, sizeof(payload_2), 1, 620);

	size_t num_flag_bytes = state_d2->constant_state->map_search_elem_state_end - state_d2->decode_state.map_search_elem_state;
	size_t num_decode = (num_flag_bytes / 3) * 8 + 1;
	size_t num_encode = (num_flag_bytes / 3) * 2 * 8 + 1;

	zassert_between_inclusive(num_flag_bytes, 78, 256, "%d\n", num_flag_bytes);

	zassert_true(zcbor_map_start_encode(state_e2, num_encode), NULL);
	for (int i = 1; i <= num_encode; i++) {
		zassert_true(zcbor_uint32_put(state_e2, i), NULL);
		zassert_true(zcbor_int32_put(state_e2, -i), NULL);
	}
	zassert_true(zcbor_map_end_encode(state_e2, num_encode), NULL);


	zassert_true(zcbor_unordered_map_start_decode(state_d2));
	zassert_true(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_uint32_pexpect), state_d2, &((uint32_t){num_decode})));
	zassert_true(zcbor_int32_expect(state_d2, -num_decode), NULL);
	zassert_true(zcbor_new_backup_w_elem_state(state_d2, state_d2->elem_count, true));
	zassert_false(zcbor_new_backup_w_elem_state(state_d2, state_d2->elem_count, true));
	zassert_equal(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE, zcbor_peek_error(state_d2), "err: %d\n", zcbor_peek_error(state_d2));
	zassert_false(zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_uint32_pexpect), state_d2, &((uint32_t){2})));
	zassert_equal(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE, zcbor_peek_error(state_d2), "err: %d\n", zcbor_peek_error(state_d2));
#endif
}


uint8_t dummy_entry_func_payload[10] = {
	0x18, 42, /* uint: 42 */};

uint32_t dummy_entry_func_result;

bool dummy_entry_function(zcbor_state_t *state, uint32_t *result)
{
	zassert_equal(state->payload, dummy_entry_func_payload, NULL);
	zassert_equal(state->elem_count, 1, NULL);
	zassert_true(zcbor_uint32_expect(state, 42), NULL);
	zassert_equal_ptr(result, &dummy_entry_func_result, NULL);
	*result = 42;
	zassert_equal(state->constant_state->manually_process_elem, true, NULL);
	zassert_equal(state->constant_state->num_backups, 2, "%d\n", state->constant_state->num_backups);
#ifdef ZCBOR_MAP_SMART_SEARCH
	size_t elem_state_bytes = state->constant_state->map_search_elem_state_end
				- state->decode_state.map_search_elem_state;
	zassert_equal(elem_state_bytes, sizeof(zcbor_state_t) + 1, "%d != %d\n",
		      elem_state_bytes, sizeof(zcbor_state_t) + 1);
#endif
	return true;
}


bool dummy_entry_function_fail(zcbor_state_t *state, uint32_t *result)
{
	/* This function is never called, but it is used to test the
	 * zcbor_entry_function_with_elem_states() function.
	 */
	ZCBOR_ERR(ZCBOR_ERR_WRONG_VALUE);
}


ZTEST(zcbor_unit_tests, test_entry_function)
{
	size_t payload_len_out;
	int err;

#ifdef ZCBOR_MAP_SMART_SEARCH
	zcbor_state_t states[6];

	err = zcbor_entry_function_with_elem_states(dummy_entry_func_payload, sizeof(dummy_entry_func_payload),
					&dummy_entry_func_result, &payload_len_out, states,
					ZCBOR_CAST_FP(dummy_entry_function),
					sizeof(states) / sizeof(zcbor_state_t) - 2, 1, (sizeof(zcbor_state_t) * 8) + 1);
	zassert_equal(err, ZCBOR_ERR_NO_FLAG_MEM, "err: %d\n", err);
#else
	zcbor_state_t states[4]; /* 2 less since these aren't used for elem_state. */
#endif

	err = zcbor_entry_function_with_elem_states(dummy_entry_func_payload, sizeof(dummy_entry_func_payload),
					&dummy_entry_func_result, &payload_len_out, states,
					ZCBOR_CAST_FP(dummy_entry_function_fail),
					sizeof(states) / sizeof(zcbor_state_t), 1, (sizeof(zcbor_state_t) * 8) + 1);
	zassert_equal(err, ZCBOR_ERR_WRONG_VALUE, "err: %d\n", err);

	err = zcbor_entry_function_with_elem_states(dummy_entry_func_payload, sizeof(dummy_entry_func_payload),
					&dummy_entry_func_result, &payload_len_out, states,
					ZCBOR_CAST_FP(dummy_entry_function),
					sizeof(states) / sizeof(zcbor_state_t), 1, (sizeof(zcbor_state_t) * 8) + 1);
	zassert_equal(err, ZCBOR_SUCCESS, "err: %d\n", err);
	zassert_equal(payload_len_out, 2, NULL);
	zassert_equal(dummy_entry_func_result, 42, NULL);
}


#ifdef ZCBOR_CANONICAL
/* Allocate a large enough buffer for 65540 elements + header */
static uint8_t huge_payload[66000];
#endif

/**
 * Test that size hints in zcbor_list_start_encode can be different than the encoded size.
 *
 * This test verifies the behavior described in the zcbor_encode.h documentation:
 * "Can be 0 if unknown, but note that using a smaller size_hint than the actual
 * number of elements will risk the _end_encode function failing because the
 * payload buffer is exhausted."
 *
 * The test checks both success and failure cases when the size hint is smaller
 * than the actual encoded size.
 */
ZTEST(zcbor_unit_tests, test_size_hint)
{
#ifndef ZCBOR_CANONICAL
	printf("Skip on non-canonical builds.\n");
#else

#ifdef ZCBOR_VERBOSE
	/* If printing is enabled, reduce the scope of the test to reduce the time taken for printing. */
	size_t num_elems[] = {1, 24, 256};
#else
	size_t num_elems[] = {1, 24, 256, 65539};
#endif

#if SIZE_MAX == UINT32_MAX
	/* 4-byte size_t cannot contain the biggest size hint. */
	size_t size_hints[] = {10, 32, 260, 100000};
#else
	size_t size_hints[] = {10, 32, 260, 100000, 0x100000010};
#endif

	size_t num_hints = ARRAY_SIZE(size_hints);
	size_t num_num_elems = ARRAY_SIZE(num_elems);

	size_t header_sizes[] = {1, 2, 3, 5, 9};

	/* Successful header size change from 1, 2, 3, 5, (9) bytes to 1, 2, 3, 5 bytes */
	for (int i = 0; i < num_num_elems; i++) {
		for (int j = 0; j < num_hints; j++) {

			ZCBOR_STATE_E(state_e, 1, huge_payload, sizeof(huge_payload), 0);

			/* Use size hint from the list to trigger a 1, 2, 3, or 5 byte preliminary header. */
			zassert_true(zcbor_list_start_encode(state_e, size_hints[j]),
				"Failed to start list with size hint %zu", size_hints[j]);

			/* Encode a number of elements to trigger 1, 2, 3, or 5 byte actual header. */
			for (int k = 0; k < num_elems[i]; k++) {
				zassert_true(zcbor_uint32_put(state_e, k % 24),
					"Failed to encode element %d", k);
			}

			const uint8_t *payload_before = state_e->payload;

			/* This will cause a memmove if i != j, i.e. if the preliminary header size
			 * is different from the actual header size. */
			zassert_true(zcbor_list_end_encode(state_e, size_hints[j]));

			size_t header_size_before = header_sizes[j];
			size_t header_size_after = header_sizes[i];
			zassert_equal(state_e->payload - payload_before, header_size_after - header_size_before);

			if (i == 0) {
				/* Element count value in header */
				zassert_equal(huge_payload[0], 0x80 | num_elems[i]);
			} else {
				/* Element count size in header */
				zassert_equal(huge_payload[0], 0x97 + i);
			}

			/* Decode and verify the encoded data */
			ZCBOR_STATE_D(state_d, 1, huge_payload, sizeof(huge_payload), 1, 0);

			zassert_true(zcbor_list_start_decode(state_d));
			zassert_equal(num_elems[i], state_d->elem_count);

			for (int k = 0; k < num_elems[i]; k++) {
				zassert_true(zcbor_uint32_expect(state_d, k % 24),
					"Failed to decode element %d", k);
			}

			zassert_true(zcbor_list_end_decode(state_d, true));
		}
	}

	/* Unsuccessful header size change from 1, 2, 3 bytes to 2, 3, 5 bytes with insufficient buffer */
	for (int i = 1; i < num_num_elems; i++) {
		for (int j = 0; j < num_hints && size_hints[j] < num_elems[i]; j++) {
			/* Create a buffer that's just barely too small for the expansion needed */
			ZCBOR_STATE_E(state_e, 1, huge_payload, num_elems[i] + header_sizes[i] - 1, 0);

			/* Use size hint from list */
			zassert_true(zcbor_list_start_encode(state_e, size_hints[j]),
				"Failed to start list with size hint %zu", size_hints[j]);

			/* Encode elements (should be successful because size hint is smaller than actual) */
			for (int k = 0; k < num_elems[i]; k++) {
				zassert_true(zcbor_uint32_put(state_e, k % 24),
					"Failed to encode element %d", k);
			}

			/* This should fail due to insufficient buffer space for 4-byte expansion */
			zassert_false(zcbor_list_end_encode(state_e, size_hints[j]),
				"Expected failure: buffer too small for header expansion");

			zassert_equal(ZCBOR_ERR_NO_PAYLOAD, zcbor_peek_error(state_e),
				"Expected ZCBOR_ERR_NO_PAYLOAD when buffer exhausted during large header expansion");
		}
	}
#endif
}


/* Test the error reported when exit_backup() has an error stashed and then also fails to
 * consume the backup. zcbor_process_backup() registers an error of its own in that case,
 * but the stashed error is what stopped the decoding, so that is the one the caller must
 * be told about.
 * The backup is consumed up front to make zcbor_process_backup() fail, which is the
 * situation a caller ends up in when its backup handling is mismatched. */
ZTEST(zcbor_unit_tests, test_end_decode_force_no_backup)
{
	/* Indefinite length list: [1, 2] */
	uint8_t list_payload[] = {0x9F, 0x01, 0x02, 0xFF};

	/* First test without backup failure. */
	ZCBOR_STATE_D(state_d, 2, list_payload, sizeof(list_payload), 10, 0);
	state_d->constant_state->stop_on_error = true;
	state_d->constant_state->enforce_canonical = false;

	zassert_true(zcbor_list_start_decode(state_d), NULL);
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);

	/* array_end_expect() fails because the list end (0xFF) has not been reached, so its
	 * error is stashed across the zcbor_process_backup() call, and the backup
	 * should be consumed regardless of the error and the stop_on_error. */
	zassert_false(zcbor_list_end_decode(state_d, true), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, zcbor_peek_error(state_d), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d)));
	zassert_equal(0, state_d->constant_state->current_backup, NULL);


	/* Test with backup failure. */
	ZCBOR_STATE_D(state_d2, 2, list_payload, sizeof(list_payload), 10, 0);
	state_d2->constant_state->stop_on_error = true;
	state_d2->constant_state->enforce_canonical = false;

	zassert_true(zcbor_list_start_decode(state_d2), NULL);
	zassert_true(zcbor_int32_expect(state_d2, 1), NULL);

	zassert_true(zcbor_process_backup(state_d2, ZCBOR_FLAG_CONSUME, ZCBOR_MAX_ELEM_COUNT), NULL);
	zassert_equal(0, state_d2->constant_state->current_backup, NULL);

	/* array_end_expect() fails because the list end (0xFF) has not been reached, so its
	 * error is stashed across the zcbor_process_backup() call, which then fails with
	 * ZCBOR_ERR_NO_BACKUP_ACTIVE. */
	zassert_false(zcbor_list_end_decode(state_d2, true), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_TYPE, zcbor_peek_error(state_d2), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d2)));
	zassert_equal(0, state_d2->constant_state->current_backup, NULL);
}



bool dummy_function_extra_backup(zcbor_state_t *state, uint32_t *result)
{
	/* This function tests logic for checking the consistency of backups. */
	(void)result;
	zassert_true(zcbor_new_backup(state, 0), NULL);
	return true;
}


bool dummy_function_removed_backup(zcbor_state_t *state, uint32_t *result)
{
	/* This function tests logic for checking the consistency of backups. */
	zassert_true(zcbor_process_backup(state, ZCBOR_FLAG_CONSUME, ZCBOR_MAX_ELEM_COUNT), NULL);
	return true;
}


bool dummy_function_extra_backup_fail(zcbor_state_t *state, uint32_t *result)
{
	/* Leave a backup behind and fail, to check that the reported error is the one
	 * from here, and not the leftover backup. */
	(void)result;
	zassert_true(zcbor_new_backup(state, 0), NULL);
	zcbor_error(state, ZCBOR_ERR_WRONG_TYPE);
	return false;
}


bool dummy_function_extra_backup_nomatch(zcbor_state_t *state, uint32_t *result)
{
	/* Change the backup count and then report failure, so the mismatch has to be caught
	 * on the path taken when the called function fails. */
	(void)result;
	zassert_true(zcbor_new_backup(state, 0), NULL);
	return false;
}


bool dummy_function_removed_backup_nomatch(zcbor_state_t *state, uint32_t *result)
{
	/* The same, in the other direction. */
	(void)result;
	zassert_true(zcbor_process_backup(state, ZCBOR_FLAG_CONSUME, ZCBOR_MAX_ELEM_COUNT), NULL);
	return false;
}


bool dummy_function_extra_backup_e(zcbor_state_t *state, const uint32_t *result)
{
	return dummy_function_extra_backup(state, (uint32_t *)result);
}


bool dummy_function_removed_backup_e(zcbor_state_t *state, const uint32_t *result)
{
	return dummy_function_removed_backup(state, (uint32_t *)result);
}


bool dummy_function_extra_backup_nomatch_e(zcbor_state_t *state, const uint32_t *result)
{
	return dummy_function_extra_backup_nomatch(state, (uint32_t *)result);
}


ZTEST(zcbor_unit_tests, test_backup_mismatch)
{
	zcbor_state_t states[4];
	uint8_t dummy_payload[10] = {0xA1, 0x18, 42, 0x18, 43 /* uint: 42, 43 */};
	uint8_t payload[10];
	bool ret;
	int err = zcbor_entry_function(dummy_payload, sizeof(dummy_payload),
					&dummy_entry_func_result, NULL, states,
					ZCBOR_CAST_FP(dummy_function_extra_backup),
					sizeof(states) / sizeof(zcbor_state_t), 1);
	zassert_equal(err, ZCBOR_ERR_BACKUP_MISMATCH, "err: %d\n", err);

	err = zcbor_entry_function(dummy_payload, sizeof(dummy_payload),
					&dummy_entry_func_result, NULL, states,
					ZCBOR_CAST_FP(dummy_function_extra_backup_fail),
					sizeof(states) / sizeof(zcbor_state_t), 1);
	zassert_equal(err, ZCBOR_ERR_WRONG_TYPE, "err: %s\n", zcbor_error_str(err));

	ZCBOR_STATE_D(state_d1, 2, dummy_payload, sizeof(dummy_payload), 1, 0);
	ret = zcbor_multi_decode_w_backup(1, 2, NULL, ZCBOR_CAST_FP(dummy_function_extra_backup), state_d1, NULL, 0);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_d1), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d1)));

	ZCBOR_STATE_D(state_d2, 2, dummy_payload, sizeof(dummy_payload), 1, 0);
	zassert_true(zcbor_new_backup(state_d2, 0), NULL);
	ret = zcbor_multi_decode_w_backup(1, 2, NULL, ZCBOR_CAST_FP(dummy_function_removed_backup), state_d2, NULL, 0);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_d2), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d2)));

	ZCBOR_STATE_D(state_d3, 2, dummy_payload, sizeof(dummy_payload), 1, 0);
	ret = zcbor_multi_decode(1, 2, NULL, ZCBOR_CAST_FP(dummy_function_extra_backup), state_d3, NULL, 0);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_d3), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d3)));

	ZCBOR_STATE_D(state_d4, 2, dummy_payload, sizeof(dummy_payload), 1, 0);
	zassert_true(zcbor_new_backup(state_d4, 0), NULL);
	ret = zcbor_multi_decode(1, 2, NULL, ZCBOR_CAST_FP(dummy_function_removed_backup), state_d4, NULL, 0);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_d4), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d4)));

	ZCBOR_STATE_D(state_d5, 2, dummy_payload, sizeof(dummy_payload), 1, 8);
	ret = zcbor_unordered_map_start_decode(state_d5);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d5)));
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(dummy_function_extra_backup), state_d5, NULL);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_d5), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d5)));

	ZCBOR_STATE_D(state_d6, 3, dummy_payload, sizeof(dummy_payload), 1, 8);
	zassert_true(zcbor_new_backup(state_d6, 1), NULL);
	ret = zcbor_unordered_map_start_decode(state_d6);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d6)));
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(dummy_function_removed_backup), state_d6, NULL);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_d6), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d6)));


	ZCBOR_STATE_E(state_e1, 2, payload, sizeof(payload), 20);
	ret = zcbor_multi_encode(1, ZCBOR_CAST_FP(dummy_function_extra_backup_e), state_e1, NULL, 0);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_e1), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_e1)));

	ZCBOR_STATE_E(state_e2, 2, payload, sizeof(payload), 20);
	zassert_true(zcbor_new_backup(state_e2, 0), NULL);
	ret = zcbor_multi_encode(1, ZCBOR_CAST_FP(dummy_function_removed_backup_e), state_e2, NULL, 0);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_e2), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_e2)));

	/* The cases above all call a function that succeeds, which means they only ever
	 * reach the checks on the success path. Repeat them with functions that report
	 * failure, so the checks on the failure path are covered too. */
	size_t num_decode;

	ZCBOR_STATE_D(state_d7, 2, dummy_payload, sizeof(dummy_payload), 1, 8);
	ret = zcbor_unordered_map_start_decode(state_d7);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d7)));
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(dummy_function_extra_backup_nomatch), state_d7, NULL);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_d7), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d7)));

	ZCBOR_STATE_D(state_d8, 3, dummy_payload, sizeof(dummy_payload), 1, 8);
	zassert_true(zcbor_new_backup(state_d8, 1), NULL);
	ret = zcbor_unordered_map_start_decode(state_d8);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d8)));
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(dummy_function_removed_backup_nomatch), state_d8, NULL);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_d8), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d8)));

	ZCBOR_STATE_D(state_d9, 2, dummy_payload, sizeof(dummy_payload), 1, 0);
	ret = zcbor_multi_decode(1, 2, &num_decode, ZCBOR_CAST_FP(dummy_function_extra_backup_nomatch), state_d9, NULL, 0);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_d9), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d9)));

	ZCBOR_STATE_D(state_d10, 2, dummy_payload, sizeof(dummy_payload), 1, 0);
	ret = zcbor_multi_decode_w_backup(1, 2, &num_decode, ZCBOR_CAST_FP(dummy_function_removed_backup_nomatch), state_d10, NULL, 0);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_d10), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d10)));

	/* The checks above are all backed up by the one at the top of the next loop iteration.
	 * Here the value cannot be skipped, so the loop ends instead, and the mismatch must
	 * still take precedence over the error from the payload. */
	uint8_t unskippable_payload[] = {0xA1, 0x18, 42, 0x1C};

	ZCBOR_STATE_D(state_d11, 2, unskippable_payload, sizeof(unskippable_payload), 1, 8);
	ret = zcbor_unordered_map_start_decode(state_d11);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d11)));
	ret = zcbor_unordered_map_search(ZCBOR_CAST_FP(dummy_function_extra_backup_nomatch), state_d11, NULL);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_d11), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d11)));

	/* A state array of 1 has no constant_state at all, so the backup checks must reach
	 * the backup count through zcbor_get_backup_num() rather than dereferencing it. */
	zcbor_state_t state_no_const_e[1];
	zcbor_new_encode_state(state_no_const_e, 1, payload, sizeof(payload), 0);
	zassert_is_null(state_no_const_e[0].constant_state, NULL);
	zassert_true(zcbor_multi_encode(1, ZCBOR_CAST_FP(zcbor_int32_encode), state_no_const_e, &(int32_t){14}, 0), NULL);

	zcbor_state_t state_no_const_d[1];
	zcbor_new_decode_state(state_no_const_d, 1, dummy_payload, sizeof(dummy_payload), 1, NULL, 0);
	zassert_is_null(state_no_const_d[0].constant_state, NULL);
	zassert_true(zcbor_multi_decode(1, 1, &num_decode, ZCBOR_CAST_FP(zcbor_any_skip), state_no_const_d, NULL, 0), NULL);

	/* zcbor_multi_encode() checks inside the loop, so the leak is caught even though the
	 * encoder fails in the same call. */
	ZCBOR_STATE_E(state_e3, 2, payload, sizeof(payload), 20);
	ret = zcbor_multi_encode(1, ZCBOR_CAST_FP(dummy_function_extra_backup_nomatch_e), state_e3, NULL, 0);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_BACKUP_MISMATCH, zcbor_peek_error(state_e3), "err: %s\n", zcbor_error_str(zcbor_peek_error(state_e3)));
}


ZTEST_SUITE(zcbor_unit_tests, NULL, NULL, NULL, NULL, NULL);
