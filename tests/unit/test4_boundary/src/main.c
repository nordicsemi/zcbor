/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Boundary and randomized tests for the zcbor encode/decode runtime:
 * integer widths, nesting/state limits, zero/max lengths, malformed and
 * truncated input, buffer exhaustion, and canonical/non-canonical behavior.
 */

#include <zephyr/ztest.h>
#include <zephyr/sys/util.h>
#include "zcbor_decode.h"
#include "zcbor_encode.h"

/* -------------------------------------------------------------------------
 * Integer width known-answer tests (canonical, minimal-length encoding).
 * ------------------------------------------------------------------------- */

struct u64_kat {
	uint64_t val;
	uint8_t enc[9];
	uint8_t len;
};

static const struct u64_kat u64_kats[] = {
	{ 0, { 0x00 }, 1 },
	{ 23, { 0x17 }, 1 },
	{ 24, { 0x18, 0x18 }, 2 },
	{ 255, { 0x18, 0xff }, 2 },
	{ 256, { 0x19, 0x01, 0x00 }, 3 },
	{ 65535, { 0x19, 0xff, 0xff }, 3 },
	{ 65536, { 0x1a, 0x00, 0x01, 0x00, 0x00 }, 5 },
	{ 0xfffffffful, { 0x1a, 0xff, 0xff, 0xff, 0xff }, 5 },
	{ 0x100000000ul, { 0x1b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 }, 9 },
	{ 0xfffffffffffffffful, { 0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 9 },
};

static const struct u64_kat i64_kats[] = {
	{ (uint64_t)-1, { 0x20 }, 1 },
	{ (uint64_t)-24, { 0x37 }, 1 },
	{ (uint64_t)-25, { 0x38, 0x18 }, 2 },
	{ (uint64_t)-256, { 0x38, 0xff }, 2 },
	{ (uint64_t)-257, { 0x39, 0x01, 0x00 }, 3 },
	{ (uint64_t)-65536, { 0x39, 0xff, 0xff }, 3 },
	{ (uint64_t)-65537, { 0x3a, 0x00, 0x01, 0x00, 0x00 }, 5 },
	{ (uint64_t)-4294967296l, { 0x3a, 0xff, 0xff, 0xff, 0xff }, 5 },
	{ (uint64_t)-4294967297l, { 0x3b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 }, 9 },
	{ (uint64_t)INT64_MIN, { 0x3b, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 9 },
};

ZTEST(zcbor_unit_tests4, test_uint64_encode_kats)
{
	for (size_t i = 0; i < ARRAY_SIZE(u64_kats); i++) {
		uint8_t buf[16];
		ZCBOR_STATE_E(state, 0, buf, sizeof(buf), 0);

		zassert_true(zcbor_uint64_put(state, u64_kats[i].val), "val %llu failed",
			(unsigned long long)u64_kats[i].val);
		size_t len = (size_t)(state->payload - buf);
		zassert_equal(len, u64_kats[i].len, "wrong length for %llu",
			(unsigned long long)u64_kats[i].val);
		zassert_mem_equal(buf, u64_kats[i].enc, len, "wrong encoding for %llu",
			(unsigned long long)u64_kats[i].val);
	}
}

ZTEST(zcbor_unit_tests4, test_int64_encode_kats)
{
	for (size_t i = 0; i < ARRAY_SIZE(i64_kats); i++) {
		uint8_t buf[16];
		ZCBOR_STATE_E(state, 0, buf, sizeof(buf), 0);

		zassert_true(zcbor_int64_put(state, (int64_t)i64_kats[i].val), "val %lld failed",
			(long long)i64_kats[i].val);
		size_t len = (size_t)(state->payload - buf);
		zassert_equal(len, i64_kats[i].len, "wrong length for %lld",
			(long long)i64_kats[i].val);
		zassert_mem_equal(buf, i64_kats[i].enc, len, "wrong encoding for %lld",
			(long long)i64_kats[i].val);
	}
}

ZTEST(zcbor_unit_tests4, test_int_widths_roundtrip)
{
	/* Encode/decode each integer width at its boundary values. */
	static const uint8_t u8_vals[] = { 0, 1, 23, 24, 255 };
	static const uint16_t u16_vals[] = { 0, 23, 24, 255, 256, 65535 };
	static const uint32_t u32_vals[] = { 0, 23, 65535, 65536, 0xfffffffful };
	static const uint64_t u64_vals[] = { 0, 0xfffffffful, 0x100000000ul, 0xfffffffffffffffful };
	static const int8_t i8_vals[] = { -128, -25, -24, -1, 0, 127 };
	static const int16_t i16_vals[] = { -32768, -257, -256, -1, 32767 };
	static const int32_t i32_vals[] = { INT32_MIN, -65537, 0, INT32_MAX };
	static const int64_t i64_vals[] = { INT64_MIN, -4294967297l, 0, INT64_MAX };
	uint8_t buf[16];
	uint8_t r8;
	uint16_t r16;
	uint32_t r32;
	uint64_t r64;
	int8_t s8;
	int16_t s16;
	int32_t s32;
	int64_t s64;
	size_t len;

	for (size_t i = 0; i < ARRAY_SIZE(u8_vals); i++) {
		ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);
		zassert_true(zcbor_uint8_put(es, u8_vals[i]));
		len = (size_t)(es->payload - buf);
		ZCBOR_STATE_D(ds, 0, buf, len, 1, 0);
		zassert_true(zcbor_uint8_decode(ds, &r8));
		zassert_equal(r8, u8_vals[i]);
		zassert_equal(ds->payload, buf + len);
	}
	for (size_t i = 0; i < ARRAY_SIZE(u16_vals); i++) {
		ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);
		zassert_true(zcbor_uint16_put(es, u16_vals[i]));
		len = (size_t)(es->payload - buf);
		ZCBOR_STATE_D(ds, 0, buf, len, 1, 0);
		zassert_true(zcbor_uint16_decode(ds, &r16));
		zassert_equal(r16, u16_vals[i]);
		zassert_equal(ds->payload, buf + len);
	}
	for (size_t i = 0; i < ARRAY_SIZE(u32_vals); i++) {
		ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);
		zassert_true(zcbor_uint32_put(es, u32_vals[i]));
		len = (size_t)(es->payload - buf);
		ZCBOR_STATE_D(ds, 0, buf, len, 1, 0);
		zassert_true(zcbor_uint32_decode(ds, &r32));
		zassert_equal(r32, u32_vals[i]);
		zassert_equal(ds->payload, buf + len);
	}
	for (size_t i = 0; i < ARRAY_SIZE(u64_vals); i++) {
		ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);
		zassert_true(zcbor_uint64_put(es, u64_vals[i]));
		len = (size_t)(es->payload - buf);
		ZCBOR_STATE_D(ds, 0, buf, len, 1, 0);
		zassert_true(zcbor_uint64_decode(ds, &r64));
		zassert_equal(r64, u64_vals[i]);
		zassert_equal(ds->payload, buf + len);
	}
	for (size_t i = 0; i < ARRAY_SIZE(i8_vals); i++) {
		ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);
		zassert_true(zcbor_int8_put(es, i8_vals[i]));
		len = (size_t)(es->payload - buf);
		ZCBOR_STATE_D(ds, 0, buf, len, 1, 0);
		zassert_true(zcbor_int8_decode(ds, &s8));
		zassert_equal(s8, i8_vals[i]);
		zassert_equal(ds->payload, buf + len);
	}
	for (size_t i = 0; i < ARRAY_SIZE(i16_vals); i++) {
		ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);
		zassert_true(zcbor_int16_put(es, i16_vals[i]));
		len = (size_t)(es->payload - buf);
		ZCBOR_STATE_D(ds, 0, buf, len, 1, 0);
		zassert_true(zcbor_int16_decode(ds, &s16));
		zassert_equal(s16, i16_vals[i]);
		zassert_equal(ds->payload, buf + len);
	}
	for (size_t i = 0; i < ARRAY_SIZE(i32_vals); i++) {
		ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);
		zassert_true(zcbor_int32_put(es, i32_vals[i]));
		len = (size_t)(es->payload - buf);
		ZCBOR_STATE_D(ds, 0, buf, len, 1, 0);
		zassert_true(zcbor_int32_decode(ds, &s32));
		zassert_equal(s32, i32_vals[i]);
		zassert_equal(ds->payload, buf + len);
	}
	for (size_t i = 0; i < ARRAY_SIZE(i64_vals); i++) {
		ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);
		zassert_true(zcbor_int64_put(es, i64_vals[i]));
		len = (size_t)(es->payload - buf);
		ZCBOR_STATE_D(ds, 0, buf, len, 1, 0);
		zassert_true(zcbor_int64_decode(ds, &s64));
		zassert_equal(s64, i64_vals[i]);
		zassert_equal(ds->payload, buf + len);
	}
}

/* -------------------------------------------------------------------------
 * Buffer exhaustion.
 * ------------------------------------------------------------------------- */

ZTEST(zcbor_unit_tests4, test_encode_buffer_exhaustion)
{
	static const uint8_t exp[9] = { 0x1b, 0x00, 0x00, 0x00, 0x01,
					0x00, 0x00, 0x00, 0x00 };
	uint8_t buf[16];

	/* 0..8 bytes cannot hold the 9-byte encoding. */
	for (size_t n = 0; n < 9; n++) {
		memset(buf, 0xAA, sizeof(buf));
		ZCBOR_STATE_E(es, 0, buf, n, 0);
		zassert_false(zcbor_uint64_put(es, 0x100000000ul),
			"encode into %zu byte buffer should fail", n);
		zassert_equal(zcbor_peek_error(es), ZCBOR_ERR_NO_PAYLOAD);
		zassert_equal(buf[0], 0xAA, "no bytes may be written on failure");
	}

	/* Exactly 9 bytes must be enough. */
	ZCBOR_STATE_E(es, 0, buf, 9, 0);
	zassert_true(zcbor_uint64_put(es, 0x100000000ul));
	zassert_equal(es->payload, buf + 9);
	zassert_mem_equal(buf, exp, 9);

	/* String that exactly fills the remaining buffer (1 header + 5 chars). */
	struct zcbor_string str = { .value = (const uint8_t *)"12345", .len = 5 };
	ZCBOR_STATE_E(es2, 0, buf, 6, 0);
	zassert_true(zcbor_tstr_encode(es2, &str), "exact fit should work");
	zassert_equal(es2->payload, buf + 6);

	ZCBOR_STATE_E(es3, 0, buf, 5, 0);
	zassert_false(zcbor_tstr_encode(es3, &str), "one byte short should fail");
	zassert_equal(zcbor_peek_error(es3), ZCBOR_ERR_NO_PAYLOAD);
}

ZTEST(zcbor_unit_tests4, test_zero_and_max_lengths)
{
	struct zcbor_string empty = { .value = (const uint8_t *)"", .len = 0 };
	struct zcbor_string out = { 0 };
	uint8_t buf[32];

	/* Zero-length strings round-trip. */
	ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);
	zassert_true(zcbor_bstr_encode(es, &empty));
	zassert_true(zcbor_tstr_encode(es, &empty));
	zassert_equal(es->payload - buf, 2);
	ZCBOR_STATE_D(ds, 0, buf, 2, 2, 0);
	zassert_true(zcbor_bstr_decode(ds, &out));
	zassert_equal(out.len, 0);
	zassert_true(zcbor_tstr_decode(ds, &out));
	zassert_equal(out.len, 0);

	/* String whose length uses the 16-bit header and fills the buffer. */
	{
		static uint8_t big[300];
		struct zcbor_string big_str = { .value = big, .len = sizeof(big) };
		uint8_t big_buf[320];

		for (size_t i = 0; i < sizeof(big); i++) {
			big[i] = (uint8_t)i;
		}
		ZCBOR_STATE_E(es2, 0, big_buf, sizeof(big_buf), 0);
		zassert_true(zcbor_bstr_encode(es2, &big_str));
		ZCBOR_STATE_D(ds2, 0, big_buf, (size_t)(es2->payload - big_buf), 1, 0);
		zassert_true(zcbor_bstr_decode(ds2, &out));
		zassert_equal(out.len, sizeof(big));
		zassert_mem_equal(out.value, big, sizeof(big));
	}
}

/* -------------------------------------------------------------------------
 * Malformed and truncated input.
 * ------------------------------------------------------------------------- */

ZTEST(zcbor_unit_tests4, test_truncated_input)
{
	/* 64-bit int with 8 payload bytes, truncated at every prefix length. */
	static const uint8_t full[9] = { 0x1b, 0x01, 0x02, 0x03, 0x04,
					0x05, 0x06, 0x07, 0x08 };
	uint64_t val;

	for (size_t n = 0; n < 9; n++) {
		ZCBOR_STATE_D(ds, 0, full, n, 1, 0);
		zassert_false(zcbor_uint64_decode(ds, &val),
			"decode of %zu-byte prefix should fail", n);
		zassert_true(zcbor_peek_error(ds) != ZCBOR_SUCCESS);
	}

	/* Truncated string payload. */
	static const uint8_t str_msg[] = { 0x45, 'a', 'b' }; /* bstr of 5, 2 bytes */
	struct zcbor_string str;

	ZCBOR_STATE_D(ds2, 0, str_msg, sizeof(str_msg), 1, 0);
	zassert_false(zcbor_bstr_decode(ds2, &str));
	zassert_equal(zcbor_peek_error(ds2), ZCBOR_ERR_NO_PAYLOAD);

	/* Invalid additional info (28-30 are reserved). */
	static const uint8_t bad_addl[] = { 0x1c };
	uint64_t v2;

	ZCBOR_STATE_D(ds3, 0, bad_addl, sizeof(bad_addl), 1, 0);
	zassert_false(zcbor_uint64_decode(ds3, &v2));
	zassert_equal(zcbor_peek_error(ds3), ZCBOR_ERR_ADDITIONAL_INVAL);

	/* Empty payload. */
	ZCBOR_STATE_D(ds4, 0, full, 0, 1, 0);
	zassert_false(zcbor_uint64_decode(ds4, &val));
	zassert_equal(zcbor_peek_error(ds4), ZCBOR_ERR_NO_PAYLOAD);

	/* 16-bit header with 1 missing byte. */
	static const uint8_t trunc16[] = { 0x19, 0x01 };
	ZCBOR_STATE_D(ds5, 0, trunc16, sizeof(trunc16), 1, 0);
	zassert_false(zcbor_uint64_decode(ds5, &val));
	zassert_equal(zcbor_peek_error(ds5), ZCBOR_ERR_NO_PAYLOAD);
}

ZTEST(zcbor_unit_tests4, test_wrong_type_and_value)
{
	uint8_t buf[8];
	struct zcbor_string str;
	uint64_t val;

	/* Integer where a bstr is expected and vice versa. */
	buf[0] = 0x17;
	ZCBOR_STATE_D(ds, 0, buf, 1, 1, 0);
	zassert_false(zcbor_bstr_decode(ds, &str));
	zassert_equal(zcbor_peek_error(ds), ZCBOR_ERR_WRONG_TYPE);

	buf[0] = 0x40;
	ZCBOR_STATE_D(ds2, 0, buf, 1, 1, 0);
	zassert_false(zcbor_uint64_decode(ds2, &val));
	zassert_equal(zcbor_peek_error(ds2), ZCBOR_ERR_WRONG_TYPE);

	/* 255 fits uint8_t and must decode. */
	buf[0] = 0x18;
	buf[1] = 0xff;
	uint8_t r8;

	ZCBOR_STATE_D(ds3, 0, buf, 2, 1, 0);
	zassert_true(zcbor_uint8_decode(ds3, &r8));
	zassert_equal(r8, 255);

	/* A 2-byte integer must not decode into an 8-bit result. */
	buf[0] = 0x19;
	buf[1] = 0x01;
	buf[2] = 0x00;
	ZCBOR_STATE_D(ds4, 0, buf, 3, 1, 0);
	zassert_false(zcbor_uint8_decode(ds4, &r8), "2-byte value must not fit uint8");
	zassert_equal(zcbor_peek_error(ds4), ZCBOR_ERR_INT_SIZE);
}

/* -------------------------------------------------------------------------
 * Nesting and state limits.
 * ------------------------------------------------------------------------- */

ZTEST(zcbor_unit_tests4, test_nesting_state_limits)
{
	/* [[[[ 1 ]]]] - 4 levels of nested arrays. */
	static const uint8_t deep[5] = { 0x81, 0x81, 0x81, 0x81, 0x01 };

	/* With enough backup states this must decode. */
	{
		ZCBOR_STATE_D(ds, 4, deep, sizeof(deep), 1, 0);
		zassert_true(zcbor_list_start_decode(ds));
		zassert_true(zcbor_list_start_decode(ds));
		zassert_true(zcbor_list_start_decode(ds));
		zassert_true(zcbor_list_start_decode(ds));
		uint32_t v;

		zassert_true(zcbor_uint32_decode(ds, &v));
		zassert_equal(v, 1);
		zassert_true(zcbor_list_end_decode(ds, true));
		zassert_true(zcbor_list_end_decode(ds, true));
		zassert_true(zcbor_list_end_decode(ds, true));
		zassert_true(zcbor_list_end_decode(ds, true));
	}

	/* With only one backup the second nesting level must fail cleanly. */
	{
		ZCBOR_STATE_D(ds, 1, deep, sizeof(deep), 1, 0);
		zassert_true(zcbor_list_start_decode(ds), "first level uses the only backup");
		zassert_false(zcbor_list_start_decode(ds),
			"second nesting level must fail without backups");
		zassert_equal(zcbor_peek_error(ds), ZCBOR_ERR_NO_BACKUP_MEM);
	}

	/* Encoding nested lists with ZCBOR_CANONICAL also needs backups.
	 * With a single backup, only one level of nesting is possible. */
	{
		uint8_t buf[32];
		ZCBOR_STATE_E(es, 1, buf, sizeof(buf), 0);

		zassert_true(zcbor_list_start_encode(es, 1));
#ifdef ZCBOR_CANONICAL
		zassert_false(zcbor_list_start_encode(es, 1),
			"second nesting level must fail without a second backup");
		zassert_equal(zcbor_peek_error(es), ZCBOR_ERR_NO_BACKUP_MEM);
#else
		zassert_true(zcbor_list_start_encode(es, 1));
		zassert_true(zcbor_list_end_encode(es, 1));
		zassert_true(zcbor_list_end_encode(es, 1));
#endif
	}
}

/* -------------------------------------------------------------------------
 * Canonical / non-canonical behavior.
 * ------------------------------------------------------------------------- */

ZTEST(zcbor_unit_tests4, test_canonical_int_encoding)
{
	/* 23 encoded non-minimally as 0x18 0x17. */
	static const uint8_t nonmin[2] = { 0x18, 0x17 };
	uint64_t val;

	/* Without canonical enforcement, the value is accepted. */
	{
		ZCBOR_STATE_D(ds, 1, nonmin, sizeof(nonmin), 1, 0);
		ds[0].constant_state->enforce_canonical = false;
		zassert_false(ZCBOR_ENFORCE_CANONICAL(ds));
		zassert_true(zcbor_uint64_decode(ds, &val));
		zassert_equal(val, 23);
	}

	/* With canonical enforcement, it must be rejected. */
	{
		ZCBOR_STATE_D(ds, 1, nonmin, sizeof(nonmin), 1, 0);
		ds[0].constant_state->enforce_canonical = true;
		zassert_true(ZCBOR_ENFORCE_CANONICAL(ds));
		zassert_false(zcbor_uint64_decode(ds, &val));
		zassert_equal(zcbor_peek_error(ds), ZCBOR_ERR_INVALID_VALUE_ENCODING);
	}

	/* Minimal encoding of 23 is accepted in both modes. */
	{
		static const uint8_t min23[1] = { 0x17 };
		ZCBOR_STATE_D(ds, 1, min23, sizeof(min23), 1, 0);
		ds[0].constant_state->enforce_canonical = true;
		zassert_true(zcbor_uint64_decode(ds, &val));
		zassert_equal(val, 23);
	}

	/* A value encoded too wide must be rejected when canonical is enforced,
	 * but the C integer width must be respected regardless (INT_SIZE). */
	{
		static const uint8_t wide[9] = { 0x1b, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x00, 0x17 };
		uint32_t v32;

		ZCBOR_STATE_D(ds, 1, wide, sizeof(wide), 1, 0);
		zassert_false(zcbor_uint32_decode(ds, &v32),
			"8-byte value must not decode into 4-byte int");
		zassert_equal(zcbor_peek_error(ds), ZCBOR_ERR_INT_SIZE);
	}
}

ZTEST(zcbor_unit_tests4, test_canonical_indefinite_length)
{
	/* Indefinite-length array: 0x9f 0x01 0x02 0xff */
	static const uint8_t indef[4] = { 0x9f, 0x01, 0x02, 0xff };

	/* Accepted when canonical is not enforced. */
	{
		ZCBOR_STATE_D(ds, 1, indef, sizeof(indef), 1, 0);
		ds[0].constant_state->enforce_canonical = false;
		zassert_false(ZCBOR_ENFORCE_CANONICAL(ds));
		zassert_true(zcbor_list_start_decode(ds));
		uint32_t v;

		zassert_true(zcbor_uint32_decode(ds, &v));
		zassert_equal(v, 1);
		zassert_true(zcbor_uint32_decode(ds, &v));
		zassert_equal(v, 2);
		zassert_true(zcbor_list_end_decode(ds, true));
	}

	/* Rejected when canonical is enforced. */
	{
		ZCBOR_STATE_D(ds, 1, indef, sizeof(indef), 1, 0);
		ds[0].constant_state->enforce_canonical = true;
		zassert_false(zcbor_list_start_decode(ds));
		zassert_equal(zcbor_peek_error(ds), ZCBOR_ERR_INVALID_VALUE_ENCODING);
	}
}

/* -------------------------------------------------------------------------
 * Randomized round-trip (deterministic PRNG).
 * ------------------------------------------------------------------------- */

static uint32_t rnd_state;

static uint32_t rnd(void)
{
	uint32_t x = rnd_state;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	rnd_state = x;
	return x;
}

ZTEST(zcbor_unit_tests4, test_random_int_roundtrip)
{
	uint8_t buf[32];
	rnd_state = 0x5a1b0b;

	for (int i = 0; i < 2000; i++) {
		uint64_t v = ((uint64_t)rnd() << 32) | rnd();
		size_t width = 1u << (rnd() & 3); /* 1, 2, 4 or 8 */

		/* Unsigned value of the given width. */
		uint64_t mask = (width == 8) ? UINT64_MAX : ((UINT64_C(1) << (width * 8)) - 1);
		uint64_t uv = v & mask;

		ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);
		zassert_true(zcbor_uint64_put(es, uv), "iteration %d", i);
		size_t len = (size_t)(es->payload - buf);
		ZCBOR_STATE_D(ds, 0, buf, len, 1, 0);
		uint64_t rv;

		zassert_true(zcbor_uint64_decode(ds, &rv), "iteration %d", i);
		zassert_equal(rv, uv, "iteration %d", i);
		zassert_equal(ds->payload, buf + len, "iteration %d", i);

		/* Signed value of the given width (sign-extended). */
		int64_t sv;
		switch (width) {
		case 1:
			sv = (int8_t)(uint8_t)uv;
			break;
		case 2:
			sv = (int16_t)(uint16_t)uv;
			break;
		case 4:
			sv = (int32_t)(uint32_t)uv;
			break;
		default:
			sv = (int64_t)uv;
			break;
		}

		ZCBOR_STATE_E(es2, 0, buf, sizeof(buf), 0);
		zassert_true(zcbor_int64_put(es2, sv), "iteration %d", i);
		len = (size_t)(es2->payload - buf);
		ZCBOR_STATE_D(ds2, 0, buf, len, 1, 0);
		int64_t rs;

		zassert_true(zcbor_int64_decode(ds2, &rs), "iteration %d", i);
		zassert_equal(rs, sv, "iteration %d", i);
		zassert_equal(ds2->payload, buf + len, "iteration %d", i);
	}
}

ZTEST(zcbor_unit_tests4, test_random_strings_and_malformed)
{
	static uint8_t sbuf[256];
	uint8_t buf[320];
	rnd_state = 0xc0ffee;

	for (int i = 0; i < 500; i++) {
		size_t slen = rnd() % sizeof(sbuf);

		for (size_t j = 0; j < slen; j++) {
			sbuf[j] = (uint8_t)rnd();
		}
		struct zcbor_string str = { .value = sbuf, .len = slen };
		struct zcbor_string out;

		ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);
		zassert_true(zcbor_bstr_encode(es, &str), "iteration %d", i);
		size_t len = (size_t)(es->payload - buf);

		ZCBOR_STATE_D(ds, 0, buf, len, 1, 0);
		zassert_true(zcbor_bstr_decode(ds, &out), "iteration %d", i);
		zassert_equal(out.len, slen, "iteration %d", i);
		zassert_mem_equal(out.value, sbuf, slen, "iteration %d", i);

		/* Random truncation of the encoded message must fail cleanly. */
		if (len > 1) {
			size_t cut = rnd() % len;

			ZCBOR_STATE_D(ds2, 0, buf, cut, 1, 0);
			zassert_false(zcbor_bstr_decode(ds2, &out),
				"truncated decode must fail (iteration %d)", i);
		}
	}
}

ZTEST(zcbor_unit_tests4, test_zero_size_int_rejected)
{
	/* Encoding with a zero size must fail cleanly instead of reading
	 * outside the input. */
	uint8_t buf[8];
	uint8_t one = 5;
	ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);

	zassert_false(zcbor_int_encode(es, &one, 0), "zero size must fail");
	zassert_equal(zcbor_peek_error(es), ZCBOR_ERR_INT_SIZE);

	ZCBOR_STATE_E(es2, 0, buf, sizeof(buf), 0);
	zassert_false(zcbor_uint_encode(es2, &one, 0), "zero size must fail");
	zassert_equal(zcbor_peek_error(es2), ZCBOR_ERR_BAD_ARG);
}

ZTEST(zcbor_unit_tests4, test_odd_size_int_encoding)
{
	/* Odd input sizes (3, 5, 6, 7) whose minimal CBOR argument is wider
	 * than the input must be zero-extended, not over-read from the input. */
	static const struct {
		uint8_t raw[7];
		uint8_t size;
		uint8_t exp[9];
		uint8_t exp_len;
	} kats[] = {
		{ { 0xff, 0xff, 0xff }, 3, { 0x1a, 0x00, 0xff, 0xff, 0xff }, 5 },
		{ { 0x00, 0x01, 0x00 }, 3, { 0x19, 0x01, 0x00 }, 3 },
		{ { 0xff, 0xff, 0xff, 0xff, 0xff }, 5,
			{ 0x1b, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff }, 9 },
		{ { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 }, 6,
			{ 0x1b, 0x00, 0x00, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 }, 9 },
		{ { 0, 0, 0, 0, 0, 0, 0xff }, 7,
			{ 0x1b, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 9 },
	};

	for (size_t i = 0; i < ARRAY_SIZE(kats); i++) {
		uint8_t buf[16];
		ZCBOR_STATE_E(es, 0, buf, sizeof(buf), 0);

		zassert_true(zcbor_uint_encode(es, kats[i].raw, kats[i].size),
			"case %zu", i);
		size_t len = (size_t)(es->payload - buf);
		zassert_equal(len, kats[i].exp_len, "case %zu", i);
		zassert_mem_equal(buf, kats[i].exp, len, "case %zu", i);
	}
}

ZTEST_SUITE(zcbor_unit_tests4, NULL, NULL, NULL, NULL, NULL);
