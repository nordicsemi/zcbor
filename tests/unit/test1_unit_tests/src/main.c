/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include "zcbor_decode.h"
#include "zcbor_encode.h"
#include "zcbor_print.h"

/* Helpers for checking that a failed call left the state completely untouched, i.e. that
 * decoding can continue as if the call had never happened. */
struct zcbor_dec_state_snapshot {
	const uint8_t *payload;
	const uint8_t *payload_end;
	size_t elem_count;
	size_t current_backup;
};

static void dec_snapshot(zcbor_state_t *state, struct zcbor_dec_state_snapshot *snap)
{
	snap->payload = state->payload;
	snap->payload_end = state->payload_end;
	snap->elem_count = state->elem_count;
	snap->current_backup = state->constant_state->current_backup;
}

static void dec_assert_unchanged(zcbor_state_t *state, const struct zcbor_dec_state_snapshot *snap,
				 const char *msg)
{
	zassert_equal(snap->payload, state->payload, "%s: payload", msg);
	zassert_equal(snap->payload_end, state->payload_end, "%s: payload_end", msg);
	zassert_equal(snap->elem_count, state->elem_count, "%s: elem_count", msg);
	zassert_equal(snap->current_backup, state->constant_state->current_backup, "%s: backup", msg);
}

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
	zassert_false(zcbor_int32_put(state_e, 1), NULL);
	zassert_false(zcbor_int64_put(state_e, 2), NULL);
	zassert_false(zcbor_uint32_put(state_e, 3), NULL);
	zassert_false(zcbor_uint64_put(state_e, 4), NULL);
	zassert_false(zcbor_size_put(state_e, 10), NULL);
	zassert_false(zcbor_int32_encode(state_e, &(int32_t){5}), NULL);
	zassert_false(zcbor_int64_encode(state_e, &(int64_t){6}), NULL);
	zassert_false(zcbor_uint32_encode(state_e, &(uint32_t){7}), NULL);
	zassert_false(zcbor_uint64_encode(state_e, &(uint64_t){8}), NULL);
	zassert_false(zcbor_size_encode(state_e, &(size_t){9}), NULL);
	zassert_false(zcbor_bstr_put_lit(state_e, "Hello"), NULL);
	zassert_false(zcbor_tstr_put_lit(state_e, "World"), NULL);
	zassert_false(zcbor_tag_put(state_e, 9), NULL);
	zassert_false(zcbor_tag_encode(state_e, &(uint32_t){10}));
	zassert_false(zcbor_bool_put(state_e, true), NULL);
	zassert_false(zcbor_bool_encode(state_e, &(bool){false}), NULL);
	zassert_false(zcbor_float32_put(state_e, 10.5), NULL);
	zassert_false(zcbor_float32_encode(state_e, &(float){11.6}), NULL);
	zassert_false(zcbor_float64_put(state_e, 12.7), NULL);
	zassert_false(zcbor_float64_encode(state_e, &(double){13.8}), NULL);
	zassert_false(zcbor_nil_put(state_e, NULL), NULL);
	zassert_false(zcbor_undefined_put(state_e, NULL), NULL);
	zassert_false(zcbor_bstr_start_encode(state_e), NULL);
	zassert_false(zcbor_bstr_end_encode(state_e, NULL), NULL);
	zassert_false(zcbor_list_start_encode(state_e, 1), NULL);
	zassert_false(zcbor_map_start_encode(state_e, 0), NULL);
	zassert_false(zcbor_map_end_encode(state_e, 0), NULL);
	zassert_false(zcbor_list_end_encode(state_e, 1), NULL);
	zassert_false(zcbor_multi_encode(1, (zcbor_encoder_t *)zcbor_int32_put, state_e, (void*)14, 0), NULL);
	zassert_false(zcbor_multi_encode_minmax(1, 1, &(size_t){1}, (zcbor_encoder_t *)zcbor_int32_put, state_e, (void*)15, 0), NULL);


	zassert_mem_equal(&state_backup, state_e, sizeof(state_backup), NULL);
	zassert_mem_equal(&constant_state_backup, state_e->constant_state, sizeof(constant_state_backup), NULL);

	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, zcbor_peek_error(state_e), NULL);
	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, zcbor_pop_error(state_e), NULL);
	zassert_equal(ZCBOR_SUCCESS, zcbor_peek_error(state_e), NULL);

	/* All succeed since the error has been popped. */
	zassert_true(zcbor_int32_put(state_e, 1), NULL);
	zassert_true(zcbor_int64_put(state_e, 2), NULL);
	zassert_true(zcbor_uint32_put(state_e, 3), NULL);
	zassert_true(zcbor_uint64_put(state_e, 4), NULL);
	zassert_true(zcbor_size_put(state_e, 10), NULL);
	zassert_true(zcbor_int32_encode(state_e, &(int32_t){5}), NULL);
	zassert_true(zcbor_int64_encode(state_e, &(int64_t){6}), NULL);
	zassert_true(zcbor_uint32_encode(state_e, &(uint32_t){7}), NULL);
	zassert_true(zcbor_uint64_encode(state_e, &(uint64_t){8}), NULL);
	zassert_true(zcbor_size_encode(state_e, &(size_t){9}), NULL);
	zassert_true(zcbor_bstr_put_lit(state_e, "Hello"), NULL);
	zassert_true(zcbor_tstr_put_lit(state_e, "World"), NULL);
	zassert_true(zcbor_tag_put(state_e, 9), NULL);
	zassert_true(zcbor_tag_encode(state_e, &(uint32_t){10}));
	zassert_true(zcbor_bool_put(state_e, true), NULL);
	zassert_true(zcbor_bool_encode(state_e, &(bool){false}), NULL);
	zassert_true(zcbor_float32_put(state_e, 10.5), NULL);
	zassert_true(zcbor_float32_encode(state_e, &(float){11.6}), NULL);
	zassert_true(zcbor_float64_put(state_e, 12.7), NULL);
	zassert_true(zcbor_float64_encode(state_e, &(double){13.8}), NULL);
	zassert_true(zcbor_nil_put(state_e, NULL), NULL);
	zassert_true(zcbor_undefined_put(state_e, NULL), NULL);
	zassert_true(zcbor_bstr_start_encode(state_e), NULL);
	zassert_true(zcbor_bstr_end_encode(state_e, NULL), NULL);
	zassert_true(zcbor_list_start_encode(state_e, 1), NULL);
	zassert_true(zcbor_map_start_encode(state_e, 0), NULL);
	zassert_true(zcbor_map_end_encode(state_e, 0), NULL);
	zassert_true(zcbor_list_end_encode(state_e, 1), NULL);
	zassert_true(zcbor_multi_encode(1, (zcbor_encoder_t *)zcbor_int32_put, state_e, (void*)14, 0), NULL);
	zassert_true(zcbor_multi_encode_minmax(1, 1, &(size_t){1}, (zcbor_encoder_t *)zcbor_int32_put, state_e, (void*)15, 0), NULL);

	ZCBOR_STATE_D(state_d, 3, payload, sizeof(payload), 30, 0);
	state_d->constant_state->stop_on_error = true;

	zassert_false(zcbor_int32_expect(state_d, 2), NULL);
	zassert_equal(ZCBOR_ERR_WRONG_VALUE, state_d->constant_state->error, "%d\r\n", state_d->constant_state->error);
	memcpy(&state_backup, state_d, sizeof(state_backup));
	memcpy(&constant_state_backup, state_d->constant_state, sizeof(constant_state_backup));

	/* All fail because of ZCBOR_ERR_WRONG_VALUE */
	zassert_false(zcbor_int32_expect(state_d, 1), NULL);
	zassert_false(zcbor_int64_expect(state_d, 2), NULL);
	zassert_false(zcbor_uint32_expect(state_d, 3), NULL);
	zassert_false(zcbor_uint64_expect(state_d, 4), NULL);
	zassert_false(zcbor_size_expect(state_d, 10), NULL);
	zassert_false(zcbor_int32_decode(state_d, &(int32_t){5}), NULL);
	zassert_false(zcbor_int64_decode(state_d, &(int64_t){6}), NULL);
	zassert_false(zcbor_uint32_decode(state_d, &(uint32_t){7}), NULL);
	zassert_false(zcbor_uint64_decode(state_d, &(uint64_t){8}), NULL);
	zassert_false(zcbor_size_decode(state_d, &(size_t){9}), NULL);
	zassert_false(zcbor_bstr_expect_lit(state_d, "Hello"), NULL);
	zassert_false(zcbor_tstr_expect_lit(state_d, "World"), NULL);
	zassert_false(zcbor_tag_decode(state_d, &(uint32_t){9}), NULL);
	zassert_false(zcbor_tag_expect(state_d, 10), NULL);
	zassert_false(zcbor_bool_expect(state_d, true), NULL);
	zassert_false(zcbor_bool_decode(state_d, &(bool){false}), NULL);
	zassert_false(zcbor_float32_expect(state_d, 10.5), NULL);
	zassert_false(zcbor_float32_decode(state_d, &(float){11.6}), NULL);
	zassert_false(zcbor_float64_expect(state_d, 12.7), NULL);
	zassert_false(zcbor_float64_decode(state_d, &(double){13.8}), NULL);
	zassert_false(zcbor_nil_expect(state_d, NULL), NULL);
	zassert_false(zcbor_undefined_expect(state_d, NULL), NULL);
	zassert_false(zcbor_bstr_start_decode(state_d, &dummy_string), NULL);
	zassert_false(zcbor_bstr_end_decode(state_d), NULL);
	zassert_false(zcbor_list_start_decode(state_d), NULL);
	zassert_false(zcbor_map_start_decode(state_d), NULL);
	zassert_false(zcbor_map_end_decode(state_d), NULL);
	zassert_false(zcbor_list_end_decode(state_d), NULL);
	zassert_false(zcbor_multi_decode(1, 1, &(size_t){1}, (zcbor_decoder_t *)zcbor_int32_expect, state_d, (void*)14, 0), NULL);
	zassert_false(zcbor_present_decode(&(bool){true}, (zcbor_decoder_t *)zcbor_int32_expect, state_d, (void*)15), NULL);

	zassert_mem_equal(&state_backup, state_d, sizeof(state_backup), NULL);
	zassert_mem_equal(&constant_state_backup, state_d->constant_state, sizeof(constant_state_backup), NULL);

	zassert_equal(ZCBOR_ERR_WRONG_VALUE, zcbor_pop_error(state_d), NULL);

	/* All succeed since the error has been popped. */
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);
	zassert_true(zcbor_int64_expect(state_d, 2), NULL);
	zassert_true(zcbor_uint32_expect(state_d, 3), NULL);
	zassert_true(zcbor_uint64_expect(state_d, 4), NULL);
	zassert_true(zcbor_size_expect(state_d, 10), NULL);
	zassert_true(zcbor_int32_decode(state_d, &(int32_t){5}), NULL);
	zassert_true(zcbor_int64_decode(state_d, &(int64_t){6}), NULL);
	zassert_true(zcbor_uint32_decode(state_d, &(uint32_t){7}), NULL);
	zassert_true(zcbor_uint64_decode(state_d, &(uint64_t){8}), NULL);
	zassert_true(zcbor_size_decode(state_d, &(size_t){9}), NULL);
	zassert_true(zcbor_bstr_expect_lit(state_d, "Hello"), NULL);
	zassert_true(zcbor_tstr_expect_lit(state_d, "World"), NULL);
	zassert_true(zcbor_tag_decode(state_d, &(uint32_t){9}), NULL);
	zassert_true(zcbor_tag_expect(state_d, 10), NULL);
	zassert_true(zcbor_bool_expect(state_d, true), NULL);
	zassert_true(zcbor_bool_decode(state_d, &(bool){false}), NULL);
	zassert_true(zcbor_float32_expect(state_d, 10.5), NULL);
	zassert_true(zcbor_float32_decode(state_d, &(float){11.6}), NULL);
	zassert_true(zcbor_float64_expect(state_d, 12.7), NULL);
	zassert_true(zcbor_float64_decode(state_d, &(double){13.8}), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	zassert_true(zcbor_undefined_expect(state_d, NULL), NULL);
	zassert_true(zcbor_bstr_start_decode(state_d, &dummy_string), NULL);
	zassert_true(zcbor_bstr_end_decode(state_d), NULL);
	zassert_true(zcbor_list_start_decode(state_d), NULL);
	zassert_true(zcbor_map_start_decode(state_d), NULL);
	zassert_true(zcbor_map_end_decode(state_d), NULL);
	zassert_true(zcbor_list_end_decode(state_d), NULL);
	zassert_true(zcbor_multi_decode(1, 1, &(size_t){1}, (zcbor_decoder_t *)zcbor_int32_expect, state_d, (void*)14, 0), NULL);
	zassert_true(zcbor_present_decode(&(bool){1}, (zcbor_decoder_t *)zcbor_int32_expect, state_d, (void*)15), NULL);

	/* Everything has been decoded. */
	zassert_equal(state_e->payload, state_d->payload, NULL);
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

	zassert_true(zcbor_bstr_put_lit(state_e, "HelloWorld"), NULL);

	ZCBOR_STATE_D(state_d, 0, payload, 8, 1, 0);
	ZCBOR_STATE_D(state_d2, 0, payload, sizeof(payload), 1, 0);

	zassert_true(zcbor_bstr_decode(state_d2, &output), NULL);
	zassert_false(zcbor_payload_at_end(state_d2), NULL);
	zassert_false(zcbor_bstr_decode(state_d, &output), NULL);
	zassert_false(zcbor_payload_at_end(state_d), NULL);
	zassert_true(zcbor_bstr_decode_fragment(state_d, &output_frags[0]), NULL);
	zassert_equal_ptr(&payload[1], output_frags[0].fragment.value, NULL);
	zassert_equal(7, output_frags[0].fragment.len, NULL);
	zassert_equal(10, output_frags[0].total_len, "%d != %d\r\n", 10, output_frags[0].total_len);
	zassert_equal(0, output_frags[0].offset, NULL);
	zassert_false(zcbor_is_last_fragment(&output_frags[0]), NULL);

	zassert_true(zcbor_payload_at_end(state_d), NULL);
	zcbor_update_state(state_d, &payload[8], sizeof(payload) - 8);
	zassert_false(zcbor_bstr_decode_fragment(state_d, &output_frags[1]), NULL);
	zcbor_next_fragment(state_d, &output_frags[0], &output_frags[1]);
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
	uint8_t frag_payload[15];

	ZCBOR_STATE_E(state_e, 0, payload, sizeof(payload), 0);
	struct zcbor_string output;
	struct zcbor_string_fragment output_frags[18];
	struct zcbor_string_fragment output_frags_backup;

	// Start positive test

	zassert_true(zcbor_bstr_put_lit(state_e,
		"HelloWorld1HelloWorld2HelloWorld3HelloWorld4HelloWorld5HelloWorld6" \
		"HelloWorld7HelloWorld8HelloWorld9HelloWorldAHelloWorldBHelloWorldC" \
		"HelloWorldDHelloWorldEHelloWorldFHelloWorldGHelloWorldHHelloWorldI"),
		NULL);

	ZCBOR_STATE_D(state_d, 0, payload, 13, 1, 0);
	ZCBOR_STATE_D(state_d2, 0, payload, sizeof(payload), 1, 0);

	zassert_true(zcbor_bstr_decode(state_d2, &output), NULL);
	zassert_true(zcbor_bstr_decode_fragment(state_d, &output_frags[0]), NULL);

	for (int i = 1; i < 18; i++) {
		zassert_true(zcbor_payload_at_end(state_d), NULL);
		zassert_false(zcbor_is_last_fragment(&output_frags[i - 1]), NULL);
		memcpy(frag_payload, &payload[11 * i + 2], 11); // + 2 because of the CBOR header
		zcbor_update_state(state_d, frag_payload, 11);
		zcbor_next_fragment(state_d, &output_frags[i - 1], &output_frags[i]);
	}
	zassert_true(zcbor_payload_at_end(state_d), NULL);
	zassert_true(zcbor_is_last_fragment(&output_frags[17]), NULL);

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
 *  This structures is then split in three fragments (output_frags) like so:
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
	ZCBOR_STATE_E(state_e, 2, payload, sizeof(payload), 0);
	struct zcbor_string output;
	struct zcbor_string_fragment output_frags[3];
	struct zcbor_string_fragment tstr_frags[2];

	zassert_true(zcbor_bstr_start_encode(state_e), NULL); // 1 B
	zassert_true(zcbor_uint32_put(state_e, 42), NULL); // 2 B
	zassert_true(zcbor_tstr_put_lit(state_e, "Hello World"), NULL); // 12 B
	zassert_true(zcbor_list_start_encode(state_e, 2), NULL); // 1 B
	zassert_true(zcbor_bool_put(state_e, true), NULL); // 1 B
	zassert_true(zcbor_nil_put(state_e, NULL), NULL); // 1 B
	zassert_true(zcbor_list_end_encode(state_e, 2), NULL); // 1 B
	zassert_true(zcbor_bstr_end_encode(state_e, NULL), NULL); // 0 B

	ZCBOR_STATE_D(state_d, 2, payload, 8, 1, 0);
	ZCBOR_STATE_D(state_d2, 0, payload, sizeof(payload), 1, 0);

#ifdef ZCBOR_CANONICAL
	#define EXP_TOTAL_LEN 17
#else
	#define EXP_TOTAL_LEN 18
#endif

	zassert_true(zcbor_bstr_decode(state_d2, &output), NULL);
	zassert_false(zcbor_bstr_start_decode(state_d, &output), NULL);
	zassert_true(zcbor_bstr_start_decode_fragment(state_d, &output_frags[0]), NULL);
	zassert_equal_ptr(&payload[1], output_frags[0].fragment.value, NULL);
	zassert_equal(7, output_frags[0].fragment.len, NULL);
	zassert_equal(EXP_TOTAL_LEN, output_frags[0].total_len, "%d != %d\r\n", EXP_TOTAL_LEN, output_frags[0].total_len);
	zassert_equal(0, output_frags[0].offset, NULL);
	zassert_false(zcbor_is_last_fragment(&output_frags[0]), NULL);
	zassert_true(zcbor_uint32_expect(state_d, 42), NULL);
	zassert_false(zcbor_tstr_expect_lit(state_d, "Hello World"), NULL);
	zassert_true(zcbor_tstr_decode_fragment(state_d, &tstr_frags[0]), NULL);
	zassert_equal_ptr(&payload[4], tstr_frags[0].fragment.value, NULL);
	zassert_equal(4, tstr_frags[0].fragment.len, NULL);
	zassert_equal(11, tstr_frags[0].total_len, NULL);
	zassert_equal(0, tstr_frags[0].offset, NULL);

	zassert_true(zcbor_payload_at_end(state_d), NULL);
	zcbor_update_state(state_d, &payload[8], 8);
	zassert_false(zcbor_bstr_decode_fragment(state_d, &output_frags[1]), NULL);
	zcbor_bstr_next_fragment(state_d, &output_frags[0], &output_frags[1]);
	zassert_equal_ptr(&payload[8], output_frags[1].fragment.value, NULL);
	zassert_equal(8, output_frags[1].fragment.len, "%d != %d\r\n", 3, output_frags[1].fragment.len);
	zassert_equal(EXP_TOTAL_LEN, output_frags[1].total_len, "%d != %d\r\n", EXP_TOTAL_LEN, output_frags[1].total_len);
	zassert_equal(7, output_frags[1].offset, NULL);
	zassert_false(zcbor_is_last_fragment(&output_frags[1]), NULL);
	zcbor_next_fragment(state_d, &tstr_frags[0], &tstr_frags[1]);
	zassert_equal_ptr(&payload[8], tstr_frags[1].fragment.value, NULL);
	zassert_equal(7, tstr_frags[1].fragment.len, "%d != %d\r\n", 7, tstr_frags[1].fragment.len);
	zassert_equal(11, tstr_frags[1].total_len, NULL);
	zassert_equal(4, tstr_frags[1].offset, NULL);
	zassert_true(zcbor_is_last_fragment(&tstr_frags[1]), NULL);
	zassert_true(zcbor_list_start_decode(state_d), NULL);

	zassert_true(zcbor_payload_at_end(state_d), NULL);
	zcbor_update_state(state_d, &payload[16], sizeof(payload) - 16);
	zassert_false(zcbor_bstr_decode_fragment(state_d, &output_frags[2]), NULL);
	zcbor_bstr_next_fragment(state_d, &output_frags[1], &output_frags[2]);
	zassert_equal_ptr(&payload[16], output_frags[2].fragment.value, NULL);
	zassert_equal(EXP_TOTAL_LEN - 15,
			output_frags[2].fragment.len, NULL);
	zassert_equal(EXP_TOTAL_LEN, output_frags[2].total_len, NULL);
	zassert_equal(15, output_frags[2].offset, NULL);
	zassert_true(zcbor_is_last_fragment(&output_frags[2]), NULL);
	zassert_true(zcbor_bool_expect(state_d, true), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	zassert_true(zcbor_list_end_decode(state_d), NULL);

	uint8_t spliced[19];
	output.value = spliced;
	output.len = sizeof(spliced);

	zassert_true(zcbor_validate_string_fragments(output_frags, 3), NULL);
	zassert_true(zcbor_splice_string_fragments(output_frags, 3, spliced, &output.len), NULL);

	zassert_equal(EXP_TOTAL_LEN, output.len, NULL);
	zassert_mem_equal(output.value, &payload[1], EXP_TOTAL_LEN, NULL);

	output.len = sizeof(spliced);

	zassert_true(zcbor_validate_string_fragments(tstr_frags, 2), NULL);
	zassert_true(zcbor_splice_string_fragments(tstr_frags, 2, spliced, &output.len), NULL);

	zassert_equal(11, output.len, NULL);
	zassert_mem_equal(output.value, &payload[4], 11, NULL);
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
	zassert_false(zcbor_list_end_encode(state_e1, 10), NULL);
	zassert_equal(ZCBOR_ERR_HIGH_ELEM_COUNT, zcbor_pop_error(state_e1), NULL);

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
	zassert_equal(int64, 1234567812345678, "%d\r\n", int64);

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
	zassert_equal(uint64, 1234567812345678, "%d\r\n", uint64);

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


ZTEST(zcbor_unit_tests, test_simple)
{
	uint8_t payload1[100];
	ZCBOR_STATE_E(state_e, 1, payload1, sizeof(payload1), 0);
	ZCBOR_STATE_D(state_d, 1, payload1, sizeof(payload1), 16, 0);
	uint8_t simple1 = 0;
	uint8_t simple2 = 2;

	zassert_true(zcbor_simple_encode(state_e, &simple1), NULL);
	zassert_true(zcbor_simple_put(state_e, 14), NULL);
	zassert_true(zcbor_simple_put(state_e, 22), NULL);
	zassert_true(zcbor_simple_put(state_e, 24), NULL);
	zassert_true(zcbor_simple_put(state_e, 255), NULL);

	zassert_true(zcbor_simple_decode(state_d, &simple2), NULL);
	zassert_true(zcbor_simple_expect(state_d, 14), NULL);
	zassert_true(zcbor_nil_expect(state_d, NULL), NULL);
	zassert_false(zcbor_undefined_expect(state_d, NULL), NULL);
	zassert_true(zcbor_simple_expect(state_d, 24), NULL);
	zassert_false(zcbor_simple_expect(state_d, 254), NULL);
	zassert_true(zcbor_simple_decode(state_d, &simple1), NULL);
	zassert_equal(0, simple2, NULL);
	zassert_equal(255, simple1, NULL);
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
	test_str(ZCBOR_ERR_MAX_DEPTH);
	test_str(ZCBOR_ERR_UNKNOWN);
	zassert_mem_equal(zcbor_error_str(-1), "ZCBOR_ERR_UNKNOWN", sizeof("ZCBOR_ERR_UNKNOWN"), NULL);
	zassert_mem_equal(zcbor_error_str(-10), "ZCBOR_ERR_UNKNOWN", sizeof("ZCBOR_ERR_UNKNOWN"), NULL);
	zassert_mem_equal(zcbor_error_str(ZCBOR_ERR_INVALID_VALUE_ENCODING + 1), "ZCBOR_ERR_UNKNOWN", sizeof("ZCBOR_ERR_UNKNOWN"), NULL);
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
	zassert_equal(state_d->payload, state_e->payload, "0x%x != 0x%x\n",
		state_d->payload, state_e->payload);
	zassert_equal(state_d->elem_count, --exp_elem_count, NULL);
}


/* zcbor_any_skip() fails if a list/map is missing elements, i.e. if there are fewer than its
 * header says, or if an indefinite-length map has a key without a value. */
ZTEST(zcbor_unit_tests, test_any_skip_elem_count)
{
	const struct {
		const char *name;
		uint8_t payload[7];
		size_t len;
		int err;
	} cases[] = {
		/* LIST(2): [1] */
		{"list missing an element", {0x82, 0x01}, 2, ZCBOR_ERR_NO_PAYLOAD},
		/* MAP(1): {1} */
		{"map missing a value", {0xA1, 0x01}, 2, ZCBOR_ERR_NO_PAYLOAD},
#ifndef ZCBOR_CANONICAL
		/* LIST(_): [LIST(2): [1]] */
		{"list ended by outer break", {0x9F, 0x82, 0x01, 0xFF}, 4, ZCBOR_ERR_ADDITIONAL_INVAL},
		/* MAP(_): {1} */
		{"indefinite map missing a value", {0xBF, 0x01, 0xFF}, 3, ZCBOR_ERR_MAP_MISALIGNED},
		/* MAP(_): {1: 2, 3} */
		{"indefinite map missing last value", {0xBF, 0x01, 0x02, 0x03, 0xFF}, 5,
			ZCBOR_ERR_MAP_MISALIGNED},
		/* LIST(1): [MAP(_): {1}] */
		{"nested indefinite map missing a value", {0x81, 0xBF, 0x01, 0xFF}, 4,
			ZCBOR_ERR_MAP_MISALIGNED},
		/* MAP(_): {} */
		{"empty indefinite map", {0xBF, 0xFF}, 2, ZCBOR_SUCCESS},
		/* MAP(_): {1: 2} */
		{"indefinite map", {0xBF, 0x01, 0x02, 0xFF}, 4, ZCBOR_SUCCESS},
		/* MAP(_): {1: MAP(_): {2: 3}} */
		{"indefinite map in indefinite map", {0xBF, 0x01, 0xBF, 0x02, 0x03, 0xFF, 0xFF}, 7,
			ZCBOR_SUCCESS},
#endif
	};
	struct zcbor_dec_state_snapshot snap;

	for (size_t i = 0; i < ZCBOR_ARRAY_SIZE(cases); i++) {
		ZCBOR_STATE_D(state_d, 0, cases[i].payload, cases[i].len, 1, 0);

		if (cases[i].err == ZCBOR_SUCCESS) {
			bool ret = zcbor_any_skip(state_d, NULL);
			zassert_true(ret, "%s err: %s\n", cases[i].name,
				zcbor_error_str(zcbor_peek_error(state_d)));
			zassert_equal(cases[i].payload + cases[i].len, state_d->payload, "%s\n",
				cases[i].name);
		} else {
			zcbor_state_t state_before;

			memcpy(&state_before, state_d, sizeof(state_before));
			dec_snapshot(state_d, &snap);
			zassert_false(zcbor_any_skip(state_d, NULL), "%s\n", cases[i].name);
			zassert_equal(cases[i].err, zcbor_peek_error(state_d), "%s err: %s\n",
				cases[i].name, zcbor_error_str(zcbor_peek_error(state_d)));
			dec_assert_unchanged(state_d, &snap, cases[i].name);
			zassert_mem_equal(&state_before, state_d, sizeof(state_before), "%s\n",
				cases[i].name);
		}
	}

	/* LIST(0x100000001): [0] */
	const uint8_t long_list[] = {0x9B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00};
	ZCBOR_STATE_D(state_d2, 0, long_list, sizeof(long_list), 1, 0);

	zassert_false(zcbor_any_skip(state_d2, NULL), NULL);
#if SIZE_MAX == UINT32_MAX
	/* The count doesn't fit in size_t, so it must not be truncated to 1. */
	zassert_equal(ZCBOR_ERR_INT_SIZE, zcbor_peek_error(state_d2), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d2)));
#else
	zassert_equal(ZCBOR_ERR_NO_PAYLOAD, zcbor_peek_error(state_d2), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d2)));
#endif
}


/* After skipping nested lists/maps, the state must be back in the list zcbor_any_skip() was
 * called in, both for indefinite and definite length lists.
 * At the end of the list, zcbor_any_skip() must fail, and leave the end to the caller. */
ZTEST(zcbor_unit_tests, test_any_skip_in_list)
{
#ifdef ZCBOR_CANONICAL
	printf("Skip on canonical builds.\n");
#else
	/* LIST(_): [LIST(2): [1, 2], MAP(_): {3: 4}, 5] */
	uint8_t payload1[] = {0x9F, 0x82, 0x01, 0x02, 0xBF, 0x03, 0x04, 0xFF, 0x05, 0xFF};
	/* LIST(2): [LIST(_): [1], 2], 3 */
	uint8_t payload2[] = {0x82, 0x9F, 0x01, 0xFF, 0x02, 0x03};
	ZCBOR_STATE_D(state_d, 1, payload1, sizeof(payload1), 1, 0);
	ZCBOR_STATE_D(state_d2, 1, payload2, sizeof(payload2), 2, 0);

	zassert_true(zcbor_list_start_decode(state_d), NULL);
	bool ret = zcbor_any_skip(state_d, NULL);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d)));
	ret = zcbor_any_skip(state_d, NULL);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d)));
	zassert_true(zcbor_int32_expect(state_d, 5), NULL);
	zassert_false(zcbor_any_skip(state_d, NULL), NULL);
	zassert_equal(ZCBOR_ERR_ADDITIONAL_INVAL, zcbor_pop_error(state_d), NULL);
	zassert_equal(payload1 + sizeof(payload1) - 1, state_d->payload, NULL);
	ret = zcbor_list_end_decode(state_d);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d)));
	zassert_equal(payload1 + sizeof(payload1), state_d->payload, NULL);

	zassert_true(zcbor_list_start_decode(state_d2), NULL);
	ret = zcbor_any_skip(state_d2, NULL);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d2)));
	zassert_equal(1, state_d2->elem_count, NULL);
	zassert_true(zcbor_int32_expect(state_d2, 2), NULL);
	zassert_false(zcbor_any_skip(state_d2, NULL), NULL);
	zassert_equal(ZCBOR_ERR_LOW_ELEM_COUNT, zcbor_pop_error(state_d2), NULL);
	ret = zcbor_list_end_decode(state_d2);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d2)));
	zassert_true(zcbor_int32_expect(state_d2, 3), NULL);
	zassert_equal(payload2 + sizeof(payload2), state_d2->payload, NULL);
#endif
}


/* Write depth levels of nesting around a 0 into payload, and return the length. Each level is
 * the head_len bytes in head (a list/map header, plus any tags and map key) before the inner
 * level, plus a 0xFF break byte after it if the level has indefinite length. */
static size_t nested_payload(uint8_t *payload, size_t depth, const uint8_t *head,
		size_t head_len, bool indefinite)
{
	size_t len = 0;

	for (size_t i = 0; i < depth; i++) {
		memcpy(&payload[len], head, head_len);
		len += head_len;
	}
	payload[len++] = 0x00;
	if (indefinite) {
		memset(&payload[len], 0xFF, depth);
		len += depth;
	}
	return len;
}


/* zcbor_any_skip() fails instead of descending into lists/maps nested more than
 * ZCBOR_MAX_SKIP_DEPTH levels deep. Tags are skipped without recursing, so they don't count. */
ZTEST(zcbor_unit_tests, test_any_skip_max_depth)
{
	const struct {
		const char *name;
		uint8_t head[3];
		size_t head_len;
		bool indefinite;
	} levels[] = {
		{"list", {0x81}, 1, false}, /* LIST(1) */
		{"map", {0xA1, 0x00}, 2, false}, /* MAP(1) with key 0 */
#ifndef ZCBOR_CANONICAL
		{"indefinite list", {0x9F}, 1, true},
		{"indefinite map", {0xBF, 0x00}, 2, true}, /* with key 0 */
#endif
		{"tagged list", {0xC1, 0xC2, 0x81}, 3, false}, /* #6.1(#6.2(LIST(1))) */
	};
	/* At most 3 bytes per level, plus the innermost 0. */
	uint8_t payload[3 * (ZCBOR_MAX_SKIP_DEPTH + 1) + 1];
	struct zcbor_dec_state_snapshot snap;
	size_t len;
	bool ret;

	for (size_t i = 0; i < ZCBOR_ARRAY_SIZE(levels); i++) {
		len = nested_payload(payload, ZCBOR_MAX_SKIP_DEPTH, levels[i].head,
				levels[i].head_len, levels[i].indefinite);
		ZCBOR_STATE_D(state_d, 0, payload, len, 1, 0);

		ret = zcbor_any_skip(state_d, NULL);
		zassert_true(ret, "%s err: %s\n", levels[i].name,
			zcbor_error_str(zcbor_peek_error(state_d)));
		zassert_equal(payload + len, state_d->payload, "%s\n", levels[i].name);
		zassert_equal(0, state_d->elem_count, "%s\n", levels[i].name);

		len = nested_payload(payload, ZCBOR_MAX_SKIP_DEPTH + 1, levels[i].head,
				levels[i].head_len, levels[i].indefinite);
		ZCBOR_STATE_D(state_d2, 0, payload, len, 1, 0);

		zcbor_state_t state_before;

		memcpy(&state_before, state_d2, sizeof(state_before));
		dec_snapshot(state_d2, &snap);
		zassert_false(zcbor_any_skip(state_d2, NULL), "%s\n", levels[i].name);
		zassert_equal(ZCBOR_ERR_MAX_DEPTH, zcbor_peek_error(state_d2), "%s err: %s\n",
			levels[i].name, zcbor_error_str(zcbor_peek_error(state_d2)));
		dec_assert_unchanged(state_d2, &snap, levels[i].name);
		zassert_mem_equal(&state_before, state_d2, sizeof(state_before), "%s\n",
			levels[i].name);
	}

	/* The innermost list counts even when it is empty, i.e. has nothing to recurse into. */
	len = nested_payload(payload, ZCBOR_MAX_SKIP_DEPTH, levels[0].head, levels[0].head_len,
			false);
	payload[len - 1] = 0x80; /* LIST(0) instead of the innermost 0 */
	ZCBOR_STATE_D(state_d3, 0, payload, len, 1, 0);

	zassert_false(zcbor_any_skip(state_d3, NULL), NULL);
	zassert_equal(ZCBOR_ERR_MAX_DEPTH, zcbor_peek_error(state_d3), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d3)));

	/* The depths of sibling lists don't add up.
	 * LIST(2): [<ZCBOR_MAX_SKIP_DEPTH - 1 levels>, <ZCBOR_MAX_SKIP_DEPTH - 1 levels>] */
	payload[0] = 0x82;
	len = 1;
	len += nested_payload(&payload[len], ZCBOR_MAX_SKIP_DEPTH - 1, levels[0].head,
			levels[0].head_len, false);
	len += nested_payload(&payload[len], ZCBOR_MAX_SKIP_DEPTH - 1, levels[0].head,
			levels[0].head_len, false);
	ZCBOR_STATE_D(state_d4, 0, payload, len, 1, 0);

	ret = zcbor_any_skip(state_d4, NULL);
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d4)));
	zassert_equal(payload + len, state_d4->payload, NULL);
}


/* zcbor_unordered_map_search() and zcbor_unordered_map_end_decode() skip elements with
 * zcbor_any_skip(), so they also fail on elements nested too deeply, and leave the state
 * untouched. The depth is counted from the skipped element, so the map itself doesn't count. */
ZTEST(zcbor_unit_tests, test_unordered_map_max_depth)
{
	const uint8_t list[] = {0x81}; /* LIST(1) */
	uint8_t payload[ZCBOR_MAX_SKIP_DEPTH + 6];
	struct zcbor_dec_state_snapshot snap;
	size_t len;

	/* MAP(2): {0: <ZCBOR_MAX_SKIP_DEPTH nested lists>, 1: 1}
	 * Searching for key 1 skips the value of key 0. */
	payload[0] = 0xA2;
	payload[1] = 0x00;
	len = 2 + nested_payload(&payload[2], ZCBOR_MAX_SKIP_DEPTH, list, sizeof(list), false);
	payload[len++] = 0x01;
	payload[len++] = 0x01;
	ZCBOR_STATE_D(state_d, 2, payload, len, 1, 2);

	zassert_true(zcbor_unordered_map_start_decode(state_d), NULL);
	bool ret = zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d,
		&(int32_t){1});
	zassert_true(ret, "err: %s\n", zcbor_error_str(zcbor_peek_error(state_d)));

	/* Same, but one level deeper. */
	len = 2 + nested_payload(&payload[2], ZCBOR_MAX_SKIP_DEPTH + 1, list, sizeof(list), false);
	payload[len++] = 0x01;
	payload[len++] = 0x01;
	ZCBOR_STATE_D(state_d2, 2, payload, len, 1, 2);

	zassert_true(zcbor_unordered_map_start_decode(state_d2), NULL);
	dec_snapshot(state_d2, &snap);
	zassert_false(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d2,
		&(int32_t){1}), NULL);
	zassert_equal(ZCBOR_ERR_MAX_DEPTH, zcbor_peek_error(state_d2), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d2)));
	dec_assert_unchanged(state_d2, &snap, "unordered_map_search");

	/* MAP(2): {1: 1, 0: <ZCBOR_MAX_SKIP_DEPTH + 1 nested lists>}
	 * The deep value can be decoded manually, but when the keys are searched for out of order,
	 * zcbor_unordered_map_end_decode() must skip it to reach the end of the map. */
	payload[1] = 0x01;
	payload[2] = 0x01;
	payload[3] = 0x00;
	len = 4 + nested_payload(&payload[4], ZCBOR_MAX_SKIP_DEPTH + 1, list, sizeof(list), false);
	ZCBOR_STATE_D(state_d3, ZCBOR_MAX_SKIP_DEPTH + 2, payload, len, 1, 2);

	zassert_true(zcbor_unordered_map_start_decode(state_d3), NULL);
	zassert_true(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d3,
		&(int32_t){0}), NULL);
	for (size_t i = 0; i < ZCBOR_MAX_SKIP_DEPTH + 1; i++) {
		zassert_true(zcbor_list_start_decode(state_d3), NULL);
	}
	zassert_true(zcbor_int32_expect(state_d3, 0), NULL);
	for (size_t i = 0; i < ZCBOR_MAX_SKIP_DEPTH + 1; i++) {
		zassert_true(zcbor_list_end_decode(state_d3), NULL);
	}
	zassert_true(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d3,
		&(int32_t){1}), NULL);
	zassert_true(zcbor_int32_expect(state_d3, 1), NULL);

	dec_snapshot(state_d3, &snap);
	zassert_false(zcbor_unordered_map_end_decode(state_d3), NULL);
	zassert_equal(ZCBOR_ERR_MAX_DEPTH, zcbor_peek_error(state_d3), "err: %s\n",
		zcbor_error_str(zcbor_peek_error(state_d3)));
	dec_assert_unchanged(state_d3, &snap, "unordered_map_end");
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


void decode_inner_map(zcbor_state_t *state, void *result)
{
	zassert_true(zcbor_unordered_map_start_decode(state), NULL);
	zassert_true(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_bool_pexpect, state, &(bool){false}), NULL);
	zassert_true(zcbor_undefined_expect(state, NULL), NULL);
	zcbor_elem_processed(state);
	zassert_true(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_tstr_expect, state, &(struct zcbor_string){"hello", 5}), NULL);
	zassert_true(zcbor_tstr_expect(state, &(struct zcbor_string){"world", 5}), NULL);
	zcbor_elem_processed(state);
	bool ret = zcbor_unordered_map_end_decode(state);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state));
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
	zassert_false(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d, &(int32_t){2}), NULL);
	zassert_equal(ZCBOR_ERR_ELEM_NOT_FOUND, zcbor_peek_error(state_d), "err: %d\n", zcbor_peek_error(state_d));
	ret = zcbor_unordered_map_end_decode(state_d);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_equal(start2, state_d->payload, NULL);

	/* Test single entry map */
	zassert_true(zcbor_unordered_map_start_decode(state_d), NULL);
	ret = zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d, &(int32_t){1});
	zassert_true(ret, "err: %d\n", zcbor_peek_error);
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);
	ret = zcbor_unordered_map_end_decode(state_d);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_equal(start3, state_d->payload, NULL);

	/* Test that looping stops both if it starts at the very start and very end of the map.
	 * Also test ZCBOR_ERR_MAP_MISALIGNED. */
	zassert_true(zcbor_unordered_map_start_decode(state_d2), NULL);
	zassert_false(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d2, &(int32_t){3}), NULL);
	ret = zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d2, &(int32_t){2});
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d2));
	zassert_false(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d2, &(int32_t){1}), NULL);
	zassert_equal(zcbor_peek_error(state_d2), ZCBOR_ERR_MAP_MISALIGNED, NULL);
	zassert_true(zcbor_int32_expect(state_d2, 2), NULL);
	zassert_true(zcbor_array_at_end(state_d2), NULL);
	zassert_false(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d2, &(int32_t){3}), NULL);
	zassert_true(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d2, &(int32_t){1}), NULL);
	zassert_true(zcbor_int32_expect(state_d2, 1), NULL);
	ret = zcbor_unordered_map_end_decode(state_d2);
	zassert_true(ret, NULL);

	/* Test that state_d3 fails because of missing flags. */
#ifdef ZCBOR_MAP_SMART_SEARCH
#ifdef ZCBOR_CANONICAL
	zassert_false(zcbor_unordered_map_start_decode(state_d3), NULL);
#else
	zassert_true(zcbor_unordered_map_start_decode(state_d3), NULL);
	zassert_false(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d3, &(int32_t){2}), NULL);
#endif
	zassert_equal(zcbor_peek_error(state_d3), ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE, NULL);
#endif

	/* Test premature map end */
	zassert_true(zcbor_unordered_map_start_decode(state_d), NULL);
	ret = zcbor_unordered_map_end_decode(state_d);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d), NULL);
#ifndef ZCBOR_CANONICAL
	zcbor_elem_processed(state_d); // Should do nothing because no elements have been discovered.
#endif
	zassert_true(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d, &(int32_t){1}), NULL);
	zassert_true(zcbor_int32_expect(state_d, 1), NULL);
	ret = zcbor_unordered_map_end_decode(state_d);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d), NULL);
	/* Cause a restart of the map */
	zassert_false(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d, &(int32_t){3}), NULL);
	ret = zcbor_unordered_map_end_decode(state_d);
	zassert_false(ret, NULL);
	zassert_equal(ZCBOR_ERR_ELEMS_NOT_PROCESSED, zcbor_peek_error(state_d), NULL);
	zassert_true(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d, &(int32_t){2}), NULL);
	zassert_true(zcbor_int32_expect(state_d, 2), NULL);
	ret = zcbor_unordered_map_end_decode(state_d);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_equal(start4, state_d->payload, NULL);

	/* Test a large map, including nesting. */
	state_d->constant_state->manually_process_elem = true;
	zassert_true(zcbor_unordered_map_start_decode(state_d), NULL);
	ret = zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d, &(int32_t){-1});
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));
	zassert_true(zcbor_tstr_decode(state_d, &str_result1), NULL);
	zcbor_elem_processed(state_d);

	zassert_true(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_bool_pexpect, state_d, &(bool){true}), NULL);
	zassert_true(zcbor_tstr_decode(state_d, &str_result2), NULL);
	zcbor_elem_processed(state_d);
	zassert_true(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d, &(int32_t){2}), NULL);
	zassert_true(zcbor_int32_decode(state_d, &int_result1), NULL);
	zcbor_elem_processed(state_d);
	zassert_true(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d, &(int32_t){1}), NULL);
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
	decode_inner_map(state_d, NULL);
	zcbor_elem_processed(state_d);

	for (size_t i = 34; i > 2; i--) {
		zassert_true(zcbor_unordered_map_search((zcbor_decoder_t *)zcbor_int32_pexpect, state_d, &i), "i: %d, err: %d\n", i, zcbor_peek_error(state_d));
		zassert_false(zcbor_int32_expect(state_d, i), NULL);
		zassert_true(zcbor_int32_expect(state_d, -1 * i), NULL);
		zcbor_elem_processed(state_d);
	}
	ret = zcbor_unordered_map_end_decode(state_d);
	zassert_true(ret, "err: %d\n", zcbor_peek_error(state_d));

	zassert_equal(int_result1, -2, NULL);
	zassert_true(zcbor_compare_strings(&str_result1, &(struct zcbor_string){"world", 5}), "%s (len: %d)\n", &str_result1.value, str_result1.len);
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
	#define CHECK_ERROR1(state) zassert_equal(ZCBOR_ERR_ADDITIONAL_INVAL, zcbor_pop_error(state), "err: %s\n", zcbor_error_str(zcbor_peek_error(state)))
	#define CHECK_ERROR2(state) zassert_equal(ZCBOR_ERR_INVALID_VALUE_ENCODING, zcbor_pop_error(state), "err: %s\n", zcbor_error_str(zcbor_peek_error(state)))

	zassert_false(zcbor_map_start_decode(state_d), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 1;
	zassert_false(zcbor_list_start_decode(state_d), NULL);
	CHECK_ERROR1(state_d);
	state_d->payload += 1;
	zassert_false(zcbor_tstr_decode(state_d, &str_result), NULL);
	CHECK_ERROR2(state_d);
	state_d->payload += 2;
	zassert_false(zcbor_tstr_decode(state_d, &str_result), NULL);
	CHECK_ERROR2(state_d);
	state_d->payload += 2;
	zassert_false(zcbor_bstr_decode(state_d, &str_result), NULL);
	CHECK_ERROR2(state_d);
	state_d->payload += 3;
	zassert_false(zcbor_bstr_decode(state_d, &str_result), NULL);
	CHECK_ERROR2(state_d);
	state_d->payload += 3;
	zassert_false(zcbor_int64_decode(state_d, &i64_result), NULL);
	CHECK_ERROR2(state_d);
	state_d->payload += 5;
	zassert_false(zcbor_int64_decode(state_d, &i64_result), NULL);
	CHECK_ERROR2(state_d);
	state_d->payload += 5;
	zassert_false(zcbor_uint64_decode(state_d, &u64_result), NULL);
	CHECK_ERROR2(state_d);
	state_d->payload += 9;
	zassert_false(zcbor_uint64_decode(state_d, &u64_result), NULL);
	CHECK_ERROR2(state_d);
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
	zassert_true(zcbor_list_end_decode(state_d), NULL);
	zassert_true(zcbor_map_end_decode(state_d), NULL);
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


ZTEST_SUITE(zcbor_unit_tests, NULL, NULL, NULL, NULL, NULL);
