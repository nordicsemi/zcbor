/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Fuzz the fragmented string API (ZCBOR_FRAGMENTS).
 *
 * The input is a config byte followed by a CBOR payload. The payload is first decoded in one piece
 * (the reference), then again after being split into sections, each copied into its own heap
 * buffer, as if the payload was received in chunks. The strings decoded from the sections must
 * match the strings decoded from the whole payload, and the decoding must otherwise behave the
 * same. Since the sections are separate allocations, ASan catches any reads across section
 * boundaries.
 *
 * Only strings can be split across sections, so the payload is only split inside strings (and
 * optionally right after them, i.e. between elements).
 *
 * Config byte:
 *  bits 0-4: The size of the sections inside strings, minus 1 (1-32 bytes).
 *  bit 5:    Also split right after string headers.
 *  bit 6:    Also split right after strings.
 *  bit 7:    The payload is a single bstr containing a CBOR sequence. Decode the bstr with
 *            zcbor_cbor_bstr_fragments_start_decode(), and split the strings inside it.
 *            The fragments of the bstr (one per section) are also checked against the bstr's
 *            contents.
 *            If not set, the payload is itself a CBOR sequence.
 *
 * The elements that are not strings are skipped with zcbor_any_skip().
 */

#include <stdlib.h>
#include <zcbor_decode.h>
#include "main_entry.h"

#define MAX_PAYLOAD 1024
#define MAX_ELEMS 64
#define MAX_SECTIONS (MAX_PAYLOAD + 1)

#define CFG_SECTION_SIZE_MASK 0x1F
#define CFG_SPLIT_AFTER_HEADER 0x20
#define CFG_SPLIT_AFTER_STRING 0x40
#define CFG_CBOR_BSTR 0x80

struct elem {
	size_t start;         ///! Offset of the element's header.
	size_t content_start; ///! Offset of the string contents (strings only).
	size_t end;           ///! Offset right after the element.
	bool is_str;
};

static struct elem elems[MAX_ELEMS];
static size_t num_elems;

static size_t sec_start[MAX_SECTIONS + 1]; ///! Section i is [sec_start[i], sec_start[i + 1]).
static uint8_t *sec_buf[MAX_SECTIONS];
static size_t num_sections;
static size_t cur_section;

static struct zcbor_string_fragment frags[MAX_SECTIONS + 1];
static uint8_t spliced[MAX_PAYLOAD];

/* The CBOR-encoded bstr (CFG_CBOR_BSTR). */
static bool in_cbor_bstr;
static size_t cbor_bstr_start; ///! Offset of the bstr's contents.
static size_t cbor_bstr_len;
static struct zcbor_string_fragment cbor_frags[MAX_SECTIONS];
static size_t num_cbor_frags;


static bool is_bstr(const zcbor_state_t *state)
{
	return ZCBOR_MAJOR_TYPE(*state->payload) == ZCBOR_MAJOR_TYPE_BSTR;
}


static bool is_tstr(const zcbor_state_t *state)
{
	return ZCBOR_MAJOR_TYPE(*state->payload) == ZCBOR_MAJOR_TYPE_TSTR;
}


/* Decode a CBOR sequence in one piece, and record where the elements are. Stops at the first
 * element that can't be decoded. */
static void walk_reference(zcbor_state_t *state, const uint8_t *base)
{
	while ((num_elems < MAX_ELEMS) && !zcbor_payload_at_end(state)) {
		struct elem *e = &elems[num_elems];

		e->start = (size_t)(state->payload - base);
		e->is_str = is_bstr(state) || is_tstr(state);

		if (e->is_str) {
			struct zcbor_string str;

			if (!(is_bstr(state) ? zcbor_bstr_decode(state, &str)
					: zcbor_tstr_decode(state, &str))) {
				break;
			}
			e->content_start = (size_t)(str.value - base);
		} else if (!zcbor_any_skip(state, NULL)) {
			break;
		}

		e->end = (size_t)(state->payload - base);
		num_elems++;
	}
}


static void add_split(size_t offset, size_t payload_len)
{
	/* Keep all sections non-empty. Splits are added in order, so only the last one needs to be
	 * checked for duplicates. */
	if ((offset > sec_start[num_sections]) && (offset < payload_len)) {
		FUZZ_ASSERT(num_sections < MAX_SECTIONS);
		sec_start[++num_sections] = offset;
	}
}


static void make_sections(const uint8_t *payload, size_t payload_len, uint8_t cfg,
		size_t outer_content_start)
{
	size_t section_size = (cfg & CFG_SECTION_SIZE_MASK) + 1;

	num_sections = 0;
	sec_start[0] = 0;

	if (cfg & CFG_SPLIT_AFTER_HEADER) {
		add_split(outer_content_start, payload_len);
	}

	for (size_t i = 0; i < num_elems; i++) {
		struct elem *e = &elems[i];

		if (!e->is_str) {
			continue;
		}
		if (cfg & CFG_SPLIT_AFTER_HEADER) {
			add_split(e->content_start, payload_len);
		}
		for (size_t off = e->content_start + section_size; off < e->end; off += section_size) {
			add_split(off, payload_len);
		}
		if (cfg & CFG_SPLIT_AFTER_STRING) {
			add_split(e->end, payload_len);
		}
	}

	num_sections++;
	sec_start[num_sections] = payload_len;

	for (size_t i = 0; i < num_sections; i++) {
		size_t len = sec_start[i + 1] - sec_start[i];

		sec_buf[i] = malloc(len);
		FUZZ_ASSERT(sec_buf[i] != NULL);
		memcpy(sec_buf[i], &payload[sec_start[i]], len);
	}
	cur_section = 0;
}


static void free_sections(void)
{
	for (size_t i = 0; i < num_sections; i++) {
		free(sec_buf[i]);
	}
}


/* The offset into the whole payload that the state is pointing to. */
static size_t current_offset(const zcbor_state_t *state)
{
	return sec_start[cur_section] + (size_t)(state->payload - sec_buf[cur_section]);
}


/* Get the fragment of the CBOR-encoded bstr in the current section, which is used up (or at the
 * end of the bstr), and check it.
 *
 * Inside a (non-CBOR-encoded) fragmented string, zcbor_str_fragment_decode() gives fragments of
 * that string instead, so there is no way to get the bstr fragment. Then the fragment is made
 * from the section, so the other fragments can still be validated and spliced. */
static void cbor_bstr_fragment(zcbor_state_t *state, bool in_str)
{
	struct zcbor_string_fragment *frag = &cbor_frags[num_cbor_frags];
	size_t start = MAX(sec_start[cur_section], cbor_bstr_start);
	size_t end = current_offset(state);
	const uint8_t *value = &sec_buf[cur_section][start - sec_start[cur_section]];

	FUZZ_ASSERT(num_cbor_frags < ZCBOR_ARRAY_SIZE(cbor_frags));
	FUZZ_ASSERT(zcbor_payload_at_end(state));

	if (in_str) {
		frag->fragment.value = value;
		frag->fragment.len = end - start;
		frag->offset = start - cbor_bstr_start;
		frag->total_len = cbor_bstr_len;
	} else {
		FUZZ_ASSERT(zcbor_str_fragment_decode(state, frag));
		FUZZ_ASSERT(frag->fragment.value == value);
		FUZZ_ASSERT(frag->fragment.len == end - start);
		FUZZ_ASSERT(frag->offset == start - cbor_bstr_start);
		FUZZ_ASSERT(frag->total_len == cbor_bstr_len);
		FUZZ_ASSERT(zcbor_payload_at_end(state));
	}
	FUZZ_ASSERT(zcbor_is_last_fragment(frag) == (cur_section == num_sections - 1));
	num_cbor_frags++;
}


/* Check the fragments of the CBOR-encoded bstr against its contents. */
static void check_cbor_bstr_fragments(const uint8_t *payload)
{
	size_t len = sizeof(spliced);

	FUZZ_ASSERT(num_cbor_frags == num_sections);
	FUZZ_ASSERT(zcbor_validate_string_fragments(cbor_frags, num_cbor_frags));
	FUZZ_ASSERT(zcbor_splice_string_fragments(cbor_frags, num_cbor_frags, spliced, &len));
	FUZZ_ASSERT(len == cbor_bstr_len);
	FUZZ_ASSERT(memcmp(spliced, &payload[cbor_bstr_start], len) == 0);
}


/* Move to the next section. @p in_str is whether this is inside a fragmented string. */
static void next_section(zcbor_state_t *state, bool in_str)
{
	FUZZ_ASSERT(zcbor_payload_at_end(state));
	FUZZ_ASSERT(cur_section + 1 < num_sections);
	if (in_cbor_bstr) {
		cbor_bstr_fragment(state, in_str);
	}
	cur_section++;
	zcbor_update_state(state, sec_buf[cur_section], sec_start[cur_section + 1]
			- sec_start[cur_section]);
}


/* Decode a string that may be split across sections, and check it against the reference. */
static void decode_fragmented_string(zcbor_state_t *state, const struct elem *e,
		const uint8_t *payload)
{
	size_t num_frags = 0;
	size_t len = 0;
	size_t remainder;

	FUZZ_ASSERT(is_bstr(state) ? zcbor_bstr_fragments_start_decode(state)
		: zcbor_tstr_fragments_start_decode(state));
	FUZZ_ASSERT(zcbor_current_string_remainder(state, &remainder));
	FUZZ_ASSERT(remainder == (e->end - e->content_start));

	while (true) {
		/* If the section is used up (e.g. if the header was at the end of the section), the
		 * fragment is empty. An empty string gets a single empty fragment. */
		FUZZ_ASSERT(num_frags < ZCBOR_ARRAY_SIZE(frags));
		FUZZ_ASSERT(zcbor_str_fragment_decode(state, &frags[num_frags]));
		num_frags++;
		FUZZ_ASSERT(zcbor_current_string_remainder(state, &remainder));
		FUZZ_ASSERT(zcbor_is_last_fragment(&frags[num_frags - 1]) == (remainder == 0));
		if (remainder == 0) {
			break;
		}
		/* The fragment used up the section. */
		next_section(state, true);
	}

	FUZZ_ASSERT(zcbor_str_fragments_end_decode(state));
	len = sizeof(spliced);
	FUZZ_ASSERT(zcbor_validate_string_fragments(frags, num_frags));
	FUZZ_ASSERT(zcbor_splice_string_fragments(frags, num_frags, spliced, &len));
	FUZZ_ASSERT(len == (e->end - e->content_start));
	FUZZ_ASSERT(memcmp(spliced, &payload[e->content_start], len) == 0);
}


/* Decode the same elements as walk_reference(), but from the sections. */
static void walk_sections(zcbor_state_t *state, const uint8_t *payload)
{
	for (size_t i = 0; i < num_elems; i++) {
		const struct elem *e = &elems[i];

		/* Only strings span sections, so other elements always start a new section if the
		 * current one is used up. */
		if (zcbor_payload_at_end(state)) {
			next_section(state, false);
		}
		FUZZ_ASSERT(current_offset(state) == e->start);

		if (e->is_str) {
			decode_fragmented_string(state, e, payload);
		} else {
			FUZZ_ASSERT(zcbor_any_skip(state, NULL));
		}
		FUZZ_ASSERT(current_offset(state) == e->end);
	}
}


bool fuzz_one_input(const uint8_t *data, size_t size)
{
	if ((size < 2) || (size > (MAX_PAYLOAD + 1))) {
		return false;
	}

	uint8_t cfg = data[0];
	const uint8_t *payload = &data[1];
	size_t payload_len = size - 1;
	size_t outer_content_start = 0;

	num_elems = 0;
	in_cbor_bstr = false;
	num_cbor_frags = 0;

	/* Reference decoding. */
	ZCBOR_STATE_D(ref_state, 1, payload, payload_len, ZCBOR_LARGE_ELEM_COUNT, 0);

	if (cfg & CFG_CBOR_BSTR) {
		struct zcbor_string outer;

		if (!zcbor_bstr_start_decode(ref_state, &outer)) {
			return false;
		}
		outer_content_start = (size_t)(outer.value - payload);
		walk_reference(ref_state, payload);

		/* Require the whole bstr to be decoded, so it can be ended. */
		if (!zcbor_payload_at_end(ref_state) || !zcbor_bstr_end_decode(ref_state)) {
			return false;
		}
		payload_len = outer_content_start + outer.len;
		cbor_bstr_start = outer_content_start;
		cbor_bstr_len = outer.len;
	} else {
		walk_reference(ref_state, payload);
		if (num_elems == 0) {
			return false;
		}
		payload_len = elems[num_elems - 1].end;
	}

	/* Fragmented decoding. */
	make_sections(payload, payload_len, cfg, outer_content_start);

	ZCBOR_STATE_D(state, 1, sec_buf[0], sec_start[1], ZCBOR_LARGE_ELEM_COUNT, 0);

	if (cfg & CFG_CBOR_BSTR) {
		FUZZ_ASSERT(zcbor_cbor_bstr_fragments_start_decode(state));
		in_cbor_bstr = true;
		walk_sections(state, payload);
		/* The fragment in the last section. */
		cbor_bstr_fragment(state, false);
		FUZZ_ASSERT(zcbor_str_fragments_end_decode(state));
		in_cbor_bstr = false;
		check_cbor_bstr_fragments(payload);
	} else {
		walk_sections(state, payload);
	}

	FUZZ_ASSERT(current_offset(state) == payload_len);
	FUZZ_ASSERT(cur_section == num_sections - 1);
	FUZZ_ASSERT(fuzz_check_err(zcbor_pop_error(state)) == ZCBOR_SUCCESS);

	free_sections();
	return true;
}
