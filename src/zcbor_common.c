/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "zcbor_common.h"
#include "zcbor_print.h"

_Static_assert((sizeof(size_t) == sizeof(void *)),
	"This code needs size_t to be the same length as pointers.");

_Static_assert((sizeof(zcbor_state_t) >= sizeof(struct zcbor_state_constant)),
	"This code needs zcbor_state_t to be at least as large as zcbor_backups_t.");


#ifdef ZCBOR_MAP_SMART_SEARCH
/** Take a backup of the elem_state by copying it to immediately after itself.
 *
 *  The copy becomes the active elem_state.
 *  This way, since the active state is after the backup, the elem_state can
 *  continue to grow without colliding with the backup.
 *
 *  @param[in] dry_run If true, only the checks are performed, and no backup is made.
 */
static bool do_elem_state_backup(zcbor_state_t *state, bool dry_run)
{
	uint8_t *flags = state->decode_state.map_search_elem_state;

	if (!flags) {
		ZCBOR_ERR(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE);
	}

	size_t flags_len = zcbor_flags_to_bytes(state->decode_state.map_elem_count);

	/* Check for integer overflow
	 * The expression is gained from rearranging: (flags + flags_len * 2) > SIZE_MAX
	 * which matches how the variables are used in the subsequent check. */
	if (flags_len > ((SIZE_MAX - (size_t)flags) / 2)) {
		ZCBOR_ERR(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE);
	}

	if ((flags + flags_len * 2) > (state->constant_state->map_search_elem_state_end)) {
		ZCBOR_ERR(ZCBOR_ERR_MAP_FLAGS_NOT_AVAILABLE);
	}

	if (!dry_run) {
		memcpy(flags + flags_len, flags, flags_len);
		state->decode_state.map_search_elem_state += flags_len;
	}

	return true;
}

/** Discard the elem_state backup by copying the active elem_state to overwrite the backup. */
static void discard_elem_state_backup(zcbor_state_t *state, zcbor_state_t *backup)
{
	uint8_t *flags = state->decode_state.map_search_elem_state;
	uint8_t *backup_flags = backup->decode_state.map_search_elem_state;
	size_t flags_len = zcbor_flags_to_bytes(state->decode_state.map_elem_count);

	/* Overwrite the backup with the current elem_state.
	 * Use memmove because the active elem_state may have expanded since the
	 * backup was made, in which case the source and destination areas overlap. */
	memmove(backup_flags, flags, flags_len);
	state->decode_state.map_search_elem_state = backup_flags;
}
#endif


bool zcbor_new_backup_w_elem_state(zcbor_state_t *state, size_t new_elem_count, bool backup_elem_state)
{
	(void)backup_elem_state; // Possibly unused
	ZCBOR_CHECK_NULL(state);
	ZCBOR_PRINT_FUNC_NAME_ARGS("(backup_elem_state=%u, backup_num=%zu)", backup_elem_state, state->constant_state->current_backup+1);
	zcbor_log("\r\n");
	ZCBOR_CHECK_ERROR();

	if ((state->constant_state->current_backup)
		>= state->constant_state->num_backups) {
		ZCBOR_ERR(ZCBOR_ERR_NO_BACKUP_MEM);
	}

#ifdef ZCBOR_MAP_SMART_SEARCH
	if (backup_elem_state) {
		ZCBOR_FAIL_IF(!do_elem_state_backup(state, true));
	}
	state->decode_state.elem_state_backed_up = backup_elem_state;
#endif

	state->payload_moved = false;

	(state->constant_state->current_backup)++;

	/* use the backup at current_backup - 1, since otherwise, the 0th
	 * backup would be unused. */
	size_t i = (state->constant_state->current_backup) - 1;

	state->constant_state->backup_list[i] = *state;

	state->elem_count = new_elem_count;

#ifdef ZCBOR_MAP_SMART_SEARCH
	if (backup_elem_state) {
		ZCBOR_FAIL_IF(!do_elem_state_backup(state, false));
	}
#endif

	return true;
}


bool zcbor_new_backup(zcbor_state_t *state, size_t new_elem_count)
{
	return zcbor_new_backup_w_elem_state(state, new_elem_count, false);
}


bool zcbor_process_backup_num(zcbor_state_t *state, uint32_t flags,
		size_t max_elem_count, size_t backup_num)
{
	ZCBOR_PRINT_FUNC_NAME_ARGS("(flags=%u, backup_num=%zu)", flags, backup_num);
	zcbor_log("\r\n");
	ZCBOR_CHECK_NULL(state);
	ZCBOR_CHECK_ERROR();

	size_t i = backup_num - 1;

	if ((backup_num == 0)
		 || (backup_num > state->constant_state->current_backup)
		 || (i >= state->constant_state->num_backups)) {
		/* The backup_num is currently 1-based, so this also catches underflow. */
		zcbor_log("Invalid backup number: %zu.\r\n", backup_num);
		ZCBOR_ERR(ZCBOR_ERR_BAD_ARG);
	}

	if (flags & ZCBOR_FLAG_CONSUME) {
		ZCBOR_ERR_IF((flags & ZCBOR_FLAG_KEEP_DECODE_STATE), ZCBOR_ERR_BAD_ARG);
		ZCBOR_ERR_IF(backup_num != state->constant_state->current_backup, ZCBOR_ERR_BAD_ARG);
	}


	zcbor_state_t local_copy = *state;
	zcbor_state_t *backup = &state->constant_state->backup_list[i];

	if (flags & ZCBOR_FLAG_RESTORE) {
		if (!(flags & ZCBOR_FLAG_KEEP_PAYLOAD)) {
			if (backup->payload_moved) {
				zcbor_log("Payload pointer out of date.\r\n");
				ZCBOR_ERR(ZCBOR_ERR_PAYLOAD_OUTDATED);
			}
		}
		*state = *backup;

#ifdef ZCBOR_MAP_SMART_SEARCH
		if (!(flags & ZCBOR_FLAG_CONSUME) && state->decode_state.elem_state_backed_up) {
			ZCBOR_FAIL_IF(!do_elem_state_backup(state, false));
		}
#endif
	}

	if (flags & ZCBOR_FLAG_CONSUME) {
#ifdef ZCBOR_MAP_SMART_SEARCH
		if (!(flags & ZCBOR_FLAG_RESTORE) && backup->decode_state.elem_state_backed_up) {
			discard_elem_state_backup(state, backup);
		}
#endif
		state->constant_state->current_backup--;
	}

	if (max_elem_count != ZCBOR_MAX_ELEM_COUNT) {
		zcbor_log("Deprecation warning: Using max_elem_count != ZCBOR_MAX_ELEM_COUNT is deprecated.\r\n");
		zcbor_log("See function documentation for details.\r\n");
	}
	if (local_copy.elem_count > max_elem_count) {
		zcbor_log("elem_count: %zu (expected max %zu)\r\n",
			local_copy.elem_count, max_elem_count);
		ZCBOR_ERR(ZCBOR_ERR_HIGH_ELEM_COUNT);
	}

	if (flags & ZCBOR_FLAG_KEEP_PAYLOAD) {
		state->payload = local_copy.payload;
	}

	if (flags & ZCBOR_FLAG_KEEP_DECODE_STATE) {
		/* Copy decode state */
		state->decode_state = local_copy.decode_state;
	}

	return true;
}


bool zcbor_process_backup(zcbor_state_t *state, uint32_t flags, size_t max_elem_count)
{
	ZCBOR_PRINT_FUNC_NAME();
	ZCBOR_CHECK_NULL(state);

	if (state->constant_state->current_backup == 0) {
		zcbor_log("No backups available.\r\n");
		ZCBOR_ERR(ZCBOR_ERR_NO_BACKUP_ACTIVE);
	}

	return zcbor_process_backup_num(state, flags, max_elem_count, state->constant_state->current_backup);
}



bool zcbor_union_start_code(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	ZCBOR_CHECK_NULL(state);

	if (!zcbor_new_backup_w_elem_state(state, state->elem_count, true)) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_union_elem_code(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	ZCBOR_CHECK_NULL(state);

	if (!zcbor_process_backup(state, ZCBOR_FLAG_RESTORE, ZCBOR_MAX_ELEM_COUNT)) {
		ZCBOR_FAIL();
	}
	return true;
}


bool zcbor_union_end_code(zcbor_state_t *state)
{
	ZCBOR_PRINT_FUNC_NAME();
	ZCBOR_CHECK_NULL(state);

	if (!zcbor_process_backup(state, ZCBOR_FLAG_CONSUME, ZCBOR_MAX_ELEM_COUNT)) {
		ZCBOR_FAIL();
	}
	return true;
}


void zcbor_new_state(zcbor_state_t *state_array, size_t n_states,
		const uint8_t *payload, size_t payload_len, size_t elem_count,
		uint8_t *flags, size_t flags_bytes)
{
	if (state_array == NULL) {
		return;
	}

	state_array[0].payload = payload;
	state_array[0].payload_end = payload + payload_len;
	state_array[0].elem_count = elem_count;
	state_array[0].payload_moved = false;
	state_array[0].decode_state.indefinite_length_array = false;
#ifdef ZCBOR_MAP_SMART_SEARCH
	state_array[0].decode_state.map_search_elem_state = flags;
	if (flags == NULL) {
		flags_bytes = 0;
	}
	state_array[0].decode_state.map_elem_count = 0;
#else
	state_array[0].decode_state.map_elems_processed = 0;
	(void)flags;
	(void)flags_bytes;
#endif
	state_array[0].inside_cbor_bstr = false;
#ifdef ZCBOR_FRAGMENTS
	state_array[0].inside_frag_str = false;
	state_array[0].frag_offset = 0;
	state_array[0].str_total_len = payload_len;
	state_array[0].frag_offset_cbor = 0;
	state_array[0].str_total_len_cbor = payload_len;
#endif
	state_array[0].constant_state = NULL;

	if (n_states < 2) {
		return;
	}

	/* Use the last state as a struct zcbor_state_constant object. */
	state_array[0].constant_state = (struct zcbor_state_constant *)&state_array[n_states - 1];
	state_array[0].constant_state->backup_list = NULL;
	state_array[0].constant_state->num_backups = n_states - 2;
	state_array[0].constant_state->current_backup = 0;
	state_array[0].constant_state->error = ZCBOR_SUCCESS;
#ifdef ZCBOR_STOP_ON_ERROR
	state_array[0].constant_state->stop_on_error = false;
#endif
	state_array[0].constant_state->enforce_canonical = ZCBOR_ENFORCE_CANONICAL_DEFAULT;
	state_array[0].constant_state->manually_process_elem = ZCBOR_MANUALLY_PROCESS_ELEM_DEFAULT;
#ifdef ZCBOR_MAP_SMART_SEARCH
	state_array[0].constant_state->map_search_elem_state_end = flags + flags_bytes;
#endif
#ifdef ZCBOR_FRAGMENTS
	state_array[0].constant_state->curr_payload_section = payload;
#endif
	if (n_states > 2) {
		state_array[0].constant_state->backup_list = &state_array[1];
	}
}


static void update_state(zcbor_state_t *state, const uint8_t *payload, size_t payload_len)
{
	const uint8_t *old_payload = state->payload;

	state->payload = payload;
	(void)old_payload;
#ifdef ZCBOR_FRAGMENTS
	ptrdiff_t prev_len = old_payload - state->constant_state->curr_payload_section;

	if (prev_len < 0) {
		zcbor_log("Previous payload section pointer invalid\n");
		return;
	}
	state->frag_offset += prev_len;
	state->frag_offset_cbor += prev_len;

	if (state->inside_cbor_bstr) {
		state->payload_end = payload + MIN(payload_len,
			(size_t)((ptrdiff_t)state->str_total_len_cbor - state->frag_offset_cbor));
	} else
#endif
	{
		state->payload_end = payload + payload_len;
	}
}


static void update_backups(zcbor_state_t *state, const uint8_t *old_payload, size_t new_payload_len)
{
	for (unsigned int i = 0; i < state->constant_state->current_backup; i++) {
		state->constant_state->backup_list[i].payload = old_payload;
		update_state(&state->constant_state->backup_list[i], state->payload, new_payload_len);
		state->constant_state->backup_list[i].payload_moved = true;
	}
}


void zcbor_update_state(zcbor_state_t *state, const uint8_t *payload, size_t payload_len)
{
	if (state == NULL) {
		return;
	}

	const uint8_t *old_payload = state->payload;
	update_state(state, payload, payload_len);
	update_backups(state, old_payload, payload_len);
	state->constant_state->curr_payload_section = payload;
}


#ifdef ZCBOR_FRAGMENTS

static size_t frag_str_total_len(zcbor_state_t *state)
{
	if (state->inside_frag_str) {
		return state->str_total_len;
	} else if (state->inside_cbor_bstr) {
		return state->str_total_len_cbor;
	} else {
		zcbor_assert(false, "Not inside a fragmented string\n");
		return 0;
	}
}

static ptrdiff_t frag_str_offset(zcbor_state_t *state)
{
	if (state->inside_frag_str) {
		return state->frag_offset;
	} else if (state->inside_cbor_bstr) {
		return state->frag_offset_cbor;
	} else {
		zcbor_assert(false, "Not inside a fragmented string\n");
		return 0;
	}
}

bool zcbor_current_string_offset(zcbor_state_t *state, size_t *offset)
{
	ZCBOR_CHECK_NULL(state);
	ZCBOR_CHECK_NULL(offset);

	if (!state->inside_frag_str && !state->inside_cbor_bstr) {
		zcbor_log("Not inside a fragmented string\n");
		ZCBOR_ERR(ZCBOR_ERR_NOT_IN_FRAGMENT);
	}

	ZCBOR_ERR_IF(state->payload < state->constant_state->curr_payload_section, ZCBOR_ERR_BAD_STATE);

	ptrdiff_t res = ((state->payload - state->constant_state->curr_payload_section)
				+ frag_str_offset(state));

	if (!((res >= 0) && res <= (ptrdiff_t)frag_str_total_len(state))) {
		zcbor_log("Payload not within string, malformed state?\n");
		ZCBOR_ERR(ZCBOR_ERR_BAD_STATE);
	}

	*offset = (size_t)res;
	return true;
}

bool zcbor_current_string_remainder(zcbor_state_t *state, size_t *remainder)
{
	size_t offset;

	ZCBOR_CHECK_NULL(state);
	ZCBOR_CHECK_NULL(remainder);

	ZCBOR_FAIL_IF(!zcbor_current_string_offset(state, &offset));
	*remainder = frag_str_total_len(state) - offset;
	return true;
}
#endif


bool zcbor_is_last_fragment(const struct zcbor_string_fragment *fragment)
{
	return (fragment->total_len == (fragment->offset + fragment->fragment.len));
}


bool zcbor_validate_string_fragments(struct zcbor_string_fragment *fragments,
		size_t num_fragments)
{
	size_t total_len = 0;

	if (fragments == NULL) {
		return false;
	}

	for (size_t i = 0; i < num_fragments; i++) {
		if (fragments[i].offset != total_len) {
			return false;
		}
		if (fragments[i].fragment.value == NULL) {
			return false;
		}
		if (fragments[i].total_len != fragments[0].total_len) {
			return false;
		}
		if ((total_len + fragments[i].fragment.len) < total_len) {
			/* Overflow */
			return false;
		}
		total_len += fragments[i].fragment.len;
		if (total_len > fragments[0].total_len) {
			return false;
		}
	}

	if (num_fragments && total_len != fragments[0].total_len) {
		return false;
	}

	if (num_fragments && (fragments[0].total_len == ZCBOR_STRING_FRAGMENT_UNKNOWN_LENGTH)) {
		for (size_t i = 0; i < num_fragments; i++) {
			fragments[i].total_len = total_len;
		}
	}

	return true;
}

bool zcbor_splice_string_fragments(struct zcbor_string_fragment *fragments,
		size_t num_fragments, uint8_t *result, size_t *result_len)
{
	size_t total_len = 0;

	if (!fragments || !result || !result_len) {
		return false;
	}

	for (size_t i = 0; i < num_fragments; i++) {
		/* Check overflow without risk of overflowing the len vars. */
		if ((total_len > *result_len)
			|| (fragments[i].fragment.len > (*result_len - total_len))) {
			return false;
		}
		memcpy(&result[total_len],
			fragments[i].fragment.value, fragments[i].fragment.len);
		total_len += fragments[i].fragment.len;
	}

	*result_len = total_len;
	return true;
}


bool zcbor_compare_strings(const struct zcbor_string *str1,
		const struct zcbor_string *str2)
{
	return (str1 != NULL) && (str2 != NULL)
		&& (str1->value != NULL) && (str2->value != NULL) && (str1->len == str2->len)
		&& (memcmp(str1->value, str2->value, str1->len) == 0);
}


size_t zcbor_header_len(uint64_t value)
{
	if (value <= ZCBOR_VALUE_IN_HEADER) {
		return 1;
	} else if (value <= 0xFF) {
		return 2;
	} else if (value <= 0xFFFF) {
		return 3;
	} else if (value <= 0xFFFFFFFF) {
		return 5;
	} else {
		return 9;
	}
}


size_t zcbor_header_len_ptr(const void *const value, size_t value_len)
{
	uint64_t val64 = 0;

	if (value_len > sizeof(val64)) {
		return 0;
	}

	memcpy(((uint8_t*)&val64) + ZCBOR_ECPY_OFFS(sizeof(val64), value_len), value, value_len);
	return zcbor_header_len(val64);
}


size_t zcbor_remaining_str_len(zcbor_state_t *state)
{
	ZCBOR_CHECK_NULL(state);

	size_t max_len = (size_t)state->payload_end - (size_t)state->payload;

	if (max_len == 0) {
		return 0;
	}

	size_t max_header_len = zcbor_header_len(max_len);
	size_t min_header_len = zcbor_header_len(max_len - max_header_len);
	size_t header_len = zcbor_header_len(max_len - min_header_len);

	return max_len - header_len;
}


int zcbor_entry_function_with_elem_states(const uint8_t *payload, size_t payload_len,
	void *result, size_t *payload_len_out, zcbor_state_t *states, zcbor_decoder_t func,
	size_t n_states, size_t elem_count, size_t n_elem_states)
{
	ZCBOR_CHECK_NULL(states);

	uint8_t *flags = NULL;
	size_t n_elem_state_bytes = 0;
	size_t n_elem_state_states = 0;

#ifdef ZCBOR_MAP_SMART_SEARCH
	if (n_elem_states > 0) {
		n_elem_state_states = ZCBOR_FLAG_STATES(n_elem_states);

		if (n_states < (n_elem_state_states + 3)) {
			return ZCBOR_ERR_NO_FLAG_MEM;
		}

		flags =	(uint8_t *)&states[n_states - n_elem_state_states]; /* Grab states from end of array. */
		n_elem_state_bytes = zcbor_flags_to_bytes(n_elem_states);
	}
#else
	(void)n_elem_states;
#endif

	zcbor_new_state(states, n_states - n_elem_state_states, payload, payload_len, elem_count, flags,
			n_elem_state_bytes);

	states[0].constant_state->manually_process_elem = true;

	bool ret = func(&states[0], result);

	if (!ret) {
		int err = zcbor_pop_error(&states[0]);

		err = (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
		return err;
	}

	if (payload_len_out != NULL) {
		*payload_len_out = MIN(payload_len,
				(size_t)states[0].payload - (size_t)payload);
	}
	return ZCBOR_SUCCESS;
}


/* Float16: */
#define F16_SIGN_OFFS 15 /* Bit offset of the sign bit. */
#define F16_EXPO_OFFS 10 /* Bit offset of the exponent. */
#define F16_EXPO_MSK 0x1F /* Bitmask for the exponent (right shifted by F16_EXPO_OFFS). */
#define F16_MANTISSA_MSK 0x3FF /* Bitmask for the mantissa. */
#define F16_MAX 65520 /* Lowest float32 value that rounds up to float16 infinity.
		       * (65519.996 rounds to 65504) */
#define F16_MIN_EXPO 24 /* Negative exponent of the non-zero float16 value closest to 0 (2^-24) */
#define F16_MIN (1.0f / (1 << F16_MIN_EXPO)) /* The non-zero float16 value closest to 0 (2^-24) */
#define F16_MIN_NORM (1.0f / (1 << 14)) /* The normalized float16 value closest to 0 (2^-14) */
#define F16_BIAS 15 /* The exponent bias of normalized float16 values. */

/* Float32: */
#define F32_SIGN_OFFS 31 /* Bit offset of the sign bit. */
#define F32_EXPO_OFFS 23 /* Bit offset of the exponent. */
#define F32_EXPO_MSK 0xFF /* Bitmask for the exponent (right shifted by F32_EXPO_OFFS). */
#define F32_MANTISSA_MSK 0x7FFFFF /* Bitmask for the mantissa. */
#define F32_BIAS 127 /* The exponent bias of normalized float32 values. */

/* Rounding: */
#define SUBNORM_ROUND_MSK (F32_MANTISSA_MSK | (1 << F32_EXPO_OFFS)) /* mantissa + lsb of expo for
								     * tiebreak. */
#define SUBNORM_ROUND_BIT_MSK (1 << (F32_EXPO_OFFS - 1)) /* msb of mantissa (0x400000) */
#define NORM_ROUND_MSK (F32_MANTISSA_MSK >> (F16_EXPO_OFFS - 1)) /* excess mantissa when going from
								  * float32 to float16 + 1 extra bit
								  * for tiebreak. */
#define NORM_ROUND_BIT_MSK (1 << (F32_EXPO_OFFS - F16_EXPO_OFFS - 1)) /* bit 12 (0x1000) */

/* Union type to avoid strict aliasing issues. */
union zcbor_float_int {
	float f;
	uint32_t i;
};

float zcbor_float16_to_32(uint16_t input)
{
	uint32_t sign = input >> F16_SIGN_OFFS;
	uint32_t expo = (input >> F16_EXPO_OFFS) & F16_EXPO_MSK;
	uint32_t mantissa = input & F16_MANTISSA_MSK;

	if ((expo == 0) && (mantissa != 0)) {
		/* Subnormal float16 - convert to normalized float32 */
		return ((float)mantissa * F16_MIN) * (sign ? -1 : 1);
	} else {
		/* Normalized / zero / Infinity / NaN */
		uint32_t new_expo = (expo == 0 /* zero */) ? 0
			: (expo == F16_EXPO_MSK /* inf/NaN */) ? F32_EXPO_MSK
				: (expo + (F32_BIAS - F16_BIAS));
		uint32_t value32 = (sign << F32_SIGN_OFFS) | (new_expo << F32_EXPO_OFFS)
			| (mantissa << (F32_EXPO_OFFS - F16_EXPO_OFFS));
		union zcbor_float_int output_u = { .i = value32 };
		return output_u.f;
	}
}


uint16_t zcbor_float32_to_16(float input)
{

	union zcbor_float_int input_u = { .f = input };
	uint32_t value32 = input_u.i;

	uint32_t sign = value32 >> F32_SIGN_OFFS;
	uint32_t expo = (value32 >> F32_EXPO_OFFS) & F32_EXPO_MSK;
	uint32_t mantissa = value32 & F32_MANTISSA_MSK;

	uint16_t value16 = (uint16_t)(sign << F16_SIGN_OFFS);

	union zcbor_float_int abs_input_u = { .i = value32 & ~(1U << F32_SIGN_OFFS) };
	float abs_input = abs_input_u.f;

	if (abs_input <= (F16_MIN / 2)) {
		/* 0 or too small for float16. Round down to 0. value16 is already correct. */
	} else if (abs_input < F16_MIN) {
		/* Round up to 2^(-24) (F16_MIN), has other rounding rules than larger values. */
		value16 |= 0x0001;
	} else if (abs_input < F16_MIN_NORM) {
		/* Subnormal float16 (normal float32) */
		uint32_t adjusted_mantissa =
			/* Adjust for the purposes of checking rounding. */
			/* The lsb of expo is needed for the cases where expo is 103 (minimum). */
			((value32 << (expo - (F32_BIAS - F16_MIN_EXPO))) & SUBNORM_ROUND_MSK);
		uint16_t rounding_bit =
			/* "Round to nearest, ties to even". */
			/* 0x400000 means ties go down towards even. (0xC00000 means ties go up.) */
			(adjusted_mantissa & SUBNORM_ROUND_BIT_MSK)
				&& (adjusted_mantissa != SUBNORM_ROUND_BIT_MSK);
		value16 |= ((uint16_t)(abs_input * (1 << 24)) + rounding_bit); /* expo is 0 */
	} else if (abs_input < F16_MAX) {
		/* Normal float16 (normal float32) */
		uint16_t rounding_bit =
			/* Bit 13 of the mantissa represents which way to round, except for the */
			/* special case where bits 0-12 and 14 are 0. */
			/* This is because of "Round to nearest, ties to even". */
			/* 0x1000 means ties go down towards even. (0x3000 means ties go up.) */
			((mantissa & NORM_ROUND_BIT_MSK)
				&& ((mantissa & NORM_ROUND_MSK) != NORM_ROUND_BIT_MSK));
		value16 |= (uint16_t)((expo - (F32_BIAS - F16_BIAS)) << F16_EXPO_OFFS);
		value16 |= (uint16_t)(mantissa >> (F32_EXPO_OFFS - F16_EXPO_OFFS));
		value16 += rounding_bit; /* Might propagate to exponent. */
	} else if (expo != F32_EXPO_MSK || !mantissa) {
		/* Infinite, or finite normal float32 too large for float16. Round up to inf. */
		value16 |= (F16_EXPO_MSK << F16_EXPO_OFFS);
	} else {
		/* NaN */
		/* Preserve msbit of mantissa. */
		uint16_t new_mantissa = (uint16_t)(mantissa >> (F32_EXPO_OFFS - F16_EXPO_OFFS));
		value16 |= (F16_EXPO_MSK << F16_EXPO_OFFS) | (new_mantissa ? new_mantissa : 1);
	}

	return value16;
}


/** Weak strnlen() implementation in case it is not available.
 *
 * This function is in the public domain, according to:
 * https://github.com/arm-embedded/gcc-arm-none-eabi.debian/blob/master/src/libiberty/strnlen.c
 */
__attribute__((__weak__))
size_t strnlen (const char *s, size_t maxlen)
{
	size_t i;

	for (i = 0; i < maxlen; ++i) {
		if (s[i] == '\0') {
			break;
		}
	}
	return i;
}


bool zcbor_cast_error(zcbor_state_t *state, void *unused)
{
	(void)state;
	(void)unused;
	zcbor_log("This function should never be called.\n");
	zcbor_log("It means ZCBOR_CAST_FP() was called with a bad function pointer.\n");
	ZCBOR_ERR(ZCBOR_ERR_BAD_ARG);
}
