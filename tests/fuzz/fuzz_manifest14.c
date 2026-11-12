/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* This test case uses code generated with --unordered-maps, so it exercises the
 * zcbor_unordered_map_*() API, with ZCBOR_MAP_SMART_SEARCH. */

#include "manifest14_decode.h"
#include "main_entry.h"

/* Keep the structs off the stack. */
static struct SUIT_Envelope envelope;
static struct SUIT_Envelope dependency;
static struct SUIT_Manifest manifest;
static struct SUIT_Command_Sequence command_seq;
static struct SUIT_Common_Sequence common_seq;


/* Decode a bstr-wrapped command sequence, if present. */
static void decode_command_sequence(bool present, const struct zcbor_string *seq)
{
	size_t payload_len_out;

	if (present) {
		(void)fuzz_check_err(cbor_decode_SUIT_Command_Sequence(seq->value, seq->len,
				&command_seq, &payload_len_out));
	}
}


static void decode_manifest(const struct zcbor_string *manifest_bstr)
{
	size_t payload_len_out;

	if (fuzz_check_err(cbor_decode_SUIT_Manifest(manifest_bstr->value, manifest_bstr->len,
			&manifest, &payload_len_out)) != ZCBOR_SUCCESS) {
		return;
	}

	struct SUIT_Common *common = &manifest.SUIT_Manifest_suit_common_cbor;

	if (common->SUIT_Common_suit_common_sequence_present) {
		const struct zcbor_string *seq = &common->SUIT_Common_suit_common_sequence;

		(void)fuzz_check_err(cbor_decode_SUIT_Common_Sequence(seq->value, seq->len,
				&common_seq, &payload_len_out));
		/* A common sequence can also be decoded as a command sequence. */
		decode_command_sequence(true, seq);
	}

	struct SUIT_Unseverable_Members *unsev = &manifest.SUIT_Manifest_SUIT_Unseverable_Members_m;

#define DECODE_UNSEVERABLE(name) decode_command_sequence( \
		unsev->SUIT_Unseverable_Members_##name##_present, \
		&unsev->SUIT_Unseverable_Members_##name)

	DECODE_UNSEVERABLE(suit_validate);
	DECODE_UNSEVERABLE(suit_load);
	DECODE_UNSEVERABLE(suit_run);
#undef DECODE_UNSEVERABLE

	struct SUIT_Severable_Members_Choice *sev =
		&manifest.SUIT_Manifest_SUIT_Severable_Members_Choice_m;

#define DECODE_SEVERABLE_CHOICE(name) decode_command_sequence( \
		sev->SUIT_Severable_Members_Choice_##name##_present \
		&& (sev->SUIT_Severable_Members_Choice_##name \
			.SUIT_Severable_Members_Choice_##name##_choice \
			== SUIT_Severable_Members_Choice_##name##_SUIT_Command_Sequence_bstr_c), \
		&sev->SUIT_Severable_Members_Choice_##name \
			.SUIT_Severable_Members_Choice_##name##_SUIT_Command_Sequence_bstr)

	DECODE_SEVERABLE_CHOICE(suit_dependency_resolution);
	DECODE_SEVERABLE_CHOICE(suit_payload_fetch);
	DECODE_SEVERABLE_CHOICE(suit_install);
#undef DECODE_SEVERABLE_CHOICE
}


bool fuzz_one_input(const uint8_t *data, size_t size)
{
	size_t payload_len_out = 0;

	/* Try both entry points. The seeds are tagged, and an input fails quickly with the entry
	 * point it doesn't match. */
	if ((fuzz_check_err(cbor_decode_SUIT_Envelope_Tagged(data, size, &envelope,
				&payload_len_out)) != ZCBOR_SUCCESS)
		&& (fuzz_check_err(cbor_decode_SUIT_Envelope(data, size, &envelope,
				&payload_len_out)) != ZCBOR_SUCCESS)) {
		return false;
	}

	struct SUIT_Severable_Manifest_Members *env_sev =
		&envelope.SUIT_Envelope_SUIT_Severable_Manifest_Members_m;

#define DECODE_ENV_SEVERABLE(name) decode_command_sequence( \
		env_sev->SUIT_Severable_Manifest_Members_##name##_present, \
		&env_sev->SUIT_Severable_Manifest_Members_##name)

	DECODE_ENV_SEVERABLE(suit_dependency_resolution);
	DECODE_ENV_SEVERABLE(suit_payload_fetch);
	DECODE_ENV_SEVERABLE(suit_install);
#undef DECODE_ENV_SEVERABLE

	decode_manifest(&envelope.SUIT_Envelope_suit_manifest);

	for (size_t i = 0; i < envelope.SUIT_Envelope_SUIT_Integrated_Dependency_m_count; i++) {
		const struct zcbor_string *dep = &envelope
			.SUIT_Envelope_SUIT_Integrated_Dependency_m[i]
			.SUIT_Envelope_SUIT_Integrated_Dependency_m
			.SUIT_Integrated_Dependency_suit_integrated_dependency_key;

		(void)fuzz_check_err(cbor_decode_SUIT_Envelope(dep->value, dep->len, &dependency,
				&payload_len_out));
	}

	return true;
}
