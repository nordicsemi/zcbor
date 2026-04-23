/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZCBOR_DECODE_H__
#define ZCBOR_DECODE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "zcbor_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The zcbor_decode library provides functions for decoding CBOR data elements.
 *
 * See The README for an introduction to CBOR, including the meaning of pint,
 * nint, bstr etc.
 */


/** See @ref zcbor_new_state() */
void zcbor_new_decode_state(zcbor_state_t *state_array, size_t n_states,
		const uint8_t *payload, size_t payload_len, size_t elem_count,
		uint8_t *elem_state, size_t elem_state_bytes);

/** Convenience macro for declaring and initializing a decoding state with backups.
 *
 *  This gives you a state variable named @p name. The variable functions like
 *  a pointer.
 *
 *  @param[in]  name          The name of the new state variable.
 *  @param[in]  num_backups   The number of backup slots to keep in the state.
 *  @param[in]  payload       The payload to work on.
 *  @param[in]  payload_size  The size (in bytes) of @p payload.
 *  @param[in]  elem_count    The starting elem_count (typically 1).
 *  @param[in]  n_flags       For use if ZCBOR_MAP_SMART_SEARCH is enabled, ignored otherwise.
 *                            The total number of unordered map search flags needed.
 *                            I.e. the largest number of elements expected in an unordered map,
 *                            including elements in nested unordered maps.
 */
#define ZCBOR_STATE_D(name, num_backups, payload, payload_size, elem_count, n_flags) \
zcbor_state_t name[((num_backups) + ZCBOR_EXTRA_STATES + ZCBOR_FLAG_STATES(n_flags))]; \
do { \
	zcbor_new_decode_state(name, ZCBOR_ARRAY_SIZE(name), payload, payload_size, elem_count, \
			(uint8_t *)&name[(num_backups) + 1], ZCBOR_FLAG_STATES(n_flags) * sizeof(zcbor_state_t)); \
} while(0)


/** The following applies to all _decode() functions listed directly below.
 *
 * @param[inout] state        The current state of the decoding.
 * @param[out]   result       Where to place the decoded value.
 * @param[in]    result_size  (if present) Size in bytes of the memory at @p result
 *
 * @retval true   If the value was decoded correctly.
 * @retval false  If the value has the wrong type, the payload overflowed, the
 *                element count was exhausted, or the value was larger than can
 *                fit in the result variable.
 *                Use zcbor_peek_error() to see the error code.
 */
bool zcbor_int8_decode(zcbor_state_t *state, int8_t *result);
bool zcbor_int16_decode(zcbor_state_t *state, int16_t *result);
bool zcbor_int32_decode(zcbor_state_t *state, int32_t *result); /* pint/nint */
bool zcbor_int64_decode(zcbor_state_t *state, int64_t *result); /* pint/nint */
bool zcbor_uint8_decode(zcbor_state_t *state, uint8_t *result);
bool zcbor_uint16_decode(zcbor_state_t *state, uint16_t *result);
bool zcbor_uint32_decode(zcbor_state_t *state, uint32_t *result); /* pint */
bool zcbor_uint64_decode(zcbor_state_t *state, uint64_t *result); /* pint */
bool zcbor_size_decode(zcbor_state_t *state, size_t *result); /* pint */
bool zcbor_int_decode(zcbor_state_t *state, void *result, size_t result_size); /* pint/nint */
bool zcbor_uint_decode(zcbor_state_t *state, void *result, size_t result_size); /* pint */
bool zcbor_bstr_decode(zcbor_state_t *state, struct zcbor_string *result); /* bstr */
bool zcbor_tstr_decode(zcbor_state_t *state, struct zcbor_string *result); /* tstr */
bool zcbor_tag_decode(zcbor_state_t *state, uint32_t *result);  /* CBOR tag */
bool zcbor_bool_decode(zcbor_state_t *state, bool *result); /* boolean CBOR simple value */
bool zcbor_float16_decode(zcbor_state_t *state, float *result); /* IEEE754 float16 */
bool zcbor_float16_bytes_decode(zcbor_state_t *state, uint16_t *result); /* IEEE754 float16 raw bytes */
bool zcbor_float16_32_decode(zcbor_state_t *state, float *result); /* IEEE754 float16 or float32 */
bool zcbor_float32_decode(zcbor_state_t *state, float *result); /* IEEE754 float32 */
bool zcbor_float32_64_decode(zcbor_state_t *state, double *result); /* IEEE754 float32 or float64 */
bool zcbor_float64_decode(zcbor_state_t *state, double *result); /* IEEE754 float64 */
bool zcbor_float_decode(zcbor_state_t *state, double *result); /* IEEE754 float16, float32, or float64 */

/** The following applies to all _expect() and _pexpect() functions listed directly below.
 *
 * @param[inout] state     The current state of the decoding.
 * @param[in]    expected  The expected value.
 *
 * @retval true   If the result was decoded correctly and has the expected value.
 * @retval false  If the decoding failed or the result doesn't have the
 *                expected value.
 *                Use zcbor_peek_error() to see the error code.
 */
bool zcbor_int8_expect(zcbor_state_t *state, int8_t expected); /* pint/nint */
bool zcbor_int16_expect(zcbor_state_t *state, int16_t expected); /* pint/nint */
bool zcbor_int32_expect(zcbor_state_t *state, int32_t expected); /* pint/nint */
bool zcbor_int64_expect(zcbor_state_t *state, int64_t expected); /* pint/nint */
bool zcbor_uint8_expect(zcbor_state_t *state, uint8_t expected); /* pint */
bool zcbor_uint16_expect(zcbor_state_t *state, uint16_t expected); /* pint */
bool zcbor_uint32_expect(zcbor_state_t *state, uint32_t expected); /* pint */
bool zcbor_uint64_expect(zcbor_state_t *state, uint64_t expected); /* pint */
bool zcbor_size_expect(zcbor_state_t *state, size_t expected); /* pint */
bool zcbor_bstr_expect(zcbor_state_t *state, struct zcbor_string *expected); /* bstr */
bool zcbor_tstr_expect(zcbor_state_t *state, struct zcbor_string *expected); /* tstr */
bool zcbor_tag_expect(zcbor_state_t *state, uint32_t expected); /* CBOR tag */
bool zcbor_bool_expect(zcbor_state_t *state, bool expected); /* boolean CBOR simple value */
bool zcbor_nil_expect(zcbor_state_t *state, void *unused); /* 'nil' CBOR simple value */
bool zcbor_undefined_expect(zcbor_state_t *state, void *unused); /* 'undefined' CBOR simple value */
bool zcbor_float16_expect(zcbor_state_t *state, float expected); /* IEEE754 float16 */
bool zcbor_float16_bytes_expect(zcbor_state_t *state, uint16_t expected); /* IEEE754 float16 raw bytes */
bool zcbor_float16_32_expect(zcbor_state_t *state, float expected); /* IEEE754 float16 or float32 */
bool zcbor_float32_expect(zcbor_state_t *state, float expected); /* IEEE754 float32 */
bool zcbor_float32_64_expect(zcbor_state_t *state, double expected); /* IEEE754 float32 or float64 */
bool zcbor_float64_expect(zcbor_state_t *state, double expected); /* IEEE754 float64 */
bool zcbor_float_expect(zcbor_state_t *state, double expected); /* IEEE754 float16, float32, or float64 */

/** Like the _expect() functions but the value is passed through a pointer.
 * (for use as a zcbor_decoder_t function) */
bool zcbor_int8_pexpect(zcbor_state_t *state, int8_t *expected); /* pint/nint */
bool zcbor_int16_pexpect(zcbor_state_t *state, int16_t *expected); /* pint/nint */
bool zcbor_int32_pexpect(zcbor_state_t *state, int32_t *expected); /* pint/nint */
bool zcbor_int64_pexpect(zcbor_state_t *state, int64_t *expected); /* pint/nint */
bool zcbor_uint8_pexpect(zcbor_state_t *state, uint8_t *expected); /* pint */
bool zcbor_uint16_pexpect(zcbor_state_t *state, uint16_t *expected); /* pint */
bool zcbor_uint32_pexpect(zcbor_state_t *state, uint32_t *expected); /* pint */
bool zcbor_uint64_pexpect(zcbor_state_t *state, uint64_t *expected); /* pint */
bool zcbor_size_pexpect(zcbor_state_t *state, size_t *expected); /* pint */
bool zcbor_tag_pexpect(zcbor_state_t *state, uint32_t *expected); /* CBOR tag */
bool zcbor_bool_pexpect(zcbor_state_t *state, bool *expected); /* boolean CBOR simple value */
bool zcbor_float16_pexpect(zcbor_state_t *state, float *expected); /* IEEE754 float16 */
bool zcbor_float16_bytes_pexpect(zcbor_state_t *state, uint16_t *expected); /* IEEE754 float16 raw bytes */
bool zcbor_float16_32_pexpect(zcbor_state_t *state, float *expected); /* IEEE754 float16 or float32 */
bool zcbor_float32_pexpect(zcbor_state_t *state, float *expected); /* IEEE754 float32 */
bool zcbor_float32_64_pexpect(zcbor_state_t *state, double *expected); /* IEEE754 float32 or float64 */
bool zcbor_float64_pexpect(zcbor_state_t *state, double *expected); /* IEEE754 float64 */
bool zcbor_float_pexpect(zcbor_state_t *state, double *expected); /* IEEE754 float16, float32, or float64 */

/** Consume and expect a pint/nint with a certain value, within a union.
 *
 * Calls @ref zcbor_union_elem_code then @ref zcbor_[u]int[32|64]_expect.
 */
bool zcbor_int8_expect_union(zcbor_state_t *state, int8_t expected);
bool zcbor_int16_expect_union(zcbor_state_t *state, int16_t expected);
bool zcbor_int32_expect_union(zcbor_state_t *state, int32_t expected);
bool zcbor_int64_expect_union(zcbor_state_t *state, int64_t expected);
bool zcbor_uint8_expect_union(zcbor_state_t *state, uint8_t expected);
bool zcbor_uint16_expect_union(zcbor_state_t *state, uint16_t expected);
bool zcbor_uint32_expect_union(zcbor_state_t *state, uint32_t expected);
bool zcbor_uint64_expect_union(zcbor_state_t *state, uint64_t expected);

/** Decode and consume a list/map header.
 *
 * The contents of the list can be decoded via subsequent function calls.
 * A state backup is created to keep track of the element count.
 * Call @ref zcbor_list_end_decode / @ref zcbor_map_end_decode when done
 * decoding the contents of the list/map
 *
 * @retval true   Header decoded correctly
 * @retval false  Header decoded incorrectly, or backup failed.
 */
bool zcbor_list_start_decode(zcbor_state_t *state);
bool zcbor_map_start_decode(zcbor_state_t *state);
bool zcbor_unordered_map_start_decode(zcbor_state_t *state);

/** Search for a key in a map.
 *
 * The CBOR spec allows elements (key-value pairs) in maps to appear in any order.
 * This function should be used when the order of elements is unknown.
 *
 * This must only be used while inside a map that has been entered via
 * @ref zcbor_unordered_map_start_decode. Use @ref zcbor_unordered_map_end_decode
 * when leaving the map.
 *
 * This function searches for keys. When this function returns successfully,
 * the @p state is pointing to the value corresponding to the found key.
 * Therefore, to be able to call this function again, the value must first be
 * decoded or skipped.
 *
 * When searching unordered maps, the found elements must be kept track of.
 * By default, this function automatically keeps track, which means it keeps a
 * running count of the number of found elements, which is checked when exiting
 * the map. You can do this manually instead, see @ref zcbor_elem_processed and
 * @ref manually_process_elem. If ZCBOR_MAP_SMART_SEARCH is defined, a flag is
 * kept for each element, instead of a rolling count.
 *
 * @note Unless ZCBOR_MAP_SMART_SEARCH is defined,
 *       elements are not individually marked as processed, so they may
 *       be returned again in a subsequent call to this function, if it is
 *       matched by the @p key_decoder of that call. Because of this, you should
 *       only use this function when you know the @p key_decoder matches no more
 *       than one of the keys. Typically this means all keys are known strings
 *       or integers, i.e. the @p key_decoder is typically a _pexpect() function.
 *
 * When searching for strings, there are convenience functions available,
 * see the zcbor_search_key_* functions.
 *
 * @param[in] key_decoder  A decoding function that will be tried against all
 *                         keys in the map until it returns true, at which point
 *                         @ref zcbor_unordered_map_search will return true.
 *                         For example, a zcbor_*_pexpect() function.
 *                         Please use @ref ZCBOR_CAST_FP to cast the function
 *                         since it will also check for compatibility.
 * @param[inout] state  The current state of decoding. Must be currently decoding
 *                      the contents of a map, and pointing to one (any) of the
 *                      keys, not one of the values. If successful, the @p state
 *                      will be pointing to the value corresponding to the
 *                      matched key. If unsuccessful, the @p state will be
 *                      unchanged.
 * @param[inout] key_result  This will be passed as the second argument to the
 *                           @p key_decoder.
 *
 * @retval true   If the key was found, i.e. @p key_decoder returned true.
 * @retval false  If the key was not found after searching all map elements.
 *                Or the map was pointing to a value (not a key).
 *                Or an unexpected error happened while skipping elements or
 *                jumping from the end of the map to the start.
 */
bool zcbor_unordered_map_search(zcbor_decoder_t key_decoder, zcbor_state_t *state, void *key_result);

/** Find a specific bstr/tstr key as part of a map with unknown element order.
 *
 * Uses @ref zcbor_unordered_map_search under the hood. Please refer to those docs
 * for the conditions under which this can be called.
 * Refer to the docs for zcbor_(t|b)str_expect_* (e.g. @ref zcbor_bstr_expect_ptr)
 * for parameter docs.
 */
bool zcbor_search_key_bstr_ptr(zcbor_state_t *state, char const *ptr, size_t len);
bool zcbor_search_key_tstr_ptr(zcbor_state_t *state, char const *ptr, size_t len);
bool zcbor_search_key_bstr_term(zcbor_state_t *state, char const *str, size_t maxlen);
bool zcbor_search_key_tstr_term(zcbor_state_t *state, char const *str, size_t maxlen);
#define zcbor_search_key_bstr_lit(state, str) zcbor_search_key_bstr_ptr(state, str, sizeof(str) - 1)
#define zcbor_search_key_tstr_lit(state, str) zcbor_search_key_tstr_ptr(state, str, sizeof(str) - 1)
#define zcbor_search_key_bstr_arr(state, str) zcbor_search_key_bstr_ptr(state, str, (sizeof(str)))
#define zcbor_search_key_tstr_arr(state, str) zcbor_search_key_tstr_ptr(state, str, (sizeof(str)))

/** Manually mark an (unordered map) element as processed.
 *
 * @note This must only be called when the @ref manually_process_elem flag is set,
 *       in which case it is mandatory.
 *       By default, i.e. when @ref manually_process_elem is not set, this function is
 *       called internally by @ref zcbor_unordered_map_search whenever a key is found.
 *
 * @note This function must be called after the key has been found, before the next key
 *       is searched for, or before the map is exited.
 *
 * If ZCBOR_MAP_SMART_SEARCH is NOT defined, this function increments the internal
 * count @ref map_elems_processed.
 *
 * If ZCBOR_MAP_SMART_SEARCH is defined, this function instead clears a flag for the
 * element (key-value pair) that is currently being processed, or that has just been
 * processed, meaning the element won't be found again via @ref zcbor_unordered_map_search.
 *
 * @ref zcbor_unordered_map_end_decode will fail if @ref map_elems_processed does not
 * match the number of elements in the map, or if any of the map element's flag is set.
 */
bool zcbor_elem_processed(zcbor_state_t *state);

/** Finalize decoding a list/map
 *
 * Check that the list/map had the correct number of elements, and restore the
 * previous element count from the backup.
 *
 * Use @ref zcbor_list_map_end_force_decode to forcibly consume the backup if
 * something has gone wrong.
 *
 * In all successful cases, the state is returned pointing to the byte/element
 * after the list/map in the payload.
 *
 * @retval true   Everything ok.
 * @retval false  Element count not correct.
 */
bool zcbor_list_end_decode(zcbor_state_t *state);
bool zcbor_map_end_decode(zcbor_state_t *state);
bool zcbor_unordered_map_end_decode(zcbor_state_t *state);
bool zcbor_list_map_end_force_decode(zcbor_state_t *state);

/** Find whether the state is at the end of a list or map.
 */
bool zcbor_array_at_end(zcbor_state_t *state);

/** Skip a single element, regardless of type and value.
 *
 * This means if the element is a map or list, this function will recursively
 * skip all its contents.
 * This function will also skip any tags preceeding the element.
 *
 * @param[inout] state   The current state of the decoding.
 * @param[in]    unused  Unused parameter to maintain signature parity with
 *                       @ref zcbor_decoder_t.
 */
bool zcbor_any_skip(zcbor_state_t *state, void *unused);

/** Decode 0 or more elements with the same type and constraints.
 *
 * The decoded values will appear consecutively in the @p result array.
 *
 * The following is an example of decoding a list containing 3 INTS followed by
 * 0 to 2 bstrs:
 *
 * @code{c}
 *     uint32_t ints[3];
 *     struct zcbor_string bstrs[2];
 *     uint32_t num_decode;
 *     bool res;
 *
 *     res = zcbor_list_start_decode(state);
 *     res = res && zcbor_multi_decode(3, 3, &num_decode, zcbor_uint32_decode,
 *                  state, ints, sizeof(ints[0]));
 *     res = res && zcbor_multi_decode(0, 2, &num_decode, zcbor_bstr_decode,
 *                  state, bstrs, sizeof(bstrs[0]));
 *     res = res && zcbor_list_end_decode(state);
 *     // check res
 * @endcode
 *
 * The @ref zcbor_decoder_t type is designed to be compatible with all single-
 * value decoder functions in this library, e.g. @ref zcbor_uint32_decode,
 * @ref zcbor_tstr_expect, @ref zcbor_nil_expect, etc. For _expect() functions,
 * @p result will be used as a value instead of an array/pointer, so
 * @p result_len will determine how much the value changes for each call.
 * To decode the same value multiple times, use a @p result_len of 0.
 * This function can also be used with custom decoder functions, such as those
 * generated by the zcbor.py script, which for example decodes larger chunks of
 * the data at once.
 *
 * @ref zcbor_multi_decode_w_backup is a variant of this function that takes a
 * backup of the elem_state before attempting each call to @p decoder.
 * This must be used if the decoder function might modify the elem_state then fail.
 * The elem_state is used when decoding unordered maps, see the
 * @ref map_search_elem_state docs for more information.
 *
 * @param[in]  min_decode    The minimum acceptable number of elements.
 * @param[in]  max_decode    The maximum acceptable number of elements.
 * @param[out] num_decode    The actual number of elements decoded.
 * @param[in]  decoder       The decoder function to call under the hood. This
 *                           function will be called with the provided arguments
 *                           repeatedly until the function fails (returns false)
 *                           or until it has been called @p max_decode times.
 *                           The result pointer is moved @p result_len bytes for
 *                           each call to @p decoder, i.e. @p result refers to
 *                           an array of result variables.
 *                           Should not be an _expect() function, use
 *                           _pexpect() instead.
 *                           Please use @ref ZCBOR_CAST_FP to cast the function
 *                           since it will also check for compatibility.
 * @param[out] result        Where to place the decoded values. Must be an array
 *                           of at least @p max_decode elements.
 * @param[in]  result_len    The length of each result variable. Must be the
 *                           length of the individual elements of @p result.
 *
 * @retval true   If at least @p min_decode variables were correctly decoded.
 * @retval false  If @p decoder failed before having decoded @p min_decode
 *                values.
 */
bool zcbor_multi_decode(size_t min_decode, size_t max_decode, size_t *num_decode,
		zcbor_decoder_t decoder, zcbor_state_t *state, void *result,
		size_t result_len);
bool zcbor_multi_decode_w_backup(size_t min_decode, size_t max_decode, size_t *num_decode,
		zcbor_decoder_t decoder, zcbor_state_t *state, void *result,
		size_t result_len);

/** Attempt to decode a value that might not be present in the data.
 *
 * Works like @ref zcbor_multi_decode, with @p present as num_decode.
 * Will return true, even if the data is not present.
 *
 * For details on @ref zcbor_present_decode_w_backup, see the docs for
 * @ref zcbor_multi_decode_w_backup.
 *
 * @param[out] present  Whether or not the data was present and successfully decoded.
 * @param[in]  decoder  The decoder to attempt.
 *                      Please use @ref ZCBOR_CAST_FP to cast the function
 *                      since it will also check for compatibility.
 * @param[out] result   The result, if present.
 *
 * @return Should always return true.
 */
bool zcbor_present_decode(bool *present,
		zcbor_decoder_t decoder,
		zcbor_state_t *state,
		void *result);
bool zcbor_present_decode_w_backup(bool *present,
		zcbor_decoder_t decoder,
		zcbor_state_t *state,
		void *result);

/** Supplementary string (bstr/tstr) decoding functions: */

/** Consume and expect a bstr/tstr with the value of the provided char/uint8_t array.
 *
 * @param[inout] state   The current state of the decoding.
 * @param[in]    str     The value to expect. A pointer to the string/array.
 *                       _term() uses strnlen(), so @p str must be null-terminated.
 *                       _lit() uses sizeof()-1, so @p str must be a (null-terminated) string literal.
 *                       _arr() uses sizeof(), so @p str must be a uint8_t array (not null-terminated).
 * @param[in]    len     (if present) The length of the string pointed to by @p str
 * @param[in]    maxlen  (if present) The maximum length of the string pointed to by @p str.
 *                       This value is passed to strnlen.
 */
bool zcbor_bstr_expect_ptr(zcbor_state_t *state, char const *ptr, size_t len);
bool zcbor_tstr_expect_ptr(zcbor_state_t *state, char const *ptr, size_t len);
bool zcbor_bstr_expect_term(zcbor_state_t *state, char const *str, size_t maxlen);
bool zcbor_tstr_expect_term(zcbor_state_t *state, char const *str, size_t maxlen);
#define zcbor_bstr_expect_lit(state, str) zcbor_bstr_expect_ptr(state, str, sizeof(str) - 1)
#define zcbor_tstr_expect_lit(state, str) zcbor_tstr_expect_ptr(state, str, sizeof(str) - 1)
#define zcbor_bstr_expect_arr(state, str) zcbor_bstr_expect_ptr(state, str, sizeof(str))
#define zcbor_tstr_expect_arr(state, str) zcbor_tstr_expect_ptr(state, str, sizeof(str))

/** Decode and consume a bstr header.
 *
 * The rest of the string can be decoded as CBOR.
 * A state backup is created to keep track of the element count and original payload_end.
 * payload_end is set to the end of the string, so when the payload is exhausted,
 * the string is considered fully decoded.
 * Call @ref zcbor_bstr_end_decode when done decoding the contents of the bstr.
 *
 * @param[inout] state   The current state of the decoding.
 * @param[out]   result  The resulting string, for reference. The string should be decoded via
 *                       functions from this API since state is pointing to the start of the string,
 *                       not the end.
 *
 * @retval true   Header decoded correctly
 * @retval false  Header decoded incorrectly, or backup failed, or payload is not large enough
 *                to contain the contents of the string. Use @ref zcbor_bstr_start_decode_fragment
 *                for decoding fragmented payloads.
 */
bool zcbor_bstr_start_decode(zcbor_state_t *state, struct zcbor_string *result);

/** Finalize decoding a CBOR-encoded bstr.
 *
 * Restore element count from backup.
 */
bool zcbor_bstr_end_decode(zcbor_state_t *state);
bool zcbor_bstr_end_force_decode(zcbor_state_t *state);


#ifdef ZCBOR_FRAGMENTS

/** Start decoding a fragmented string.
 *
 * I.e. a string spread over multiple non-contiguous payload sections.
 *
 * NOTE: The fragmented string API is experimental.
 *
 * After calling this, you can retrieve a fragment with @ref zcbor_str_fragment_decode,
 * then update the payload with @ref zcbor_update_state.
 * Repeat until the string is fully decoded, then call @ref zcbor_bstr_fragments_end_decode.
 */
bool zcbor_bstr_fragments_start_decode(zcbor_state_t *state);
bool zcbor_tstr_fragments_start_decode(zcbor_state_t *state);

/** Start decoding a fragmented CBOR-encoded bytestring.
 *
 * I.e. a string spread over multiple non-contiguous payload sections.
 *
 * NOTE: The fragmented string API is experimental.
 *
 * This is an alternative to zcbor_*str_fragments_start_decode() to be used if the payload
 * contains CBOR data that will be decoded directly with other zcbor_*() functions.
 *
 * A state backup is created to keep track of the element count and original payload_end.
 * After calling this, you can decode elements using other zcbor functions,
 * then update the payload with @ref zcbor_update_state.
 * Repeat until the string is fully decoded, then call @ref zcbor_bstr_fragments_end_decode.
 * When the current payload section contains the end of the string,
 * payload_end is set to the end of the string, so there is no risk of decoding past the end.
 * Then, when @ref zcbor_bstr_fragments_end_decode has been called, payload_end is restored to the
 * end of the payload section.
 * (This is analogous to how @ref zcbor_bstr_start_decode works)
 */
bool zcbor_cbor_bstr_fragments_start_decode(zcbor_state_t *state);

/** Retrieve a string fragment.
 *
 * NOTE: The fragmented string API is experimental.
 *
 * For non-cbor-encoded strings (started with @ref zcbor_bstr_fragments_start_decode
 * or @ref zcbor_tstr_fragments_start_decode): Consumes bytes from the payload until
 * either the end of the payload or the end of the string.
 *
 * For cbor-encoded strings (started with @ref zcbor_cbor_bstr_fragments_start_decode):
 * No bytes are consumed from the payload. Instead, this function reports the part of
 * the current payload section that has already been decoded via other zcbor functions.
 * Any such call to this function will have no memory of previous calls to this function,
 * so the returned fragment always starts at the same point until the payload is updated.
 *
 * @param[inout] state     The current state of the decoding.
 * @param[out]   fragment  The resulting string fragment.
 */
bool zcbor_str_fragment_decode(zcbor_state_t *state, struct zcbor_string_fragment *fragment);

/** Finish decoding a fragmented string. */
bool zcbor_str_fragments_end_decode(zcbor_state_t *state);

#endif /* ZCBOR_FRAGMENTS */

#ifdef __cplusplus
}
#endif

#endif /* ZCBOR_DECODE_H__ */
