#!/usr/bin/env python3
#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

from unittest import TestCase, main, skipIf
from subprocess import Popen, PIPE
from re import sub
from pathlib import Path
from pprint import pprint
from ecdsa import VerifyingKey
from hashlib import sha256
import cbor2
from platform import python_version_tuple
from sys import platform, exit
from yaml import safe_load
from tempfile import mkdtemp
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
p_tests = Path(p_root, 'tests')
p_cases = Path(p_tests, 'cases')
p_manifest12 = Path(p_cases, 'manifest12.cddl')
p_manifest14 = Path(p_cases, 'manifest14.cddl')
p_manifest16 = Path(p_cases, 'manifest16.cddl')
p_manifest20 = Path(p_cases, 'manifest20.cddl')
p_test_vectors12 = tuple(Path(p_cases, f'manifest12_example{i}.cborhex') for i in range(6))
p_test_vectors14 = tuple(Path(p_cases, f'manifest14_example{i}.cborhex') for i in range(6))
p_test_vectors16 = tuple(Path(p_cases, f'manifest14_example{i}.cborhex') for i in range(6))  # Identical to manifest14.
p_test_vectors20 = tuple(Path(p_cases, f'manifest20_example{i}.cborhex') for i in range(6))
p_optional = Path(p_cases, 'optional.cddl')
p_corner_cases = Path(p_cases, 'corner_cases.cddl')
p_cose = Path(p_cases, 'cose.cddl')
p_manifest14_priv = Path(p_cases, 'manifest14.priv')
p_manifest14_pub = Path(p_cases, 'manifest14.pub')
p_map_bstr_cddl = Path(p_cases, 'map_bstr.cddl')
p_map_bstr_yaml = Path(p_cases, 'map_bstr.yaml')
p_yaml_compat_cddl = Path(p_cases, 'yaml_compatibility.cddl')
p_yaml_compat_yaml = Path(p_cases, 'yaml_compatibility.yaml')
p_pet_cddl = Path(p_cases, 'pet.cddl')
p_README = Path(p_root, 'README.md')
p_prelude = Path(p_root, 'zcbor', 'prelude.cddl')
p_VERSION = Path(p_root, 'zcbor', 'VERSION')


class TestManifest(TestCase):
    """Class for testing examples against CDDL for various versions of the SUIT manifest spec."""
    def decode_file(self, data_path, *cddl_paths):
        data = bytes.fromhex(data_path.read_text(encoding="utf-8").replace("\n", ""))
        self.decode_string(data, *cddl_paths)

    def decode_string(self, data_string, *cddl_paths):
        cddl_str = " ".join((Path(p).read_text(encoding="utf-8") for p in cddl_paths))
        self.my_types = zcbor.DataTranslator.from_cddl(cddl_str, 16).my_types
        cddl = self.my_types["SUIT_Envelope_Tagged"]
        self.decoded = cddl.decode_str(data_string)


class TestEx0Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[0], p_manifest12)

    def test_manifest_digest(self):
        self.assertEqual(
            bytes.fromhex("5c097ef64bf3bb9b494e71e1f2418eef8d466cc902f639a855ec9af3e9eddb99"),
            self.decoded.suit_authentication_wrapper.SUIT_Digest_bstr.suit_digest_bytes)

    def test_signature(self):
        self.assertEqual(
            1,
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].COSE_Sign1_Tagged_m.protected.uintint[0].uintint_key)
        self.assertEqual(
            -7,
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].COSE_Sign1_Tagged_m.protected.uintint[0].uintint)
        self.assertEqual(
            bytes.fromhex("a19fd1f23b17beed321cece7423dfb48c457b8f1f6ac83577a3c10c6773f6f3a7902376b59540920b6c5f57bac5fc8543d8f5d3d974faa2e6d03daa534b443a7"),
            self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].COSE_Sign1_Tagged_m.signature)

    def test_validate_run(self):
        self.assertEqual(
            "suit_condition_image_match_m_l",
            self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_validate[0].suit_validate.union[0].SUIT_Condition_m.union_choice)
        self.assertEqual(
            "suit_directive_run_m_l",
            self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_run[0].suit_run.union[0].SUIT_Directive_m.union_choice)

    def test_image_size(self):
        self.assertEqual(34768, self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0].SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[3].suit_parameter_image_size)


class TestEx0InvManifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def test_duplicate_type(self):
        with self.assertRaises(ValueError, msg="Duplicate CDDL type found"):
            self.decode_file(p_test_vectors12[0], p_manifest12, p_manifest12)


class TestEx1Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[1], p_manifest12)

    def test_components(self):
        self.assertEqual(
            [b'\x00'],
            self.decoded.suit_manifest.suit_common.suit_components[0][0].bstr)

    def test_uri(self):
        self.assertEqual(
            "http://example.com/file.bin",
            self.decoded.suit_manifest.SUIT_Severable_Manifest_Members.suit_install[0].suit_install.union[0].SUIT_Directive_m.suit_directive_set_parameters_m_l.map[0].suit_parameter_uri)


class TestEx2Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[2], p_manifest12)

    def test_severed_uri(self):
        self.assertEqual(
            "http://example.com/very/long/path/to/file/file.bin",
            self.decoded.SUIT_Severable_Manifest_Members.suit_install[0].suit_install.union[0].SUIT_Directive_m.suit_directive_set_parameters_m_l.map[0].suit_parameter_uri)

    def test_severed_text(self):
        self.assertIn(
            "Example 2",
            self.decoded.SUIT_Severable_Manifest_Members.suit_text[0].suit_text.SUIT_Text_Keys.suit_text_manifest_description[0])
        self.assertEqual(
            [b'\x00'],
            self.decoded.SUIT_Severable_Manifest_Members.suit_text[0].suit_text.SUIT_Component_Identifier[0].SUIT_Component_Identifier_key.bstr)
        self.assertEqual(
            "arm.com",
            self.decoded.SUIT_Severable_Manifest_Members.suit_text[0].suit_text.SUIT_Component_Identifier[0].SUIT_Component_Identifier.SUIT_Text_Component_Keys.suit_text_vendor_domain[0])
        self.assertEqual(
            "This component is a demonstration. The digest is a sample pattern, not a real one.",
            self.decoded.SUIT_Severable_Manifest_Members.suit_text[0].suit_text.SUIT_Component_Identifier[0].SUIT_Component_Identifier.SUIT_Text_Component_Keys.suit_text_component_description[0])


class TestEx3Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[3], p_manifest12)

    def test_A_B_offset(self):
        self.assertEqual(
            33792,
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[1].SUIT_Common_Commands_m.suit_directive_try_each_m_l.SUIT_Directive_Try_Each_Argument_m.SUIT_Command_Sequence_bstr[0].union[0].SUIT_Directive_m.suit_directive_override_parameters_m_l.map[0].suit_parameter_component_offset)
        self.assertEqual(
            541696,
            self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[1].SUIT_Common_Commands_m.suit_directive_try_each_m_l.SUIT_Directive_Try_Each_Argument_m.SUIT_Command_Sequence_bstr[1].union[0].SUIT_Directive_m.suit_directive_override_parameters_m_l.map[0].suit_parameter_component_offset)


class TestEx4Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[4], p_manifest12)

    def test_load_decompress(self):
        self.assertEqual(
            0,
            self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_load[0].suit_load.union[1].SUIT_Directive_m.suit_directive_set_parameters_m_l.map[3].suit_parameter_source_component)
        self.assertEqual(
            "SUIT_Compression_Algorithm_zlib_m",
            self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_load[0].suit_load.union[1].SUIT_Directive_m.suit_directive_set_parameters_m_l.map[2].suit_parameter_compression_info.suit_compression_algorithm)


class TestEx5Manifest12(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors12[5], p_manifest12)

    def test_two_image_match(self):
        self.assertEqual(
            "suit_condition_image_match_m_l",
            self.decoded.suit_manifest.SUIT_Severable_Manifest_Members.suit_install[0].suit_install.union[3].SUIT_Condition_m.union_choice)
        self.assertEqual(
            "suit_condition_image_match_m_l",
            self.decoded.suit_manifest.SUIT_Severable_Manifest_Members.suit_install[0].suit_install.union[7].SUIT_Condition_m.union_choice)


def dumps(obj):
    return cbor2.dumps(obj, canonical=True)


def loads(string):
    return cbor2.loads(string)


def make_mutable(obj):
    """Convert cbor2 6.x immutable types to mutable equivalents.

    cbor2 6.x returns frozendict and tuple for maps and arrays inside tagged
    structures. This wrapper converts them back to dict and list so the decoded
    objects can be mutated (needed by tests that modify and re-encode CBOR).
    """
    if isinstance(obj, cbor2.CBORTag):
        return cbor2.CBORTag(obj.tag, make_mutable(obj.value))
    elif isinstance(obj, (dict, zcbor.zfrozendict)):
        return {k: make_mutable(v) for k, v in obj.items()}
    elif isinstance(obj, (list, tuple)):
        return [make_mutable(v) for v in obj]
    return obj


def mutable_loads(string):
    return make_mutable(loads(string))


class TestEx0Manifest14(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.key = VerifyingKey.from_pem(p_manifest14_pub.read_text(encoding="utf-8"))

    def do_test_authentication(self):
        self.assertEqual("COSE_Sign1_Tagged_m", self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].union_choice)
        self.assertEqual(-7, self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].COSE_Sign1_Tagged_m.Headers_m.protected.header_map_bstr.Generic_Headers.uint1union[0].int)

        manifest_signature = self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].COSE_Sign1_Tagged_m.signature
        signature_header = self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].COSE_Sign1_Tagged_m.Headers_m.protected.header_map_bstr_bstr
        manifest_suit_digest = self.decoded.suit_authentication_wrapper.SUIT_Digest_bstr_bstr

        sig_struct = dumps(["Signature1", signature_header, b'', manifest_suit_digest])

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
        self.manifest_digest = bytes.fromhex("60c61d6eb7a1aaeddc49ce8157a55cff0821537eeee77a4ded44155b03045132")

    def test_structure(self):
        self.assertEqual("COSE_Sign1_Tagged_m", self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].union_choice)
        self.assertEqual(-7, self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].COSE_Sign1_Tagged_m.Headers_m.protected.header_map_bstr.Generic_Headers.uint1union[0].int)
        self.assertEqual(self.manifest_digest, self.decoded.suit_authentication_wrapper.SUIT_Digest_bstr.suit_digest_bytes)
        self.assertEqual(1, self.decoded.suit_manifest.suit_manifest_sequence_number)
        self.assertEqual(bytes.fromhex("fa6b4a53d5ad5fdfbe9de663e4d41ffe"), self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0].SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[0].suit_parameter_vendor_identifier.RFC4122_UUID_m)
        self.assertEqual(bytes.fromhex("1492af1425695e48bf429b2d51f2ab45"), self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0].SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[1].suit_parameter_class_identifier)
        self.assertEqual(bytes.fromhex("00112233445566778899aabbccddeeff0123456789abcdeffedcba9876543210"), self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0].SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[2].suit_parameter_image_digest.suit_digest_bytes)
        self.assertEqual('cose_alg_sha_256_m', self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0].SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[2].suit_parameter_image_digest.suit_digest_algorithm_id.union_choice)
        self.assertEqual(34768, self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0].SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[3].suit_parameter_image_size)
        self.assertEqual(4, len(self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0].SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map))
        self.assertEqual(15, self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[1].SUIT_Condition_m.suit_condition_vendor_identifier_m_l.SUIT_Rep_Policy_m)
        self.assertEqual(15, self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[2].SUIT_Condition_m.suit_condition_class_identifier_m_l.SUIT_Rep_Policy_m)
        self.assertEqual(3, len(self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union))
        self.assertEqual(2, len(self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0]))
        self.assertEqual(2, len(self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0].SUIT_Common_Commands_m))
        self.assertEqual(1, len(self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0].SUIT_Common_Commands_m.suit_directive_override_parameters_m_l))
        self.assertEqual(4, len(self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0].SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map))
        self.assertEqual(2, len(self.decoded.suit_manifest.suit_common.suit_common_sequence[0].suit_common_sequence.union[0].SUIT_Common_Commands_m.suit_directive_override_parameters_m_l.map[0]))

    def test_cbor_pen(self):
        data = bytes.fromhex(p_test_vectors14[1].read_text(encoding="utf-8").replace("\n", ""))
        struct = mutable_loads(data)
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
        struct = mutable_loads(data)
        struct2 = loads(struct.value[2])  # authentication
        struct3 = cbor2.CBORTag(99999, loads(struct2[1]).value)  # invalid tag for COSE_Sign1
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
        struct = mutable_loads(data)
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
        struct = mutable_loads(data)
        struct.value[23] = b''  # Invalid integrated payload key
        data = dumps(struct)
        try:
            self.decode_string(data, p_manifest14, p_cose)
        except (zcbor.CddlValidationError, cbor2.CBORDecodeEOF) as e:
            return
        else:
            assert False, "Should have failed validation"

    def test_inv3(self):
        data = bytes.fromhex(p_test_vectors14[1].read_text(encoding="utf-8").replace("\n", ""))
        struct = mutable_loads(data)
        struct2 = loads(struct.value[3])  # manifest
        struct3 = loads(struct2[3])  # common sequence
        struct4 = loads(struct3[4])  # override params
        self.assertEqual(struct4[0], 20)
        self.assertTrue(isinstance(struct4[1][1], bytes))
        struct4[1][1] += b'x'  # vendor ID: wrong length
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
            bytes.fromhex('2bfc4d0cc6680be7dd9f5ca30aa2bb5d1998145de33d54101b80e2ca49faf918'),
            self.decoded.suit_manifest.SUIT_Severable_Members_Choice.suit_text[0].SUIT_Digest_m.suit_digest_bytes)
        self.assertEqual(
            bytes.fromhex('2bfc4d0cc6680be7dd9f5ca30aa2bb5d1998145de33d54101b80e2ca49faf918'),
            sha256(dumps(self.decoded.SUIT_Severable_Manifest_Members.suit_text[0].suit_text_bstr)).digest())
        self.assertEqual('arm.com', self.decoded.SUIT_Severable_Manifest_Members.suit_text[0].suit_text.SUIT_Component_Identifier[0].SUIT_Component_Identifier.SUIT_Text_Component_Keys.suit_text_vendor_domain[0])
        self.assertEqual('This component is a demonstration. The digest is a sample pattern, not a real one.', self.decoded.SUIT_Severable_Manifest_Members.suit_text[0].suit_text.SUIT_Component_Identifier[0].SUIT_Component_Identifier.SUIT_Text_Component_Keys.suit_text_component_description[0])

        # Check manifest description. The concatenation and .replace() call are there to add
        # trailing whitespace to all blank lines except the first.
        # This is done in this way to avoid editors automatically removing the whitespace.
        self.assertEqual('''## Example 2: Simultaneous Download, Installation, Secure Boot, Severed Fields
''' + '''
    This example covers the following templates:

    * Compatibility Check ({{template-compatibility-check}})
    * Secure Boot ({{template-secure-boot}})
    * Firmware Download ({{firmware-download-template}})

    This example also demonstrates severable elements ({{ovr-severable}}), and text ({{manifest-digest-text}}).'''.replace("\n\n", "\n    \n"), self.decoded.SUIT_Severable_Manifest_Members.suit_text[0].suit_text.SUIT_Text_Keys.suit_text_manifest_description[0])


class TestEx3Manifest14(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors14[3], p_manifest14, p_cose)
        self.slots = (33792, 541696)

    def test_try_each(self):
        self.assertEqual(2, len(self.decoded.suit_manifest.SUIT_Severable_Members_Choice.suit_install[0].SUIT_Command_Sequence_bstr.union[0].SUIT_Directive_m.suit_directive_try_each_m_l.SUIT_Directive_Try_Each_Argument_m.SUIT_Command_Sequence_bstr))
        self.assertEqual(self.slots[0], self.decoded.suit_manifest.SUIT_Severable_Members_Choice.suit_install[0].SUIT_Command_Sequence_bstr.union[0].SUIT_Directive_m.suit_directive_try_each_m_l.SUIT_Directive_Try_Each_Argument_m.SUIT_Command_Sequence_bstr[0].union[0].SUIT_Directive_m.suit_directive_override_parameters_m_l.map[0].suit_parameter_component_slot)
        self.assertEqual(self.slots[1], self.decoded.suit_manifest.SUIT_Severable_Members_Choice.suit_install[0].SUIT_Command_Sequence_bstr.union[0].SUIT_Directive_m.suit_directive_try_each_m_l.SUIT_Directive_Try_Each_Argument_m.SUIT_Command_Sequence_bstr[1].union[0].SUIT_Directive_m.suit_directive_override_parameters_m_l.map[0].suit_parameter_component_slot)


class TestEx4Manifest14(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors14[4], p_manifest14, p_cose)

    def test_components(self):
        self.assertEqual(3, len(self.decoded.suit_manifest.suit_common.suit_components[0]))
        self.assertEqual(b'\x00', self.decoded.suit_manifest.suit_common.suit_components[0][0].bstr[0])
        self.assertEqual(b'\x02', self.decoded.suit_manifest.suit_common.suit_components[0][1].bstr[0])
        self.assertEqual(b'\x01', self.decoded.suit_manifest.suit_common.suit_components[0][2].bstr[0])


class TestEx5Manifest14(TestManifest):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.decode_file(p_test_vectors14[5], p_manifest14, p_cose)

    def test_validate(self):
        self.assertEqual(4, len(self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_validate[0].suit_validate.union))
        self.assertEqual(15, self.decoded.suit_manifest.SUIT_Unseverable_Members.suit_validate[0].suit_validate.union[1].SUIT_Condition_m.suit_condition_image_match_m_l.SUIT_Rep_Policy_m)


class TestEx5InvManifest14(TestManifest):
    def test_invalid_rep_policy(self):
        data = bytes.fromhex(p_test_vectors14[5].read_text(encoding="utf-8").replace("\n", ""))
        struct = mutable_loads(data)
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
        self.manifest_digest = bytes.fromhex("ef14b7091e8adae8aa3bb6fca1d64fb37e19dcf8b35714cfdddc5968c80ff50e")

    def test_structure(self):
        self.assertEqual("COSE_Sign1_Tagged_m", self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].union_choice)
        self.assertEqual(-7, self.decoded.suit_authentication_wrapper.SUIT_Authentication_Block_bstr[0].COSE_Sign1_Tagged_m.Headers_m.protected.header_map_bstr.Generic_Headers.uint1union[0].int)
        self.assertEqual(self.manifest_digest, self.decoded.suit_authentication_wrapper.SUIT_Digest_bstr.suit_digest_bytes)
        self.assertEqual(1, self.decoded.suit_manifest.suit_manifest_sequence_number)
        self.assertEqual(bytes.fromhex("fa6b4a53d5ad5fdfbe9de663e4d41ffe"), self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[0].SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[0].suit_parameter_vendor_identifier.RFC4122_UUID_m)
        self.assertEqual(bytes.fromhex("1492af1425695e48bf429b2d51f2ab45"), self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[0].SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[1].suit_parameter_class_identifier)
        self.assertEqual(bytes.fromhex("00112233445566778899aabbccddeeff0123456789abcdeffedcba9876543210"), self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[0].SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[2].suit_parameter_image_digest.suit_digest_bytes)
        self.assertEqual('cose_alg_sha_256_m', self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[0].SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[2].suit_parameter_image_digest.suit_digest_algorithm_id.union_choice)
        self.assertEqual(34768, self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[0].SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[3].suit_parameter_image_size)
        self.assertEqual(4, len(self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[0].SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map))
        self.assertEqual(15, self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[1].SUIT_Condition_m.suit_condition_vendor_identifier_m_l.SUIT_Rep_Policy_m)
        self.assertEqual(15, self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[2].SUIT_Condition_m.suit_condition_class_identifier_m_l.SUIT_Rep_Policy_m)
        self.assertEqual(3, len(self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union))
        self.assertEqual(2, len(self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[0]))
        self.assertEqual(2, len(self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[0].SUIT_Shared_Commands_m))
        self.assertEqual(1, len(self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[0].SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l))
        self.assertEqual(4, len(self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[0].SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map))
        self.assertEqual(2, len(self.decoded.suit_manifest.suit_common.suit_shared_sequence[0].suit_shared_sequence.union[0].SUIT_Shared_Commands_m.suit_directive_override_parameters_m_l.map[0]))


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
        self.assertEqual(exp_retcode, call0.returncode, stderr0.decode('utf-8'))
        return stdout0, stderr0


class TestCLI(PopenTest):
    def get_std_args(self, input, cmd="convert"):
        return ["zcbor", cmd, "--cddl", str(p_manifest12), "--input", str(input), "-t", "SUIT_Envelope_Tagged", "--yaml-compatibility"]

    def do_testManifest(self, n):
        self.popen_test(self.get_std_args(p_test_vectors12[n], cmd="validate"), "")
        stdout0, _ = self.popen_test(self.get_std_args(p_test_vectors12[n]) + ["--output", "-", "--output-as", "cbor"], "")

        self.popen_test(self.get_std_args("-", cmd="validate") + ["--input-as", "cbor"], stdout0)
        stdout1, _ = self.popen_test(self.get_std_args("-") + ["--input-as", "cbor", "--output", "-", "--output-as", "json"], stdout0)

        self.popen_test(self.get_std_args("-", cmd="validate") + ["--input-as", "json"], stdout1)
        stdout2, _ = self.popen_test(self.get_std_args("-") + ["--input-as", "json", "--output", "-", "--output-as", "yaml"], stdout1)

        self.popen_test(self.get_std_args("-", cmd="validate") + ["--input-as", "yaml"], stdout2)
        stdout3, _ = self.popen_test(self.get_std_args("-") + ["--input-as", "yaml", "--output", "-", "--output-as", "cbor"], stdout2)

        self.assertEqual(stdout0, stdout3)

        self.popen_test(self.get_std_args("-", cmd="validate") + ["--input-as", "cbor"], stdout3)
        stdout4, _ = self.popen_test(self.get_std_args("-") + ["--input-as", "cbor", "--output", "-", "--output-as", "cborhex"], stdout3)

        self.popen_test(self.get_std_args("-", cmd="validate") + ["--input-as", "cborhex"], stdout4)
        stdout5, _ = self.popen_test(self.get_std_args("-") + ["--input-as", "cborhex", "--output", "-", "--output-as", "json"], stdout4)

        self.assertEqual(stdout1, stdout5)

        self.maxDiff = None

        with open(p_test_vectors12[n], 'r', encoding="utf-8") as f:
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
        stdout1, _ = self.popen_test(["zcbor", "convert", "--cddl", str(p_map_bstr_cddl), "--input", str(p_map_bstr_yaml), "-t", "map", "--yaml-compatibility", "--output", "-"], "")
        self.assertEqual(dumps({"test": bytes.fromhex("1234abcd"), "test2": cbor2.CBORTag(1234, bytes.fromhex("1a2b3c4d")), ("test3",): dumps(1234)}), stdout1)

    def test_decode_encode(self):
        _, stderr1 = self.popen_test(["zcbor", "code", "--cddl", str(p_map_bstr_cddl), "-t", "map"], "", exp_retcode=2)
        self.assertIn(b"error: Please specify at least one of --decode or --encode", stderr1)

    def test_output_present(self):
        args = ["zcbor", "code", "--cddl", str(p_map_bstr_cddl), "-t", "map", "-d"]
        _, stderr1 = self.popen_test(args, "", exp_retcode=2)
        self.assertIn(
            b"error: Please specify both --output-c and --output-h "
            b"unless --output-cmake is specified.",
            stderr1)

        _, stderr2 = self.popen_test(args + ["--output-c", "/tmp/map.c"], "", exp_retcode=2)
        self.assertIn(
            b"error: Please specify both --output-c and --output-h "
            b"unless --output-cmake is specified.",
            stderr2)

    def do_test_file_header(self, from_file=False):
        tempd = Path(mkdtemp())
        file_header = """Sample

file header"""
        if from_file:
            (tempd / "file_header.txt").write_text(file_header, encoding="utf-8")
            file_header_input = str(tempd / "file_header.txt")
        else:
            file_header_input = file_header

        _, __ = self.popen_test(["zcbor", "code", "--cddl", str(p_pet_cddl), "-t", "Pet", "--output-cmake", str(tempd / "pet.cmake"), "-d", "-e", "--file-header", (file_header_input), "--dq", "5"], "")
        exp_cmake_header = f"""#
# Sample
#
# file header
#
# Generated using zcbor version {p_VERSION.read_text(encoding="utf-8")}
# https://github.com/NordicSemiconductor/zcbor
# Generated with a --default-max-qty of 5
#""".splitlines()
        exp_c_header = f"""/*
 * Sample
 *
 * file header
 *
 * Generated using zcbor version {p_VERSION.read_text(encoding="utf-8")}
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 5
 */""".splitlines()
        self.assertEqual(exp_cmake_header, (tempd / "pet.cmake").read_text(encoding="utf-8").splitlines()[:9])
        for p in (tempd / "src" / "pet_decode.c", tempd / "src" / "pet_encode.c",
                  tempd / "include" / "pet_decode.h", tempd / "include" / "pet_encode.h",
                  tempd / "include" / "pet_types.h"):
            self.assertEqual(exp_c_header, p.read_text(encoding="utf-8").splitlines()[:9])
        rmtree(tempd)

    def test_file_header(self):
        self.do_test_file_header()
        self.do_test_file_header(from_file=True)


class TestOptional(TestCase):
    def test_optional_0(self):
        with open(p_optional, 'r', encoding="utf-8") as f:
            cddl_res = zcbor.DataTranslator.from_cddl(f.read(), 16)
        cddl = cddl_res.my_types['cfg']
        test_yaml = """
            mem_config:
                - 0
                - 5"""
        decoded = cddl.decode_str_yaml(test_yaml)
        self.assertEqual(decoded.mem_config[0].READ.union_choice, "uint0")
        self.assertEqual(decoded.mem_config[0].N, [5])


class TestUndefined(TestCase):
    def test_undefined_0(self):
        cddl_res = zcbor.DataTranslator.from_cddl(
            p_prelude.read_text(encoding="utf-8") + '\n' + p_corner_cases.read_text(encoding="utf-8"), 16)
        cddl = cddl_res.my_types['Simples']
        test_yaml = "[true, false, true, null, [zcbor_undefined]]"

        decoded = cddl.decode_str_yaml(test_yaml, yaml_compat=True)
        self.assertEqual(True, decoded.boolval)

        encoded = cddl.str_to_yaml(cddl.from_yaml(test_yaml, yaml_compat=True), yaml_compat=True)
        self.assertEqual(safe_load(encoded), safe_load(test_yaml))


class TestFloat(TestCase):
    def test_float_0(self):
        cddl_res = zcbor.DataTranslator.from_cddl(
            p_prelude.read_text(encoding="utf-8") + '\n' + p_corner_cases.read_text(encoding="utf-8"), 16)
        cddl = cddl_res.my_types['Floats']
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
        self.popen_test(["zcbor", "validate", "-c", p_yaml_compat_cddl, "-i", p_yaml_compat_yaml, "-t", "Yaml_compatibility_example"], exp_retcode=1)
        self.popen_test(["zcbor", "validate", "-c", p_yaml_compat_cddl, "-i", p_yaml_compat_yaml, "-t", "Yaml_compatibility_example", "--yaml-compatibility"])
        stdout1, _ = self.popen_test(["zcbor", "convert", "-c", p_yaml_compat_cddl, "-i", p_yaml_compat_yaml, "-o", "-", "-t", "Yaml_compatibility_example", "--yaml-compatibility"])
        stdout2, _ = self.popen_test(["zcbor", "convert", "-c", p_yaml_compat_cddl, "-i", "-", "-o", "-", "--output-as", "yaml", "-t", "Yaml_compatibility_example", "--yaml-compatibility"], stdout1)
        self.assertEqual(safe_load(stdout2), safe_load(p_yaml_compat_yaml.read_text(encoding="utf-8")))


class TestIntmax(TestCase):
    def test_intmax1(self):
        cddl_res = zcbor.DataTranslator.from_cddl(
            p_prelude.read_text(encoding="utf-8") + '\n' + p_corner_cases.read_text(encoding="utf-8"), 16)
        cddl = cddl_res.my_types['Intmax1']
        test_yaml = f"[-128, 127, 255, -32768, 32767, 65535, -2147483648, 2147483647, 4294967295, -9223372036854775808, 9223372036854775807, 18446744073709551615]"
        decoded = cddl.decode_str_yaml(test_yaml)

    def test_intmax2(self):
        cddl_res = zcbor.DataTranslator.from_cddl(
            p_prelude.read_text(encoding="utf-8") + '\n' + p_corner_cases.read_text(encoding="utf-8"), 16)
        cddl = cddl_res.my_types['Intmax2']
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


class TestInvalidIdentifiers(TestCase):
    def test_invalid_identifiers0(self):
        cddl_res = zcbor.DataTranslator.from_cddl(
            p_prelude.read_text(encoding="utf-8") + '\n' + p_corner_cases.read_text(encoding="utf-8"), 16)
        cddl = cddl_res.my_types['InvalidIdentifiers']
        test_yaml = "['1one', 2, '{[a-z]}']"
        decoded = cddl.decode_str_yaml(test_yaml)
        self.assertTrue(decoded.f_1one_tstr)
        self.assertTrue(decoded.f_)
        self.assertTrue(decoded.a_z_tstr)


if __name__ == "__main__":
    main()
