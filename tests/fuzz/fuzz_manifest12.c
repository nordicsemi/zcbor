/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "manifest12_decode.h"
#include "main_entry.h"

/* The structs are large because of --default-max-qty, so keep them off the stack. */
static struct SUIT_Envelope envelope;
static struct SUIT_Envelope dependency;
static struct SUIT_Command_Sequence command_seq;


/* Decode a bstr-wrapped command sequence, if present. */
static void decode_command_sequence(bool present, const struct zcbor_string *seq)
{
	size_t payload_len_out;

	if (present) {
		(void)cbor_decode_SUIT_Command_Sequence(seq->value, seq->len,
				&command_seq, &payload_len_out);
	}
}


static void decode_severable_members(const struct SUIT_Severable_Manifest_Members *m)
{
#define DECODE_SEVERABLE(name) decode_command_sequence( \
		m->SUIT_Severable_Manifest_Members_##name##_present, \
		&m->SUIT_Severable_Manifest_Members_##name)

	DECODE_SEVERABLE(suit_dependency_resolution);
	DECODE_SEVERABLE(suit_payload_fetch);
	DECODE_SEVERABLE(suit_install);
#undef DECODE_SEVERABLE
}


static void decode_unseverable_members(const struct SUIT_Unseverable_Members *m)
{
#define DECODE_UNSEVERABLE(name) decode_command_sequence( \
		m->SUIT_Unseverable_Members_##name##_present, \
		&m->SUIT_Unseverable_Members_##name)

	DECODE_UNSEVERABLE(suit_validate);
	DECODE_UNSEVERABLE(suit_load);
	DECODE_UNSEVERABLE(suit_run);
#undef DECODE_UNSEVERABLE
}


bool fuzz_one_input(const uint8_t *data, size_t size)
{
	size_t payload_len_out = 0;

	/* Try both entry points. The seeds are untagged, and an input fails quickly with the
	 * entry point it doesn't match. */
	if ((cbor_decode_SUIT_Envelope_Tagged(data, size, &envelope,
				&payload_len_out) != ZCBOR_SUCCESS)
		&& (cbor_decode_SUIT_Envelope(data, size, &envelope,
				&payload_len_out) != ZCBOR_SUCCESS)) {
		return false;
	}

	struct SUIT_Manifest *manifest = &envelope.SUIT_Envelope_suit_manifest_cbor;

	decode_severable_members(&envelope.SUIT_Envelope_SUIT_Severable_Manifest_Members_m);
	decode_severable_members(&manifest->SUIT_Manifest_SUIT_Severable_Manifest_Members_m);
	decode_unseverable_members(&manifest->SUIT_Manifest_SUIT_Unseverable_Members_m);

	for (size_t i = 0; i < envelope.SUIT_Envelope_suit_integrated_dependency_key_count; i++) {
		const struct zcbor_string *dep = &envelope
			.SUIT_Envelope_suit_integrated_dependency_key[i]
			.SUIT_Envelope_suit_integrated_dependency_key;

		(void)cbor_decode_SUIT_Envelope(dep->value, dep->len, &dependency,
				&payload_len_out);
	}

	return true;
}
