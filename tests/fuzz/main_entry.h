/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MAIN_ENTRY_H__
#define MAIN_ENTRY_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <zcbor_common.h>

/** Implemented by each test case.
 *
 * @retval true   If the input was decoded successfully.
 * @retval false  If the input was rejected.
 */
bool fuzz_one_input(const uint8_t *data, size_t size);

/** Report a crash (abort) if @p err means that zcbor has a bug, as opposed to the input being
 *  invalid. Returns @p err so calls can be wrapped. */
int fuzz_check_err(int err);

/** Report a crash (abort) with a message. */
void fuzz_fail(const char *file, int line, const char *msg);

/** Check a condition that must hold for any input. */
#define FUZZ_ASSERT(cond) \
do { \
	if (!(cond)) { \
		fuzz_fail(__FILE__, __LINE__, #cond); \
	} \
} while (0)

/** Size of the buffers used by FUZZ_ROUNDTRIP. */
#define FUZZ_ENC_BUF_SIZE 0x10000

/** Check that the decoded @p result (decoded from @p size bytes) can be encoded, and that the
 *  encoded data decodes successfully and re-encodes to exactly the same bytes.
 *
 *  The decoded structs are not compared directly, since they contain padding, and pointers into
 *  the payload. The first encoding is not compared to the input, since the input doesn't need to
 *  be canonical.
 *
 *  Requires that both cbor_decode_<type>() and cbor_encode_<type>() are generated.
 *  Inputs larger than a quarter of the buffer size are skipped since the encoding can be larger
 *  than the input, e.g. because list headers are sized for the maximum number of elements.
 */
#define FUZZ_ROUNDTRIP(type, result, size) \
do { \
	static uint8_t enc1[FUZZ_ENC_BUF_SIZE]; \
	static uint8_t enc2[FUZZ_ENC_BUF_SIZE]; \
	static struct type result2; \
	size_t enc1_len = 0, enc2_len = 0, dec_len = 0; \
	int rt_err; \
\
	if ((size) > (FUZZ_ENC_BUF_SIZE / 4)) { \
		break; \
	} \
	/* Encode result into enc1. */ \
	rt_err = fuzz_check_err(cbor_encode_##type(enc1, sizeof(enc1), (result), &enc1_len)); \
	FUZZ_ASSERT(rt_err == ZCBOR_SUCCESS); \
	/* Decode enc1 into result2, which must use all of enc1. */ \
	rt_err = fuzz_check_err(cbor_decode_##type(enc1, enc1_len, &result2, &dec_len)); \
	FUZZ_ASSERT(rt_err == ZCBOR_SUCCESS); \
	FUZZ_ASSERT(dec_len == enc1_len); \
	/* Encode result2 into enc2, which must be identical to enc1. */ \
	rt_err = fuzz_check_err(cbor_encode_##type(enc2, sizeof(enc2), &result2, &enc2_len)); \
	FUZZ_ASSERT(rt_err == ZCBOR_SUCCESS); \
	FUZZ_ASSERT(enc2_len == enc1_len); \
	FUZZ_ASSERT(memcmp(enc1, enc2, enc1_len) == 0); \
} while (0)

#endif /* MAIN_ENTRY_H__ */
