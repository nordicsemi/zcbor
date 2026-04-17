/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include "zcbor_decode.h"
#include "zcbor_common.h"
#include "zcbor_print.h"


/** Return value length from additional value.
 */
static size_t additional_len(uint8_t additional)
{
	if (additional <= ZCBOR_VALUE_IN_HEADER) {
		return 0;
	} else if (ZCBOR_VALUE_IS_1_BYTE <= additional && additional <= ZCBOR_VALUE_IS_8_BYTES) {
		/* 24 => 1
		 * 25 => 2
		 * 26 => 4
		 * 27 => 8
		 */
		return 1U << (additional - ZCBOR_VALUE_IS_1_BYTE);
	}
	return 0xF;
}


static bool initial_checks(zcbor_state_t *state)
{
	ZCBOR_CHECK_NULL(state);
	ZCBOR_CHECK_ERROR();
	ZCBOR_CHECK_PAYLOAD();
	return true;
}


#ifdef ZCBOR_FRAGMENTS
static bool initial_checks_w_frags(zcbor_state_t *state)
{
	ZCBOR_FAIL_IF(!initial_checks(state));
	ZCBOR_ERR_IF(state->inside_frag_str, ZCBOR_ERR_INSIDE_STRING);
	return true;
}

#define INITIAL_CHECKS_INSIDE_FRAG_STR() \
do {\
	if (!initial_checks(state)) { \
		ZCBOR_FAIL(); \
	} \
} while(0)

#define INITIAL_CHECKS() \
do {\
	if (!initial_checks_w_frags(state)) { \
		ZCBOR_FAIL(); \
	} \
} while(0)

#else

#define INITIAL_CHECKS() \
do {\
	if (!initial_checks(state)) { \
		ZCBOR_FAIL(); \
	} \
} while(0)
#endif


static bool type_check(zcbor_state_t *state, zcbor_major_type_t exp_major_type)
{
	INITIAL_CHECKS();
	zcbor_major_type_t major_type = ZCBOR_MAJOR_TYPE(*state->payload);

	if (major_type != exp_major_type) {
		ZCBOR_ERR(ZCBOR_ERR_WRONG_TYPE);
	}
	return true;
}

#define INITIAL_CHECKS_WITH_TYPE(exp_major_type) \
do {\
	if (!type_check(state, exp_major_type)) { \
		ZCBOR_FAIL(); \
	} \
} while(0)

static void err_restore(zcbor_state_t *state, int err)
{
	state->payload = state->payload_bak;
	state->elem_count++;
	zcbor_error(state, err);
}

#define ERR_RESTORE(err) \
do { \
	err_restore(state, err); \
	ZCBOR_FAIL(); \
} while(0)

#define FAIL_RESTORE() \
do { \
	state->payload = state->payload_bak; \
	state->elem_count++; \
	ZCBOR_FAIL(); \
} while(0)


static void endian_copy(uint8_t *dst, const uint8_t *src, size_t src_len)
{
#ifdef ZCBOR_BIG_ENDIAN
	memcpy(dst, src, src_len);
#else
	for (size_t i = 0; i < src_len; i++) {
		dst[i] = src[src_len - 1 - i];
	}
#endif /* ZCBOR_BIG_ENDIAN */
}


/** Get a single value. I.e. a number or simple value, or a list/map/string header.
 *
 * @details @p state->payload must point to the header byte. This function will
 *          retrieve the value (either from within the additional info, or from
 *          the subsequent bytes) and return it in the result. The result can
 *          have arbitrary length within 1-8 bytes.
 *
 *          The function will also validate
 *           - That @p state->payload doesn't overrun past @p state->payload_end.
 *           - That @p state->elem_count has not been exhausted.
 *           - That the value is encoded in a canonical way (if enforce_canonical
 *             is enabled).
 *
 *          @p state->payload and @p state->elem_count are updated if the function
 *          succeeds. If not, they are left unchanged. @p state->payload is updated
 *          to point to the next byte after the value (for a list/map/tstr/bstr
 *          this means the first byte of the list/string payload).
 *          @p state->elem_count is decremented by one.
 *
 *          @p state->payload_bak is updated to point to the start of the value.
 *
 *          CBOR values are always big-endian, so this function converts from
 *          big to little-endian if necessary (@ref ZCBOR_BIG_ENDIAN).
 *
 * @param state[inout] The current state of the decoding.
 * @param result[out]  Pointer to an integer where the result will be stored.
 * @param result_len   The size of the result integer.
 * @param indefinite_length_array[inout]
 *                     As input: NULL if an indefinite length value is not expected.
 *                     As output: Whether the value signifies an indefinite length array.
 *
 * @return             true if the value was successfully extracted, false otherwise.
 */
static bool value_extract(zcbor_state_t *state,
		void *const result, size_t result_len, bool *indefinite_length_array)
{
	zcbor_trace(state, "value_extract");

	INITIAL_CHECKS();
	ZCBOR_ERR_IF((state->elem_count == 0), ZCBOR_ERR_LOW_ELEM_COUNT);

	zcbor_assert_state(result_len != 0, "0-length result not supported.\r\n");
	zcbor_assert_state(result_len <= 8, "result sizes above 8 bytes not supported.\r\n");
	zcbor_assert_state(result != NULL, "result cannot be NULL.\r\n");

	uint8_t header_byte = *state->payload;
	uint8_t additional = ZCBOR_ADDITIONAL(header_byte);
	size_t len = 0;

	if ((additional == ZCBOR_VALUE_IS_INDEFINITE_LENGTH) && (indefinite_length_array != NULL)) {
		/* Indefinite length is not allowed in canonical CBOR */
		ZCBOR_ERR_IF(ZCBOR_ENFORCE_CANONICAL(state),
			ZCBOR_ERR_INVALID_VALUE_ENCODING);

		*indefinite_length_array = true;
	} else {
		len = additional_len(additional);
		uint8_t *result_offs = (uint8_t *)result + ZCBOR_ECPY_OFFS(result_len, MAX(1, len));

		ZCBOR_ERR_IF(additional > ZCBOR_VALUE_IS_8_BYTES, ZCBOR_ERR_ADDITIONAL_INVAL);
		ZCBOR_ERR_IF(len > result_len, ZCBOR_ERR_INT_SIZE);
		ZCBOR_ERR_IF((state->payload + len + 1) > state->payload_end,
			ZCBOR_ERR_NO_PAYLOAD);

		memset(result, 0, result_len);

		if (len == 0) {
			*result_offs = additional;
		} else {
			endian_copy(result_offs, state->payload + 1, len);

			/* Check whether value could have been encoded shorter.
			   Only check when enforcing canonical CBOR, and never check floats. */
			if (ZCBOR_ENFORCE_CANONICAL(state) && !ZCBOR_IS_FLOAT(header_byte)) {
				ZCBOR_ERR_IF((zcbor_header_len_ptr(result, result_len) != (len + 1)),
					ZCBOR_ERR_INVALID_VALUE_ENCODING);
			}
		}
	}

	state->payload_bak = state->payload;
	(state->payload) += len + 1;
	(state->elem_count)--;
	return true;
}

static int val_to_int(zcbor_major_type_t major_type, void *result, size_t result_size)
{
#ifdef ZCBOR_BIG_ENDIAN
	int8_t msbyte = *(int8_t *)result;
#else
	int8_t msbyte = ((int8_t *)result)[result_size - 1];
#endif
	uint8_t *result_uint8 = (uint8_t *)result;

	if ((result_size == 0) || (msbyte < 0)) {
		/* Value is too large to fit in a signed integer. */
		return ZCBOR_ERR_INT_SIZE;
	}

	if (major_type == ZCBOR_MAJOR_TYPE_NINT) {
		/* Convert from CBOR's representation by flipping all bits. */
		for (unsigned int i = 0; i < result_size; i++) {
			result_uint8[i] = (uint8_t)~result_uint8[i];
		}
	}

	return ZCBOR_SUCCESS;
}


bool zcbor_int_decode(zcbor_state_t *state, void *result, size_t result_size)
{
	ZCBOR_PRINT_FUNC_NAME();
	INITIAL_CHECKS();
	zcbor_major_type_t major_type = ZCBOR_MAJOR_TYPE(*state->payload);

	if (major_type != ZCBOR_MAJOR_TYPE_PINT
		&& major_type != ZCBOR_MAJOR_TYPE_NINT) {
		/* Value to be read doesn't have the right type. */
		ZCBOR_ERR(ZCBOR_ERR_WRONG_TYPE);
	}

	if (!value_extract(state, result, result_size, NULL)) {
		ZCBOR_FAIL();
	}

	int err = val_to_int(major_type, result, result_size);
	if (err != ZCBOR_SUCCESS) {
		ERR_RESTORE(err);
	}

	return true;
}


bool zcbor_int8_decode(zcbor_state_t *state, int8_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_int_decode(state, result, sizeof(*result));
}


bool zcbor_int16_decode(zcbor_state_t *state, int16_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_int_decode(state, result, sizeof(*result));
}


bool zcbor_int32_decode(zcbor_state_t *state, int32_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_int_decode(state, result, sizeof(*result));
}


bool zcbor_int64_decode(zcbor_state_t *state, int64_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_int_decode(state, result, sizeof(*result));
}


bool zcbor_uint_decode(zcbor_state_t *state, void *result, size_t result_size)
{
	ZCBOR_PRINT_FUNC_NAME();
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_PINT);

	if (!value_extract(state, result, result_size, NULL)) {
		zcbor_log("uint with size %zu failed.\r\n", result_size);
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_uint8_decode(zcbor_state_t *state, uint8_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint_decode(state, result, sizeof(*result));
}


bool zcbor_uint16_decode(zcbor_state_t *state, uint16_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint_decode(state, result, sizeof(*result));
}


bool zcbor_uint32_decode(zcbor_state_t *state, uint32_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint_decode(state, result, sizeof(*result));
}


bool zcbor_uint64_decode(zcbor_state_t *state, uint64_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint_decode(state, result, sizeof(*result));
}


bool zcbor_int8_expect_union(zcbor_state_t *state, int8_t result)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_int8_expect(state, result);
}


bool zcbor_int16_expect_union(zcbor_state_t *state, int16_t result)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_int16_expect(state, result);
}


bool zcbor_int32_expect_union(zcbor_state_t *state, int32_t result)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_int32_expect(state, result);
}


bool zcbor_int64_expect_union(zcbor_state_t *state, int64_t result)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_int64_expect(state, result);
}


bool zcbor_uint8_expect_union(zcbor_state_t *state, uint8_t result)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_uint8_expect(state, result);
}


bool zcbor_uint16_expect_union(zcbor_state_t *state, uint16_t result)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_uint16_expect(state, result);
}


bool zcbor_uint32_expect_union(zcbor_state_t *state, uint32_t result)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_uint32_expect(state, result);
}


bool zcbor_uint64_expect_union(zcbor_state_t *state, uint64_t result)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (!zcbor_union_elem_code(state)) {
		ZCBOR_FAIL();
	}
	return zcbor_uint64_expect(state, result);
}


bool zcbor_int8_expect(zcbor_state_t *state, int8_t expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_int64_expect(state, expected);
}


bool zcbor_int8_pexpect(zcbor_state_t *state, int8_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_int64_expect(state, *expected);
}


bool zcbor_int16_expect(zcbor_state_t *state, int16_t expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_int64_expect(state, expected);
}


bool zcbor_int16_pexpect(zcbor_state_t *state, int16_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_int64_expect(state, *expected);
}


bool zcbor_int32_expect(zcbor_state_t *state, int32_t expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_int64_expect(state, expected);
}


bool zcbor_int32_pexpect(zcbor_state_t *state, int32_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_int32_expect(state, *expected);
}


bool zcbor_int64_expect(zcbor_state_t *state, int64_t expected)
{
	ZCBOR_PRINT_FUNC_NAME_ARGS("(%" PRIi64 ")", expected);
	int64_t actual;

	if (!zcbor_int64_decode(state, &actual)) {
		ZCBOR_FAIL();
	}

	if (actual != expected) {
		zcbor_log("%" PRIi64 " != %" PRIi64 "\r\n", actual, expected);
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_int64_pexpect(zcbor_state_t *state, int64_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_int64_expect(state, *expected);
}


#ifdef ZCBOR_SUPPORTS_SIZE_T
bool zcbor_size_decode(zcbor_state_t *state, size_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint_decode(state, result, sizeof(*result));
}
#endif


bool zcbor_uint8_expect(zcbor_state_t *state, uint8_t expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint64_expect(state, expected);
}

bool zcbor_uint8_pexpect(zcbor_state_t *state, uint8_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint64_expect(state, *expected);
}

bool zcbor_uint16_expect(zcbor_state_t *state, uint16_t expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint64_expect(state, expected);
}

bool zcbor_uint16_pexpect(zcbor_state_t *state, uint16_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint64_expect(state, *expected);
}


bool zcbor_uint32_expect(zcbor_state_t *state, uint32_t expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint64_expect(state, expected);
}


bool zcbor_uint32_pexpect(zcbor_state_t *state, uint32_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint32_expect(state, *expected);
}


bool zcbor_uint64_expect(zcbor_state_t *state, uint64_t expected)
{
	ZCBOR_PRINT_FUNC_NAME_ARGS("(%" PRIu64 ")", expected);
	uint64_t actual;

	if (!zcbor_uint64_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		zcbor_log("%" PRIu64 " != %" PRIu64 "\r\n", actual, expected);
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_uint64_pexpect(zcbor_state_t *state, uint64_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint64_expect(state, *expected);
}


#ifdef ZCBOR_SUPPORTS_SIZE_T
bool zcbor_size_expect(zcbor_state_t *state, size_t expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_uint64_expect(state, expected);
}


bool zcbor_size_pexpect(zcbor_state_t *state, size_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_size_expect(state, *expected);
}
#endif


static bool str_start_decode(zcbor_state_t *state,
		struct zcbor_string *result, zcbor_major_type_t exp_major_type)
{
	INITIAL_CHECKS_WITH_TYPE(exp_major_type);

	if (!value_extract(state, &result->len, sizeof(result->len), NULL)) {
		ZCBOR_FAIL();
	}

	result->value = state->payload;
	return true;
}


/** Check if there is a potential overflow when decoding a string.
 *
 * This function is used to check if the payload contains the whole string.
 * The payload in the state must point to the start of the string payload, the
 * length having already been extracted from the header via @ref value_extract or
 * @ref str_start_decode.
 *
 * @param state The current state of the decoding.
 * @param len   The decoded length of the string.
 * @return      true if ok (no overflow), false otherwise.
 */
static bool str_overflow_check(zcbor_state_t *state, size_t len)
{
	/* Casting to size_t is safe since value_extract() checks that
	 * payload_end is bigger that payload. */
	if (len > (size_t)(state->payload_end - state->payload)) {
		zcbor_log("error: 0x%zu > 0x%zu\r\n",
			len,
			(state->payload_end - state->payload));
		ERR_RESTORE(ZCBOR_ERR_NO_PAYLOAD);
	}
	return true;
}


static bool str_start_decode_with_overflow_check(zcbor_state_t *state,
		struct zcbor_string *result, zcbor_major_type_t exp_major_type)
{
	bool res = str_start_decode(state, result, exp_major_type);

	if (!res || !str_overflow_check(state, result->len)) {
		ZCBOR_FAIL();
	}

	return true;
}


/** Reset map element processing if we are in an unordered map. */
static bool exit_map(zcbor_state_t *state)
{
#ifdef ZCBOR_MAP_SMART_SEARCH
	/* This has no effect if we are not in an unordered map, since map_elem_count is 0. */
	uint8_t *new_elem_state = state->decode_state.map_search_elem_state
		+ zcbor_flags_to_bytes(state->decode_state.map_elem_count);

	if (new_elem_state > state->constant_state->map_search_elem_state_end) {
		zcbor_log("map_search_elem_state overflowed!\r\n");
		ZCBOR_ERR(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE);
	}

	state->decode_state.map_search_elem_state = new_elem_state;
#else
	state->decode_state.map_elems_processed = 0;
#endif
	state->decode_state.map_elem_count = 0;
	return true;
}


bool zcbor_bstr_start_decode(zcbor_state_t *state, struct zcbor_string *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	struct zcbor_string dummy;
	if (result == NULL) {
		result = &dummy;
	}

	if(!str_start_decode_with_overflow_check(state, result, ZCBOR_MAJOR_TYPE_BSTR)) {
		ZCBOR_FAIL();
	}

	if (!zcbor_new_backup(state, ZCBOR_MAX_ELEM_COUNT)) {
		FAIL_RESTORE();
	}
	ZCBOR_FAIL_IF(!exit_map(state)); // Exit the enclosing map if any

	state->payload_end = result->value + result->len;
	state->inside_cbor_bstr = true;

	return true;
}


bool zcbor_bstr_end_decode(zcbor_state_t *state)
{
	ZCBOR_CHECK_NULL(state);
	ZCBOR_ERR_IF(state->payload != state->payload_end, ZCBOR_ERR_PAYLOAD_NOT_CONSUMED);

	if (!zcbor_process_backup(state,
			ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_KEEP_PAYLOAD,
			ZCBOR_MAX_ELEM_COUNT)) {
		ZCBOR_FAIL();
	}

	return true;
}


#ifdef ZCBOR_FRAGMENTS

static bool start_decode_fragments(zcbor_state_t *state,
	zcbor_major_type_t exp_major_type, bool cbor_bstr)
{
	struct zcbor_string string_hdr;

	if(!str_start_decode(state, &string_hdr, exp_major_type)) {
		ZCBOR_FAIL();
	}

	if (state->inside_cbor_bstr) {
		size_t remainder;

		if (!zcbor_current_string_remainder(state, &remainder)) {
			FAIL_RESTORE();
		}
		if (remainder < string_hdr.len) {
			ERR_RESTORE(ZCBOR_ERR_TOO_LARGE_FOR_STRING);
		}
	}

	ptrdiff_t new_offset = state->constant_state->curr_payload_section - state->payload;

	if (cbor_bstr) {
		if (!zcbor_new_backup(state, ZCBOR_MAX_ELEM_COUNT)) {
			FAIL_RESTORE();
		}
		state->frag_offset_cbor = new_offset;
		state->str_total_len_cbor = string_hdr.len;
		state->inside_cbor_bstr = true;
	} else {
		state->frag_offset = new_offset;
		state->str_total_len = string_hdr.len;
		state->inside_frag_str = true;
	}

	return true;
}


bool zcbor_bstr_fragments_start_decode(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	return start_decode_fragments(state, ZCBOR_MAJOR_TYPE_BSTR, false);
}


bool zcbor_tstr_fragments_start_decode(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	return start_decode_fragments(state, ZCBOR_MAJOR_TYPE_TSTR, false);
}


bool zcbor_cbor_bstr_fragments_start_decode(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	return start_decode_fragments(state, ZCBOR_MAJOR_TYPE_BSTR, true);
}


bool zcbor_str_fragment_decode(zcbor_state_t *state, struct zcbor_string_fragment *fragment)
{
	ZCBOR_PRINT_FUNC_NAME();
	INITIAL_CHECKS_INSIDE_FRAG_STR();

	size_t len, offset, remainder;

	ZCBOR_FAIL_IF(!zcbor_current_string_offset(state, &offset));
	ZCBOR_FAIL_IF(!zcbor_current_string_remainder(state, &remainder));

	if (state->inside_frag_str) {
		len = MIN((size_t)state->payload_end - (size_t)state->payload, remainder);
		state->payload += len;
		fragment->total_len = state->str_total_len;
		fragment->offset = offset;

	} else {
		len = MIN((size_t)state->payload - (size_t)state->constant_state->curr_payload_section,
					offset);
		fragment->total_len = state->str_total_len_cbor;
		fragment->offset = offset - len;
	}

	fragment->fragment.value = state->payload - len;
	fragment->fragment.len = len;

	zcbor_log("\r\n");
	return true;
}


bool zcbor_str_fragments_end_decode(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();

	size_t remainder;

	ZCBOR_FAIL_IF(!zcbor_current_string_remainder(state, &remainder));
	ZCBOR_ERR_IF(remainder != 0, ZCBOR_ERR_NOT_AT_END);

	if (state->inside_frag_str) {
		state->inside_frag_str = false;
	} else {
		if (!zcbor_bstr_end_decode(state)) {
			ZCBOR_FAIL();
		}
		state->inside_cbor_bstr = false;
	}

	return true;
}

#endif /* ZCBOR_FRAGMENTS */


static bool str_decode(zcbor_state_t *state, struct zcbor_string *result,
		zcbor_major_type_t exp_major_type)
{
	if (!str_start_decode_with_overflow_check(state, result, exp_major_type)) {
		ZCBOR_FAIL();
	}

	state->payload += result->len;
	return true;
}


static bool str_expect(zcbor_state_t *state, struct zcbor_string *result,
		zcbor_major_type_t exp_major_type)
{
	struct zcbor_string tmp_result;

	if (!str_decode(state, &tmp_result, exp_major_type)) {
		ZCBOR_FAIL();
	}
	if (!zcbor_compare_strings(&tmp_result, result)) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_bstr_decode(zcbor_state_t *state, struct zcbor_string *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	return str_decode(state, result, ZCBOR_MAJOR_TYPE_BSTR);
}


bool zcbor_bstr_expect(zcbor_state_t *state, struct zcbor_string *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return str_expect(state, expected, ZCBOR_MAJOR_TYPE_BSTR);
}


bool zcbor_tstr_decode(zcbor_state_t *state, struct zcbor_string *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	return str_decode(state, result, ZCBOR_MAJOR_TYPE_TSTR);
}


bool zcbor_tstr_expect(zcbor_state_t *state, struct zcbor_string *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return str_expect(state, expected, ZCBOR_MAJOR_TYPE_TSTR);
}


bool zcbor_bstr_expect_ptr(zcbor_state_t *state, char const *ptr, size_t len)
{
	ZCBOR_PRINT_FUNC_NAME();
	struct zcbor_string zs = { .value = (const uint8_t *)ptr, .len = len };

	return zcbor_bstr_expect(state, &zs);
}


bool zcbor_tstr_expect_ptr(zcbor_state_t *state, char const *ptr, size_t len)
{
	ZCBOR_PRINT_FUNC_NAME();
	struct zcbor_string zs = { .value = (const uint8_t *)ptr, .len = len };

	return zcbor_tstr_expect(state, &zs);
}


bool zcbor_bstr_expect_term(zcbor_state_t *state, char const *string, size_t maxlen)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_bstr_expect_ptr(state, string, strnlen(string, maxlen));
}


bool zcbor_tstr_expect_term(zcbor_state_t *state, char const *string, size_t maxlen)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_tstr_expect_ptr(state, string, strnlen(string, maxlen));
}


static bool list_map_start_decode(zcbor_state_t *state,
		zcbor_major_type_t exp_major_type)
{
	size_t new_elem_count;
	bool indefinite_length_array = false;

	INITIAL_CHECKS_WITH_TYPE(exp_major_type);

	if (!value_extract(state, &new_elem_count, sizeof(new_elem_count), &indefinite_length_array)) {
		ZCBOR_FAIL();
	}

	if (!zcbor_new_backup(state, indefinite_length_array
				? ZCBOR_LARGE_ELEM_COUNT : new_elem_count)) {
		FAIL_RESTORE();
	}
	state->decode_state.map_start_backup_num = state->constant_state->current_backup;

	state->decode_state.indefinite_length_array = indefinite_length_array;

	ZCBOR_FAIL_IF(!exit_map(state)); // Exit the enclosing map if any

	return true;
}


bool zcbor_list_start_decode(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	return list_map_start_decode(state, ZCBOR_MAJOR_TYPE_LIST);
}


bool zcbor_map_start_decode(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	bool ret = list_map_start_decode(state, ZCBOR_MAJOR_TYPE_MAP);

	if (ret && !state->decode_state.indefinite_length_array) {
		if (state->elem_count >= (ZCBOR_MAX_ELEM_COUNT / 2)) {
			/* The new elem_count is too large. */
			ERR_RESTORE(ZCBOR_ERR_INT_SIZE);
		}
		state->elem_count *= 2;
	}
	return ret;
}


bool zcbor_array_at_end(zcbor_state_t *state)
{
	const bool indefinite_length_array = state->decode_state.indefinite_length_array;

	return ((!indefinite_length_array && (state->elem_count == 0))
		|| (indefinite_length_array
			&& (state->payload < state->payload_end)
			&& (*state->payload == 0xFF)));
}


static size_t update_map_elem_count(zcbor_state_t *state, size_t elem_count);
#ifdef ZCBOR_MAP_SMART_SEARCH
static bool allocate_map_flags(zcbor_state_t *state, size_t elem_count);
#endif


bool zcbor_unordered_map_start_decode(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	ZCBOR_FAIL_IF(!zcbor_map_start_decode(state));

	state->decode_state.map_elem_count = 0;
	state->decode_state.counting_map_elems = state->decode_state.indefinite_length_array;

	if (!state->decode_state.counting_map_elems) {
		size_t old_flags = update_map_elem_count(state, state->elem_count);
#ifdef ZCBOR_MAP_SMART_SEARCH
		ZCBOR_FAIL_IF(!allocate_map_flags(state, old_flags));
#endif
		(void)old_flags;
	}

	return true;
}


/** Return the max (starting) elem_count of the current container.
 *
 *  Should only be used for unordered maps (started with @ref zcbor_unordered_map_start_decode)
 */
static size_t zcbor_current_max_elem_count(zcbor_state_t *state)
{
	return (state->decode_state.indefinite_length_array ? \
		ZCBOR_LARGE_ELEM_COUNT : state->decode_state.map_elem_count * 2);
}


static bool map_restart(zcbor_state_t *state)
{
	if (!zcbor_process_backup_num(state,
			ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_KEEP_DECODE_STATE,
			ZCBOR_MAX_ELEM_COUNT,
			state->decode_state.map_start_backup_num)) {
		ZCBOR_FAIL();
	}

	state->elem_count = zcbor_current_max_elem_count(state);
	return true;
}

/** Return the index of the current map element.
 *
 *  This is calculated based on the elem_count. Since elem_count counts both keys
 *  and values, index_offset decides how to round when dividing by 2.
 */
__attribute__((used))
static size_t get_current_index(zcbor_state_t *state, uint32_t index_offset)
{
	size_t current_elem_count_index = zcbor_current_max_elem_count(state) - state->elem_count;

	if (index_offset > current_elem_count_index) {
		/* Invalid case, should be caught earlier, return 0 to prevent underflow. */
		return 0;
	}

	return ((current_elem_count_index - index_offset) / 2);
}


#ifdef ZCBOR_MAP_SMART_SEARCH
#define FLAG_MODE_GET_CURRENT 0
#define FLAG_MODE_CLEAR_CURRENT 1
#define FLAG_MODE_CLEAR_UNUSED 2

static bool manipulate_flags(zcbor_state_t *state, uint32_t mode)
{
	const size_t last_index = (state->decode_state.map_elem_count - 1);

	/* Add mode as index_offset to get_current_index() because for GET, you want
	 * the index you are pointing to, while for CLEAR_CURRENT, you want the one you
	 * just processed. This only comes into play when elem_count is even. */
	size_t index = (mode == FLAG_MODE_CLEAR_UNUSED) ? last_index : get_current_index(state, mode);

	ZCBOR_ERR_IF((index >= state->decode_state.map_elem_count),
		ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE);
	uint8_t *flag_byte = &state->decode_state.map_search_elem_state[index >> 3];
	uint8_t flag_mask = (uint8_t)(1 << (index & 7));

	switch(mode) {
	case FLAG_MODE_GET_CURRENT:
		return (!!(*flag_byte & flag_mask));
	case FLAG_MODE_CLEAR_CURRENT:
		*flag_byte &= ~flag_mask;
		return true;
	case FLAG_MODE_CLEAR_UNUSED:
		*flag_byte &= (uint8_t)((flag_mask << 1) - 1);
		return true;
	}
	return false;
}


static bool should_try_key(zcbor_state_t *state)
{
	return manipulate_flags(state, FLAG_MODE_GET_CURRENT);
}


bool zcbor_elem_processed(zcbor_state_t *state)
{
	if (state->elem_count >= zcbor_current_max_elem_count(state)) {
		/* If elem_count is at the start of the map, we have not processed anything yet. */
		ZCBOR_ERR(ZCBOR_ERR_MAP_MISALIGNED);
	}
	return manipulate_flags(state, FLAG_MODE_CLEAR_CURRENT);
}


static bool allocate_map_flags(zcbor_state_t *state, size_t old_flags)
{
	size_t new_bytes = zcbor_flags_to_bytes(state->decode_state.map_elem_count);
	size_t old_bytes = zcbor_flags_to_bytes(old_flags);
	size_t extra_bytes = new_bytes - old_bytes;

	ZCBOR_ERR_IF(!state->constant_state, ZCBOR_ERR_CONSTANT_STATE_MISSING);
	const uint8_t *flags_end = state->constant_state->map_search_elem_state_end;

	if (extra_bytes) {
		if ((state->decode_state.map_search_elem_state + new_bytes) > flags_end) {
			state->decode_state.map_elem_count
				= 8 * (size_t)(flags_end - state->decode_state.map_search_elem_state);
			ZCBOR_ERR(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE);
		}

		memset(&state->decode_state.map_search_elem_state[new_bytes - extra_bytes], 0xFF, extra_bytes);
	}
	return true;
}
#else

static bool should_try_key(zcbor_state_t *state)
{
	return (state->decode_state.map_elems_processed < state->decode_state.map_elem_count);
}


bool zcbor_elem_processed(zcbor_state_t *state)
{
	if (state->elem_count >= zcbor_current_max_elem_count(state)) {
		/* If elem_count is at the start of the map, we have not processed anything yet. */
		ZCBOR_ERR(ZCBOR_ERR_MAP_MISALIGNED);
	}
	if (should_try_key(state)) {
		state->decode_state.map_elems_processed++;
	}
	return true;
}
#endif


static size_t update_map_elem_count(zcbor_state_t *state, size_t elem_count)
{
	size_t old_map_elem_count = state->decode_state.map_elem_count;

	state->decode_state.map_elem_count = MAX(old_map_elem_count, elem_count / 2);
	return old_map_elem_count;
}


static bool handle_map_end(zcbor_state_t *state)
{
	state->decode_state.counting_map_elems = false;
	return map_restart(state);
}


static bool try_key(zcbor_state_t *state, void *key_result, zcbor_decoder_t key_decoder)
{
	uint8_t const *payload_bak2 = state->payload;
	size_t elem_count_bak = state->elem_count;

	if (!key_decoder(state, (uint8_t *)key_result)) {
		state->payload = payload_bak2;
		state->elem_count = elem_count_bak;
		return false;
	}

	zcbor_log("Found element at index %zu.\n", get_current_index(state, 1));
	return true;
}


bool zcbor_unordered_map_search(zcbor_decoder_t key_decoder, zcbor_state_t *state, void *key_result)
{
	ZCBOR_PRINT_FUNC_NAME();
	/* elem_count cannot be odd since the map consists of key-value-pairs.
	 * This might mean that this function was called while pointing at a value (instead
	 * of a key). */
	ZCBOR_ERR_IF(state->elem_count & 1, ZCBOR_ERR_MAP_MISALIGNED);

	uint8_t const *payload_bak = state->payload;
	size_t elem_count = state->elem_count;

	/* Loop once through all the elements of the map. */
	do {
		if (zcbor_array_at_end(state)) {
			if (!handle_map_end(state)) {
				goto error;
			}
			continue; /* This continue is needed so the loop stops both if elem_count is
			           * at the very start or the very end of the map. */
		}

		if (state->decode_state.counting_map_elems) {
			size_t m_elem_count = ZCBOR_LARGE_ELEM_COUNT - state->elem_count + 2;
			size_t old_flags = update_map_elem_count(state, m_elem_count);
#ifdef ZCBOR_MAP_SMART_SEARCH
			ZCBOR_FAIL_IF(!allocate_map_flags(state, old_flags));
#endif
			(void)old_flags;
		}

		if (should_try_key(state)) {
			if (try_key(state, key_result, key_decoder)) {
				if (!ZCBOR_MANUALLY_PROCESS_ELEM(state)) {
					ZCBOR_FAIL_IF(!zcbor_elem_processed(state));
				}
				return true;
			}
		} else {
			zcbor_log("Skipping element at index %zu.\n", get_current_index(state, 0));
		}

		/* Skip over both the key and the value. */
		if (!zcbor_any_skip(state, NULL) || !zcbor_any_skip(state, NULL)) {
			goto error;
		}
	} while (state->elem_count != elem_count);

	zcbor_error(state, ZCBOR_ERR_ELEM_NOT_FOUND);
error:
	state->payload = payload_bak;
	state->elem_count = elem_count;
	ZCBOR_FAIL();
}


bool zcbor_search_key_bstr_ptr(zcbor_state_t *state, char const *ptr, size_t len)
{
	struct zcbor_string zs = { .value = (const uint8_t *)ptr, .len = len };

	return zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_bstr_expect), state, &zs);
}


bool zcbor_search_key_tstr_ptr(zcbor_state_t *state, char const *ptr, size_t len)
{
	struct zcbor_string zs = { .value = (const uint8_t *)ptr, .len = len };

	return zcbor_unordered_map_search(ZCBOR_CAST_FP(zcbor_tstr_expect), state, &zs);
}


bool zcbor_search_key_bstr_term(zcbor_state_t *state, char const *str, size_t maxlen)
{
	return zcbor_search_key_bstr_ptr(state, str, strnlen(str, maxlen));
}


bool zcbor_search_key_tstr_term(zcbor_state_t *state, char const *str, size_t maxlen)
{
	return zcbor_search_key_tstr_ptr(state, str, strnlen(str, maxlen));
}


static bool array_end_expect(zcbor_state_t *state)
{
	INITIAL_CHECKS();
	ZCBOR_ERR_IF(*state->payload != 0xFF, ZCBOR_ERR_WRONG_TYPE);

	state->payload++;
	return true;
}


static bool list_map_end_decode(zcbor_state_t *state)
{
	ZCBOR_CHECK_NULL(state);

	zcbor_state_t state_copy = *state;

	if (!zcbor_process_backup(state,
			ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_KEEP_PAYLOAD,
			ZCBOR_MAX_ELEM_COUNT)) {
		ZCBOR_FAIL();
	}

	if (state_copy.decode_state.indefinite_length_array) {
		if (!array_end_expect(state)) {
			ZCBOR_FAIL();
		}
		state_copy.decode_state.indefinite_length_array = false;
	} else {
		if (state_copy.elem_count > 0) {
			zcbor_log("%zu elements left in map or array (should be 0).\r\n", state_copy.elem_count);
			ZCBOR_ERR(ZCBOR_ERR_HIGH_ELEM_COUNT);
		}
	}

	return true;
}


bool zcbor_list_end_decode(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	return list_map_end_decode(state);
}


bool zcbor_map_end_decode(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	return list_map_end_decode(state);
}


bool zcbor_unordered_map_end_decode(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	/* Checking zcbor_array_at_end() ensures that check is valid.
	 * In case the map is at the end, but state->decode_state.counting_map_elems isn't updated.*/
	if (!zcbor_array_at_end(state) && state->decode_state.counting_map_elems) {
		zcbor_log("unprocessed element(s) in map after index %zu\n",
				state->decode_state.map_elem_count);
		ZCBOR_ERR(ZCBOR_ERR_ELEMS_NOT_PROCESSED);
	}

	if (state->decode_state.map_elem_count > 0) {
#ifdef ZCBOR_MAP_SMART_SEARCH
		manipulate_flags(state, FLAG_MODE_CLEAR_UNUSED);

		for (size_t i = 0; i < zcbor_flags_to_bytes(state->decode_state.map_elem_count); i++) {
			if (state->decode_state.map_search_elem_state[i] != 0) {
				zcbor_log("unprocessed element(s) in map: [%zu] = 0x%02x\n",
						i, state->decode_state.map_search_elem_state[i]);
				ZCBOR_ERR(ZCBOR_ERR_ELEMS_NOT_PROCESSED);
			}
		}
#else
		ZCBOR_ERR_IF(should_try_key(state), ZCBOR_ERR_ELEMS_NOT_PROCESSED);
#endif
	}
	while (!zcbor_array_at_end(state)) {
		zcbor_any_skip(state, NULL);
	}
	return zcbor_map_end_decode(state);
}


bool zcbor_list_map_end_force_decode(zcbor_state_t *state)
{
	if (!zcbor_process_backup(state,
			ZCBOR_FLAG_RESTORE | ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_KEEP_PAYLOAD,
			ZCBOR_MAX_ELEM_COUNT)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_simple_decode(zcbor_state_t *state, uint8_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_SIMPLE);

	uint8_t additional = ZCBOR_ADDITIONAL(*state->payload);

	/* Simple values must be 0-23 (additional is 0-23) or 24-255 (additional is 24).
	 * Other additional values are not considered simple values. */
	ZCBOR_ERR_IF(additional > ZCBOR_VALUE_IS_1_BYTE, ZCBOR_ERR_WRONG_TYPE);

	if (!value_extract(state, result, sizeof(*result), NULL)) {
		ZCBOR_FAIL();
	}

	/* Simple values less than 24 MUST be encoded in the additional information.
	   Simple values 24 to 31 inclusive are unused. Ref: RFC8949 sec 3.3 */
	if ((additional == ZCBOR_VALUE_IS_1_BYTE) && (*result < 32)) {
		ERR_RESTORE(ZCBOR_ERR_INVALID_VALUE_ENCODING);
	}
	return true;
}


bool zcbor_simple_expect(zcbor_state_t *state, uint8_t expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	uint8_t actual;

	if (!zcbor_simple_decode(state, &actual)) {
		ZCBOR_FAIL();
	}

	if (actual != expected) {
		zcbor_log("simple value %u != %u\r\n", actual, expected);
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}

	return true;
}


bool zcbor_simple_pexpect(zcbor_state_t *state, uint8_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_simple_expect(state, *expected);
}


bool zcbor_nil_expect(zcbor_state_t *state, void *unused)
{
	ZCBOR_PRINT_FUNC_NAME();
	(void)unused;
	return zcbor_simple_expect(state, ZCBOR_NIL_VAL);
}


bool zcbor_undefined_expect(zcbor_state_t *state, void *unused)
{
	ZCBOR_PRINT_FUNC_NAME();
	(void)unused;
	return zcbor_simple_expect(state, ZCBOR_UNDEF_VAL);
}


bool zcbor_bool_decode(zcbor_state_t *state, bool *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	uint8_t value;

	if (!zcbor_simple_decode(state, &value)) {
		ZCBOR_FAIL();
	}
	value -= ZCBOR_BOOL_TO_SIMPLE;
	if (value > 1) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_TYPE);
	}
	*result = value;

	zcbor_log("boolval: %u\r\n", *result);
	return true;
}


bool zcbor_bool_expect(zcbor_state_t *state, bool expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_simple_expect(state, (uint8_t)(!!expected) + ZCBOR_BOOL_TO_SIMPLE);
}


bool zcbor_bool_pexpect(zcbor_state_t *state, bool *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_bool_expect(state, *expected);
}


static bool float_check(zcbor_state_t *state, uint8_t additional_val)
{
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_SIMPLE);
	ZCBOR_ERR_IF(ZCBOR_ADDITIONAL(*state->payload) != additional_val, ZCBOR_ERR_FLOAT_SIZE);
	return true;
}


bool zcbor_float16_bytes_decode(zcbor_state_t *state, uint16_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	ZCBOR_FAIL_IF(!float_check(state, ZCBOR_VALUE_IS_2_BYTES));

	if (!value_extract(state, result, sizeof(*result), NULL)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float16_bytes_expect(zcbor_state_t *state, uint16_t expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	uint16_t actual;

	if (!zcbor_float16_bytes_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_float16_bytes_pexpect(zcbor_state_t *state, uint16_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_float16_bytes_expect(state, *expected);
}


bool zcbor_float16_decode(zcbor_state_t *state, float *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	uint16_t value16;

	if (!zcbor_float16_bytes_decode(state, &value16)) {
		ZCBOR_FAIL();
	}

	*result = zcbor_float16_to_32(value16);
	return true;
}


bool zcbor_float16_expect(zcbor_state_t *state, float expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	float actual;

	if (!zcbor_float16_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_float16_pexpect(zcbor_state_t *state, float *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_float16_expect(state, *expected);
}


bool zcbor_float32_decode(zcbor_state_t *state, float *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	ZCBOR_FAIL_IF(!float_check(state, ZCBOR_VALUE_IS_4_BYTES));

	if (!value_extract(state, result, sizeof(*result), NULL)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float32_expect(zcbor_state_t *state, float expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	float actual;

	if (!zcbor_float32_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_float32_pexpect(zcbor_state_t *state, float *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_float32_expect(state, *expected);
}


bool zcbor_float16_32_decode(zcbor_state_t *state, float *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (zcbor_float16_decode(state, result)) {
		/* Do nothing */
	} else if (!zcbor_float32_decode(state, result)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float16_32_expect(zcbor_state_t *state, float expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (zcbor_float16_expect(state, expected)) {
		/* Do nothing */
	} else if (!zcbor_float32_expect(state, expected)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float16_32_pexpect(zcbor_state_t *state, float *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_float16_32_expect(state, *expected);
}


bool zcbor_float64_decode(zcbor_state_t *state, double *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	ZCBOR_FAIL_IF(!float_check(state, ZCBOR_VALUE_IS_8_BYTES));

	if (!value_extract(state, result, sizeof(*result), NULL)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float64_expect(zcbor_state_t *state, double expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	double actual;

	if (!zcbor_float64_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_float64_pexpect(zcbor_state_t *state, double *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_float64_expect(state, *expected);
}


bool zcbor_float32_64_decode(zcbor_state_t *state, double *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	float float_result;

	if (zcbor_float32_decode(state, &float_result)) {
		*result = (double)float_result;
	} else if (!zcbor_float64_decode(state, result)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float32_64_expect(zcbor_state_t *state, double expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (zcbor_float64_expect(state, expected)) {
		/* Do nothing */
	} else if (!zcbor_float32_expect(state, (float)expected)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float32_64_pexpect(zcbor_state_t *state, double *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_float32_64_expect(state, *expected);
}


bool zcbor_float_decode(zcbor_state_t *state, double *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	float float_result;

	if (zcbor_float16_decode(state, &float_result)) {
		*result = (double)float_result;
	} else if (zcbor_float32_decode(state, &float_result)) {
		*result = (double)float_result;
	} else if (!zcbor_float64_decode(state, result)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float_expect(zcbor_state_t *state, double expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	if (zcbor_float16_expect(state, (float)expected)) {
		/* Do nothing */
	} else if (zcbor_float32_expect(state, (float)expected)) {
		/* Do nothing */
	} else if (!zcbor_float64_expect(state, expected)) {
		ZCBOR_FAIL();
	}

	return true;
}


bool zcbor_float_pexpect(zcbor_state_t *state, double *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_float_expect(state, *expected);
}


bool zcbor_any_skip(zcbor_state_t *state, void *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	INITIAL_CHECKS();
	zcbor_assert_state(result == NULL,
			"'any' type cannot be returned, only skipped.\r\n");
	(void)result;

	zcbor_major_type_t major_type = ZCBOR_MAJOR_TYPE(*state->payload);
	uint64_t value = 0; /* In case of indefinite_length_array. */
	zcbor_state_t state_copy;

	memcpy(&state_copy, state, sizeof(zcbor_state_t));

	while (major_type == ZCBOR_MAJOR_TYPE_TAG) {
		uint32_t tag_dummy;

		if (!zcbor_tag_decode(&state_copy, &tag_dummy)) {
			ZCBOR_FAIL();
		}
		ZCBOR_ERR_IF(state_copy.payload >= state_copy.payload_end, ZCBOR_ERR_NO_PAYLOAD);
		major_type = ZCBOR_MAJOR_TYPE(*state_copy.payload);
	}

	bool indefinite_length_array = false;
	bool *ila_ptr = ((major_type == ZCBOR_MAJOR_TYPE_MAP)
			|| (major_type == ZCBOR_MAJOR_TYPE_LIST)) ? &indefinite_length_array : NULL;

	if (!value_extract(&state_copy, &value, sizeof(value), ila_ptr)) {
		/* Can happen because of elem_count (or payload_end) */
		ZCBOR_FAIL();
	}

	switch (major_type) {
		case ZCBOR_MAJOR_TYPE_BSTR:
		case ZCBOR_MAJOR_TYPE_TSTR:
			/* 'value' is the length of the BSTR or TSTR. */
			ZCBOR_FAIL_IF(!str_overflow_check(state, (size_t)value));
			(state_copy.payload) += value;
			break;
		case ZCBOR_MAJOR_TYPE_MAP:
			ZCBOR_ERR_IF(value > (SIZE_MAX / 2), ZCBOR_ERR_INT_SIZE);
			value *= 2;
			/* fallthrough */
		case ZCBOR_MAJOR_TYPE_LIST:
			if (indefinite_length_array) {
				value = ZCBOR_LARGE_ELEM_COUNT;
			}
			state_copy.elem_count = (size_t)value;
			state_copy.decode_state.indefinite_length_array = indefinite_length_array;
			while (!zcbor_array_at_end(&state_copy)) {
				if (!zcbor_any_skip(&state_copy, NULL)) {
					ZCBOR_FAIL();
				}
			}
			if (indefinite_length_array && !array_end_expect(&state_copy)) {
				ZCBOR_FAIL();
			}
			break;
		default:
			/* Do nothing */
			break;
	}

	state->payload = state_copy.payload;
	state->elem_count--;

	return true;
}


bool zcbor_tag_decode(zcbor_state_t *state, uint32_t *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	INITIAL_CHECKS_WITH_TYPE(ZCBOR_MAJOR_TYPE_TAG);

	if (!value_extract(state, result, sizeof(*result), NULL)) {
		ZCBOR_FAIL();
	}
	state->elem_count++;
	return true;
}


bool zcbor_tag_expect(zcbor_state_t *state, uint32_t expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	uint32_t actual;

	if (!zcbor_tag_decode(state, &actual)) {
		ZCBOR_FAIL();
	}
	if (actual != expected) {
		ERR_RESTORE(ZCBOR_ERR_WRONG_VALUE);
	}
	return true;
}


bool zcbor_tag_pexpect(zcbor_state_t *state, uint32_t *expected)
{
	ZCBOR_PRINT_FUNC_NAME();
	return zcbor_tag_expect(state, *expected);
}


static bool multi_decode_backup(size_t min_decode,
		size_t max_decode,
		size_t *num_decode,
		zcbor_decoder_t decoder,
		zcbor_state_t *state,
		void *result,
		size_t result_len,
		bool backup)
{
	ZCBOR_PRINT_FUNC_NAME();
	ZCBOR_CHECK_NULL(state);
	ZCBOR_CHECK_ERROR();

	for (size_t i = 0; i < max_decode; i++) {
		uint8_t const *payload_bak;
		size_t elem_count_bak;

		if (backup) {
			if (!zcbor_new_backup_w_elem_state(state, state->elem_count, true)) {
				ZCBOR_FAIL();
			}
		} else {
			payload_bak = state->payload;
			elem_count_bak = state->elem_count;
		}

		if (!decoder(state, (uint8_t *)result + i*result_len)) {
			*num_decode = i;

			if (backup) {
				if (!zcbor_process_backup(state,
						ZCBOR_FLAG_CONSUME | ZCBOR_FLAG_RESTORE,
						ZCBOR_MAX_ELEM_COUNT)) {
					ZCBOR_FAIL();
				}
			} else {
				state->payload = payload_bak;
				state->elem_count = elem_count_bak;
			}

			zcbor_log("Found %zu elements.\r\n", i);
			ZCBOR_ERR_IF(i < min_decode, ZCBOR_ERR_ITERATIONS);
			return true;
		}

		if (backup) {
			if (!zcbor_process_backup(state, ZCBOR_FLAG_CONSUME, ZCBOR_MAX_ELEM_COUNT)) {
				ZCBOR_FAIL();
			}
		}
	}
	zcbor_log("Found %zu elements.\r\n", max_decode);
	*num_decode = max_decode;
	return true;
}


bool zcbor_multi_decode(size_t min_decode,
		size_t max_decode,
		size_t *num_decode,
		zcbor_decoder_t decoder,
		zcbor_state_t *state,
		void *result,
		size_t result_len)
{
	ZCBOR_PRINT_FUNC_NAME();
	return multi_decode_backup(min_decode, max_decode, num_decode, decoder, state, result, result_len, false);
}

bool zcbor_multi_decode_w_backup(size_t min_decode,
		size_t max_decode,
		size_t *num_decode,
		zcbor_decoder_t decoder,
		zcbor_state_t *state,
		void *result,
		size_t result_len)
{
	ZCBOR_PRINT_FUNC_NAME();
	return multi_decode_backup(min_decode, max_decode, num_decode, decoder, state, result, result_len, true);
}


bool zcbor_present_decode(bool *present,
		zcbor_decoder_t decoder,
		zcbor_state_t *state,
		void *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	ZCBOR_CHECK_NULL(state);
	size_t num_decode = 0;
	bool retval = multi_decode_backup(0, 1, &num_decode, decoder, state, result, 0, false);

	zcbor_assert_state(retval, "zcbor_multi_decode should not fail with these parameters.\r\n");

	*present = !!num_decode;
	return retval;
}


bool zcbor_present_decode_w_backup(bool *present,
		zcbor_decoder_t decoder,
		zcbor_state_t *state,
		void *result)
{
	ZCBOR_PRINT_FUNC_NAME();
	size_t num_decode = 0;
	bool retval = multi_decode_backup(0, 1, &num_decode, decoder, state, result, 0, true);

	*present = !!num_decode;
	return retval;
}


void zcbor_new_decode_state(zcbor_state_t *state_array, size_t n_states,
		const uint8_t *payload, size_t payload_len, size_t elem_count,
		uint8_t *flags, size_t flags_bytes)
{
	zcbor_new_state(state_array, n_states, payload, payload_len, elem_count, flags, flags_bytes);
}
