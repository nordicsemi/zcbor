#!/usr/bin/env python3
#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

from unittest import TestCase, main
from subprocess import Popen, PIPE
from regex import sub, search, escape, compile
from pathlib import Path
from ecdsa import VerifyingKey
from hashlib import sha256
import cbor2
from sys import exit
from yaml import safe_load, safe_dump
from tempfile import mkdtemp, NamedTemporaryFile
from shutil import rmtree
from os import linesep

try:
    import zcbor
except ImportError:
    print("""
The zcbor package must be installed to run these tests.
During development, install with `pip3 install -e .` to install in a way
that picks up changes in the files without having to reinstall.
""")
    exit(1)


p_root = Path(__file__).absolute().parents[2]
p_tests = Path(p_root, "tests")
p_cases = Path(p_tests, "cases")
p_manifest12 = Path(p_cases, "manifest12.cddl")
p_manifest14 = Path(p_cases, "manifest14.cddl")
p_manifest16 = Path(p_cases, "manifest16.cddl")
p_manifest20 = Path(p_cases, "manifest20.cddl")
p_test_vectors12 = tuple(Path(p_cases, f"manifest12_example{i}.cborhex") for i in range(6))
p_test_vectors14 = tuple(Path(p_cases, f"manifest14_example{i}.cborhex") for i in range(6))
p_test_vectors16 = tuple(
    Path(p_cases, f"manifest14_example{i}.cborhex") for i in range(6)
)  # Identical to manifest14.
p_test_vectors20 = tuple(Path(p_cases, f"manifest20_example{i}.cborhex") for i in range(6))
p_optional = Path(p_cases, "optional.cddl")
p_corner_cases = Path(p_cases, "corner_cases.cddl")
p_cose = Path(p_cases, "cose.cddl")
p_manifest14_priv = Path(p_cases, "manifest14.priv")
p_manifest14_pub = Path(p_cases, "manifest14.pub")
p_map_bstr_cddl = Path(p_cases, "map_bstr.cddl")
p_map_bstr_yaml = Path(p_cases, "map_bstr.yaml")
p_yaml_compat_cddl = Path(p_cases, "yaml_compatibility.cddl")
p_yaml_compat_yaml = Path(p_cases, "yaml_compatibility.yaml")
p_pet_cddl = Path(p_cases, "pet.cddl")
p_README = Path(p_root, "README.md")
p_prelude = Path(p_root, "zcbor", "prelude.cddl")
p_VERSION = Path(p_root, "zcbor", "VERSION")


class TestManifest(TestCase):
    """Class for testing examples against CDDL for various versions of the SUIT manifest spec."""

    def decode_file(self, data_path, *cddl_paths):
        data = bytes.fromhex(data_path.read_text(encoding="utf-8").replace("\n", ""))
        self.decode_string(data, *cddl_paths)

    def decode_string(self, data_string, *cddl_paths):
        cddl_str = " ".join((Path(p).read_text(encoding="utf-8") for p in cddl_paths))
        self.my_types = zcbor.DataTranslator.from_cddl(cddl_string=cddl_str, default_max_qty=16).my_types
        cddl = self.my_types["SUIT_Envelope_Tagged"]
        self.decoded = cddl.decode_str(data_string)


class TestEx0Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[0], p_manifest12)

    def test_manifest_digest(self):
        self.assertEqual(
            bytes.fromhex("5c097ef64bf3bb9b494e71e1f2418eef8d466cc902f639a855ec9af3e9eddb99"),
            self.decoded.suit_authentication_wrapper.SUIT_Digest_bstr.suit_digest_bytes,
        )

    def test_signature(self):
        self.assertEqual(
            1,
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0]
            .COSE_Sign1_Tagged_m.protected.uintint[0]
            .uintint_key,
        )
        self.assertEqual(
            -7,
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0]
            .COSE_Sign1_Tagged_m.protected.uintint[0]
            .uintint,
        )
        self.assertEqual(
            bytes.fromhex(
                "a19fd1f23b17beed321cece7423dfb48c457b8f1f6ac83577a3c10c6773f6f3a7902376b59540920b6c5f57bac5fc8543d8f5d3d974faa2e6d03daa534b443a7"
            ),
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[
                0
            ].COSE_Sign1_Tagged_m.signature,
        )

    def test_validate_run(self):
        self.assertEqual(
            "suit_condition_image_match_m_l",
            self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_validate[0]
            .suit_validate.union[0]
            .SUIT_Condition_m.union_choice,
        )
        self.assertEqual(
            "suit_directive_run_m_l",
            self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_run[0]
            .suit_run.union[0]
            .SUIT_Directive_m.union_choice,
        )

    def test_image_size(self):
        self.assertEqual(
            34768,
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
            .suit_common_sequence.union[0]
            .SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[3]
            .suit_parameter_image_size,
        )


class TestEx0InvManifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def test_duplicate_type(self):
        with self.assertRaises(zcbor.CddlParsingError, msg="Duplicate CDDL type found"):
            self.decode_file(p_test_vectors12[0], p_manifest12, p_manifest12)


class TestEx1Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[1], p_manifest12)

    def test_components(self):
        self.assertEqual([b"\x00"], self.decoded.suit_manifest.suit_common.suit_components[0][0].bstr)

    def test_uri(self):
        self.assertEqual(
            "http://example.com/file.bin",
            self.decoded.suit_manifest.SUIT_Severable_Manifest_Members.suit_install[0]
            .suit_install.union[0]
            .SUIT_Directive_m.suit_directive_set_parameters_m_l.map[0]
            .suit_parameter_uri,
        )


class TestEx2Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[2], p_manifest12)

    def test_severed_uri(self):
        self.assertEqual(
            "http://example.com/very/long/path/to/file/file.bin",
            self.decoded.SUIT_Severable_Manifest_Members.suit_install[0]
            .suit_install.union[0]
            .SUIT_Directive_m.suit_directive_set_parameters_m_l.map[0]
            .suit_parameter_uri,
        )

    def test_severed_text(self):
        self.assertIn(
            "Example 2",
            self.decoded.SUIT_Severable_Manifest_Members.suit_text[
                0
            ].suit_text.SUIT_Text_Keys.suit_text_manifest_description[0],
        )
        self.assertEqual(
            [b"\x00"],
            self.decoded.SUIT_Severable_Manifest_Members.suit_text[0]
            .suit_text.SUIT_Component_Identifier[0]
            .SUIT_Component_Identifier_key.bstr,
        )
        self.assertEqual(
            "arm.com",
            self.decoded.SUIT_Severable_Manifest_Members.suit_text[0]
            .suit_text.SUIT_Component_Identifier[0]
            .SUIT_Component_Identifier.SUIT_Text_Component_Keys.suit_text_vendor_domain[0],
        )
        self.assertEqual(
            "This component is a demonstration. The digest is a sample pattern, not a real one.",
            self.decoded.SUIT_Severable_Manifest_Members.suit_text[0]
            .suit_text.SUIT_Component_Identifier[0]
            .SUIT_Component_Identifier.SUIT_Text_Component_Keys.suit_text_component_description[0],
        )


class TestEx3Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[3], p_manifest12)

    def test_A_B_offset(self):
        self.assertEqual(
            33792,
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
            .suit_common_sequence.union[1]
            .SUIT_Common_Commands_m.suit_directive_try_each_m_l.SUIT_Directive_Try_Each_Argument_m.SUIT_Command_Sequence_bstr[
                0
            ]
            .union[0]
            .SUIT_Directive_m.suit_directive_override_parameters_m_l.map[0]
            .suit_parameter_component_offset,
        )
        self.assertEqual(
            541696,
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
            .suit_common_sequence.union[1]
            .SUIT_Common_Commands_m.suit_directive_try_each_m_l.SUIT_Directive_Try_Each_Argument_m.SUIT_Command_Sequence_bstr[
                1
            ]
            .union[0]
            .SUIT_Directive_m.suit_directive_override_parameters_m_l.map[0]
            .suit_parameter_component_offset,
        )


class TestEx4Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[4], p_manifest12)

    def test_load_decompress(self):
        self.assertEqual(
            0,
            self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_load[0]
            .suit_load.union[1]
            .SUIT_Directive_m.suit_directive_set_parameters_m_l.map[3]
            .suit_parameter_source_component,
        )
        self.assertEqual(
            "SUIT_Compression_Algorithm_zlib_m",
            self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_load[0]
            .suit_load.union[1]
            .SUIT_Directive_m.suit_directive_set_parameters_m_l.map[2]
            .suit_parameter_compression_info.suit_compression_algorithm,
        )


class TestEx5Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[5], p_manifest12)

    def test_two_image_match(self):
        self.assertEqual(
            "suit_condition_image_match_m_l",
            self.decoded.suit_manifest.SUIT_Severable_Manifest_Members.suit_install[0]
            .suit_install.union[3]
            .SUIT_Condition_m.union_choice,
        )
        self.assertEqual(
            "suit_condition_image_match_m_l",
            self.decoded.suit_manifest.SUIT_Severable_Manifest_Members.suit_install[0]
            .suit_install.union[7]
            .SUIT_Condition_m.union_choice,
        )


def dumps(obj):
    return cbor2.dumps(obj, canonical=True)


def loads(string):
    return cbor2.loads(string)


class TestEx0Manifest14(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.key = VerifyingKey.from_pem(p_manifest14_pub.read_text(encoding="utf-8"))

    def do_test_authentication(self):
        self.assertEqual(
            "COSE_Sign1_Tagged_m",
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].union_choice,
        )
        self.assertEqual(
            -7,
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0]
            .COSE_Sign1_Tagged_m.Headers_m.protected.header_map_bstr.Generic_Headers.uint1union[0]
            .int,
        )

        manifest_signature = self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[
            0
        ].COSE_Sign1_Tagged_m.signature
        signature_header = self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[
            0
        ].COSE_Sign1_Tagged_m.Headers_m.protected.header_map_bstr_bstr
        manifest_suit_digest = self.decoded.suit_authentication_wrapper.SUIT_Digest_bstr_bstr

        sig_struct = dumps(["Signature1", signature_header, b"", manifest_suit_digest])

        self.key.verify(manifest_signature, sig_struct, hashfunc=sha256)

    def test_auth_0(self):
        self.decode_file(p_test_vectors14[0], p_manifest14, p_cose)
        self.do_test_authentication()

    def test_auth_1(self):
        self.decode_file(p_test_vectors14[1], p_manifest14, p_cose)
        self.do_test_authentication()

    def test_auth_2(self):
        self.decode_file(p_test_vectors14[2], p_manifest14, p_cose)
        self.do_test_authentication()

    def test_auth_3(self):
        self.decode_file(p_test_vectors14[3], p_manifest14, p_cose)
        self.do_test_authentication()

    def test_auth_4(self):
        self.decode_file(p_test_vectors14[4], p_manifest14, p_cose)
        self.do_test_authentication()

    def test_auth_5(self):
        self.decode_file(p_test_vectors14[5], p_manifest14, p_cose)
        self.do_test_authentication()


class TestEx1Manifest14(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors14[1], p_manifest14, p_cose)
        self.manifest_digest = bytes.fromhex(
            "60c61d6eb7a1aaeddc49ce8157a55cff0821537eeee77a4ded44155b03045132"
        )

    def test_structure(self):
        self.assertEqual(
            "COSE_Sign1_Tagged_m",
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].union_choice,
        )
        self.assertEqual(
            -7,
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0]
            .COSE_Sign1_Tagged_m.Headers_m.protected.header_map_bstr.Generic_Headers.uint1union[0]
            .int,
        )
        self.assertEqual(
            self.manifest_digest,
            self.decoded.suit_authentication_wrapper.SUIT_Digest_bstr.suit_digest_bytes,
        )
        self.assertEqual(1, self.decoded.suit_manifest.suit_manifest_sequence_number)
        self.assertEqual(
            bytes.fromhex("fa6b4a53d5ad5fdfbe9de663e4d41ffe"),
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
            .suit_common_sequence.union[0]
            .SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[0]
            .suit_parameter_vendor_identifier.RFC4122_UUID_m,
        )
        self.assertEqual(
            bytes.fromhex("1492af1425695e48bf429b2d51f2ab45"),
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
            .suit_common_sequence.union[0]
            .SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[1]
            .suit_parameter_class_identifier,
        )
        self.assertEqual(
            bytes.fromhex("00112233445566778899aabbccddeeff0123456789abcdeffedcba9876543210"),
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
            .suit_common_sequence.union[0]
            .SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[2]
            .suit_parameter_image_digest.suit_digest_bytes,
        )
        self.assertEqual(
            "cose_alg_sha_256_m",
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
            .suit_common_sequence.union[0]
            .SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[2]
            .suit_parameter_image_digest.suit_digest_algorithm_id.union_choice,
        )
        self.assertEqual(
            34768,
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
            .suit_common_sequence.union[0]
            .SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[3]
            .suit_parameter_image_size,
        )
        self.assertEqual(
            4,
            len(
                self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
                .suit_common_sequence.union[0]
                .SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map
            ),
        )
        self.assertEqual(
            15,
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
            .suit_common_sequence.union[1]
            .SUIT_Condition_m.suit_condition_vendor_identifier_m_l.SUIT_Rep_Policy_m,
        )
        self.assertEqual(
            15,
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
            .suit_common_sequence.union[2]
            .SUIT_Condition_m.suit_condition_class_identifier_m_l.SUIT_Rep_Policy_m,
        )
        self.assertEqual(
            3,
            len(
                self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union
            ),
        )
        self.assertEqual(
            2,
            len(
                self.decoded.suit_manifest.suit_common.suit_common_sequence[
                    0
                ].suit_common_sequence.union[0]
            ),
        )
        self.assertEqual(
            2,
            len(
                self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
                .suit_common_sequence.union[0]
                .SUIT_Common_Commands_m
            ),
        )
        self.assertEqual(
            1,
            len(
                self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
                .suit_common_sequence.union[0]
                .SUIT_Common_Commands_m.suit_directive_override_parameters_m_l
            ),
        )
        self.assertEqual(
            4,
            len(
                self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
                .suit_common_sequence.union[0]
                .SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map
            ),
        )
        self.assertEqual(
            2,
            len(
                self.decoded.suit_manifest.suit_common.suit_common_sequence[0]
                .suit_common_sequence.union[0]
                .SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[0]
            ),
        )

    def test_cbor_pen(self):
        data = bytes.fromhex(p_test_vectors14[1].read_text(encoding="utf-8").replace("\n", ""))
        struct = loads(data)
        struct2 = loads(struct.value[3])  # manifest
        struct3 = loads(struct2[3])  # common sequence
        struct4 = loads(struct3[4])  # override params
        self.assertEqual(struct4[0], 20)
        self.assertTrue(isinstance(struct4[1][1], bytes))
        struct4[1][1] = cbor2.CBORTag(112, struct4[1][1])  # Add the tag for cbor-pen
        struct3[4] = dumps(struct4)
        struct2[3] = dumps(struct3)
        struct.value[3] = dumps(struct2)
        data = dumps(struct)
        self.decode_string(data, p_manifest14, p_cose)


class TestEx1InvManifest14(TestManifest):
    def test_inv0(self):
        data = bytes.fromhex(p_test_vectors14[1].read_text(encoding="utf-8").replace("\n", ""))
        struct = loads(data)
        struct2 = loads(struct.value[2])  # authentication
        struct3 = loads(struct2[1])
        struct3.tag = 99999  # invalid tag for COSE_Sign1
        struct2[1] = dumps(struct3)
        struct.value[2] = dumps(struct2)
        data = dumps(struct)
        try:
            self.decode_string(data, p_manifest14, p_cose)
        except zcbor.CddlValidationError as e:
            return
        else:
            assert False, "Should have failed validation"

    def test_inv1(self):
        data = bytes.fromhex(p_test_vectors14[1].read_text(encoding="utf-8").replace("\n", ""))
        struct = loads(data)
        struct2 = loads(struct.value[3])  # manifest
        struct2[1] += 1  # invalid manifest version
        struct.value[3] = dumps(struct2)
        data = dumps(struct)
        try:
            self.decode_string(data, p_manifest14, p_cose)
        except zcbor.CddlValidationError as e:
            return
        else:
            assert False, "Should have failed validation"

    def test_inv2(self):
        data = bytes.fromhex(p_test_vectors14[1].read_text(encoding="utf-8").replace("\n", ""))
        struct = loads(data)
        struct.value[23] = b""  # Invalid integrated payload key
        data = dumps(struct)
        try:
            self.decode_string(data, p_manifest14, p_cose)
        except (zcbor.CddlValidationError, cbor2.CBORDecodeEOF) as e:
            return
        except:
            raise
        else:
            assert False, "Should have failed validation"

    def test_inv3(self):
        data = bytes.fromhex(p_test_vectors14[1].read_text(encoding="utf-8").replace("\n", ""))
        struct = loads(data)
        struct2 = loads(struct.value[3])  # manifest
        struct3 = loads(struct2[3])  # common sequence
        struct4 = loads(struct3[4])  # override params
        self.assertEqual(struct4[0], 20)
        self.assertTrue(isinstance(struct4[1][1], bytes))
        struct4[1][1] += b"x"  # vendor ID: wrong length
        struct3[4] = dumps(struct4)
        struct2[3] = dumps(struct3)
        struct.value[3] = dumps(struct2)
        data = dumps(struct)
        try:
            self.decode_string(data, p_manifest14, p_cose)
        except zcbor.CddlValidationError as e:
            return
        else:
            assert False, "Should have failed validation"


class TestEx2Manifest14(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors14[2], p_manifest14, p_cose)

    def test_text(self):
        self.assertEqual(
            bytes.fromhex("2bfc4d0cc6680be7dd9f5ca30aa2bb5d1998145de33d54101b80e2ca49faf918"),
            self.decoded.suit_manifest.SUIT_Severable_Members_Choice.suit_text[
                0
            ].SUIT_Digest_m.suit_digest_bytes,
        )
        self.assertEqual(
            bytes.fromhex("2bfc4d0cc6680be7dd9f5ca30aa2bb5d1998145de33d54101b80e2ca49faf918"),
            sha256(
                dumps(self.decoded.SUIT_Severable_Manifest_Members.suit_text[0].suit_text_bstr)
            ).digest(),
        )
        self.assertEqual(
            "arm.com",
            self.decoded.SUIT_Severable_Manifest_Members.suit_text[0]
            .suit_text.SUIT_Component_Identifier[0]
            .SUIT_Component_Identifier.SUIT_Text_Component_Keys.suit_text_vendor_domain[0],
        )
        self.assertEqual(
            "This component is a demonstration. The digest is a sample pattern, not a real one.",
            self.decoded.SUIT_Severable_Manifest_Members.suit_text[0]
            .suit_text.SUIT_Component_Identifier[0]
            .SUIT_Component_Identifier.SUIT_Text_Component_Keys.suit_text_component_description[0],
        )

        # Check manifest description. The concatenation and .replace() call are there to add
        # trailing whitespace to all blank lines except the first.
        # This is done in this way to avoid editors automatically removing the whitespace.
        self.assertEqual(
            """## Example 2: Simultaneous Download, Installation, Secure Boot, Severed Fields
"""
            + """
    This example covers the following templates:

    * Compatibility Check ({{template-compatibility-check}})
    * Secure Boot ({{template-secure-boot}})
    * Firmware Download ({{firmware-download-template}})

    This example also demonstrates severable elements ({{ovr-severable}}), and text ({{manifest-digest-text}}).""".replace(
                "\n\n", "\n    \n"
            ),
            self.decoded.SUIT_Severable_Manifest_Members.suit_text[
                0
            ].suit_text.SUIT_Text_Keys.suit_text_manifest_description[0],
        )


class TestEx3Manifest14(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors14[3], p_manifest14, p_cose)
        self.slots = (33792, 541696)

    def test_try_each(self):
        self.assertEqual(
            2,
            len(
                self.decoded.suit_manifest.SUIT_Severable_Members_Choice.suit_install[0]
                .SUIT_Command_Sequence_bstr.union[0]
                .SUIT_Directive_m.suit_directive_try_each_m_l.SUIT_Directive_Try_Each_Argument_m.SUIT_Command_Sequence_bstr
            ),
        )
        self.assertEqual(
            self.slots[0],
            self.decoded.suit_manifest.SUIT_Severable_Members_Choice.suit_install[0]
            .SUIT_Command_Sequence_bstr.union[0]
            .SUIT_Directive_m.suit_directive_try_each_m_l.SUIT_Directive_Try_Each_Argument_m.SUIT_Command_Sequence_bstr[
                0
            ]
            .union[0]
            .SUIT_Directive_m.suit_directive_override_parameters_m_l.map[0]
            .suit_parameter_component_slot,
        )
        self.assertEqual(
            self.slots[1],
            self.decoded.suit_manifest.SUIT_Severable_Members_Choice.suit_install[0]
            .SUIT_Command_Sequence_bstr.union[0]
            .SUIT_Directive_m.suit_directive_try_each_m_l.SUIT_Directive_Try_Each_Argument_m.SUIT_Command_Sequence_bstr[
                1
            ]
            .union[0]
            .SUIT_Directive_m.suit_directive_override_parameters_m_l.map[0]
            .suit_parameter_component_slot,
        )


class TestEx4Manifest14(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors14[4], p_manifest14, p_cose)

    def test_components(self):
        self.assertEqual(3, len(self.decoded.suit_manifest.suit_common.suit_components[0]))
        self.assertEqual(b"\x00", self.decoded.suit_manifest.suit_common.suit_components[0][0].bstr[0])
        self.assertEqual(b"\x02", self.decoded.suit_manifest.suit_common.suit_components[0][1].bstr[0])
        self.assertEqual(b"\x01", self.decoded.suit_manifest.suit_common.suit_components[0][2].bstr[0])


class TestEx5Manifest14(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors14[5], p_manifest14, p_cose)

    def test_validate(self):
        self.assertEqual(
            4,
            len(
                self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_validate[0].suit_validate.union
            ),
        )
        self.assertEqual(
            15,
            self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_validate[0]
            .suit_validate.union[1]
            .SUIT_Condition_m.suit_condition_image_match_m_l.SUIT_Rep_Policy_m,
        )


class TestEx5InvManifest14(TestManifest):
    def test_invalid_rep_policy(self):
        data = bytes.fromhex(p_test_vectors14[5].read_text(encoding="utf-8").replace("\n", ""))
        struct = loads(data)
        struct2 = loads(struct.value[3])  # manifest
        struct3 = loads(struct2[10])  # suit_validate
        struct3[3] += 16  # invalid Rep_Policy
        struct2[10] = dumps(struct3)
        struct.value[3] = dumps(struct2)
        data = dumps(struct)
        try:
            self.decode_string(data, p_manifest14, p_cose)
        except zcbor.CddlValidationError as e:
            return
        else:
            assert False, "Should have failed validation"


class TestEx0Manifest16(TestEx0Manifest14):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors16[0], p_manifest16, p_cose)


class TestEx1Manifest16(TestEx1Manifest14):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors16[1], p_manifest16, p_cose)


class TestEx1InvManifest16(TestEx1InvManifest14):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors16[1], p_manifest16, p_cose)


class TestEx2Manifest16(TestEx2Manifest14):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors16[2], p_manifest16, p_cose)


class TestEx3Manifest16(TestEx3Manifest14):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors16[3], p_manifest16, p_cose)


# Comment out because example 4 uses compression which is unsupported in manifest16
# class TestEx4Manifest16(TestEx4Manifest14):
#     def __init__(self, *args, **kwargs):
#         super().__init__(*args, **kwargs)
#         self.decode_file(p_test_vectors16[4], p_manifest16, p_cose)


class TestEx5Manifest16(TestEx5Manifest14):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors16[5], p_manifest16, p_cose)


class TestEx5InvManifest16(TestEx5InvManifest14):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors16[5], p_manifest16, p_cose)


class TestEx0Manifest20(TestEx0Manifest16):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors20[0], p_manifest20, p_cose)


class TestEx1Manifest20(TestEx1Manifest16):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors20[1], p_manifest20, p_cose)
        self.manifest_digest = bytes.fromhex(
            "ef14b7091e8adae8aa3bb6fca1d64fb37e19dcf8b35714cfdddc5968c80ff50e"
        )

    def test_structure(self):
        self.assertEqual(
            "COSE_Sign1_Tagged_m",
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].union_choice,
        )
        self.assertEqual(
            -7,
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0]
            .COSE_Sign1_Tagged_m.Headers_m.protected.header_map_bstr.Generic_Headers.uint1union[0]
            .int,
        )
        self.assertEqual(
            self.manifest_digest,
            self.decoded.suit_authentication_wrapper.SUIT_Digest_bstr.suit_digest_bytes,
        )
        self.assertEqual(1, self.decoded.suit_manifest.suit_manifest_sequence_number)
        self.assertEqual(
            bytes.fromhex("fa6b4a53d5ad5fdfbe9de663e4d41ffe"),
            self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
            .suit_shared_sequence.union[0]
            .SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[0]
            .suit_parameter_vendor_identifier.RFC4122_UUID_m,
        )
        self.assertEqual(
            bytes.fromhex("1492af1425695e48bf429b2d51f2ab45"),
            self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
            .suit_shared_sequence.union[0]
            .SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[1]
            .suit_parameter_class_identifier,
        )
        self.assertEqual(
            bytes.fromhex("00112233445566778899aabbccddeeff0123456789abcdeffedcba9876543210"),
            self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
            .suit_shared_sequence.union[0]
            .SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[2]
            .suit_parameter_image_digest.suit_digest_bytes,
        )
        self.assertEqual(
            "cose_alg_sha_256_m",
            self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
            .suit_shared_sequence.union[0]
            .SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[2]
            .suit_parameter_image_digest.suit_digest_algorithm_id.union_choice,
        )
        self.assertEqual(
            34768,
            self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
            .suit_shared_sequence.union[0]
            .SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[3]
            .suit_parameter_image_size,
        )
        self.assertEqual(
            4,
            len(
                self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
                .suit_shared_sequence.union[0]
                .SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map
            ),
        )
        self.assertEqual(
            15,
            self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
            .suit_shared_sequence.union[1]
            .SUIT_Condition_m.suit_condition_vendor_identifier_m_l.SUIT_Rep_Policy_m,
        )
        self.assertEqual(
            15,
            self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
            .suit_shared_sequence.union[2]
            .SUIT_Condition_m.suit_condition_class_identifier_m_l.SUIT_Rep_Policy_m,
        )
        self.assertEqual(
            3,
            len(
                self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union
            ),
        )
        self.assertEqual(
            2,
            len(
                self.decoded.suit_manifest.suit_common.suit_shared_sequence[
                    0
                ].suit_shared_sequence.union[0]
            ),
        )
        self.assertEqual(
            2,
            len(
                self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
                .suit_shared_sequence.union[0]
                .SUIT_Shared_Commands_m
            ),
        )
        self.assertEqual(
            1,
            len(
                self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
                .suit_shared_sequence.union[0]
                .SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l
            ),
        )
        self.assertEqual(
            4,
            len(
                self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
                .suit_shared_sequence.union[0]
                .SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map
            ),
        )
        self.assertEqual(
            2,
            len(
                self.decoded.suit_manifest.suit_common.suit_shared_sequence[0]
                .suit_shared_sequence.union[0]
                .SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[0]
            ),
        )


class TestEx1InvManifest20(TestEx1InvManifest16):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors20[1], p_manifest20, p_cose)


class TestEx2Manifest20(TestEx2Manifest16):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors20[2], p_manifest20, p_cose)


class TestEx3Manifest20(TestEx3Manifest16):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors20[3], p_manifest20, p_cose)
        self.slots = (0, 1)


class TestEx4Manifest20(TestEx4Manifest14):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors20[4], p_manifest20, p_cose)


class TestEx5Manifest20(TestEx5Manifest16):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors20[5], p_manifest20, p_cose)


class TestEx5InvManifest20(TestEx5InvManifest16):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors20[5], p_manifest20, p_cose)


class PopenTest(TestCase):
    def popen_test(self, args, input="", exp_retcode=0):
        call0 = Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE)
        stdout0, stderr0 = call0.communicate(input)
        self.assertEqual(exp_retcode, call0.returncode, stderr0.decode("utf-8"))
        return stdout0, stderr0


class TempdTest(TestCase):
    def setUp(self):
        self.tempd = Path(mkdtemp())
        return super().setUp()

    def tearDown(self):
        rmtree(self.tempd)
        self.tempd = None
        return super().tearDown()


class CLI_Test(PopenTest):
    def get_std_args(self, input, cmd="convert"):
        return [
            "zcbor",
            cmd,
            "--cddl",
            str(p_manifest12),
            "--input",
            str(input),
            "-t",
            "SUIT_Envelope_Tagged",
            "--yaml-compatibility",
        ]


class TestCLI1(CLI_Test):
    def do_testManifest(self, n):
        self.popen_test(self.get_std_args(p_test_vectors12[n], cmd="validate"), "")
        stdout0, _ = self.popen_test(
            self.get_std_args(p_test_vectors12[n]) + ["--output", "-", "--output-as", "cbor"], ""
        )

        self.popen_test(self.get_std_args("-", cmd="validate") + ["--input-as", "cbor"], stdout0)
        stdout1, _ = self.popen_test(
            self.get_std_args("-") + ["--input-as", "cbor", "--output", "-", "--output-as", "json"],
            stdout0,
        )

        self.popen_test(self.get_std_args("-", cmd="validate") + ["--input-as", "json"], stdout1)
        stdout2, _ = self.popen_test(
            self.get_std_args("-") + ["--input-as", "json", "--output", "-", "--output-as", "yaml"],
            stdout1,
        )

        self.popen_test(self.get_std_args("-", cmd="validate") + ["--input-as", "yaml"], stdout2)
        stdout3, _ = self.popen_test(
            self.get_std_args("-") + ["--input-as", "yaml", "--output", "-", "--output-as", "cbor"],
            stdout2,
        )

        self.assertEqual(stdout0, stdout3)

        self.popen_test(self.get_std_args("-", cmd="validate") + ["--input-as", "cbor"], stdout3)
        stdout4, _ = self.popen_test(
            self.get_std_args("-") + ["--input-as", "cbor", "--output", "-", "--output-as", "cborhex"],
            stdout3,
        )

        self.popen_test(self.get_std_args("-", cmd="validate") + ["--input-as", "cborhex"], stdout4)
        stdout5, _ = self.popen_test(
            self.get_std_args("-") + ["--input-as", "cborhex", "--output", "-", "--output-as", "json"],
            stdout4,
        )

        self.assertEqual(stdout1, stdout5)

        self.maxDiff = None

        with open(p_test_vectors12[n], "r", encoding="utf-8") as f:
            self.assertEqual(sub(r"\W+", "", f.read()), sub(r"\W+", "", stdout4.decode("utf-8")))

    def test_0(self):
        self.do_testManifest(0)

    def test_1(self):
        self.do_testManifest(1)

    def test_2(self):
        self.do_testManifest(2)

    def test_3(self):
        self.do_testManifest(3)

    def test_4(self):
        self.do_testManifest(4)

    def test_5(self):
        self.do_testManifest(5)

    def test_map_bstr(self):
        stdout1, _ = self.popen_test(
            [
                "zcbor",
                "convert",
                "--cddl",
                str(p_map_bstr_cddl),
                "--input",
                str(p_map_bstr_yaml),
                "-t",
                "map",
                "--yaml-compatibility",
                "--output",
                "-",
            ],
            "",
        )
        self.assertEqual(
            dumps(
                {
                    "test": bytes.fromhex("1234abcd"),
                    "test2": cbor2.CBORTag(1234, bytes.fromhex("1a2b3c4d")),
                    ("test3",): dumps(1234),
                }
            ),
            stdout1,
        )

    def test_decode_encode(self):
        _, stderr1 = self.popen_test(
            ["zcbor", "code", "--cddl", str(p_map_bstr_cddl), "-t", "map"], "", exp_retcode=2
        )
        self.assertIn(b"error: Please specify at least one of --decode or --encode", stderr1)

    def test_output_present(self):
        args = ["zcbor", "code", "--cddl", str(p_map_bstr_cddl), "-t", "map", "-d"]
        _, stderr1 = self.popen_test(args, "", exp_retcode=2)
        self.assertIn(
            b"error: Please specify both --output-c and --output-h "
            b"unless --output-cmake is specified.",
            stderr1,
        )

        _, stderr2 = self.popen_test(args + ["--output-c", "/tmp/map.c"], "", exp_retcode=2)
        self.assertIn(
            b"error: Please specify both --output-c and --output-h "
            b"unless --output-cmake is specified.",
            stderr2,
        )


class TestFileHeader(CLI_Test, TempdTest):
    def do_test_file_header(self, from_file=False):
        file_header = """Sample

file header"""
        if from_file:
            (self.tempd / "file_header.txt").write_text(file_header, encoding="utf-8")
            file_header_input = str(self.tempd / "file_header.txt")
        else:
            file_header_input = file_header

        _, __ = self.popen_test(
            [
                "zcbor",
                "code",
                "--cddl",
                str(p_pet_cddl),
                "-t",
                "Pet",
                "--output-cmake",
                str(self.tempd / "pet.cmake"),
                "-d",
                "-e",
                "--file-header",
                (file_header_input),
                "--dq",
                "5",
            ],
            "",
        )
        exp_cmake_header = f"""#
# Sample
#
# file header
#
# Generated using zcbor version {p_VERSION.read_text(encoding="utf-8")}
# https://github.com/NordicSemiconductor/zcbor
#""".splitlines()
        exp_c_header = f"""/*
 * Sample
 *
 * file header
 *
 * Generated using zcbor version {p_VERSION.read_text(encoding="utf-8")}
 * https://github.com/NordicSemiconductor/zcbor
 */""".splitlines()
        self.assertEqual(
            exp_cmake_header,
            (self.tempd / "pet.cmake").read_text(encoding="utf-8").splitlines()[:8],
        )
        for p in (
            self.tempd / "src" / "pet_decode.c",
            self.tempd / "src" / "pet_encode.c",
            self.tempd / "include" / "pet_decode.h",
            self.tempd / "include" / "pet_encode.h",
            self.tempd / "include" / "pet_types.h",
        ):
            self.assertEqual(exp_c_header, p.read_text(encoding="utf-8").splitlines()[:8])

    def test_file_header(self):
        self.do_test_file_header()
        self.do_test_file_header(from_file=True)


class TestOptional(TestCase):
    def test_optional_0(self):
        with open(p_optional, "r", encoding="utf-8") as f:
            cddl_res = zcbor.DataTranslator.from_cddl(cddl_string=f.read(), default_max_qty=16)
        cddl = cddl_res.my_types["cfg"]
        test_yaml = """
            mem_config:
                - 0
                - 5"""
        decoded = cddl.decode_str_yaml(test_yaml)
        self.assertEqual(decoded.mem_config[0].READ.union_choice, "uint0")
        self.assertEqual(decoded.mem_config[0].N, [5])


class CornerCaseTest(TestCase):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        try:
            self.cddl_res = zcbor.DataTranslator.from_cddl(
                cddl_string=p_prelude.read_text(encoding="utf-8")
                + "\n"
                + p_corner_cases.read_text(encoding="utf-8"),
                default_max_qty=16,
            )
        except zcbor.CddlParsingError as e:
            print(zcbor.format_parsing_error(e))
            raise


class TestUndefined(CornerCaseTest):
    def test_undefined_0(self):
        cddl = self.cddl_res.my_types["Simples"]
        test_yaml = "[true, false, true, null, [zcbor_undefined]]"

        decoded = cddl.decode_str_yaml(test_yaml, yaml_compat=True)
        self.assertEqual(True, decoded.boolval)

        encoded = cddl.str_to_yaml(cddl.from_yaml(test_yaml, yaml_compat=True), yaml_compat=True)
        self.assertEqual(safe_load(encoded), safe_load(test_yaml))


class TestFloat(CornerCaseTest):
    def test_float_0(self):
        cddl = self.cddl_res.my_types["Floats"]
        test_yaml = f"[3.1415, 1234567.89, 0.000123, 3.1415, 2.71828, 5.0, {1 / 3}]"

        decoded = cddl.decode_str_yaml(test_yaml)
        self.assertEqual(3.1415, decoded.float_16)
        self.assertEqual(1234567.89, decoded.float_32)
        self.assertEqual(0.000123, decoded.float_64)
        self.assertEqual(2, len(decoded.floats))
        self.assertEqual(5, decoded.floats[0])
        self.assertEqual(1 / 3, decoded.floats[1])

        encoded = cddl.str_to_yaml(cddl.from_yaml(test_yaml))
        self.assertEqual(safe_load(encoded), safe_load(test_yaml))


class TestYamlCompatibility(PopenTest):
    def test_yaml_compatibility(self):
        self.popen_test(
            [
                "zcbor",
                "validate",
                "-c",
                p_yaml_compat_cddl,
                "-i",
                p_yaml_compat_yaml,
                "-t",
                "Yaml_compatibility_example",
            ],
            exp_retcode=1,
        )
        self.popen_test(
            [
                "zcbor",
                "validate",
                "-c",
                p_yaml_compat_cddl,
                "-i",
                p_yaml_compat_yaml,
                "-t",
                "Yaml_compatibility_example",
                "--yaml-compatibility",
            ]
        )
        stdout1, _ = self.popen_test(
            [
                "zcbor",
                "convert",
                "-c",
                p_yaml_compat_cddl,
                "-i",
                p_yaml_compat_yaml,
                "-o",
                "-",
                "-t",
                "Yaml_compatibility_example",
                "--yaml-compatibility",
            ]
        )
        stdout2, _ = self.popen_test(
            [
                "zcbor",
                "convert",
                "-c",
                p_yaml_compat_cddl,
                "-i",
                "-",
                "-o",
                "-",
                "--output-as",
                "yaml",
                "-t",
                "Yaml_compatibility_example",
                "--yaml-compatibility",
            ],
            stdout1,
        )
        self.assertEqual(safe_load(stdout2), safe_load(p_yaml_compat_yaml.read_text(encoding="utf-8")))


class TestIntmax(CornerCaseTest):
    def test_intmax1(self):
        cddl = self.cddl_res.my_types["Intmax1"]
        test_yaml = f"[-128, 127, 255, -32768, 32767, 65535, -2147483648, 2147483647, 4294967295, -9223372036854775808, 9223372036854775807, 18446744073709551615]"
        decoded = cddl.decode_str_yaml(test_yaml)

    def test_intmax2(self):
        cddl = self.cddl_res.my_types["Intmax2"]
        test_yaml1 = f"[-128, 0, -32768, 0, -2147483648, 0, -9223372036854775808, 0]"
        decoded = cddl.decode_str_yaml(test_yaml1)
        self.assertEqual(decoded.INT_8, -128)
        self.assertEqual(decoded.UINT_8, 0)
        self.assertEqual(decoded.INT_16, -32768)
        self.assertEqual(decoded.UINT_16, 0)
        self.assertEqual(decoded.INT_32, -2147483648)
        self.assertEqual(decoded.UINT_32, 0)
        self.assertEqual(decoded.INT_64, -9223372036854775808)
        self.assertEqual(decoded.UINT_64, 0)

        test_yaml2 = f"[127, 255, 32767, 65535, 2147483647, 4294967295, 9223372036854775807, 18446744073709551615]"
        decoded = cddl.decode_str_yaml(test_yaml2)
        self.assertEqual(decoded.INT_8, 127)
        self.assertEqual(decoded.UINT_8, 255)
        self.assertEqual(decoded.INT_16, 32767)
        self.assertEqual(decoded.UINT_16, 65535)
        self.assertEqual(decoded.INT_32, 2147483647)
        self.assertEqual(decoded.UINT_32, 4294967295)
        self.assertEqual(decoded.INT_64, 9223372036854775807)
        self.assertEqual(decoded.UINT_64, 18446744073709551615)


class TestInvalidIdentifiers(CornerCaseTest):
    def test_invalid_identifiers0(self):
        cddl = self.cddl_res.my_types["InvalidIdentifiers"]
        test_yaml = "['1one', 2, '{[a-z]}']"
        decoded = cddl.decode_str_yaml(test_yaml)
        self.assertTrue(decoded.f_1one_tstr)
        self.assertTrue(decoded.f_)
        self.assertTrue(decoded.a_z_tstr)


class TestIntSize(PopenTest, TempdTest):
    def do_test_int_size(self, mode, defines_arg, bit_size):
        self.popen_test(
            [
                "zcbor",
                "code",
                "--cddl",
                p_corner_cases,
                "-t",
                "Intmax1",
                "Intmax2",
                "Intmax4",
                "Intmax5",
                "Intmax6",
                "DefaultInt",
                f"--{mode}",
                "--default-bit-size",
                str(bit_size),
                "--output-cmake",
                self.tempd / "intmax.cmake",
            ]
            + defines_arg
        )

        expected_output_types = [
            "int8_t Intmax2_INT_8",
            "uint8_t Intmax2_UINT_8",
            "int16_t Intmax2_INT_16",
            "uint16_t Intmax2_UINT_16",
            "int32_t Intmax2_INT_32",
            "uint32_t Intmax2_UINT_32",
            "int64_t Intmax2_INT_64",
            "uint64_t Intmax2_UINT_64",
            "int16_t Intmax6_INT_8_PLUS1",
            "int16_t Intmax6_UINT_8_PLUS1",
            "int32_t Intmax6_INT_16_PLUS1",
            "uint32_t Intmax6_UINT_16_PLUS1",
            "int64_t Intmax6_INT_32_PLUS1",
            "uint64_t Intmax6_UINT_32_PLUS1",
            f"int{bit_size}_t DefaultInt_int",
            f"uint{bit_size}_t DefaultInt_uint",
        ]

        expected_defines = {
            "INTMAX1_INT_8_MIN_VAL": "INT8_MIN",
            "INTMAX1_INT_8_MAX_VAL": "INT8_MAX",
            "INTMAX1_UINT_8_MAX_VAL": "UINT8_MAX",
            "INTMAX1_INT_16_MIN_VAL": "INT16_MIN",
            "INTMAX1_INT_16_MAX_VAL": "INT16_MAX",
            "INTMAX1_UINT_16_MAX_VAL": "UINT16_MAX",
            "INTMAX1_INT_32_MIN_VAL": "INT32_MIN",
            "INTMAX1_INT_32_MAX_VAL": "INT32_MAX",
            "INTMAX1_UINT_32_MAX_VAL": "UINT32_MAX",
            "INTMAX1_INT_64_MIN_VAL": "INT64_MIN",
            "INTMAX1_INT_64_MAX_VAL": "INT64_MAX",
            "INTMAX1_UINT_64_MAX_VAL": "UINT64_MAX",
            "INTMAX4_INT_8_MIN_VAL": "INT8_MIN",
            "INTMAX4_INT_8_MAX_VAL": "INT8_MAX",
            "INTMAX4_UINT_8_MAX_VAL": "UINT8_MAX",
            "INTMAX4_INT_16_MIN_VAL": "INT16_MIN",
            "INTMAX4_INT_16_MAX_VAL": "INT16_MAX",
            "INTMAX4_UINT_16_MAX_VAL": "UINT16_MAX",
            "INTMAX4_INT_32_MIN_VAL": "INT32_MIN",
            "INTMAX4_INT_32_MAX_VAL": "INT32_MAX",
            "INTMAX4_UINT_32_MAX_VAL": "UINT32_MAX",
            "INTMAX4_INT_64_MIN_VAL": "INT64_MIN",
            "INTMAX4_INT_64_MAX_VAL": "INT64_MAX",
            "INTMAX4_UINT_64_MAX_VAL": "UINT64_MAX",
            "INTMAX5_INT_8_MIN_PLUS1_VAL": "-129",
            "INTMAX5_INT_8_MAX_PLUS1_VAL": "128",
            "INTMAX5_UINT_8_MAX_PLUS1_VAL": "256",
            "INTMAX5_INT_16_MIN_PLUS1_VAL": "-32769",
            "INTMAX5_INT_16_MAX_PLUS1_VAL": "32768",
            "INTMAX5_UINT_16_MAX_PLUS1_VAL": "65536",
            "INTMAX5_INT_32_MIN_PLUS1_VAL": "-2147483649LL",
            "INTMAX5_INT_32_MAX_PLUS1_VAL": "2147483648",
            "INTMAX5_UINT_32_MAX_PLUS1_VAL": "4294967296ULL",
        }

        lit_func = "put" if mode == "encode" else "expect"
        lit_p_func = "encode" if mode == "encode" else "pexpect"
        result_var = "input" if mode == "encode" else "result"
        expected_output_code = [
            f"zcbor_int8_{lit_func}(state, (INTMAX1_INT_8_MIN_VAL)",
            f"zcbor_uint8_{lit_func}(state, (INTMAX1_INT_8_MAX_VAL)",
            f"zcbor_uint8_{lit_func}(state, (INTMAX1_UINT_8_MAX_VAL)",
            f"zcbor_int16_{lit_func}(state, (INTMAX1_INT_16_MIN_VAL)",
            f"zcbor_uint16_{lit_func}(state, (INTMAX1_INT_16_MAX_VAL)",
            f"zcbor_uint16_{lit_func}(state, (INTMAX1_UINT_16_MAX_VAL)",
            f"zcbor_int32_{lit_func}(state, (INTMAX1_INT_32_MIN_VAL)",
            f"zcbor_uint32_{lit_func}(state, (INTMAX1_INT_32_MAX_VAL)",
            f"zcbor_uint32_{lit_func}(state, (INTMAX1_UINT_32_MAX_VAL)",
            f"zcbor_int64_{lit_func}(state, (INTMAX1_INT_64_MIN_VAL)",
            f"zcbor_uint64_{lit_func}(state, (INTMAX1_INT_64_MAX_VAL)",
            f"zcbor_uint64_{lit_func}(state, (INTMAX1_UINT_64_MAX_VAL)",
            f"zcbor_int8_{mode}(state, (&(*{result_var}).Intmax2_INT_8)",
            f"zcbor_uint8_{mode}(state, (&(*{result_var}).Intmax2_UINT_8)",
            f"zcbor_int16_{mode}(state, (&(*{result_var}).Intmax2_INT_16)",
            f"zcbor_uint16_{mode}(state, (&(*{result_var}).Intmax2_UINT_16)",
            f"zcbor_int32_{mode}(state, (&(*{result_var}).Intmax2_INT_32)",
            f"zcbor_uint32_{mode}(state, (&(*{result_var}).Intmax2_UINT_32)",
            f"zcbor_int64_{mode}(state, (&(*{result_var}).Intmax2_INT_64)",
            f"zcbor_uint64_{mode}(state, (&(*{result_var}).Intmax2_UINT_64)",
            f"&(*{result_var}).Intmax4_INT_8_MIN_count, ZCBOR_CUSTOM_CAST_FP(zcbor_int8_{lit_p_func}), state, (&(int8_t){{INTMAX4_INT_8_MIN_VAL}})",
            f"&(*{result_var}).Intmax4_INT_8_MAX_count, ZCBOR_CUSTOM_CAST_FP(zcbor_uint8_{lit_p_func}), state, (&(uint8_t){{INTMAX4_INT_8_MAX_VAL}})",
            f"&(*{result_var}).Intmax4_UINT_8_MAX_count, ZCBOR_CUSTOM_CAST_FP(zcbor_uint8_{lit_p_func}), state, (&(uint8_t){{INTMAX4_UINT_8_MAX_VAL}})",
            f"&(*{result_var}).Intmax4_INT_16_MIN_count, ZCBOR_CUSTOM_CAST_FP(zcbor_int16_{lit_p_func}), state, (&(int16_t){{INTMAX4_INT_16_MIN_VAL}})",
            f"&(*{result_var}).Intmax4_INT_16_MAX_count, ZCBOR_CUSTOM_CAST_FP(zcbor_uint16_{lit_p_func}), state, (&(uint16_t){{INTMAX4_INT_16_MAX_VAL}})",
            f"&(*{result_var}).Intmax4_UINT_16_MAX_count, ZCBOR_CUSTOM_CAST_FP(zcbor_uint16_{lit_p_func}), state, (&(uint16_t){{INTMAX4_UINT_16_MAX_VAL}})",
            f"&(*{result_var}).Intmax4_INT_32_MIN_count, ZCBOR_CUSTOM_CAST_FP(zcbor_int32_{lit_p_func}), state, (&(int32_t){{INTMAX4_INT_32_MIN_VAL}})",
            f"&(*{result_var}).Intmax4_INT_32_MAX_count, ZCBOR_CUSTOM_CAST_FP(zcbor_uint32_{lit_p_func}), state, (&(uint32_t){{INTMAX4_INT_32_MAX_VAL}})",
            f"&(*{result_var}).Intmax4_UINT_32_MAX_count, ZCBOR_CUSTOM_CAST_FP(zcbor_uint32_{lit_p_func}), state, (&(uint32_t){{INTMAX4_UINT_32_MAX_VAL}})",
            f"&(*{result_var}).Intmax4_INT_64_MIN_count, ZCBOR_CUSTOM_CAST_FP(zcbor_int64_{lit_p_func}), state, (&(int64_t){{INTMAX4_INT_64_MIN_VAL}})",
            f"&(*{result_var}).Intmax4_INT_64_MAX_count, ZCBOR_CUSTOM_CAST_FP(zcbor_uint64_{lit_p_func}), state, (&(uint64_t){{INTMAX4_INT_64_MAX_VAL}})",
            f"&(*{result_var}).Intmax4_UINT_64_MAX_count, ZCBOR_CUSTOM_CAST_FP(zcbor_uint64_{lit_p_func}), state, (&(uint64_t){{INTMAX4_UINT_64_MAX_VAL}}),",
            f"zcbor_int16_{lit_func}(state, (INTMAX5_INT_8_MIN_PLUS1_VAL))",
            f"zcbor_uint8_{lit_func}(state, (INTMAX5_INT_8_MAX_PLUS1_VAL))",
            f"zcbor_uint16_{lit_func}(state, (INTMAX5_UINT_8_MAX_PLUS1_VAL))",
            f"zcbor_int32_{lit_func}(state, (INTMAX5_INT_16_MIN_PLUS1_VAL))",
            f"zcbor_uint16_{lit_func}(state, (INTMAX5_INT_16_MAX_PLUS1_VAL))",
            f"zcbor_uint32_{lit_func}(state, (INTMAX5_UINT_16_MAX_PLUS1_VAL))",
            f"zcbor_int64_{lit_func}(state, (INTMAX5_INT_32_MIN_PLUS1_VAL))",
            f"zcbor_uint32_{lit_func}(state, (INTMAX5_INT_32_MAX_PLUS1_VAL))",
            f"zcbor_uint64_{lit_func}(state, (INTMAX5_UINT_32_MAX_PLUS1_VAL))",
            f"zcbor_int16_{mode}(state, (&(*{result_var}).Intmax6_INT_8_PLUS1)",
            f"zcbor_int16_{mode}(state, (&(*{result_var}).Intmax6_UINT_8_PLUS1)",
            f"zcbor_int32_{mode}(state, (&(*{result_var}).Intmax6_INT_16_PLUS1)",
            f"zcbor_uint32_{mode}(state, (&(*{result_var}).Intmax6_UINT_16_PLUS1)",
            f"zcbor_int64_{mode}(state, (&(*{result_var}).Intmax6_INT_32_PLUS1)",
            f"zcbor_uint64_{mode}(state, (&(*{result_var}).Intmax6_UINT_32_PLUS1)",
            f"zcbor_int{bit_size}_{mode}(state, (&(*{result_var}).DefaultInt_int)",
            f"zcbor_uint{bit_size}_{mode}(state, (&(*{result_var}).DefaultInt_uint)",
        ]

        if defines_arg == []:
            # Create expected_output_code_lit for use when not using --defines
            # Replace usage of defined constants with their literal values
            expected_output_code_lit = []
            for e in expected_output_code:
                e2 = e
                for n, v in expected_defines.items():
                    e2 = e2.replace(n, v)
                expected_output_code_lit.append(e2)

        output_c = (self.tempd / "src" / f"intmax_{mode}.c").read_text()
        output_h = (self.tempd / "include" / f"intmax_{mode}.h").read_text()
        output_h_types = (self.tempd / "include" / "intmax_types.h").read_text()

        for e in expected_output_types:
            self.assertIn(e, output_h_types, f"Expected '{e}' in output_h_types")

        if defines_arg == []:
            for n, v in expected_defines.items():
                self.assertNotIn(f"#define {n} ({v})", output_h, f"Expected no '{e}' in output_h")
            for e in expected_output_code_lit:
                self.assertIn(e, output_c, f"Expected '{e}' in output_c")
        else:
            for n, v in expected_defines.items():
                self.assertIn(f"#define {n} ({v})", output_h, f"Expected '{e}' in output_h")
            for e in expected_output_code:
                self.assertIn(e, output_c, f"Expected '{e}' in output_c")

    def test_int_size(self):
        """Test that the correct integer types are generated for different bit sizes."""

        for mode in ("decode", "encode"):
            for defines_arg in ([], ["--defines"]):
                for bit_size in (8, 16, 32, 64):
                    self.do_test_int_size(mode, defines_arg, bit_size)


class TestCanonical(PopenTest):
    canonical_cddl = """
        Dict = {+tstr => int}
        Canonical = [
            num: float,
            map: {
                +int => tstr
            },
            cbor_bstr: bstr .cbor Dict
        ]
    """

    test_yaml = b"""
        [
            1.5,
            {2: "two", 1: "one"},
            {"zcbor_bstr": {"two": 2, "one": 1}},
        ]
    """

    # fmt: off
    expected_payload = bytes((
        0x83,
            0xfb, 0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xA2,
                0x02, 0x63, ord('t'), ord('w'), ord('o'),
                0x01, 0x63, ord('o'), ord('n'), ord('e'),
            0x4B, 0xA2,
                0x63, ord('t'), ord('w'), ord('o'), 0x02,
                0x63, ord('o'), ord('n'), ord('e'), 0x01,
        )
    )

    expected_payload_canonical = bytes((
        0x83,
            0xf9, 0x3e, 0x00,
            0xA2,
                0x01, 0x63, ord('o'), ord('n'), ord('e'),
                0x02, 0x63, ord('t'), ord('w'), ord('o'),
            0x4B, 0xA2,
                0x63, ord('o'), ord('n'), ord('e'), 0x01,
                0x63, ord('t'), ord('w'), ord('o'), 0x02,
        )
    )
    # fmt: on

    def test_canonical(self):
        cddl = zcbor.DataTranslator.from_cddl(
            cddl_string=self.canonical_cddl, default_max_qty=16
        ).my_types["Canonical"]

        out = cddl.from_yaml(self.test_yaml, yaml_compat=True)
        out_canon = cddl.from_yaml(self.test_yaml, yaml_compat=True, canonical=True)
        self.assertEqual(self.expected_payload, out)
        self.assertEqual(self.expected_payload_canonical, out_canon)

    def test_canonical_cli(self):
        # Must have delete=False because of permission error on Windows.
        # Cannot use delete_on_close because it is not present in Python 3.11 and earlier.
        try:
            with NamedTemporaryFile(mode="w", delete=False) as fc:
                fc.write(self.canonical_cddl)
                fc.flush()
                args = [
                    "zcbor",
                    "convert",
                    "-c",
                    fc.name,
                    "-i",
                    "-",
                    "--input-as",
                    "yaml",
                    "-o",
                    "-",
                    "-t",
                    "Canonical",
                    "--yaml-compatibility",
                ]
                out, _ = self.popen_test(args, self.test_yaml, exp_retcode=0)
                out_canon, _ = self.popen_test(
                    args + ["--output-canonical"], self.test_yaml, exp_retcode=0
                )

                args[args.index("yaml")] = "cborhex"
                out2, _ = self.popen_test(
                    args, bytes(self.expected_payload.hex(), encoding="utf-8"), exp_retcode=0
                )
                out_canon2, _ = self.popen_test(
                    args + ["--output-canonical"],
                    bytes(self.expected_payload.hex(), encoding="utf-8"),
                    exp_retcode=0,
                )

        finally:
            Path(fc.name).unlink()

        self.assertEqual(self.expected_payload, out)
        self.assertEqual(self.expected_payload_canonical, out_canon)
        self.assertEqual(self.expected_payload, out2)
        self.assertEqual(self.expected_payload_canonical, out_canon2)


class TestExceptions(TestCase):
    def do_test_exception(self, cddl_string):
        with self.assertRaises(zcbor.CddlParsingError) as cm:
            zcbor.CodeGenerator.from_cddl(
                cddl_string=cddl_string,
                mode="decode",
                entry_type_names=["test"],
                default_bit_size=32,
            )
        return cm.exception

    def test_exception_formatting(self):
        failing_cddl_string = 'test = [foo: .size 3 "bar"] // int'
        expected_error = """
CDDL parsing error:
Cannot have .size before type
  while parsing CDDL: '.size 3 "bar"'
  while parsing CDDL: 'foo: .size 3 "bar"'
  while parsing CDDL: '[foo: .size 3 "bar"]'
  while parsing CDDL: '[foo: .size 3 "bar"] // int'
  while parsing type test""".strip()
        self.assertEqual(
            expected_error, zcbor.format_parsing_error(self.do_test_exception(failing_cddl_string))
        )

    def test_duplicate_type(self):
        exc = self.do_test_exception("foo = 1\nfoo = 2")
        self.assertEqual("Duplicate CDDL type found: foo", str(exc))

    def test_duplicate_type(self):
        exc = self.do_test_exception("foo = 1\nfoo = 2")
        self.assertEqual("Duplicate CDDL type found: foo", str(exc))

    def test_double_type(self):
        exc = self.do_test_exception("foo = uint int")
        self.assertEqual("Cannot have two types: UINT, INT", str(exc))

    def test_default_type(self):
        exc = self.do_test_exception("foo = ?nil .default 1")
        self.assertEqual("zcbor does not support .default values for the NIL type", str(exc))

    def test_default_quant(self):
        exc = self.do_test_exception("foo = *uint .default 1")
        self.assertEqual("zcbor currently supports .default only with the ? quantifier.", str(exc))

    def test_default_type_match(self):
        exc = self.do_test_exception("foo = ?bstr .default 1")
        self.assertEqual(
            "Type of .default value does not match type of element. (BSTR != UINT)", str(exc)
        )

    def test_label(self):
        exc = self.do_test_exception("foo = uint bar:")
        self.assertEqual("Cannot have two types: UINT, OTHER", str(exc))

    def test_quantifier_placement(self):
        exc = self.do_test_exception("foo = uint +")
        self.assertEqual("Cannot have quantifier after type: +", str(exc))

    def test_size_placement(self):
        exc = self.do_test_exception("foo = .size 2 uint")
        self.assertEqual("Cannot have .size before type", str(exc))

    def test_size_type(self):
        exc = self.do_test_exception("foo = nil .size 2")
        self.assertEqual(".size cannot be applied to NIL", str(exc))

    def test_size_type(self):
        exc = self.do_test_exception("foo = nil .size 2..4")
        self.assertEqual(".size cannot be applied to NIL", str(exc))

    def test_size_value(self):
        exc = self.do_test_exception("foo = int .size 9")
        self.assertEqual("Integers must have size from 0 to 8, not 9.", str(exc))

    def test_max_size(self):
        exc = self.do_test_exception("foo = int .size 4..10")
        self.assertEqual("Integers must have size from 0 to 8, not 10.", str(exc))

    def test_cbor_bstr(self):
        exc1 = self.do_test_exception("foo = tstr .cbor int")
        exc2 = self.do_test_exception("foo = tstr .cborseq int")
        self.assertEqual(".cbor and .cborseq must be used with bstr.", str(exc1))
        self.assertEqual(".cbor and .cborseq must be used with bstr.", str(exc2))

    def test_bits_int(self):
        exc = self.do_test_exception("foo = int .bits bar")
        self.assertEqual(".bits must be used with uint.", str(exc))

    def test_duplicate_key(self):
        exc = self.do_test_exception("foo = 1 => 2 => tstr")
        self.assertEqual("Cannot have two keys: //UINT1 and //UINT2", str(exc))

    def test_group_key(self):
        exc = self.do_test_exception("foo = (1, 2) => tstr")
        self.assertEqual(
            "A key cannot be a group because it might represent more than 1 type.", str(exc)
        )

    def test_list_key(self):
        exc = self.do_test_exception("foo = [1 => tstr]")
        self.assertEqual(
            f"""LIST[   (//UINT1) => TSTR]{linesep}List member(s) cannot have key: [(//UINT1) => TSTR] pointing to []""",
            str(exc),
        )

    def test_multiple_keys(self):
        exc = self.do_test_exception("foo = 1 => (2 => tstr)")
        self.assertEqual("Element already has key.", str(exc))

    def test_unparsed(self):
        exc = self.do_test_exception("foo = bar")
        self.assertEqual("bar has not been parsed.", str(exc))

    def test_ambiguous_any(self):
        exc = self.do_test_exception("foo = [*any, 1]")
        self.assertEqual(
            "ambiguous quantity of 'any' is not supported in list, except as last element: */ANY",
            str(exc),
        )

    def test_control_group_member_type(self):
        exc = self.do_test_exception("foo = &(1, int)")
        self.assertEqual(
            """control group member INT of GROUP[   //UINT1,
        INT] must be literal positive integer.""",
            str(exc),
        )

    def test_float_size(self):
        exc = self.do_test_exception("foo = float .size 2..9")
        self.assertEqual("Floats must have 2, 4 or 8 bytes of precision.", str(exc))

    def test_default_type_mismatch(self):
        exc = self.do_test_exception("foo = ?uint .default -1")
        self.assertEqual(
            "Type of .default value does not match type of element. (UINT != NINT)", str(exc)
        )

    def test_default_type_unsupported(self):
        exc = self.do_test_exception("foo = ?[uint] .default 1")
        self.assertEqual("zcbor does not support .default values for the LIST type", str(exc))

    def test_default_union_no_match(self):
        exc = self.do_test_exception("foo = ?(10 / nint / bstr / 'hello1' / \"hello\") .default 'hello'")
        self.assertEqual(".default value does not match any member of the union.", str(exc))

    def test_double_default(self):
        exc = self.do_test_exception("foo = (?uint .default 1) .default 2")
        self.assertEqual("Element already has .default.", str(exc))

    def test_ambigouous_default(self):
        exc = self.do_test_exception("""three_to_four = (3..4)
               foo = ?int .default three_to_four""")
        self.assertEqual(".default cannot have: range.", str(exc))


class TestUnicodeEscape(TestCase):
    def test_unicode_escape0(self):
        expected = "Domino's 🁳 + ⌘"
        cddl_res = zcbor.DataTranslator.from_cddl(
            cddl_string=p_prelude.read_text(encoding="utf-8")
            + "\n"
            + p_corner_cases.read_text(encoding="utf-8"),
            default_max_qty=16,
        )
        cddl = cddl_res.my_types["UnicodeEscapeTstr"]
        test_yaml = safe_dump([expected] * 4)
        cddl.decode_str_yaml(test_yaml)

        test_json = r"""[
            "D\u006fmino's 🁳 + \u2318",
            "Domino's 🁳 + ⌘",
            "Domino's \uD83C\uDC73 + \u2318",
            "Domino's 🁳 + ⌘"
        ]"""

        cddl.from_json(test_json)


class TestFuncPointer(PopenTest, TempdTest):
    def test_func_pointer(self):
        # fmt: off
        self.popen_test(
            [
                "zcbor",
                "code",
                "--cddl", p_corner_cases,
                "-t", "NestedListMap", "NestedMapListMap", "Numbers", "Numbers2", "TaggedUnion",
                "NumberMap", "Strings", "Simple2", "Optional", "Union", "Map", "EmptyMap",
                "Level1", "Range", "ValueRange", "SingleBstr", "SingleInt", "SingleInt2",
                "Unabstracted", "QuantityRange", "DoubleMap", "Floats", "Floats2", "Floats3",
                "Prelude", "CBORBstr", "MapLength", "UnionInt1", "UnionInt2", "Intmax1", "Intmax2",
                "Intmax3", "Intmax4", "InvalidIdentifiers", "Uint64List", "BstrSize",
                "MapUnionPrimAlias", "Keywords", "EmptyContainer", "SingleElemList",
                "Choice1", "Choice2", "Choice3", "Choice4", "Choice5", "OptList",
                "-d", "-e",
                "--output-cmake", self.tempd / "fptr.cmake",
            ]
        )
        # fmt: on

        output_c_e = (self.tempd / "src" / f"fptr_encode.c").read_text()
        output_c_d = (self.tempd / "src" / f"fptr_decode.c").read_text()

        in_macro_re = {
            mode: compile(
                rf"\tbool\(\*\)\(zcbor_state_t \*\, (const )?struct (?P<arg_type>\w+) \*\): +\(\(zcbor_{mode}r_t \*\)func\), \\"
            )
            for mode in ("decode", "encode")
        }
        macro_call_re = r"ZCBOR_CUSTOM_CAST_FP\((?P<func>\w+)\)"
        paren_re = r"(?P<paren>\((?P<item>(?>[^\(\)]+|(?&paren))*)\))"
        func_cast1_re = compile(rf"{macro_call_re}, state, (?P<arg>({paren_re}|[^,\(\)]*))")
        func_cast2_re = compile(rf"{macro_call_re}, sizeof\(states\)")

        def check_file(output_c, mode):
            funcs = set()
            for line in output_c.splitlines():
                in_macro = in_macro_re[mode].fullmatch(line)
                if not in_macro:
                    self.assertFalse(
                        search(rf"\(zcbor_(en|de)coder_t *\)(?!ZCBOR_CUSTOM_CAST_FP)", line),
                        f"Unexpected cast: {line}",
                    )
                    func_cast1 = func_cast1_re.search(line)
                    func_cast2 = func_cast2_re.search(line)
                    if func_cast1 and "#define" not in line:
                        funcs.add((func_cast1.group("func"), "state", func_cast1.group("arg")))
                    if func_cast2:
                        funcs.add(
                            (
                                func_cast2.group("func"),
                                "states",
                                "result" if mode == "decode" else "input",
                            )
                        )
                    if (
                        "ZCBOR_CUSTOM_CAST_FP" in line
                        and not func_cast1
                        and not func_cast2
                        and not "#define" in line
                    ):
                        self.fail(f"Unexpected ZCBOR_CUSTOM_CAST_FP in line: {line}")
            self.assertGreater(len(funcs), 80, f"Too few functions found in {mode} output")
            for f, s, a in funcs:
                self.assertTrue(
                    search(rf"{f}\({s}, {escape(a)}\);", output_c),
                    f"Function {f}({s}, {a}); not found in {mode} output",
                )

        check_file(output_c_d, "decode")
        check_file(output_c_e, "encode")


class TestControlGroups(TestCase):
    def test_single_member(self):
        """Test that control groups with a single member don't cause an exception,

        and that they decode data correctly"""
        cddl_res = zcbor.DataTranslator.from_cddl(
            cddl_string="""
                single_member_cg = &(foo: 3)
                bar = uint .bits single_member_cg
            """,
        )
        cddl = cddl_res.my_types["bar"]
        self.assertEqual(0, cddl.decode_str_yaml("0"))
        self.assertEqual(8, cddl.decode_str_yaml("8"))
        with self.assertRaises(zcbor.CddlValidationError):
            cddl.decode_str_yaml("1")


def cp_from_cddl(cddl):
    return zcbor.CddlParser.from_cddl(cddl_string=cddl)


class TestParsingErrors(TestCase):
    def cddl_parsing_error_test(self, invalid_cddl, valid_cddl, message_regex=None):
        """Check that invalid CDDL raises an error and check that a valid CDDL variant passes."""
        self.assertTrue(cp_from_cddl(valid_cddl))
        if message_regex is not None:
            self.assertRaisesRegex(zcbor.CddlParsingError, message_regex, cp_from_cddl, invalid_cddl)
        else:
            self.assertRaises(zcbor.CddlParsingError, cp_from_cddl, invalid_cddl)

    def test_invalid_cddl(self):
        """Check that certain CDDL formatting errors are caught."""

        self.cddl_parsing_error_test(
            "foo = int .bar 1234", "foo = int .lt 1234", r"Invalid control operator: '\.bar'"
        )
        # Two values
        self.cddl_parsing_error_test("foo = 1 .eq 2", "foo = int .eq 2", r".*Attempting to set value.*")
        self.cddl_parsing_error_test(
            f"foo = int .eq uint",
            f"foo = int .eq 1000000",
            r".eq value must be unambiguous\.",
        )
        self.cddl_parsing_error_test(
            "foo = float .eq 2",
            "foo = float .eq 2.0",
            r"Type of \.eq value does not match type of element\. \(FLOAT != UINT\)",
        )

        self.cddl_parsing_error_test(
            'foo = ?int .default "hello"',
            'foo = ?tstr .default "hello"',
            r"Type of \.default value does not match type of element",
        )
        self.cddl_parsing_error_test(
            f"foo = ?int .default uint",
            f"foo = ?int .default 1000000",
            r".default must be unambiguous\.",
        )
        self.cddl_parsing_error_test(
            'foo = tstr .default "hello"',
            'foo = ?tstr .default "hello"',
            r"zcbor currently supports \.default only with the \? quantifier",
        )
        self.cddl_parsing_error_test(
            "foo = ?tstr .default bar bar = baz baz = bar",
            'foo = ?tstr .default "hello"',
            r"Circular reference detected when unpacking .default",
        )
        self.cddl_parsing_error_test(
            "foo = ?tstr .default bar bar = baz baz = bar",
            'foo = ?tstr .default "hello"',
            r"Circular reference detected when unpacking .default",
        )

        self.cddl_parsing_error_test("foo = {int}", "foo = {1 => int}", r"Missing map key")
        self.cddl_parsing_error_test(
            "bar = (int) foo = {bar}", "bar = (1 => int) foo = {bar}", r"Missing map key"
        )

        self.cddl_parsing_error_test(
            f"foo = int .size uint",
            f"foo = int .size 1",
            r".size operator must have a literal value or range\.",
        )
        self.cddl_parsing_error_test(
            "foo = int .size -1 .. 2",
            "foo = int .size 1 .. 2",
            r"Size range must be non-negative and min <= max \(got: min -1, max 2\)",
        )
        self.cddl_parsing_error_test(
            f"foo = bool .size 1", f"foo = int .size 1", r"\.size cannot be applied to BOOL"
        )
        self.cddl_parsing_error_test(
            f"foo = [] .size 1", f"foo = int .size 1", r"\.size cannot be applied to LIST"
        )
        self.cddl_parsing_error_test(
            f"foo = int .size 1 .. uint",
            f"foo = int .size 1 .. 2",
            r"Range value must be unambiguous\.",
        )
        self.cddl_parsing_error_test(
            f"foo = int .size 1..2..",
            f"foo = int .size 1..2",
            r"Must have exactly one range specifier '..'/'...': 1..2..",
        )
        self.cddl_parsing_error_test(
            f"foo = int .size 1....2",
            f"foo = int .size 1...2",
            r"Must have exactly one range specifier '..'/'...': 1....2",
        )
        self.cddl_parsing_error_test(
            f"foo = int .size -2",
            f"foo = int .size 2",
            r".size must be one of \('UINT',\), got NINT\.",
        )
        self.cddl_parsing_error_test(
            "foo = float .size 2.0",
            "foo = float .size 2",
            r".size must be one of \('UINT',\), got FLOAT\.",
        )
        self.cddl_parsing_error_test(
            "foo = ?bstr .size 0..10 .default '' .cborseq *int",
            "foo = ?bstr .size 0..10 .default ''",
            r"zcbor does not support \.default and \.cbor\(seq\) together",
        )
        self.cddl_parsing_error_test(
            "foo = ?bstr .size 0..10 .default '' .cbor *int",
            "foo = ?bstr .size 0..10 .default ''",
            r"zcbor does not support \.default and \.cbor\(seq\) together",
        )

        for ctrl_op in (".lt", ".gt", ".ge", ".le"):
            self.cddl_parsing_error_test(
                f"foo = bool {ctrl_op} 1",
                f"foo = int {ctrl_op} 1",
                r"Inequality value must be applied to a number, got BOOL",
            )
            self.cddl_parsing_error_test(
                f"foo = [] {ctrl_op} 1",
                f"foo = int {ctrl_op} 1",
                r"Inequality value must be applied to a number, got LIST",
            )
            self.cddl_parsing_error_test(
                f"foo = 1 {ctrl_op} 1",
                f"foo = int {ctrl_op} 1",
                r"Inequality value is not needed when value is known: 1",
            )
            self.cddl_parsing_error_test(
                f"foo = int {ctrl_op} 2.0",
                f"foo = int {ctrl_op} 2",
                r"Inequality value for integer must be UINT or NINT, got FLOAT\.",
            )
            self.cddl_parsing_error_test(
                f"foo = uint {ctrl_op} 2.0",
                f"foo = uint {ctrl_op} 2",
                r"Inequality value for integer must be same type \(UINT\), got FLOAT\.",
            )
            self.cddl_parsing_error_test(
                f"foo = float {ctrl_op} true",
                f"foo = float {ctrl_op} 1.0",
                r"Inequality value must be one of \('FLOAT', 'UINT', 'NINT'\), got BOOL\.",
            )
            self.cddl_parsing_error_test(
                f"foo = int {ctrl_op} uint",
                f"foo = int {ctrl_op} 1",
                r"Inequality value must be unambiguous\.",
            )
            self.cddl_parsing_error_test(
                f"foo = int {ctrl_op} 1 {ctrl_op} 2",
                f"foo = int {ctrl_op} 1",
                rf"Element already has {ctrl_op}\.",
            )
            self.cddl_parsing_error_test(
                f"""
                foo = int {ctrl_op} bar
                bar = +#6.123(3)
                """,
                f"""
                foo = int {ctrl_op} bar
                bar = 3
                """,
                r"Inequality value cannot have: (tag, quantifier|quantifier, tag)",
            )
            self.cddl_parsing_error_test(
                f"foo = int {ctrl_op} 3..4",
                f"foo = int {ctrl_op} 3",
                r"Inequality value cannot have: range.",
            )

    def test_num_range(self):
        """Check that invalid CDDL with a range of numbers raises an error."""
        self.cddl_parsing_error_test(
            "foo = 1..0", "foo = 0..1", r"Range has larger min \(1\) than max \(0\)"
        )
        self.cddl_parsing_error_test(
            "foo = int .gt 3 .size 2..9",
            "foo = int .gt 3 .size 2..4",
            "Integers must have size from 0 to 8, not 9.",
        )
        self.cddl_parsing_error_test(
            "foo = 0..1..",
            "foo = 0..1",
            r"Must have exactly one range specifier '\.\.'/'\.\.\.': 0..1..",
        )
        self.cddl_parsing_error_test(
            "foo = -1.0..2", "foo = -1.0..2.0", r"Range values must both be int or both float."
        )
        self.cddl_parsing_error_test(
            "foo = 1..2.0", "foo = 1..2", r"Range values must both be int or both float."
        )

        self.cddl_parsing_error_test(
            "foo = 2..1", "foo = 0..1", r"Range has larger min \(2\) than max \(1\)"
        )
        self.cddl_parsing_error_test(
            "foo = 3.0...3.0",
            "foo = 3.0..3.0",
            r"Range with equal min and max must be inclusive \(got 3.0, exclusive\)",
        )


class OtherTests(TestCase):
    def test_unions1(self):
        parsed = cp_from_cddl("NestedUnion = (1 // (2 // 3))").my_types["NestedUnion"]
        self.assertEqual(2, len(parsed.value))

    def test_unions2(self):
        parsed = cp_from_cddl("""NestedUnion2 = [#6.12345(bstr) // #6.23456("foo//bar")]""")
        parsed = parsed.my_types["NestedUnion2"]
        self.assertEqual(2, len(parsed.value[0].value))

    def test_ctrl_ops1(self):
        parsed = cp_from_cddl("foo = ?bstr .cbor [int, tstr] .size 0..50")
        parsed = parsed.my_types["foo"]
        self.assertEqual("BSTR", parsed.type)
        self.assertEqual(0, parsed.min_qty)
        self.assertEqual(1, parsed.max_qty)
        self.assertEqual(0, parsed.min_size)
        self.assertEqual(50, parsed.max_size)
        self.assertEqual("LIST", parsed.cbor.type)
        self.assertEqual(2, len(parsed.cbor.value))

    def test_ctrl_ops2(self):
        parsed = cp_from_cddl("foo = uint .size 2..4 .gt 3 .le 1000000").my_types["foo"]
        parsed = parsed.my_types["foo"]
        self.assertEqual("UINT", parsed.type)
        self.assertEqual(2, parsed.min_size)
        self.assertEqual(3, parsed.max_size)
        self.assertEqual(2**8, parsed.min_value)
        self.assertEqual(1000000, parsed.max_value)

    def test_default1(self):
        parsed = cp_from_cddl("foo = ?int .default 42").my_types["foo"]
        self.assertEqual("INT", parsed.type)
        self.assertEqual(42, parsed.default.value)

    def test_default2(self):
        parsed = cp_from_cddl("foo = ?bool .default false").my_types["foo"]
        self.assertEqual("BOOL", parsed.type)
        self.assertEqual(False, parsed.default.value)

    def test_default3(self):
        parsed = cp_from_cddl("foo = ?([1, 2, 3] / [*tstr]) .default [1, 2, 3]").my_types["foo"]
        self.assertEqual("UNION", parsed.type)
        self.assertEqual("LIST", parsed.value[0].type)
        self.assertEqual("LIST", parsed.default.type)
        self.assertEqual(3, len(parsed.default.value))
        self.assertEqual("UINT", parsed.default.value[2].type)
        self.assertEqual(3, parsed.default.value[2].value)

    def test_default4(self):
        # Test that these don't raise an error
        cp_from_cddl("foo = ?(false / 0) .default false").my_types["foo"]
        cp_from_cddl("foo = ?(false / 0) .default 0").my_types["foo"]

    def test_nested_map_tstr_keys1(self):
        cddl = """foo = {"map1": {"map2": uint, "map3": uint}}"""
        parsed = cp_from_cddl(cddl).my_types["foo"]
        self.assertEqual("MAP", parsed.type)
        self.assertEqual(1, len(parsed.value))
        self.assertEqual("MAP", parsed.value[0].type)
        self.assertEqual(2, len(parsed.value[0].value))
        self.assertEqual("map1", parsed.value[0].key.value)
        self.assertEqual("UINT", parsed.value[0].value[0].type)
        self.assertEqual("map2", parsed.value[0].value[0].key.value)
        self.assertEqual("UINT", parsed.value[0].value[1].type)
        self.assertEqual("map3", parsed.value[0].value[1].key.value)

    def test_nested_map_tstr_keys2(self):
        cddl = r"""foo = {"\"map1\"": {"\"map2\"": uint,}}"""
        parsed = cp_from_cddl(cddl).my_types["foo"]
        self.assertEqual("MAP", parsed.type)
        self.assertEqual(1, len(parsed.value))
        self.assertEqual("MAP", parsed.value[0].type)
        self.assertEqual(1, len(parsed.value[0].value))
        self.assertEqual('\\"map1\\"', parsed.value[0].key.value)
        self.assertEqual(6, parsed.value[0].key.size)
        self.assertEqual("UINT", parsed.value[0].value[0].type)
        self.assertEqual('\\"map2\\"', parsed.value[0].value[0].key.value)
        self.assertEqual(6, parsed.value[0].value[0].key.size)


if __name__ == "__main__":
    main()
