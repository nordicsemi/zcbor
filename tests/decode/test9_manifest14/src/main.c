/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#ifdef MANIFEST16
	#include "manifest16_decode.h"
#else
	#include "manifest14_decode.h"
#endif
#include <example0.h>


ZTEST(cbor_decode_test9, test_suit14_ex0_auth)
{
	struct SUIT_Envelope envelope;
	struct SUIT_Digest *digest;
	struct COSE_Sign1 *cose_sign1;
	size_t out_len;
	uint8_t exp_digest[] = {
		0xa6, 0xc4, 0x59, 0x0a, 0xc5, 0x30, 0x43, 0xa9,
		0x8e, 0x8c, 0x41, 0x06, 0xe1, 0xe3, 0x1b, 0x30,
		0x55, 0x16, 0xd7, 0xcf, 0x0a, 0x65, 0x5e, 0xdd,
		0xfa, 0xc6, 0xd4, 0x5c, 0x81, 0x0e, 0x03, 0x6a,
	};
	uint8_t exp_signature[] = {
		0xd1, 0x1a, 0x2d, 0xd9, 0x61, 0x0f, 0xb6, 0x2a,
		0x70, 0x73, 0x35, 0xf5, 0x84, 0x07, 0x92, 0x25,
		0x70, 0x9f, 0x96, 0xe8, 0x11, 0x7e, 0x7e, 0xee,
		0xd9, 0x8a, 0x2f, 0x20, 0x7d, 0x05, 0xc8, 0xec,
		0xfb, 0xa1, 0x75, 0x52, 0x08, 0xf6, 0xab, 0xea,
		0x97, 0x7b, 0x8a, 0x6e, 0xfe, 0x3b, 0xc2, 0xca,
		0x32, 0x15, 0xe1, 0x19, 0x3b, 0xe2, 0x01, 0x46,
		0x7d, 0x05, 0x2b, 0x42, 0xdb, 0x6b, 0x72, 0x87,
	};

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Envelope_Tagged(example0, sizeof(example0), &envelope, &out_len), NULL);
	zassert_equal(sizeof(example0), out_len, NULL);

	digest = &envelope.SUIT_Envelope_suit_authentication_wrapper_cbor.SUIT_Authentication_SUIT_Digest_bstr_cbor;
	zassert_equal(suit_cose_hash_algs_cose_alg_sha_256_m_c,
		digest->SUIT_Digest_suit_digest_algorithm_id.suit_cose_hash_algs_choice, NULL);
	zassert_equal(sizeof(exp_digest),
		digest->SUIT_Digest_suit_digest_bytes.len, NULL);
	zassert_mem_equal(exp_digest,
		digest->SUIT_Digest_suit_digest_bytes.value,
		digest->SUIT_Digest_suit_digest_bytes.len, NULL);
	zassert_equal(1, envelope.SUIT_Envelope_suit_authentication_wrapper_cbor.SUIT_Authentication_Block_bstr_count, NULL);
	zassert_equal(SUIT_Authentication_Block_COSE_Sign1_Tagged_m_c,
		envelope.SUIT_Envelope_suit_authentication_wrapper_cbor.SUIT_Authentication_Block_bstr[0].SUIT_Authentication_Block_bstr_cbor.SUIT_Authentication_Block_choice, NULL);
	cose_sign1 = &envelope.SUIT_Envelope_suit_authentication_wrapper_cbor.SUIT_Authentication_Block_bstr[0].SUIT_Authentication_Block_bstr_cbor.SUIT_Authentication_Block_COSE_Sign1_Tagged_m;
	zassert_equal(empty_or_serialized_map_header_map_bstr_c,
		cose_sign1->COSE_Sign1_Headers_m.Headers_protected.empty_or_serialized_map_choice, NULL);
	zassert_equal(0, cose_sign1->COSE_Sign1_Headers_m.Headers_protected.empty_or_serialized_map_header_map_bstr_cbor.header_map_label_count, NULL);
	zassert_true(cose_sign1->COSE_Sign1_Headers_m.Headers_protected.empty_or_serialized_map_header_map_bstr_cbor.header_map_Generic_Headers_m.Generic_Headers_uint1union_present, NULL);
	zassert_false(cose_sign1->COSE_Sign1_Headers_m.Headers_protected.empty_or_serialized_map_header_map_bstr_cbor.header_map_Generic_Headers_m.Generic_Headers_label_m_l_present, NULL);
	zassert_false(cose_sign1->COSE_Sign1_Headers_m.Headers_protected.empty_or_serialized_map_header_map_bstr_cbor.header_map_Generic_Headers_m.Generic_Headers_uint3union_present, NULL);
	zassert_false(cose_sign1->COSE_Sign1_Headers_m.Headers_protected.empty_or_serialized_map_header_map_bstr_cbor.header_map_Generic_Headers_m.Generic_Headers_uint4bstr_present, NULL);
	zassert_false(cose_sign1->COSE_Sign1_Headers_m.Headers_protected.empty_or_serialized_map_header_map_bstr_cbor.header_map_Generic_Headers_m.Generic_Headers_uint5bstr_present, NULL);
	zassert_false(cose_sign1->COSE_Sign1_Headers_m.Headers_protected.empty_or_serialized_map_header_map_bstr_cbor.header_map_Generic_Headers_m.Generic_Headers_uint6bstr_present, NULL);
	zassert_equal(Generic_Headers_uint1union_int_c,
		cose_sign1->COSE_Sign1_Headers_m.Headers_protected.empty_or_serialized_map_header_map_bstr_cbor.header_map_Generic_Headers_m.Generic_Headers_uint1union.Generic_Headers_uint1union_choice, NULL);
	zassert_equal(-7,
		cose_sign1->COSE_Sign1_Headers_m.Headers_protected.empty_or_serialized_map_header_map_bstr_cbor.header_map_Generic_Headers_m.Generic_Headers_uint1union.Generic_Headers_uint1union_int, NULL);
	zassert_equal(sizeof(exp_signature), cose_sign1->COSE_Sign1_signature.len, NULL);
	zassert_mem_equal(exp_signature, cose_sign1->COSE_Sign1_signature.value,
		cose_sign1->COSE_Sign1_signature.len, NULL);
	zassert_equal(COSE_Sign1_payload_nil_c, cose_sign1->COSE_Sign1_payload_choice, NULL);
}


/* Check the common sequence. */
ZTEST(cbor_decode_test9, test_suit14_ex0_common_sequence)
{
	struct SUIT_Envelope envelope;
	struct SUIT_Manifest manifest;
	struct SUIT_Common_Sequence common_sequence;
	struct SUIT_Command_Sequence command_sequence;
	struct SUIT_Parameters *parameter;
	struct SUIT_Condition *condition;
	size_t out_len;
	uint8_t exp_vendor_id[] = {
		0xfa, 0x6b, 0x4a, 0x53, 0xd5, 0xad, 0x5f, 0xdf,
		0xbe, 0x9d, 0xe6, 0x63, 0xe4, 0xd4, 0x1f, 0xfe,
	};
	uint8_t exp_class_id[] = {
		0x14, 0x92, 0xaf, 0x14, 0x25, 0x69, 0x5e, 0x48,
		0xbf, 0x42, 0x9b, 0x2d, 0x51, 0xf2, 0xab, 0x45,
	};
	uint8_t exp_digest[] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
		0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
	};
	uint32_t exp_rep_policy = (
		(1 << suit_reporting_bits_suit_send_record_success_c)
		| (1 << suit_reporting_bits_suit_send_record_failure_c)
		| (1 << suit_reporting_bits_suit_send_sysinfo_success_c)
		| (1 << suit_reporting_bits_suit_send_sysinfo_failure_c));

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Envelope_Tagged(example0, sizeof(example0), &envelope, &out_len), NULL);
	zassert_equal(sizeof(example0), out_len, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Manifest(envelope.SUIT_Envelope_suit_manifest.value,
		envelope.SUIT_Envelope_suit_manifest.len, &manifest, &out_len), NULL);
	zassert_equal(envelope.SUIT_Envelope_suit_manifest.len, out_len, NULL);
	zassert_true(manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_components_present, NULL);
	zassert_true(manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence_present, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Common_Sequence(
		manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence.SUIT_Common_suit_common_sequence.value,
		manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence.SUIT_Common_suit_common_sequence.len,
		&common_sequence, &out_len), NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Command_Sequence(
		manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence.SUIT_Common_suit_common_sequence.value,
		manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence.SUIT_Common_suit_common_sequence.len,
		&command_sequence, &out_len), NULL);
	zassert_equal(
		manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence.SUIT_Common_suit_common_sequence.len,
		out_len, NULL);
	zassert_equal(3, common_sequence.SUIT_Common_Sequence_union_count, NULL);
	zassert_equal(SUIT_Common_Sequence_union_SUIT_Common_Commands_m_c, common_sequence.SUIT_Common_Sequence_union[0].SUIT_Common_Sequence_union_choice, NULL);
	zassert_equal(SUIT_Common_Commands_suit_directive_override_parameters_m_l_c, common_sequence.SUIT_Common_Sequence_union[0].SUIT_Common_Sequence_union_SUIT_Common_Commands_m.SUIT_Common_Commands_choice, NULL);
	zassert_equal(4, common_sequence.SUIT_Common_Sequence_union[0].SUIT_Common_Sequence_union_SUIT_Common_Commands_m.suit_directive_override_parameters_m_l_map_SUIT_Parameters_m_count, NULL);
	parameter = &common_sequence.SUIT_Common_Sequence_union[0].SUIT_Common_Sequence_union_SUIT_Common_Commands_m.suit_directive_override_parameters_m_l_map_SUIT_Parameters_m[0].suit_directive_override_parameters_m_l_map_SUIT_Parameters_m;
	zassert_equal(
		SUIT_Parameters_suit_parameter_vendor_identifier_c,
		parameter->SUIT_Parameters_choice, NULL);
	zassert_equal(
		SUIT_Parameters_suit_parameter_vendor_identifier_RFC4122_UUID_m_c,
		parameter->SUIT_Parameters_suit_parameter_vendor_identifier_choice, NULL);
	zassert_equal(
		parameter->SUIT_Parameters_suit_parameter_vendor_identifier_RFC4122_UUID_m.len,
		sizeof(exp_vendor_id), NULL);
	zassert_mem_equal(exp_vendor_id,
		parameter->SUIT_Parameters_suit_parameter_vendor_identifier_RFC4122_UUID_m.value,
		parameter->SUIT_Parameters_suit_parameter_vendor_identifier_RFC4122_UUID_m.len, NULL);

	parameter = &common_sequence.SUIT_Common_Sequence_union[0].SUIT_Common_Sequence_union_SUIT_Common_Commands_m.suit_directive_override_parameters_m_l_map_SUIT_Parameters_m[1].suit_directive_override_parameters_m_l_map_SUIT_Parameters_m;
	zassert_equal(
		SUIT_Parameters_suit_parameter_class_identifier_c,
		parameter->SUIT_Parameters_choice, NULL);
	zassert_equal(
		parameter->SUIT_Parameters_suit_parameter_class_identifier.len,
		sizeof(exp_class_id), NULL);
	zassert_mem_equal(exp_class_id,
		parameter->SUIT_Parameters_suit_parameter_class_identifier.value,
		parameter->SUIT_Parameters_suit_parameter_class_identifier.len, NULL);

	parameter = &common_sequence.SUIT_Common_Sequence_union[0].SUIT_Common_Sequence_union_SUIT_Common_Commands_m.suit_directive_override_parameters_m_l_map_SUIT_Parameters_m[2].suit_directive_override_parameters_m_l_map_SUIT_Parameters_m;
	zassert_equal(
		SUIT_Parameters_suit_parameter_image_digest_c,
		parameter->SUIT_Parameters_choice, NULL);
	zassert_equal(
		parameter->SUIT_Parameters_suit_parameter_image_digest_cbor.SUIT_Digest_suit_digest_bytes.len,
		sizeof(exp_digest), NULL);
	zassert_equal(suit_cose_hash_algs_cose_alg_sha_256_m_c,
		parameter->SUIT_Parameters_suit_parameter_image_digest_cbor.SUIT_Digest_suit_digest_algorithm_id.suit_cose_hash_algs_choice, NULL);
	zassert_mem_equal(exp_digest,
		parameter->SUIT_Parameters_suit_parameter_image_digest_cbor.SUIT_Digest_suit_digest_bytes.value,
		parameter->SUIT_Parameters_suit_parameter_image_digest_cbor.SUIT_Digest_suit_digest_bytes.len, NULL);

	parameter = &common_sequence.SUIT_Common_Sequence_union[0].SUIT_Common_Sequence_union_SUIT_Common_Commands_m.suit_directive_override_parameters_m_l_map_SUIT_Parameters_m[3].suit_directive_override_parameters_m_l_map_SUIT_Parameters_m;
	zassert_equal(
		SUIT_Parameters_suit_parameter_image_size_c,
		parameter->SUIT_Parameters_choice, NULL);
	zassert_equal(34768, parameter->SUIT_Parameters_suit_parameter_image_size, NULL);

	zassert_equal(SUIT_Common_Sequence_union_SUIT_Condition_m_c, common_sequence.SUIT_Common_Sequence_union[1].SUIT_Common_Sequence_union_choice, NULL);
	condition = &common_sequence.SUIT_Common_Sequence_union[1].SUIT_Common_Sequence_union_SUIT_Condition_m;
	zassert_equal(SUIT_Condition_suit_condition_vendor_identifier_m_l_c,
		condition->SUIT_Condition_choice, NULL);
	zassert_equal(exp_rep_policy,
		condition->SUIT_Condition_suit_condition_vendor_identifier_m_l_SUIT_Rep_Policy_m, NULL);
	zassert_equal(exp_rep_policy,
		condition->SUIT_Condition_suit_condition_class_identifier_m_l_SUIT_Rep_Policy_m, NULL);

	zassert_equal(SUIT_Common_Sequence_union_SUIT_Condition_m_c, common_sequence.SUIT_Common_Sequence_union[2].SUIT_Common_Sequence_union_choice, NULL);
	condition = &common_sequence.SUIT_Common_Sequence_union[2].SUIT_Common_Sequence_union_SUIT_Condition_m;
	zassert_equal(SUIT_Condition_suit_condition_class_identifier_m_l_c,
		condition->SUIT_Condition_choice, NULL);
	zassert_equal(exp_rep_policy,
		condition->SUIT_Condition_suit_condition_class_identifier_m_l_SUIT_Rep_Policy_m, NULL);
}

/* Check the common sequence when decoded as a command sequence.
 * This should work fine because SUIT_Common_Sequence is a subset of
 * SUIT_Command_Sequence.
 */
ZTEST(cbor_decode_test9, test_suit14_ex0_common_sequence_as_command_sequence)
{
	struct SUIT_Envelope envelope;
	struct SUIT_Manifest manifest;
	struct SUIT_Common_Sequence common_sequence;
	struct SUIT_Command_Sequence command_sequence;
	struct SUIT_Parameters *parameter;
	struct SUIT_Condition *condition;
	size_t out_len;
	uint8_t exp_vendor_id[] = {
		0xfa, 0x6b, 0x4a, 0x53, 0xd5, 0xad, 0x5f, 0xdf,
		0xbe, 0x9d, 0xe6, 0x63, 0xe4, 0xd4, 0x1f, 0xfe,
	};
	uint8_t exp_class_id[] = {
		0x14, 0x92, 0xaf, 0x14, 0x25, 0x69, 0x5e, 0x48,
		0xbf, 0x42, 0x9b, 0x2d, 0x51, 0xf2, 0xab, 0x45,
	};
	uint8_t exp_digest[] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
		0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
	};
	uint32_t exp_rep_policy = (
		(1 << suit_reporting_bits_suit_send_record_success_c)
		| (1 << suit_reporting_bits_suit_send_record_failure_c)
		| (1 << suit_reporting_bits_suit_send_sysinfo_success_c)
		| (1 << suit_reporting_bits_suit_send_sysinfo_failure_c));

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Envelope_Tagged(example0, sizeof(example0), &envelope, &out_len), NULL);
	zassert_equal(sizeof(example0), out_len, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Manifest(envelope.SUIT_Envelope_suit_manifest.value,
		envelope.SUIT_Envelope_suit_manifest.len, &manifest, &out_len), NULL);
	zassert_equal(envelope.SUIT_Envelope_suit_manifest.len, out_len, NULL);
	zassert_true(manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_components_present, NULL);
	zassert_true(manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence_present, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Common_Sequence(
		manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence.SUIT_Common_suit_common_sequence.value,
		manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence.SUIT_Common_suit_common_sequence.len,
		&common_sequence, &out_len), NULL);
	zassert_equal(
		manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence.SUIT_Common_suit_common_sequence.len,
		out_len, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Command_Sequence(
		manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence.SUIT_Common_suit_common_sequence.value,
		manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence.SUIT_Common_suit_common_sequence.len,
		&command_sequence, &out_len), NULL);
	zassert_equal(
		manifest.SUIT_Manifest_suit_common_cbor.SUIT_Common_suit_common_sequence.SUIT_Common_suit_common_sequence.len,
		out_len, NULL);
	zassert_equal(3, command_sequence.SUIT_Command_Sequence_union_count, NULL);
	zassert_equal(SUIT_Command_Sequence_union_SUIT_Directive_m_c, command_sequence.SUIT_Command_Sequence_union[0].SUIT_Command_Sequence_union_choice, NULL);
	zassert_equal(SUIT_Directive_suit_directive_override_parameters_m_l_c, command_sequence.SUIT_Command_Sequence_union[0].SUIT_Command_Sequence_union_SUIT_Directive_m.SUIT_Directive_choice, NULL);
	zassert_equal(4, command_sequence.SUIT_Command_Sequence_union[0].SUIT_Command_Sequence_union_SUIT_Directive_m.suit_directive_override_parameters_m_l_map_SUIT_Parameters_m_count, NULL);

	parameter = &command_sequence.SUIT_Command_Sequence_union[0].SUIT_Command_Sequence_union_SUIT_Directive_m.suit_directive_override_parameters_m_l_map_SUIT_Parameters_m[0].suit_directive_override_parameters_m_l_map_SUIT_Parameters_m;
	zassert_equal(
		SUIT_Parameters_suit_parameter_vendor_identifier_c,
		parameter->SUIT_Parameters_choice, NULL);
	zassert_equal(
		SUIT_Parameters_suit_parameter_vendor_identifier_RFC4122_UUID_m_c,
		parameter->SUIT_Parameters_suit_parameter_vendor_identifier_choice, NULL);
	zassert_equal(
		parameter->SUIT_Parameters_suit_parameter_vendor_identifier_RFC4122_UUID_m.len,
		sizeof(exp_vendor_id), NULL);
	zassert_mem_equal(exp_vendor_id,
		parameter->SUIT_Parameters_suit_parameter_vendor_identifier_RFC4122_UUID_m.value,
		parameter->SUIT_Parameters_suit_parameter_vendor_identifier_RFC4122_UUID_m.len, NULL);

	parameter = &command_sequence.SUIT_Command_Sequence_union[0].SUIT_Command_Sequence_union_SUIT_Directive_m.suit_directive_override_parameters_m_l_map_SUIT_Parameters_m[1].suit_directive_override_parameters_m_l_map_SUIT_Parameters_m;
	zassert_equal(
		SUIT_Parameters_suit_parameter_class_identifier_c,
		parameter->SUIT_Parameters_choice, NULL);
	zassert_equal(
		parameter->SUIT_Parameters_suit_parameter_class_identifier.len,
		sizeof(exp_class_id), NULL);
	zassert_mem_equal(exp_class_id,
		parameter->SUIT_Parameters_suit_parameter_class_identifier.value,
		parameter->SUIT_Parameters_suit_parameter_class_identifier.len, NULL);

	parameter = &command_sequence.SUIT_Command_Sequence_union[0].SUIT_Command_Sequence_union_SUIT_Directive_m.suit_directive_override_parameters_m_l_map_SUIT_Parameters_m[2].suit_directive_override_parameters_m_l_map_SUIT_Parameters_m;
	zassert_equal(
		SUIT_Parameters_suit_parameter_image_digest_c,
		parameter->SUIT_Parameters_choice, NULL);
	zassert_equal(
		parameter->SUIT_Parameters_suit_parameter_image_digest_cbor.SUIT_Digest_suit_digest_bytes.len,
		sizeof(exp_digest), NULL);
	zassert_equal(suit_cose_hash_algs_cose_alg_sha_256_m_c,
		parameter->SUIT_Parameters_suit_parameter_image_digest_cbor.SUIT_Digest_suit_digest_algorithm_id.suit_cose_hash_algs_choice, NULL);
	zassert_mem_equal(exp_digest,
		parameter->SUIT_Parameters_suit_parameter_image_digest_cbor.SUIT_Digest_suit_digest_bytes.value,
		parameter->SUIT_Parameters_suit_parameter_image_digest_cbor.SUIT_Digest_suit_digest_bytes.len, NULL);

	parameter = &command_sequence.SUIT_Command_Sequence_union[0].SUIT_Command_Sequence_union_SUIT_Directive_m.suit_directive_override_parameters_m_l_map_SUIT_Parameters_m[3].suit_directive_override_parameters_m_l_map_SUIT_Parameters_m;
	zassert_equal(
		SUIT_Parameters_suit_parameter_image_size_c,
		parameter->SUIT_Parameters_choice, NULL);
	zassert_equal(34768, parameter->SUIT_Parameters_suit_parameter_image_size, NULL);

	zassert_equal(SUIT_Command_Sequence_union_SUIT_Condition_m_c, command_sequence.SUIT_Command_Sequence_union[1].SUIT_Command_Sequence_union_choice, NULL);
	condition = &command_sequence.SUIT_Command_Sequence_union[1].SUIT_Command_Sequence_union_SUIT_Condition_m;
	zassert_equal(SUIT_Condition_suit_condition_vendor_identifier_m_l_c,
		condition->SUIT_Condition_choice, NULL);
	zassert_equal(exp_rep_policy,
		condition->SUIT_Condition_suit_condition_vendor_identifier_m_l_SUIT_Rep_Policy_m, NULL);
	zassert_equal(exp_rep_policy,
		condition->SUIT_Condition_suit_condition_class_identifier_m_l_SUIT_Rep_Policy_m, NULL);

	zassert_equal(SUIT_Command_Sequence_union_SUIT_Condition_m_c, command_sequence.SUIT_Command_Sequence_union[2].SUIT_Command_Sequence_union_choice, NULL);
	condition = &command_sequence.SUIT_Command_Sequence_union[2].SUIT_Command_Sequence_union_SUIT_Condition_m;
	zassert_equal(SUIT_Condition_suit_condition_class_identifier_m_l_c,
		condition->SUIT_Condition_choice, NULL);
	zassert_equal(exp_rep_policy,
		condition->SUIT_Condition_suit_condition_class_identifier_m_l_SUIT_Rep_Policy_m, NULL);
}

ZTEST(cbor_decode_test9, test_suit14_ex0_validate_run)
{
	struct SUIT_Envelope envelope;
	struct SUIT_Manifest manifest;
	struct SUIT_Command_Sequence command_sequence;
	struct SUIT_Condition *condition;
	struct SUIT_Directive *directive;
	size_t out_len;
	uint32_t exp_rep_policy1 = (
		(1 << suit_reporting_bits_suit_send_record_success_c)
		| (1 << suit_reporting_bits_suit_send_record_failure_c)
		| (1 << suit_reporting_bits_suit_send_sysinfo_success_c)
		| (1 << suit_reporting_bits_suit_send_sysinfo_failure_c));
	uint32_t exp_rep_policy2 = (1 << suit_reporting_bits_suit_send_record_failure_c);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Envelope_Tagged(example0, sizeof(example0), &envelope, &out_len), NULL);
	zassert_equal(sizeof(example0), out_len, NULL);
	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Manifest(envelope.SUIT_Envelope_suit_manifest.value,
		envelope.SUIT_Envelope_suit_manifest.len, &manifest, &out_len), NULL);
	zassert_equal(envelope.SUIT_Envelope_suit_manifest.len, out_len, NULL);
	zassert_true(manifest.SUIT_Manifest_SUIT_Unseverable_Members_m.SUIT_Unseverable_Members_suit_validate_present, NULL);
	zassert_false(manifest.SUIT_Manifest_SUIT_Unseverable_Members_m.SUIT_Unseverable_Members_suit_load_present, NULL);
	zassert_true(manifest.SUIT_Manifest_SUIT_Unseverable_Members_m.SUIT_Unseverable_Members_suit_run_present, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Command_Sequence(
		manifest.SUIT_Manifest_SUIT_Unseverable_Members_m.SUIT_Unseverable_Members_suit_validate.SUIT_Unseverable_Members_suit_validate.value,
		manifest.SUIT_Manifest_SUIT_Unseverable_Members_m.SUIT_Unseverable_Members_suit_validate.SUIT_Unseverable_Members_suit_validate.len,
		&command_sequence, &out_len), NULL);
	zassert_equal(
		manifest.SUIT_Manifest_SUIT_Unseverable_Members_m.SUIT_Unseverable_Members_suit_validate.SUIT_Unseverable_Members_suit_validate.len,
		out_len, NULL);
	zassert_equal(1, command_sequence.SUIT_Command_Sequence_union_count, NULL);
	zassert_equal(SUIT_Command_Sequence_union_SUIT_Condition_m_c, command_sequence.SUIT_Command_Sequence_union[0].SUIT_Command_Sequence_union_choice, NULL);
	condition = &command_sequence.SUIT_Command_Sequence_union[0].SUIT_Command_Sequence_union_SUIT_Condition_m;
	zassert_equal(SUIT_Condition_suit_condition_image_match_m_l_c,
		condition->SUIT_Condition_choice, NULL);
	zassert_equal(exp_rep_policy1,
		condition->SUIT_Condition_suit_condition_image_match_m_l_SUIT_Rep_Policy_m, NULL);

	zassert_equal(ZCBOR_SUCCESS, cbor_decode_SUIT_Command_Sequence(
		manifest.SUIT_Manifest_SUIT_Unseverable_Members_m.SUIT_Unseverable_Members_suit_run.SUIT_Unseverable_Members_suit_run.value,
		manifest.SUIT_Manifest_SUIT_Unseverable_Members_m.SUIT_Unseverable_Members_suit_run.SUIT_Unseverable_Members_suit_run.len,
		&command_sequence, &out_len), NULL);
	zassert_equal(
		manifest.SUIT_Manifest_SUIT_Unseverable_Members_m.SUIT_Unseverable_Members_suit_run.SUIT_Unseverable_Members_suit_run.len,
		out_len, NULL);
	zassert_equal(1, command_sequence.SUIT_Command_Sequence_union_count, NULL);
	zassert_equal(SUIT_Command_Sequence_union_SUIT_Directive_m_c, command_sequence.SUIT_Command_Sequence_union[0].SUIT_Command_Sequence_union_choice, NULL);
	directive = &command_sequence.SUIT_Command_Sequence_union[0].SUIT_Command_Sequence_union_SUIT_Directive_m;
	zassert_equal(SUIT_Directive_suit_directive_run_m_l_c,
		directive->SUIT_Directive_choice, NULL);
	zassert_equal(exp_rep_policy2,
		directive->SUIT_Directive_suit_directive_run_m_l_SUIT_Rep_Policy_m, NULL);

}

ZTEST_SUITE(cbor_decode_test9, NULL, NULL, NULL, NULL, NULL);
