/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include "manifest2_decode.h"
#include "zcbor_print.h"


/* draft-ietf-suit-manifest-02 Example 0 */
uint8_t test_vector1[] = {
	0xa2, 0x01, 0x58, 0x54, 0xd2, 0x84, 0x43, 0xa1, 0x01,
	0x26, 0xa1, 0x04, 0x48, 0x74, 0x65, 0x73, 0x74, 0x20,
	0x6b, 0x65, 0x79, 0xf6, 0x58, 0x40, 0xeb, 0xec, 0xb6,
	0x6c, 0xbe, 0xcb, 0x19, 0xdc, 0xed, 0xac, 0xf8, 0x45,
	0x9c, 0x1a, 0x22, 0xa1, 0x45, 0x37, 0x81, 0xba, 0x98,
	0xd8, 0xff, 0xb9, 0xd4, 0xe2, 0x91, 0x2f, 0x29, 0xd2,
	0x3b, 0xac, 0x5a, 0xe3, 0xd5, 0x1f, 0x1f, 0xf0, 0xc1,
	0xb1, 0xdf, 0x05, 0xe2, 0x07, 0xca, 0x17, 0x48, 0x3a,
	0x57, 0xed, 0xe9, 0x14, 0xcf, 0x82, 0x6b, 0x73, 0x59,
	0x91, 0x37, 0x88, 0x1c, 0x83, 0x64, 0xc8, 0x02, 0x58,
	0x51, 0xa4, 0x01, 0x01, 0x02, 0x01, 0x03, 0x58, 0x40,
	0xa2, 0x02, 0x4c, 0x81, 0x82, 0x45, 0x46, 0x6c, 0x61,
	0x73, 0x68, 0x43, 0x00, 0x34, 0x01, 0x04, 0x58, 0x2e,
	0x82, 0x13, 0xa2, 0x0b, 0x58, 0x24, 0x82, 0x02, 0x58,
	0x20, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01,
	0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc,
	0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0x0c, 0x19, 0x87,
	0xd0, 0x0c, 0x47, 0x86, 0x0c, 0x00, 0x03, 0xf6, 0x17,
	0xf6
};

static struct SUIT_Outer_Wrapper outerwrapper1;
static struct SUIT_Command_Sequence sequence;

ZTEST(cbor_decode_test2, test_5)
{
	struct SUIT_Manifest *manifest;
	size_t decode_len;
	struct SUIT_Component_Identifier *component;
	uint8_t expected_component0[] = {0x46, 0x6c, 0x61, 0x73, 0x68};
	uint8_t expected_component1[] = {0x00, 0x34, 0x01};
	int res;

	zcbor_log("test_vector at: 0x%zu\r\n", (size_t)test_vector1);
	zcbor_log("test_vector end at: 0x%zu\r\n",
				((size_t)test_vector1) + sizeof(test_vector1));
	memset(&outerwrapper1, 0, sizeof(outerwrapper1));
	res = cbor_decode_SUIT_Outer_Wrapper(test_vector1, sizeof(test_vector1),
						&outerwrapper1, &decode_len);
	zassert_equal(ZCBOR_SUCCESS, res, "top-level decoding failed.");
	zassert_equal(sizeof(test_vector1), decode_len, NULL);
	zassert_equal(SUIT_Manifest_Wrapped_suit_manifest_c, outerwrapper1
			.SUIT_Outer_Wrapper_SUIT_Manifest_Wrapped_m
			.SUIT_Manifest_Wrapped_choice,
			"wrong manifest variant");
	manifest = &outerwrapper1
			.SUIT_Outer_Wrapper_SUIT_Manifest_Wrapped_m
			.SUIT_Manifest_Wrapped_suit_manifest_cbor;
	zassert_equal(1, manifest
			->SUIT_Manifest_suit_manifest_sequence_number,
			"wrong sequence number");
	zassert_equal(1, manifest
			->SUIT_Manifest_suit_common_present,
			"common should be present");
	zassert_equal(0, manifest
			->SUIT_Manifest_suit_dependency_resolution_present,
			"should not be present");
	zassert_equal(0, manifest
			->SUIT_Manifest_suit_payload_fetch_present,
			"should not be present");
	zassert_equal(0, manifest
			->SUIT_Manifest_suit_install_present,
			"should not be present");
	zassert_equal(0, manifest
			->SUIT_Manifest_suit_validate_present,
			"should not be present");
	zassert_equal(0, manifest
			->SUIT_Manifest_suit_load_present,
			"should not be present");
	zassert_equal(1, manifest
			->SUIT_Manifest_suit_run_present,
			"should not be present");
	zassert_equal(0, manifest
			->SUIT_Manifest_suit_text_present,
			"should not be present");
	zassert_equal(0, manifest
			->SUIT_Manifest_suit_coswid_present,
			"should not be present");
	zassert_equal(1, manifest
			->SUIT_Manifest_suit_common
			.SUIT_Manifest_suit_common_cbor
			.SUIT_Common_suit_components
			.SUIT_Common_suit_components_cbor
			.SUIT_Components_SUIT_Component_Identifier_m_count,
			"Wrong number of common components");
	component = &manifest
			->SUIT_Manifest_suit_common
			.SUIT_Manifest_suit_common_cbor
			.SUIT_Common_suit_components
			.SUIT_Common_suit_components_cbor
			.SUIT_Components_SUIT_Component_Identifier_m[0];
	zassert_equal(2, component
			->SUIT_Component_Identifier_bstr_count,
			"Wrong number of elements in component");
	zassert_equal(sizeof(expected_component0), component
			->SUIT_Component_Identifier_bstr[0]
			.len,
			"component elem 0 len doesn't match.");
	zassert_mem_equal(expected_component0, component
			->SUIT_Component_Identifier_bstr[0]
			.value,
			sizeof(expected_component0),
			"component elem 0 doesn't match.");
	zassert_equal(sizeof(expected_component1), component
			->SUIT_Component_Identifier_bstr[1]
			.len,
			"component elem 1 len doesn't match.");
	zassert_mem_equal(expected_component1, component
			->SUIT_Component_Identifier_bstr[1]
			.value,
			sizeof(expected_component1),
			"component elem 1 doesn't match.");

	zcbor_log("\r\n");
	zassert_equal(1, manifest
			->SUIT_Manifest_suit_common
			.SUIT_Manifest_suit_common_cbor
			.SUIT_Common_suit_common_sequence_present,
			"common sequence should be present.");
	memset(&sequence, 0, sizeof(sequence));
	res = cbor_decode_SUIT_Command_Sequence(
			manifest
			->SUIT_Manifest_suit_common
			.SUIT_Manifest_suit_common_cbor
			.SUIT_Common_suit_common_sequence
			.value,
			manifest
			->SUIT_Manifest_suit_common
			.SUIT_Manifest_suit_common_cbor
			.SUIT_Common_suit_common_sequence
			.len,
			&sequence, &decode_len);
	zassert_equal(ZCBOR_SUCCESS, res, "Parsing common sequence failed.");
	zassert_equal(manifest
			->SUIT_Manifest_suit_common
			.SUIT_Manifest_suit_common_cbor
			.SUIT_Common_suit_common_sequence
			.len, decode_len, NULL);
	zassert_equal(1, sequence
			.SUIT_Command_Sequence_SUIT_Command_m_count,
			"Should be one command (was %d).", sequence
			.SUIT_Command_Sequence_SUIT_Command_m_count);
	zassert_equal(SUIT_Command_SUIT_Directive_m_c, sequence
			.SUIT_Command_Sequence_SUIT_Command_m[0]
			.SUIT_Command_choice,
			"Should be a directive.");
	zassert_equal(2, sequence
			.SUIT_Command_Sequence_SUIT_Command_m[0]
			.SUIT_Command_SUIT_Directive_m
			.suit_directive_set_parameters_m_l_map_SUIT_Parameters_m_count,
			"Should be two vars (parameters).");
	zcbor_log("\r\n");

	memset(&sequence, 0, sizeof(sequence));
	res = cbor_decode_SUIT_Command_Sequence(
			manifest
			->SUIT_Manifest_suit_run
			.value,
			manifest
			->SUIT_Manifest_suit_run
			.len,
			&sequence, &decode_len);
	zassert_equal(ZCBOR_SUCCESS, res, "Parsing run command sequence failed.");
	zassert_equal(manifest
			->SUIT_Manifest_suit_run
			.len, decode_len, NULL);
}

ZTEST_SUITE(cbor_decode_test2, NULL, NULL, NULL, NULL, NULL);
