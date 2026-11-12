/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "manifest12_decode.h"
#include "main_entry.h"

bool fuzz_one_input(const uint8_t *data, size_t size)
{
    size_t payload_len_out = 0;
    /* The structs are large because of --default-max-qty, so keep them off the stack. */
    static struct SUIT_Envelope result;
    static struct SUIT_Envelope result2;
    static struct SUIT_Command_Sequence command_seq;

    /* Try both entry points. The seeds are untagged, and an input fails quickly with the
     * entry point it doesn't match. */
    int ret = cbor_decode_SUIT_Envelope_Tagged(data, size,
                               &result,
                               &payload_len_out);
    if (ret != ZCBOR_SUCCESS) {
        ret = cbor_decode_SUIT_Envelope(data, size, &result, &payload_len_out);
    }
    if (ret != ZCBOR_SUCCESS) {
        return false;
    }

    for (int i = 0; i < result.SUIT_Envelope_suit_integrated_dependency_key_count; i++) {
        ret = cbor_decode_SUIT_Envelope(
            result.SUIT_Envelope_suit_integrated_dependency_key[i].SUIT_Envelope_suit_integrated_dependency_key.value,
            result.SUIT_Envelope_suit_integrated_dependency_key[i].SUIT_Envelope_suit_integrated_dependency_key.len,
            &result2, &payload_len_out);
        if (ret != ZCBOR_SUCCESS) {
            return false;
        }
    }

    if (result.SUIT_Envelope_suit_manifest_cbor
              .SUIT_Manifest_SUIT_Severable_Manifest_Members_m
              .SUIT_Severable_Manifest_Members_suit_dependency_resolution_present) {
        ret = cbor_decode_SUIT_Command_Sequence(
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Severable_Manifest_Members_m
                .SUIT_Severable_Manifest_Members_suit_dependency_resolution.value,
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Severable_Manifest_Members_m
                .SUIT_Severable_Manifest_Members_suit_dependency_resolution.len,
            &command_seq, &payload_len_out);
        if (ret != ZCBOR_SUCCESS) {
            return false;
        }
    }

    if (result.SUIT_Envelope_suit_manifest_cbor
              .SUIT_Manifest_SUIT_Severable_Manifest_Members_m
              .SUIT_Severable_Manifest_Members_suit_payload_fetch_present) {
        ret = cbor_decode_SUIT_Command_Sequence(
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Severable_Manifest_Members_m
                .SUIT_Severable_Manifest_Members_suit_payload_fetch.value,
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Severable_Manifest_Members_m
                .SUIT_Severable_Manifest_Members_suit_payload_fetch.len,
            &command_seq, &payload_len_out);
        if (ret != ZCBOR_SUCCESS) {
            return false;
        }
    }

    if (result.SUIT_Envelope_suit_manifest_cbor
              .SUIT_Manifest_SUIT_Severable_Manifest_Members_m
              .SUIT_Severable_Manifest_Members_suit_install_present) {
        ret = cbor_decode_SUIT_Command_Sequence(
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Severable_Manifest_Members_m
                .SUIT_Severable_Manifest_Members_suit_install.value,
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Severable_Manifest_Members_m
                .SUIT_Severable_Manifest_Members_suit_install.len,
            &command_seq, &payload_len_out);
        if (ret != ZCBOR_SUCCESS) {
            return false;
        }
    }

    if (result.SUIT_Envelope_suit_manifest_cbor
              .SUIT_Manifest_SUIT_Unseverable_Members_m
              .SUIT_Unseverable_Members_suit_validate_present) {
        ret = cbor_decode_SUIT_Command_Sequence(
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Unseverable_Members_m
                .SUIT_Unseverable_Members_suit_validate.value,
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Unseverable_Members_m
                .SUIT_Unseverable_Members_suit_validate.len,
            &command_seq, &payload_len_out);
        if (ret != ZCBOR_SUCCESS) {
            return false;
        }
    }

    if (result.SUIT_Envelope_suit_manifest_cbor
              .SUIT_Manifest_SUIT_Unseverable_Members_m
              .SUIT_Unseverable_Members_suit_load_present) {
        ret = cbor_decode_SUIT_Command_Sequence(
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Unseverable_Members_m
                .SUIT_Unseverable_Members_suit_load.value,
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Unseverable_Members_m
                .SUIT_Unseverable_Members_suit_load.len,
            &command_seq, &payload_len_out);
        if (ret != ZCBOR_SUCCESS) {
            return false;
        }
    }

    if (result.SUIT_Envelope_suit_manifest_cbor
              .SUIT_Manifest_SUIT_Unseverable_Members_m
              .SUIT_Unseverable_Members_suit_run_present) {
        ret = cbor_decode_SUIT_Command_Sequence(
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Unseverable_Members_m
                .SUIT_Unseverable_Members_suit_run.value,
            result.SUIT_Envelope_suit_manifest_cbor
                .SUIT_Manifest_SUIT_Unseverable_Members_m
                .SUIT_Unseverable_Members_suit_run.len,
            &command_seq, &payload_len_out);
        if (ret != ZCBOR_SUCCESS) {
            return false;
        }
    }
    return true;
}
