#!/usr/bin/env python3
#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

from regex import compile, S, M
from pprint import pformat, pprint
from os import path, linesep, makedirs
from collections import defaultdict, namedtuple
from collections.abc import Hashable, Mapping
from typing import NamedTuple
from argparse import ArgumentParser, ArgumentTypeError, RawDescriptionHelpFormatter, FileType
from datetime import datetime
from copy import copy
from itertools import tee, chain
from cbor2 import (loads, dumps, CBORTag, CBORDecoder, CBORDecodeError, CBORDecodeEOF, undefined,
                   CBORSimpleValue)
import cbor2
import builtins

if hasattr(cbor2, "frozendict"):
    from cbor2 import frozendict as zfrozendict
elif hasattr(cbor2, "FrozenDict"):
    from cbor2 import FrozenDict as zfrozendict
elif hasattr(builtins, "frozendict"):
    from builtins import frozendict as zfrozendict
else:
    assert False, "Couldn't find a frozendict implementation."

from yaml import safe_load as yaml_load, dump as yaml_dump
from json import loads as json_load, dumps as json_dump
from io import BytesIO
from subprocess import Popen, PIPE
from pathlib import Path, PurePath, PurePosixPath
from shutil import copyfile
import sys
from site import USER_BASE
from textwrap import wrap, indent
from importlib.metadata import version

regex_cache = {}
indentation = "\t"
newl_ind = "\n" + indentation

SCRIPT_PATH = Path(__file__).absolute().parent
PACKAGE_PATH = Path(__file__).absolute().parents[1]
PRELUDE_PATH = SCRIPT_PATH / "prelude.cddl"
VERSION_PATH = SCRIPT_PATH / "VERSION"
C_SRC_PATH = PACKAGE_PATH / "src"
C_INCLUDE_PATH = PACKAGE_PATH / "include"

__version__ = VERSION_PATH.read_text(encoding="utf-8").strip()

UINT8_MAX = 0xFF
UINT16_MAX = 0xFFFF
UINT32_MAX = 0xFFFFFFFF
UINT64_MAX = 0xFFFFFFFFFFFFFFFF

INT8_MAX = 0x7F
INT16_MAX = 0x7FFF
INT32_MAX = 0x7FFFFFFF
INT64_MAX = 0x7FFFFFFFFFFFFFFF

INT8_MIN = -0x80
INT16_MIN = -0x8000
INT32_MIN = -0x80000000
INT64_MIN = -0x8000000000000000


def getrp(pattern, flags=0):
    """Get a compiled regex pattern from the cache. Add it to the cache if not present."""
    pattern_key = pattern if not flags else (pattern, flags)
    if pattern_key not in regex_cache:
        regex_cache[pattern_key] = compile(pattern, flags)
    return regex_cache[pattern_key]


def sizeof(num):
    """Size of "additional" field if num is encoded as int"""
    if num <= 23:
        return 0
    elif num <= UINT8_MAX:
        return 1
    elif num <= UINT16_MAX:
        return 2
    elif num <= UINT32_MAX:
        return 4
    elif num <= UINT64_MAX:
        return 8
    else:
        raise ValueError("Number too large (more than 64 bits).")


def verbose_print(verbose_flag, *things):
    """Print only if verbose"""
    if verbose_flag:
        print(*things)


def verbose_pprint(verbose_flag, *things):
    """Pretty print only if verbose"""
    if verbose_flag:
        pprint(*things)


global_counter = 0


def counter(reset=False):
    """Retrieve a unique id."""
    global global_counter
    if reset:
        global_counter = 0
        return global_counter
    global_counter += 1
    return global_counter


def strict_load(f):
    """Strict variety of cbor2.load that ensures all data is consumed by a single CBOR element."""
    dec = CBORDecoder(f)
    obj = dec.decode()  # Raises CBORDecodeEOF or CBORDecodeError on failure.
    try:
        dec.read(1)
    except CBORDecodeEOF:
        return obj
    else:
        raise CBORDecodeError("Data not fully decoded.")


def strict_loads(b):
    """Strict variety of cbor2.loads that ensures all data is consumed."""
    f = BytesIO(b)
    return strict_load(f)


def list_replace_if_not_null(lst, i, r):
    """Replace an element in a list or tuple and return the list."""
    if lst[i] == "NULL":
        return lst
    if isinstance(lst, tuple):
        convert = tuple
        lst = list(lst)
    else:
        assert isinstance(lst, list)
        convert = list
    lst[i] = r
    return convert(lst)


def val_or_null(value, var_name):
    """Return a code snippet that assigns to and the returns a variable

    Return a code snippet that assigns the value to a variable var_name and
    returns pointer to the variable, or returns NULL if the value is None.
     """
    return "(%s = %d, &%s)" % (var_name, value, var_name) if value is not None else "NULL"


def tmp_str_or_null(value):
    """Assign the min_value variable."""
    value_str = f'"{value}"' if value is not None else 'NULL'
    len_str = f"""sizeof({f'"{value}"'}) - 1, &tmp_str)"""
    return f"(tmp_str.value = (uint8_t *){value_str}, tmp_str.len = {len_str}"


def min_bool_or_null(value):
    """Assign the max_value variable."""
    return f"(&(bool){{{int(value)}}})"


def deref_if_not_null(access):
    return access if access == "NULL" else "&" + access


def xcode_args(res, *sargs):
    """Return an argument list for a function call to a encoder/decoder function."""
    if len(sargs) > 0:
        return "state, %s, %s, %s" % (
            "&(%s)" % res if res != "NULL" else res, sargs[0], sargs[1])
    else:
        return "state, %s" % (
            "(%s)" % res if res != "NULL" else res)


def xcode_statement(func, *sargs, **kwargs):
    """Return the code that calls a encoder/decoder function with a given arguments."""
    if func is None:
        return "1"
    return "(%s(%s))" % (func, xcode_args(*sargs, **kwargs))


def add_semicolon(decl):
    if len(decl) != 0 and decl[-1][-1] != ";":
        decl[-1] += ";"
    return decl


def struct_ptr_name(mode):
    """Return the name of the struct argument for a given mode."""
    return "result" if mode == "decode" else "input"


def ternary_if_chain(access, names, xcode_strings):
    return "((%s == %s) ? %s%s: %s)" % (
        access,
        names[0],
        xcode_strings[0],
        newl_ind,
        ternary_if_chain(access, names[1:], xcode_strings[1:]) if len(names) > 1 else "false")


val_conversions = {
    (2**64) - 1: "UINT64_MAX",
    (2**63) - 1: "INT64_MAX",
    (2**32) - 1: "UINT32_MAX",
    (2**31) - 1: "INT32_MAX",
    (2**16) - 1: "UINT16_MAX",
    (2**15) - 1: "INT16_MAX",
    (2**8) - 1: "UINT8_MAX",
    (2**7) - 1: "INT8_MAX",
    -(2**63): "INT64_MIN",
    -(2**31): "INT32_MIN",
    -(2**15): "INT16_MIN",
    -(2**7): "INT8_MIN",
}


def val_to_str(val):
    if isinstance(val, bool):
        return str(val).lower()
    elif isinstance(val, Hashable) and val in val_conversions:
        return val_conversions[val]
    return str(val)


class CddlParser:
    """Class for parsing CDDL.

    One instance represents one CBOR data item, with a few caveats:
    - For repeated data, one instance represents all repetitions.
    - For "OTHER" types, one instance points to another type definition.
    - For "GROUP" and "UNION" types, there is no separate data item for the instance.
    """
    def __init__(self, default_max_qty, my_types, my_control_groups, base_name=None,
                 short_names=False, base_stem=''):
        self.id_prefix = "temp_" + str(counter())
        self.id_num = None  # Unique ID number. Only populated if needed.
        # The value of the data item. Has different meaning for different
        # types.
        self.value = None
        self.max_value = None  # Maximum value. Only used for numbers and bools.
        self.min_value = None  # Minimum value. Only used for numbers and bools.
        # The readable label associated with the element in the CDDL.
        self.label = None
        self.min_qty = 1  # The minimum number of times this element is repeated.
        self.max_qty = 1  # The maximum number of times this element is repeated.
        # The size of the element. Only used for integers, byte strings, and
        # text strings.
        self.size = None
        self.min_size = None  # Minimum size.
        self.max_size = None  # Maximum size.
        # Key element. Only for children of "MAP" elements. self.key is of the
        # same class as self.
        self.key = None
        # The element specified via.cbor or.cborseq(only for byte
        # strings).self.cbor is of the same class as self.
        self.cbor = None
        # Any tags (type 6) to precede the element.
        self.tags = []
        # The CDDL string used to determine the min_qty and max_qty. Not used after
        # min_qty and max_qty are determined.
        self.quantifier = None
        # Sockets are types starting with "$" or "$$". Do not fail if these aren't defined.
        self.is_socket = False
        # If the type has a ".bits <group_name>", this will contain <group_name> which can be looked
        # up in my_control_groups.
        self.bits = None
        # The "type" of the element. This follows the CBOR types loosely, but are more related to
        # CDDL concepts. The possible types are "INT", "UINT", "NINT", "FLOAT", "BSTR", "TSTR",
        # "BOOL", "NIL", "UNDEF", "LIST", "MAP","GROUP", "UNION" and "OTHER". "OTHER" represents a
        # CDDL type defined with '='.
        self.type = None
        self.match_str = ""
        self.errors = list()

        self.my_types = my_types
        self.my_control_groups = my_control_groups
        self.default_max_qty = default_max_qty  # args.default_max_qty
        self.base_name = base_name  # Used as default for self.get_base_name()
        # Stem which can be used when generating an id.
        self.base_stem = base_stem.replace("-", "_")
        self.short_names = short_names

        if type(self) not in type(self).cddl_regexes:
            self.cddl_regexes_init()

    @classmethod
    def from_cddl(cddl_class, cddl_string, default_max_qty, *args, **kwargs):
        my_types = dict()

        type_strings = cddl_class.get_types(cddl_string)
        # Separate type_strings as keys in two dicts, one dict for strings that start with &( which
        # are special control operators for .bits, and one dict for all the regular types.
        my_types = \
            {my_type: None for my_type, val in type_strings.items() if not val.startswith("&(")}
        my_control_groups = \
            {my_cg: None for my_cg, val in type_strings.items() if val.startswith("&(")}

        # Parse the definitions, replacing the each string with a
        # CodeGenerator instance.
        for my_type, cddl_string in type_strings.items():
            parsed = cddl_class(*args, default_max_qty, my_types, my_control_groups, **kwargs,
                                base_stem=my_type)
            parsed.get_value(cddl_string.replace("\n", " ").lstrip("&"))
            parsed = parsed.flatten()[0]
            if my_type in my_types:
                my_types[my_type] = parsed
            elif my_type in my_control_groups:
                my_control_groups[my_type] = parsed

        counter(True)

        # post_validate all the definitions.
        for my_type in my_types:
            my_types[my_type].set_id_prefix()
            my_types[my_type].post_validate()
            my_types[my_type].set_base_names()
        for my_control_group in my_control_groups:
            my_control_groups[my_control_group].set_id_prefix()
            my_control_groups[my_control_group].post_validate_control_group()

        return CddlTypes(my_types, my_control_groups)

    @staticmethod
    def strip_comments(instr):
        """Strip CDDL comments (';') from the string."""
        return getrp(r"\;.*?(\n|$)").sub('', instr)

    @staticmethod
    def resolve_backslashes(instr):
        """Replace escaped newlines with spaces."""
        return getrp(r"\\\n").sub(" ", instr)

    @classmethod
    def get_types(cls, cddl_string):
        """Returns a dict containing multiple typename=>string"""
        instr = cls.strip_comments(cddl_string)
        instr = cls.resolve_backslashes(instr)
        type_regex = \
            r"(\s*?\$?\$?([\w-]+)\s*(\/{0,2})=\s*(.*?)(?=(\Z|\s*\$?\$?[\w-]+\s*\/{0,2}=(?!\>))))"
        result = defaultdict(lambda: "")
        types = [
            (key, value, slashes)
            for (_1, key, slashes, value, _2) in getrp(type_regex, S | M).findall(instr)]
        for key, value, slashes in types:
            if slashes:
                result[key] += slashes
                result[key] += value
                result[key] = result[key].lstrip(slashes)  # strip from front
            else:
                if key in result:
                    raise ValueError(f"Duplicate CDDL type found: {key}")
                result[key] = value
        return dict(result)

    backslash_quotation_mark = r'\"'

    def generate_base_name(self):
        """Generate a (hopefully) unique and descriptive name"""
        byte_multi = (8 if self.type in ["INT", "UINT", "NINT", "FLOAT"] else 1)

        # The first non-None entry is used:
        raw_name = ((
            # The label is the default if present:
            self.label
            # Name a key/value pair by its key type or string value:
            or (self.key.value if self.key and self.key.type in ["TSTR", "OTHER"] else None)
            # Name a string by its expected value:
            or (f"{self.value.replace(self.backslash_quotation_mark, '')}_{self.type.lower()}"
                if self.type == "TSTR" and self.value is not None else None)
            # Name an integer by its expected value:
            or (f"{self.type.lower()}{abs(self.value)}"
                if self.type in ["INT", "UINT", "NINT"] and self.value is not None else None)
            # Name a type by its type name
            or (next((key for key, value in self.my_types.items() if value == self), None))
            # Name a control group by its name
            or (next((key for key, value in self.my_control_groups.items() if value == self), None))
            # Name an instance by its type:
            or (self.value + "_m" if self.type == "OTHER" else None)
            # Name a list by its first element:
            or (self.value[0].get_base_name() + "_l"
                if self.type in ["LIST", "GROUP"] and self.value else None)
            # Name a cbor-encoded bstr by its expected cbor contents:
            or ((self.cbor.value + "_bstr")
                if self.cbor and self.cbor.type in ["TSTR", "OTHER"] else None)
            # Name a key value pair by its key (regardless of the key type)
            or ((self.key.generate_base_name() + self.type.lower()) if self.key else None)
            # Name an element by its minimum/maximum "size" (if the min == the max)
            or (f"{self.type.lower()}{self.min_size * byte_multi}"
                if (self.min_size is not None) and self.min_size == self.max_size else None)
            # Name an element by its minimum/maximum "size" (if the min != the max)
            or (f"{self.type.lower()}{self.min_size * byte_multi}-{self.max_size * byte_multi}"
                if (self.min_size is not None) and (self.max_size is not None) else None)
            # Name an element by its type.
            or self.type.lower()).replace("-", "_"))

        # Make the name compatible with C variable names
        # (don't start with a digit, don't use accented letters or symbols other than '_')
        name_regex = getrp(r'[a-zA-Z_][a-zA-Z\d_]*')
        if name_regex.fullmatch(raw_name) is None:
            latinized_name = getrp(r'[^a-zA-Z\d_]').sub("", raw_name)
            if name_regex.fullmatch(latinized_name) is None:
                # Add '_' if name starts with a digit or is empty after removing accented chars.
                latinized_name = "_" + latinized_name
            assert name_regex.fullmatch(latinized_name) is not None, \
                f"Couldn't make '{raw_name}' valid. '{latinized_name}' is invalid."
            return latinized_name
        return raw_name

    def get_base_name(self):
        """Base name used for functions, variables, and typedefs."""
        if not self.base_name:
            self.set_base_name(self.generate_base_name())
        return self.base_name

    def set_base_name(self, base_name):
        """Set an explicit base name for this element."""
        self.base_name = base_name.replace("-", "_")

    def set_base_names(self):
        """Recursively set the base names of this element's children, keys, and cbor elements."""
        if self.cbor:
            self.cbor.set_base_name(self.var_name().strip("_") + "_cbor")
        if self.key:
            self.key.set_base_name(self.var_name().strip("_") + "_key")

        if self.type in ["LIST", "MAP", "GROUP", "UNION"]:
            for child in self.value:
                child.set_base_names()
        if self.cbor:
            self.cbor.set_base_names()
        if self.key:
            self.key.set_base_names()

    def id(self, with_prefix=True):
        """Add uniqueness to the base name."""
        raw_name = self.get_base_name()
        if not with_prefix and self.short_names:
            return raw_name
        if (self.id_prefix
                and (f"{self.id_prefix}_" not in raw_name)
                and (self.id_prefix != raw_name.strip("_"))):
            return f"{self.id_prefix}_{raw_name}"
        if (self.base_stem
                and (f"{self.base_stem}_" not in raw_name)
                and (self.base_stem != raw_name.strip("_"))):
            return f"{self.base_stem}_{raw_name}"
        return raw_name

    def init_args(self):
        """Return the args that should be used to initialize a new instance of this class."""
        return (self.default_max_qty,)

    def init_kwargs(self):
        """Return the kwargs that should be used to initialize a new instance of this class."""
        return {
            "my_types": self.my_types, "my_control_groups": self.my_control_groups,
            "short_names": self.short_names}

    def set_id_prefix(self, id_prefix=''):
        self.id_prefix = id_prefix
        if self.type in ["LIST", "MAP", "GROUP", "UNION"]:
            for child in self.value:
                if child.single_func_impl_condition():
                    child.set_id_prefix(self.generate_base_name())
                else:
                    child.set_id_prefix(self.child_base_id())
        if self.cbor:
            self.cbor.set_id_prefix(self.child_base_id())
        if self.key:
            self.key.set_id_prefix(self.child_base_id())

    def child_base_id(self):
        """Id to pass to children for them to use as basis for their id/base name."""
        return self.id()

    def mrepr(self, newline):
        """Human readable representation."""
        reprstr = ''
        if self.quantifier:
            reprstr += self.quantifier
        if self.label:
            reprstr += self.label + ':'
        for tag in self.tags:
            reprstr += f"#6.{tag}"
        if self.key:
            reprstr += repr(self.key) + " => "
        if self.is_unambiguous():
            reprstr += '/'
        if self.is_unambiguous_repeated():
            reprstr += '/'
        reprstr += self.type
        if self.size:
            reprstr += '(%d)' % self.size
        if newline:
            reprstr += '\n'
        if self.value:
            reprstr += pformat(self.value, indent=4, width=1)
        if self.cbor:
            reprstr += " cbor: " + repr(self.cbor)
        return reprstr.replace('\n', '\n    ')

    def _flatten(self):
        """Recursively flatten children, key, and cbor elements."""
        new_value = []
        if self.type in ["LIST", "MAP", "GROUP", "UNION"]:
            for child in self.value:
                new_value.extend(
                    child.flatten(allow_multi=self.type != "UNION"))
            self.value = new_value
        if self.key:
            self.key = self.key.flatten()[0]
        if self.cbor:
            self.cbor = self.cbor.flatten()[0]

    def flatten(self, allow_multi=False):
        """Remove unneccessary abstractions, like single-element groups or unions."""
        self._flatten()
        if self.type == "OTHER" and self.is_socket and self.value not in self.my_types:
            return []
        if self.type in ["GROUP", "UNION"]\
                and (len(self.value) == 1)\
                and (not (self.key and self.value[0].key)):
            self.value[0].min_qty *= self.min_qty
            self.value[0].max_qty *= self.max_qty
            if not self.value[0].label:
                self.value[0].label = self.label
            if not self.value[0].key:
                self.value[0].key = self.key
            self.value[0].tags.extend(self.tags)
            return self.value
        elif allow_multi and self.type in ["GROUP"] and self.min_qty == 1 and self.max_qty == 1:
            return self.value
        else:
            return [self]

    def set_min_value(self, min_value):
        self.min_value = min_value

    def set_max_value(self, max_value):
        self.max_value = max_value

    def type_and_value(self, new_type, value_generator):
        """Set the self.type and self.value of this element."""
        if self.type is not None:
            raise TypeError(
                "Cannot have two values: %s, %s" %
                (self.type, new_type))
        if new_type is None:
            raise TypeError("Cannot set None as type")
        if new_type == "UNION" and self.value is not None:
            raise ValueError("Did not expect multiple parsed values for union")

        self.type = new_type
        self.set_value(value_generator)

    def set_value(self, value_generator):
        """Set the value of this element.

        value_generator must be a function that returns the value of the element."""
        value = value_generator()
        self.value = value

        if self.type == "OTHER" and self.value.startswith("$"):
            self.value = self.value.lstrip("$")
            self.is_socket = True

        if self.type in ["BSTR", "TSTR"]:
            if value is not None:
                self.set_size(len(value))
        if self.type in ["UINT", "NINT"]:
            if value is not None:
                self.size = sizeof(value)
                self.set_min_value(value)
                self.set_max_value(value)
        if self.type == "NINT":
            self.max_value = -1

    def type_and_range(self, new_type, min_val, max_val, inc_end=True):
        """Set the self.type and self.minValue and self.max_value (or self.min_size and
        self.max_size depending on the type) of this element. For use during CDDL parsing.
        """
        if not inc_end:
            max_val -= 1
        if new_type not in ["INT", "UINT", "NINT"]:
            raise TypeError(
                "Only integers (not %s) can have range" %
                (new_type,))
        if min_val > max_val:
            raise TypeError(
                "Range has larger minimum than maximum (min %d, max %d)" %
                (min_val, max_val))
        if min_val == max_val:
            return self.type_and_value(new_type, min_val)
        self.type = new_type
        self.set_min_value(min_val)
        self.set_max_value(max_val)
        if new_type in "UINT":
            self.set_size_range(sizeof(min_val), sizeof(max_val))
        if new_type == "NINT":
            self.set_size_range(sizeof(abs(max_val)), sizeof(abs(min_val)))
        if new_type == "INT":
            self.set_size_range(None, max(sizeof(abs(max_val)), sizeof(abs(min_val))))

    def type_value_size(self, new_type, value, size):
        """Set the self.value and self.size of this element."""
        self.type_and_value(new_type, value)
        self.set_size(size)

    def type_value_size_range(self, new_type, value, min_size, max_size):
        """Set the self.value and self.min_size and self.max_size of this element."""
        self.type_and_value(new_type, value)
        self.set_size_range(min_size, max_size)

    def set_label(self, label):
        """Set the self.label of this element. For use during CDDL parsing."""
        if self.type is not None:
            raise TypeError("Cannot have label after value: " + label)
        self.label = label

    def set_quantifier(self, quantifier):
        """Set the self.quantifier, self.min_qty, and self.max_qty of this element"""
        if self.type is not None:
            raise TypeError(
                "Cannot have quantifier after value: " + quantifier)

        quantifier_mapping = [
            (r"\?", lambda mo: (0, 1)),
            (r"\*", lambda mo: (0, None)),
            (r"\+", lambda mo: (1, None)),
            (r"(.*?)\*\*?(.*)",
                lambda mo: (int(mo.groups()[0] or "0", 0), int(mo.groups()[1] or "0", 0) or None)),
        ]

        self.quantifier = quantifier
        for (reg, handler) in quantifier_mapping:
            match_obj = getrp(reg).match(quantifier)
            if match_obj:
                (self.min_qty, self.max_qty) = handler(match_obj)
                if self.max_qty is None:
                    self.max_qty = self.default_max_qty
                return
        raise ValueError("invalid quantifier: %s" % quantifier)

    def set_size(self, size):
        """Set the self.size of this element.

        This will also set the self.minValue and self.max_value of UINT types.
        """
        if self.type is None:
            raise TypeError("Cannot have size before value: " + str(size))
        elif self.type in ["INT", "UINT", "NINT"]:
            value = 256**size
            if self.type == "INT":
                self.max_value = int((value >> 1) - 1)
            if self.type == "UINT":
                self.max_value = int(value - 1)
            if self.type in ["INT", "NINT"]:
                self.min_value = int(-1 * (value >> 1))
        elif self.type in ["BSTR", "TSTR", "FLOAT"]:
            self.set_size_range(size, size)
        else:
            raise TypeError(".size cannot be applied to %s" % self.type)

    def set_size_range(self, min_size, max_size_in, inc_end=True):
        """Set the self.minValue and self.max_value or self.min_size and self.max_size of this
        element based on what values can be contained within an integer of a certain size.
        """
        max_size = max_size_in if inc_end else max_size_in - 1

        if (min_size and min_size < 0 or max_size and max_size < 0) \
           or (None not in [min_size, max_size] and min_size > max_size):
            raise TypeError(
                "Invalid size range (min %d, max %d)" %
                (min_size, max_size))

        self.set_min_size(min_size)
        self.set_max_size(max_size)

    def set_min_size(self, min_size):
        """Set self.min_size, and self.minValue if type is UINT."""
        if self.type == "UINT":
            self.minValue = 256**min(0, abs(min_size - 1)) if min_size is not None else None
        self.min_size = min_size if min_size is not None else None

    def set_max_size(self, max_size):
        """Set self.max_size, and self.max_value if type is UINT."""
        if self.type == "UINT" and max_size and self.max_value is None:
            if max_size > 8:
                raise TypeError(
                    "Size too large for integer. size %d" %
                    max_size)
            self.max_value = 256**max_size - 1
        self.max_size = max_size

    def set_cbor(self, cbor, cborseq):
        """Set the self.cbor of this element. For use during CDDL parsing."""
        if self.type != "BSTR":
            raise TypeError(
                "%s must be used with bstr." %
                (".cborseq" if cborseq else ".cbor",))
        self.cbor = cbor
        if cborseq:
            self.cbor.max_qty = self.default_max_qty

    def set_bits(self, bits):
        """Set the self.bits of this element. For use during CDDL parsing."""
        if self.type != "UINT":
            raise TypeError(".bits must be used with bstr.")
        self.bits = bits

    def set_key(self, key):
        """Set the self.key of this element. For use during CDDL parsing."""
        if self.key is not None:
            raise TypeError("Cannot have two keys: " + key)
        if key.type == "GROUP":
            raise TypeError("A key cannot be a group because it might represent more than 1 type.")
        self.key = key

    def set_key_or_label(self, key_or_label):
        """Set the self.label OR self.key of this element.

        In the CDDL "foo: bar", foo can be either a label or a key depending on whether it is in a
        map. This code uses a slightly different method for choosing between label and key.
        If the string is recognized as a type, it is treated as a key. For use during CDDL parsing.
        """
        if key_or_label in self.my_types:
            self.set_key(self.parse(key_or_label)[0])
            assert self.key.type == "OTHER", "This should only be able to produce an OTHER key."
            if self.label is None:
                self.set_label(key_or_label)
        else:
            self.set_label(key_or_label)

    def add_tag(self, tag):
        self.tags.append(int(tag))

    def union_add_value(self, value, doubleslash=False):
        """Append to the self.value of this element.

        Used with the "UNION" type, which has a python list as self.value. The list represents the
        "children" of the type. For use during CDDL parsing.
        """
        if self.type != "UNION":
            convert_val = copy(self)
            self.__init__(*self.init_args(), **self.init_kwargs())
            self.type_and_value("UNION", lambda: [convert_val])

            self.base_name = convert_val.base_name
            convert_val.base_name = None
            self.base_stem = convert_val.base_stem

            if not doubleslash:
                self.label = convert_val.label
                self.key = convert_val.key
                self.quantifier = convert_val.quantifier
                self.max_qty = convert_val.max_qty
                self.min_qty = convert_val.min_qty

                convert_val.label = None
                convert_val.key = None
                convert_val.quantifier = None
                convert_val.max_qty = 1
                convert_val.min_qty = 1
        self.value.append(value)

    def convert_to_key(self):
        """The current element is the key, so copy it to a new element and set the key to the new"""
        convert_val = copy(self)
        self.__init__(*self.init_args(), **self.init_kwargs())
        self.set_key(convert_val)

        self.label = convert_val.label
        self.quantifier = convert_val.quantifier
        self.max_qty = convert_val.max_qty
        self.min_qty = convert_val.min_qty
        self.base_name = convert_val.base_name
        self.base_stem = convert_val.base_stem

        convert_val.label = None
        convert_val.quantifier = None
        convert_val.max_qty = 1
        convert_val.min_qty = 1
        convert_val.base_name = None

    # A dict with lists of regexes and their corresponding handlers.
    # This is a dict in case multiple inheritors of CddlParser are used at once, in which case
    # they may have slightly different handlers.
    cddl_regexes = dict()

    def cddl_regexes_init(self):
        """Initialize the cddl_regexes dict"""
        match_uint = r"(0x[0-9a-fA-F]+|0o[0-7]+|0b[01]+|\d+)"
        match_int = r"(-?" + match_uint + ")"
        match_nint = r"(-" + match_uint + ")"

        self_type = type(self)

        # The "range_types" match the contents of brackets i.e. (), [], and {},
        # and strings, i.e. ' or "
        range_types = [
            (r'(?P<bracket>\[(?P<item>(?>[^[\]]+|(?&bracket))*)\])',
             lambda m_self, list_str: m_self.type_and_value(
                 "LIST", lambda: m_self.parse(list_str))),
            (r'(?P<paren>\((?P<item>(?>[^\(\)]+|(?&paren))*)\))',
             lambda m_self, group_str: m_self.type_and_value(
                 "GROUP", lambda: m_self.parse(group_str))),
            (r'(?P<curly>{(?P<item>(?>[^{}]+|(?&curly))*)})',
             lambda m_self, map_str: m_self.type_and_value(
                 "MAP", lambda: m_self.parse(map_str))),
            (r'\'(?P<item>.*?)(?<!\\)\'',
             lambda m_self, string: m_self.type_and_value("BSTR", lambda: string)),
            (r'\"(?P<item>.*?)(?<!\\)\"',
             lambda m_self, string: m_self.type_and_value("TSTR", lambda: string)),
        ]
        range_types_regex = '|'.join([regex for (regex, _) in range_types])
        for i in range(range_types_regex.count("item")):
            range_types_regex = range_types_regex.replace("item", "it%dem" % i, 1)

        # The following regexes match different parts of the element. The order of the list is
        # important because it implements the operator precendence defined in the CDDL spec.
        # The range_types are separate because they are reused in one of the other regexes.
        self_type.cddl_regexes[self_type] = range_types + [
            (r'\/\/\s*(?P<item>.+?)(?=\/\/|\Z)',
             lambda m_self, union_str: m_self.union_add_value(
                 m_self.parse("(%s)" % union_str if ',' in union_str else union_str)[0],
                 doubleslash=True)),
            (r'(?P<item>[^\W\d][\w-]*)\s*:',
             self_type.set_key_or_label),
            (r'((\=\>)|:)',
             lambda m_self, _: m_self.convert_to_key()),
            (r'([+*?])',
             self_type.set_quantifier),
            (r'(' + match_uint + r'\*\*?' + match_uint + r'?)',
             self_type.set_quantifier),
            (r'\/\s*(?P<item>((' + range_types_regex + r')|[^,\[\]{}()])+?)(?=\/|\Z|,)',
             lambda m_self, union_str: m_self.union_add_value(
                 m_self.parse(union_str)[0])),
            (r'(uint|nint|int|float|bstr|tstr|bool|nil|any)(?![\w-])',
             lambda m_self, type_str: m_self.type_and_value(type_str.upper(), lambda: None)),
            (r'undefined(?!\w)',
             lambda m_self, _: m_self.type_and_value("UNDEF", lambda: None)),
            (r'float16(?![\w-])',
             lambda m_self, _: m_self.type_value_size("FLOAT", lambda: None, 2)),
            (r'float16-32(?![\w-])',
             lambda m_self, _: m_self.type_value_size_range("FLOAT", lambda: None, 2, 4)),
            (r'float32(?![\w-])',
             lambda m_self, _: m_self.type_value_size("FLOAT", lambda: None, 4)),
            (r'float32-64(?![\w-])',
             lambda m_self, _: m_self.type_value_size_range("FLOAT", lambda: None, 4, 8)),
            (r'float64(?![\w-])',
             lambda m_self, _: m_self.type_value_size("FLOAT", lambda: None, 8)),
            (r'\-?\d*\.\d+',
             lambda m_self, num: m_self.type_and_value("FLOAT", lambda: float(num))),
            (match_uint + r'\.\.' + match_uint,
             lambda m_self, _range: m_self.type_and_range(
                 "UINT", *map(lambda num: int(num, 0), _range.split("..")))),
            (match_nint + r'\.\.' + match_uint,
             lambda m_self, _range: m_self.type_and_range(
                 "INT", *map(lambda num: int(num, 0), _range.split("..")))),
            (match_nint + r'\.\.' + match_nint,
             lambda m_self, _range: m_self.type_and_range(
                 "NINT", *map(lambda num: int(num, 0), _range.split("..")))),
            (match_uint + r'\.\.\.' + match_uint,
             lambda m_self, _range: m_self.type_and_range(
                 "UINT", *map(lambda num: int(num, 0), _range.split("...")), inc_end=False)),
            (match_nint + r'\.\.\.' + match_uint,
             lambda m_self, _range: m_self.type_and_range(
                 "INT", *map(lambda num: int(num, 0), _range.split("...")), inc_end=False)),
            (match_nint + r'\.\.\.' + match_nint,
             lambda m_self, _range: m_self.type_and_range(
                 "NINT", *map(lambda num: int(num, 0), _range.split("...")), inc_end=False)),
            (match_nint,
             lambda m_self, num: m_self.type_and_value("NINT", lambda: int(num, 0))),
            (match_uint,
             lambda m_self, num: m_self.type_and_value("UINT", lambda: int(num, 0))),
            (r'true(?!\w)',
             lambda m_self, _: m_self.type_and_value("BOOL", lambda: True)),
            (r'false(?!\w)',
             lambda m_self, _: m_self.type_and_value("BOOL", lambda: False)),
            (r'#6\.(?P<item>\d+)',
             self_type.add_tag),
            (r'(\$?\$?[\w-]+)',
             lambda m_self, other_str: m_self.type_and_value("OTHER", lambda: other_str)),
            (r'\.size \(?(?P<item>' + match_int + r'\.\.' + match_int + r')\)?',
             lambda m_self, _range: m_self.set_size_range(
                 *map(lambda num: int(num, 0), _range.split("..")))),
            (r'\.size \(?(?P<item>' + match_int + r'\.\.\.' + match_int + r')\)?',
             lambda m_self, _range: m_self.set_size_range(
                 *map(lambda num: int(num, 0), _range.split("...")), inc_end=False)),
            (r'\.size \(?(?P<item>' + match_uint + r')\)?',
             lambda m_self, size: m_self.set_size(int(size, 0))),
            (r'\.gt \(?(?P<item>' + match_int + r')\)?',
             lambda m_self, minvalue: m_self.set_min_value(int(minvalue, 0) + 1)),
            (r'\.lt \(?(?P<item>' + match_int + r')\)?',
             lambda m_self, maxvalue: m_self.set_max_value(int(maxvalue, 0) - 1)),
            (r'\.ge \(?(?P<item>' + match_int + r')\)?',
             lambda m_self, minvalue: m_self.set_min_value(int(minvalue, 0))),
            (r'\.le \(?(?P<item>' + match_int + r')\)?',
             lambda m_self, maxvalue: m_self.set_max_value(int(maxvalue, 0))),
            (r'\.eq \(?(?P<item>' + match_int + r')\)?',
             lambda m_self, value: m_self.set_value(lambda: int(value, 0))),
            (r'\.eq \"(?P<item>.*?)(?<!\\)\"',
             lambda m_self, value: m_self.set_value(lambda: value)),
            (r'\.cbor (\((?P<item>(?>[^\(\)]+|(?1))*)\))',
             lambda m_self, type_str: m_self.set_cbor(m_self.parse(type_str)[0], False)),
            (r'\.cbor (?P<item>[^\s,]+)',
             lambda m_self, type_str: m_self.set_cbor(m_self.parse(type_str)[0], False)),
            (r'\.cborseq (\((?P<item>(?>[^\(\)]+|(?1))*)\))',
             lambda m_self, type_str: m_self.set_cbor(m_self.parse(type_str)[0], True)),
            (r'\.cborseq (?P<item>[^\s,]+)',
             lambda m_self, type_str: m_self.set_cbor(m_self.parse(type_str)[0], True)),
            (r'\.bits (?P<item>[\w-]+)',
             lambda m_self, bits_str: m_self.set_bits(bits_str))
        ]

    def get_value(self, instr):
        """Parse from the beginning of instr (string) until a full element has been parsed.

        self will become that element. This function is recursive, so if a nested element
        ("MAP"/"LIST"/"UNION"/"GROUP") is encountered, this function will create new instances and
        add them to self.value as a list. Likewise, if a key or cbor definition is encountered, a
        new element will be created and assigned to self.key or self.cbor. When new elements are
        created, get_value() is called on those elements, via parse().
        """
        types = type(self).cddl_regexes[type(self)]

        # Keep parsing until a comma, or to the end of the string.
        while instr != '' and instr[0] != ',':
            match_obj = None
            for (reg, handler) in types:
                match_obj = getrp(reg).match(instr)
                if match_obj:
                    try:
                        match_str = match_obj.group("item")
                    except IndexError:
                        match_str = match_obj.group(0)
                    try:
                        handler(self, match_str)
                    except Exception as e:
                        raise Exception("Failed while parsing this: '%s'" % match_str) from e
                    self.match_str += match_str
                    old_len = len(instr)
                    instr = getrp(reg).sub('', instr, count=1).lstrip()
                    if old_len == len(instr):
                        raise Exception("empty match")
                    break

            if not match_obj:
                raise TypeError("Could not parse this: '%s'" % instr)

        instr = instr[1:]
        if not self.type:
            raise ValueError("No proper value while parsing: %s" % instr)

        # Return the unparsed part of the string.
        return instr.strip()

    def elem_has_key(self):
        """For checking whether this element has a key (i.e. that it is a valid "MAP" child)

        This must have some recursion since CDDL allows the key to be hidden
        behind layers of indirection.
        """
        return self.key is not None\
            or (self.type == "OTHER" and self.my_types[self.value].elem_has_key())\
            or (self.type in ["GROUP", "UNION"]
                and (self.value and all(child.elem_has_key() for child in self.value)))

    def post_validate(self):
        """Function for performing validations that must be done after all parsing is complete.

        This is recursive, so it will post_validate all its children + key + cbor.
        """
        # Validation of this element.
        if self.type in ["LIST", "MAP"]:
            none_keys = [child for child in self.value if not child.elem_has_key()]
            child_keys = [child for child in self.value if child not in none_keys]
            if self.type == "MAP" and none_keys:
                raise TypeError(
                    "Map member(s) must have key: " + str(none_keys) + " pointing to "
                    + str(
                        [self.my_types[elem.value] for elem in none_keys
                            if elem.type == "OTHER"]))
            if self.type == "LIST" and child_keys:
                raise TypeError(
                    str(self) + linesep
                    + "List member(s) cannot have key: " + str(child_keys) + " pointing to "
                    + str(
                        [self.my_types[elem.value] for elem in child_keys
                            if elem.type == "OTHER"]))
        if self.type == "OTHER":
            if self.value not in self.my_types.keys() or not isinstance(
                    self.my_types[self.value], type(self)):
                raise TypeError("%s has not been parsed." % self.value)
        if self.type == "LIST":
            for child in self.value[:-1]:
                if child.type == "ANY":
                    if child.min_qty != child.max_qty:
                        raise TypeError(f"ambiguous quantity of 'any' is not supported in list, "
                                        + "except as last element:\n{str(child)}")
        if self.type == "UNION" and len(self.value) > 1:
            if any(((not child.key and child.type == "ANY") or (
                    child.key and child.key.type == "ANY")) for child in self.value):
                raise TypeError(
                    "'any' inside union is not supported since it would always be triggered.")

        # Validation of child elements.
        if self.type in ["MAP", "LIST", "UNION", "GROUP"]:
            for child in self.value:
                child.post_validate()
        if self.key:
            self.key.post_validate()
        if self.cbor:
            self.cbor.post_validate()

    def post_validate_control_group(self):
        if self.type != "GROUP":
            raise TypeError("control groups must be of GROUP type.")
        for c in self.value:
            if c.type != "UINT" or c.value is None or c.value < 0:
                raise TypeError("control group members must be literal positive integers.")

    def parse(self, instr):
        """Parses entire instr and returns a list of instances."""
        instr = instr.strip()
        values = []
        while instr != '':
            value = type(self)(*self.init_args(), **self.init_kwargs(), base_stem=self.base_stem)
            instr = value.get_value(instr)
            values.append(value)
        return values

    def __repr__(self):
        return self.mrepr(False)


c_keywords = [
    "alignas", "alignof", "atomic_bool", "atomic_int", "auto", "bool", "break", "case", "char",
    "complex", "const", "constexpr", "continue", "default", "do", "double", "else", "enum",
    "extern", "false", "float", "for", "goto", "if", "imaginary", "inline", "int", "long",
    "noreturn", "nullptr", "register", "restrict", "return", "short", "signed", "sizeof", "static",
    "static_assert", "struct", "switch", "thread_local", "true", "typedef", "typeof",
    "typeof_unqual", "union", "unsigned", "void", "volatile", "while"]


c_keywords_underscore = [
    "_Alignas", "_Alignof", "_Atomic", "_BitInt", "_Bool", "_Complex", "_Decimal128", "_Decimal32",
    "_Decimal64", "_Generic", "_Imaginary", "_Noreturn", "_Pragma", "_Static_assert",
    "_Thread_local"]


class CddlXcoder(CddlParser):

    def __init__(self, *args, **kwargs):
        super(CddlXcoder, self).__init__(*args, **kwargs)

        # The prefix used for C code accessing this element, i.e. the struct
        # hierarchy leading up to this element.
        self.accessPrefix = None
        self.is_delegated = False
        # Used as a guard against endless recursion in self.dependsOn()
        self.dependsOnCall = False
        self.skipped = False

    def var_name(self, with_prefix=False, observe_skipped=True):
        """Name of variables and enum members for this element."""
        if (observe_skipped and self.skip_condition()
                and self.type in ["LIST", "MAP", "GROUP"] and self.value):
            return self.value[0].var_name(with_prefix)
        name = self.id(with_prefix=with_prefix)
        if name in c_keywords:
            name = name.capitalize()
        elif name in c_keywords_underscore:
            name = "_" + name
        return name

    def skip_condition(self):
        """Whether this element should have its result variable omitted."""
        if self.skipped:
            return True
        if self.type in ["LIST", "MAP", "GROUP"]:
            return not self.repeated_multi_var_condition()
        if self.type == "OTHER":
            return ((not self.repeated_multi_var_condition())
                    and (not self.multi_var_condition())
                    and (self.single_func_impl_condition() or self in self.my_types.values()))
        return False

    def set_skipped(self, skipped):
        if self.range_check_condition() \
           and self.repeated_single_func_impl_condition() \
           and not self.key:
            self.skipped = True
        else:
            self.skipped = skipped
        return

    def delegate_type_condition(self):
        """Whether to use the C type of the first child as this type's C type"""
        ret = self.type in ["LIST", "MAP", "GROUP"]
        return ret

    def is_delegated_type(self):
        return self.is_delegated

    def set_access_prefix(self, prefix, is_delegated=False):
        """Recursively set the access prefix for this element and all its children."""
        self.accessPrefix = prefix
        if self.type in ["LIST", "MAP", "GROUP", "UNION"]:
            self.set_skipped(self.skip_condition())
            list(map(lambda child: child.set_skipped(child.skip_condition()),
                     self.value))
            list(map(lambda child: child.set_access_prefix(
                     self.var_access(),
                     is_delegated=(self.delegate_type_condition()
                                   or (is_delegated and self.skip_condition()))),
                     self.value))
        elif self in self.my_types.values():
            self.set_skipped(not self.multi_member())
        if self.key is not None:
            self.key.set_access_prefix(self.var_access())
        if self.cbor_var_condition():
            self.cbor.set_access_prefix(self.var_access())
        self.is_delegated = is_delegated and not self.skip_condition()
        return

    def multi_member(self):
        """Whether this type has multiple member variables."""
        return self.multi_var_condition() or self.repeated_multi_var_condition()

    def is_unambiguous_value(self):
        """Whether this element is a non-compound value that can be known a priori."""
        return (self.type in ["NIL", "UNDEF", "ANY"]
                or (self.type in ["INT", "NINT", "UINT", "FLOAT", "BSTR", "TSTR", "BOOL"]
                    and self.value is not None)
                or (self.type == "OTHER" and self.my_types[self.value].is_unambiguous()))

    def is_unambiguous_repeated(self):
        """Whether the repeated part of this element is known a priori."""
        return (self.is_unambiguous_value()
                and (self.key is None or self.key.is_unambiguous_repeated())
                or (self.type in ["LIST", "GROUP", "MAP"] and len(self.value) == 0)
                or (self.type in ["LIST", "GROUP", "MAP"]
                    and all((child.is_unambiguous() for child in self.value))))

    def is_unambiguous(self):
        """Whether or not we can know the exact encoding of this element a priori."""
        return (self.is_unambiguous_repeated() and (self.min_qty == self.max_qty))

    def access_append_delimiter(self, prefix, delimiter, *suffix):
        """Create an access prefix based on an existing prefix, delimiter and a
        suffix.
        """
        assert prefix is not None, "No access prefix for %s" % self.var_name()
        return delimiter.join((prefix,) + suffix)

    def access_append(self, *suffix):
        """Create an access prefix from this element's prefix, delimiter and a
        provided suffix.
        """
        suffix = list(suffix)
        return self.access_append_delimiter(self.accessPrefix, '.', *suffix)

    def var_access(self):
        """"Path" to this element's variable."""
        if self.is_unambiguous():
            return "NULL"
        return self.access_append()

    def val_access(self):
        """"Path" to access this element's actual value variable."""
        if self.is_unambiguous_repeated():
            ret = "NULL"
        elif self.skip_condition() or self.is_delegated_type():
            ret = self.var_access()
        else:
            ret = self.access_append(self.var_name())
        return ret

    def repeated_val_access(self):
        if self.is_unambiguous_repeated():
            return "NULL"
        return self.access_append(self.var_name())

    def present_var_condition(self):
        """Whether to include a "present" variable for this element."""
        return self.min_qty == 0 and isinstance(self.max_qty, int) and self.max_qty <= 1

    def count_var_condition(self):
        """Whether to include a "count" variable for this element."""
        return isinstance(self.max_qty, str) or self.max_qty > 1

    def is_cbor(self):
        """Whether to include a "cbor" variable for this element."""
        return (self.type not in ["NIL", "UNDEF", "ANY"]) \
            and ((self.type != "OTHER") or (self.my_types[self.value].is_cbor()))

    def cbor_var_condition(self):
        """Whether to include a "cbor" variable for this element."""
        return (self.cbor is not None) and self.cbor.is_cbor()

    def choice_var_condition(self):
        """Whether to include a "choice" variable for this element."""
        return self.type == "UNION"

    def reduced_key_var_condition(self):
        """Whether this specific type is a key."""
        if self.key is not None:
            return True
        return False

    def key_var_condition(self):
        """Whether to include a "key" variable for this element."""
        if self.reduced_key_var_condition():
            return True
        if self.type == "OTHER" and self.my_types[self.value].key_var_condition():
            return True
        if (self.type in ["GROUP", "UNION"]
                and len(self.value) >= 1
                and self.value[0].reduced_key_var_condition()):
            return True
        return False

    def self_repeated_multi_var_condition(self):
        """Whether this value adds any repeated elements by itself. I.e. excluding
        multiple elements from children.
        """
        return (self.key_var_condition()
                or self.cbor_var_condition()
                or self.choice_var_condition())

    def multi_val_condition(self):
        """Whether this element's actual value has multiple members."""
        return (
            self.type in ["LIST", "MAP", "GROUP", "UNION"]
            and (len(self.value) > 1 or (len(self.value) == 1 and self.value[0].multi_member())))

    def repeated_multi_var_condition(self):
        """Whether any extra variables are to be included for this element for each
        repetition.
        """
        return self.self_repeated_multi_var_condition() or self.multi_val_condition()

    def multi_var_condition(self):
        """Whether any extra variables are to be included for this element outside
        of repetitions.
        Also, whether this element must involve a call to multi_xcode(), i.e. unless
        it's repeated exactly once.
        """
        return self.present_var_condition() or self.count_var_condition()

    def range_check_condition(self):
        """Whether this element needs a check (memcmp) for a string value."""
        if self.type == "OTHER":
            return self.my_types[self.value].range_check_condition()
        if self.type not in ["INT", "NINT", "UINT", "BSTR", "TSTR"]:
            return False
        if self.value is not None:
            return False
        if self.type in ["INT", "NINT", "UINT"] \
                and (self.min_value is not None or self.max_value is not None):
            return True
        if self.type == "UINT" and self.bits:
            return True
        if self.type in ["BSTR", "TSTR"] \
                and (self.min_size is not None or self.max_size is not None):
            return True
        return False

    def type_def_condition(self):
        """Whether this element should have a typedef in the code."""
        if self in self.my_types.values() and self.multi_member() and not self.is_unambiguous():
            return True
        return False

    def repeated_type_def_condition(self):
        """Whether this type needs a typedef for its repeated part."""
        return (
            self.repeated_multi_var_condition()
            and self.multi_var_condition()
            and not self.is_unambiguous_repeated())

    def single_func_impl_condition(self):
        """Whether this element needs its own encoder/decoder function."""
        return (
            False
            or self.reduced_key_var_condition()
            or self.cbor_var_condition()
            or (self.tags and self in self.my_types.values())
            or self.type_def_condition()
            or (self.type in ["LIST", "MAP"])
            or (self.type == "GROUP" and len(self.value) != 0))

    def repeated_single_func_impl_condition(self):
        """Whether this element needs its own encoder/decoder function."""
        return self.repeated_type_def_condition() \
            or (self.type in ["LIST", "MAP", "GROUP"] and self.multi_member()) \
            or (
                self.multi_var_condition()
                and (self.self_repeated_multi_var_condition() or self.range_check_condition()))

    def int_val(self):
        """If this element is an integer, or starts with an integer, return the integer value."""
        if self.key:
            return self.key.int_val()
        elif self.type in ("UINT", "NINT") and self.is_unambiguous():
            return self.value
        elif self.type == "GROUP" and not self.count_var_condition():
            return self.value[0].int_val()
        elif self.type == "OTHER" \
                and not self.count_var_condition() \
                and not self.single_func_impl_condition() \
                and not self.my_types[self.value].single_func_impl_condition():
            return self.my_types[self.value].int_val()
        return None

    def is_int_disambiguated(self):
        """Whether this element starts with a specific integer that can be used to immediately
        disambiguate it from other elements.
        """
        return self.int_val() is not None

    def all_children_disambiguated(self, min_val, max_val):
        """Whether all children of this element can be disambiguated via a starting integer.

        This is relevant because it allows the decoder to directly decode the integer into an enum
        value.
        The min_val and max_val are to check whether the integers are within a certain range.
        """
        values = set(child.int_val() for child in self.value)
        retval = (len(values) == len(self.value)) and None not in values \
            and max(values) <= max_val and min(values) >= min_val
        return retval

    def all_children_int_disambiguated(self):
        """See all_children_disambiguated()"""
        return self.all_children_disambiguated(INT32_MIN, INT32_MAX)

    def all_children_uint_disambiguated(self):
        """See all_children_disambiguated()"""
        return self.all_children_disambiguated(0, INT32_MAX)

    def present_var_name(self):
        """Name of the "present" variable for this element."""
        return "%s_present" % (self.var_name())

    def present_var_access(self):
        """Full "path" of the "present" variable for this element."""
        return self.access_append(self.present_var_name())

    def count_var_name(self):
        """Name of the "count" variable for this element."""
        return "%s_count" % (self.var_name())

    def count_var_access(self):
        """Full "path" of the "count" variable for this element."""
        return self.access_append(self.count_var_name())

    def choice_var_name(self):
        """Name of the "choice" variable for this element."""
        return self.var_name() + "_choice"

    def enum_var_name(self):
        """Name of the enum entry for this element."""
        return self.var_name(with_prefix=True) + "_c"

    def enum_var(self, int_val=False):
        """Enum entry for this element."""
        return f"{self.enum_var_name()} = {val_to_str(self.int_val())}" \
               if int_val else self.enum_var_name()

    def choice_var_access(self):
        """Full "path" of the "choice" variable for this element."""
        return self.access_append(self.choice_var_name())


class CddlValidationError(Exception):
    pass


class KeyTuple(tuple):
    """Subclass of tuple for holding key,value pairs.

    This is to make it possible to use isinstance() to separate it from other tuples."""
    def __new__(cls, *in_tuple):
        return super(KeyTuple, cls).__new__(cls, *in_tuple)


class DataTranslator(CddlXcoder):
    """Convert data between CBOR, JSON and YAML, and validate against the provided CDDL.

    Decode and validate CBOR into Python structures to be able to make Python scripts that
    manipulate CBOR code.
    """

    @staticmethod
    def format_obj(obj):
        """Format a Python object for printing by adding newlines and indentation."""
        formatted = pformat(obj)
        out_str = ""
        indent = 0
        new_line = True
        for c in formatted:
            if new_line:
                if c == " ":
                    continue
                new_line = False
            out_str += c
            if c in "[(":
                indent += 1
            if c in ")]" and indent > 0:
                indent -= 1
            if c in "[(,":
                out_str += linesep
                out_str += "  " * indent
                new_line = True
        return out_str

    def id(self):
        """Override the id() function.

        If the name starts with an underscore, prepend an 'f',
        since namedtuple() doesn't support identifiers that start with an underscore.
        """
        return getrp(r"\A_").sub("f_", self.generate_base_name())

    def var_name(self):
        """Override the var_name()"""
        return self.id()

    def _decode_assert(self, test, msg=""):
        """Check a condition and raise a CddlValidationError if not."""
        if not test:
            raise CddlValidationError(
                f"Data did not decode correctly {'(' + msg + ')' if msg else ''}")

    def _check_tag(self, obj):
        """Check that no unexpected tags are attached to this data.

        Return whether a tag was present.
        """
        tags = copy(self.tags)  # All expected tags
        # Process all tags present in obj
        while isinstance(obj, CBORTag):
            if obj.tag in tags or self.type == "ANY":
                if obj.tag in tags:
                    tags.remove(obj.tag)
                obj = obj.value
                continue
            elif self.type in ["OTHER", "GROUP", "UNION"]:
                break
            self._decode_assert(False, f"Tag ({obj.tag}) not expected for {self}")
        # Check that all expected tags were found in obj.
        self._decode_assert(not tags, f"Expected tags ({tags}), but none present.")
        return obj

    def _expected_type(self):
        """Return our expected python type as returned by cbor2."""
        return {
            "UINT": lambda: (int,),
            "INT": lambda: (int,),
            "NINT": lambda: (int,),
            "FLOAT": lambda: (float,),
            "TSTR": lambda: (str,),
            "BSTR": lambda: (bytes,),
            "NIL": lambda: (type(None),),
            "UNDEF": lambda: (type(undefined),),
            "ANY": lambda: (int, float, str, bytes, type(None), type(undefined), bool, list, tuple,
                            dict, zfrozendict),
            "BOOL": lambda: (bool,),
            "LIST": lambda: (tuple, list),
            "MAP": lambda: (dict, zfrozendict),
        }[self.type]()

    def _check_type(self, obj):
        """Check that the decoded object has the correct type."""
        if self.type not in ["OTHER", "GROUP", "UNION"]:
            exp_type = self._expected_type()
            self._decode_assert(
                isinstance(obj, exp_type),
                f"{str(self)}: Wrong type ({type(obj)}) of {str(obj)}, expected {str(exp_type)}")
            if self.type in ["INT", "UINT", "NINT"]:
                # cbor2 gives a Python int for both uint and nint, and bool is a subclass of int, so
                # check the CBOR type here: bool for bools, uint for values >= 0, and nint for
                # values < 0.
                cbor_type = "bool" if isinstance(obj, bool) else "uint" if obj >= 0 else "nint"
                allowed = {"INT": ("uint", "nint"), "UINT": ("uint",), "NINT": ("nint",)}[self.type]
                self._decode_assert(
                    cbor_type in allowed,
                    f"{str(self)}: Wrong type ({cbor_type}) of {str(obj)}, "
                    f"expected {self.type.lower()}")

    def _check_value(self, obj):
        """Check that the decode value conforms to the restrictions in the CDDL."""
        if self.type in ["UINT", "INT", "NINT", "FLOAT", "TSTR", "BSTR", "BOOL"] \
                and self.value is not None:
            value = self.value
            if self.type == "BSTR":
                value = self.value.encode("utf-8")
            self._decode_assert(
                self.value == obj,
                f"{obj} should have value {self.value} according to {self.var_name()}")
        if self.type in ["UINT", "INT", "NINT", "FLOAT"]:
            if self.min_value is not None:
                self._decode_assert(obj >= self.min_value, "Minimum value: " + str(self.min_value))
            if self.max_value is not None:
                self._decode_assert(obj <= self.max_value, "Maximum value: " + str(self.max_value))
        if self.type == "UINT":
            if self.bits:
                mask = sum(((1 << b.value) for b in self.my_control_groups[self.bits].value))
                self._decode_assert(not (obj & ~mask), "Allowed bitmask: " + bin(mask))
        if self.type in ["TSTR", "BSTR"]:
            if self.min_size is not None:
                self._decode_assert(
                    len(obj) >= self.min_size, "Minimum length: " + str(self.min_size))
            if self.max_size is not None:
                self._decode_assert(
                    len(obj) <= self.max_size, "Maximum length: " + str(self.max_size))

    def _check_key(self, obj):
        """Check that the object is not a KeyTuple, which would mean it's not properly processed."""
        self._decode_assert(
            not isinstance(obj, KeyTuple), "Unexpected key found: (key,value)=" + str(obj))

    def _flatten_obj(self, obj):
        """Recursively remove intermediate objects that have single members. Keep lists as is."""
        if isinstance(obj, tuple) and len(obj) == 1:
            return self._flatten_obj(obj[0])
        return obj

    def _flatten_list(self, name, obj):
        """Return the contents of a list if it has a single member and the same name as us."""
        if (isinstance(obj, list)
                and len(obj) == 1
                and (isinstance(obj[0], list) or isinstance(obj[0], tuple))
                and len(obj[0]) == 1
                and hasattr(obj[0], name)):
            return [obj[0][0]]
        return obj

    def _construct_obj(self, my_list):
        """Construct a namedtuple object from my_list. my_list contains tuples of name/value.

        Also, attempt to flatten redundant levels of abstraction.
        """
        if my_list == []:
            return None
        names, values = tuple(zip(*my_list))
        if len(values) == 1:
            values = (self._flatten_obj(values[0]), )
        values = tuple(self._flatten_list(names[i], values[i]) for i in range(len(values)))
        assert (not any((isinstance(elem, KeyTuple) for elem in values))), \
            f"KeyTuple not processed: {values}"
        return namedtuple("_", names)(*values)

    def _add_if(self, my_list, obj, expect_key=False, name=None):
        """Add construct obj and add it to my_list if relevant.

        Also, process any KeyTuples present.
        """
        if expect_key and self.type == "OTHER" and self.key is None:
            self.my_types[self.value]._add_if(my_list, obj)
            return
        if self.is_unambiguous():
            return
        if isinstance(obj, list):
            for i in range(len(obj)):
                if isinstance(obj[i], KeyTuple):
                    retvals = list()
                    self._add_if(retvals, obj[i])
                    obj[i] = self._construct_obj(retvals)
                if self.type == "BSTR" and self.cbor_var_condition() and isinstance(obj[i], bytes):
                    assert all((isinstance(o, bytes) for o in obj)), \
                           """Unsupported configuration for cbor bstr. If a list contains a
CBOR-formatted bstr, all elements must be bstrs. If not, it is a programmer error."""
        if isinstance(obj, KeyTuple):
            key, obj = obj
            if key is not None:
                self.key._add_if(my_list, key, name=self.var_name() + "_key")
        if self.type == "BSTR" and self.cbor_var_condition():
            # If a bstr is CBOR-formatted, add both the string and the decoding of the string here
            if isinstance(obj, list) and all((isinstance(o, bytes) for o in obj)):
                # One or more bstr in a list (i.e. it is optional or repeated)
                my_list.append((name or self.var_name(), [self.cbor.decode_str(o) for o in obj]))
                my_list.append(((name or self.var_name()) + "_bstr", obj))
                return
            if isinstance(obj, bytes):
                my_list.append((name or self.var_name(), self.cbor.decode_str(obj)))
                my_list.append(((name or self.var_name()) + "_bstr", obj))
                return
        my_list.append((name or self.var_name(), obj))

    def _iter_is_empty(self, it):
        """Throw CddlValidationError if iterator is not empty.

        This consumes one element if present.
        """
        try:
            val = next(it)
        except StopIteration:
            return True
        raise CddlValidationError(
            f"Iterator not consumed while parsing \n{self}\nRemaining elements:\n elem: "
            + "\n elem: ".join(str(elem) for elem in ([val] + list(it))))

    def _iter_next(self, it):
        """Get next element from iterator, throw CddlValidationError instead of StopIteration."""
        try:
            next_obj = next(it)
            return next_obj
        except StopIteration:
            raise CddlValidationError("Iterator empty")

    def _decode_single_obj(self, obj):
        """Decode single CDDL value, excluding repetitions"""
        self._check_key(obj)
        obj = self._check_tag(obj)
        self._check_type(obj)
        self._check_value(obj)
        if self.type in ["UINT", "INT", "NINT", "FLOAT", "TSTR",
                         "BSTR", "BOOL", "NIL", "UNDEF", "ANY"]:
            return obj
        elif self.type == "OTHER":
            return self.my_types[self.value]._decode_single_obj(obj)
        elif self.type == "LIST":
            retval = list()
            child_val = iter(obj)
            for child in self.value:
                ret = child._decode_full(child_val)
                child_val, child_obj = ret
                child._add_if(retval, child_obj)
            self._iter_is_empty(child_val)
            return self._construct_obj(retval)
        elif self.type == "MAP":
            retval = list()
            child_val = iter(KeyTuple(item) for item in obj.items())
            for child in self.value:
                child_val, child_key_val = child._decode_full(child_val)
                child._add_if(retval, child_key_val, expect_key=True)
            self._iter_is_empty(child_val)
            return self._construct_obj(retval)
        elif self.type == "UNION":
            retval = list()
            for child in self.value:
                try:
                    child_obj = child._decode_single_obj(obj)
                    child._add_if(retval, child_obj)
                    retval.append(("union_choice", child.var_name()))
                    return self._construct_obj(retval)
                except CddlValidationError as c:
                    self.errors.append(str(c))
            self._decode_assert(False, "No matches for union: " + str(self))
        assert False, "Unexpected type: " + self.type

    def _handle_key(self, next_obj):
        """Decode key and value in the form of a KeyTuple"""
        self._decode_assert(
            isinstance(next_obj, KeyTuple), f"Expected key: {self.key} value=" + pformat(next_obj))
        key, obj = next_obj
        key_res = self.key._decode_single_obj(key)
        obj_res = self._decode_single_obj(obj)
        res = KeyTuple((key_res if not self.key.is_unambiguous() else None, obj_res))
        return res

    def _decode_obj(self, it):
        """Decode single CDDL value, excluding repetitions.

        May consume 0 to n CBOR objects via the iterator.
        """
        my_list = list()
        if self.key is not None:
            it, it_copy = tee(it)
            key_res = self._handle_key(self._iter_next(it_copy))
            return it_copy, key_res
        if self.tags:
            it, it_copy = tee(it)
            maybe_tag = next(it_copy)
            if isinstance(maybe_tag, CBORTag):
                tag_res = self._decode_single_obj(maybe_tag)
                return it_copy, tag_res
        if self.type == "OTHER" and self.key is None:
            return self.my_types[self.value]._decode_full(it)
        elif self.type == "GROUP":
            my_list = list()
            child_it = it
            for child in self.value:
                child_it, child_obj = child._decode_full(child_it)
                if child.key is not None:
                    child._add_if(my_list, child_obj, expect_key=True)
                else:
                    child._add_if(my_list, child_obj)
            ret = (child_it, self._construct_obj(my_list))
        elif self.type == "UNION":
            my_list = list()
            child_it = it
            found = False
            for child in self.value:
                try:
                    child_it, it_copy = tee(child_it)
                    child_it, child_obj = child._decode_full(child_it)
                    child._add_if(my_list, child_obj)
                    my_list.append(("union_choice", child.var_name()))
                    ret = (child_it, self._construct_obj(my_list))
                    found = True
                    break
                except CddlValidationError as c:
                    self.errors.append(str(c))
                    child_it = it_copy
            self._decode_assert(found, "No matches for union: " + str(self))
        else:
            ret = (it, self._decode_single_obj(self._iter_next(it)))
        return ret

    def _decode_full(self, it):
        """Decode single CDDL value, with repetitions.

        May consume 0 to n CBOR objects via the iterator.
        """
        if self.multi_var_condition():
            retvals = []
            for i in range(self.min_qty):
                it, retval = self._decode_obj(it)
                retvals.append(retval if not self.is_unambiguous_repeated() else None)
            try:
                for i in range(self.max_qty - self.min_qty):
                    it, it_copy = tee(it)
                    it, retval = self._decode_obj(it)
                    retvals.append(retval if not self.is_unambiguous_repeated() else None)
            except CddlValidationError as c:
                self.errors.append(str(c))
                it = it_copy
            return it, retvals
        else:
            ret = self._decode_obj(it)
            return ret

    def decode_obj(self, obj):
        """CBOR object => python object"""
        it = iter([obj])
        try:
            _, decoded = self._decode_full(it)
            self._iter_is_empty(it)
        except CddlValidationError as e:
            if self.errors:
                print("Errors:")
                pprint(self.errors)
            raise e
        return decoded

    def decode_str_yaml(self, yaml_str, yaml_compat=False):
        """YAML => python object"""
        yaml_obj = yaml_load(yaml_str)
        obj = self._from_yaml_obj(yaml_obj) if yaml_compat else yaml_obj
        self.validate_obj(obj)
        return self.decode_obj(obj)

    def decode_str(self, cbor_str):
        """CBOR bytestring => python object"""
        cbor_obj = loads(cbor_str)
        return self.decode_obj(cbor_obj)

    def validate_obj(self, obj):
        """Validate CBOR object against CDDL. Exception if not valid."""
        self.decode_obj(obj)
        return True

    def validate_str(self, cbor_str):
        """Validate CBOR bytestring against CDDL. Exception if not valid."""
        cbor_obj = loads(cbor_str)
        return self.validate_obj(cbor_obj)

    def _from_yaml_obj(self, obj):
        """Convert object from YAML/JSON (with special dicts for bstr, tag etc) to CBOR object
        that cbor2 understands.
        """
        if isinstance(obj, list):
            if len(obj) == 1 and obj[0] == "zcbor_undefined":
                return undefined
            return [self._from_yaml_obj(elem) for elem in obj]
        elif isinstance(obj, dict):
            if ["zcbor_bstr"] == list(obj.keys()):
                if isinstance(obj["zcbor_bstr"], str):
                    bstr = bytes.fromhex(obj["zcbor_bstr"])
                else:
                    bstr = dumps(self._from_yaml_obj(obj["zcbor_bstr"]))
                return bstr
            elif ["zcbor_tag", "zcbor_tag_val"] == list(obj.keys()):
                return CBORTag(obj["zcbor_tag"], self._from_yaml_obj(obj["zcbor_tag_val"]))
            retval = dict()
            for key, val in obj.items():
                match = getrp(r"zcbor_keyval\d+").fullmatch(key)
                if match is not None:
                    new_key = self._from_yaml_obj(val["key"])
                    new_val = self._from_yaml_obj(val["val"])
                    if isinstance(new_key, list):
                        new_key = tuple(new_key)
                    retval[new_key] = new_val
                else:
                    retval[key] = self._from_yaml_obj(val)
            return retval
        return obj

    def _to_yaml_obj(self, obj):
        """inverse of _from_yaml_obj"""
        if isinstance(obj, list) or isinstance(obj, tuple):
            return [self._to_yaml_obj(elem) for elem in obj]
        elif isinstance(obj, (dict, zfrozendict)):
            retval = dict()
            i = 0
            for key, val in obj.items():
                if not isinstance(key, str):
                    retval[f"zcbor_keyval{i}"] = {
                        "key": self._to_yaml_obj(key), "val": self._to_yaml_obj(val)}
                    i += 1
                else:
                    retval[key] = self._to_yaml_obj(val)
            return retval
        elif isinstance(obj, bytes):
            try:
                bstr_obj = self._to_yaml_obj(strict_loads(obj))
            except CBORDecodeError:
                bstr_obj = obj.hex()
            return {"zcbor_bstr": bstr_obj}
        elif isinstance(obj, CBORTag):
            return {"zcbor_tag": obj.tag, "zcbor_tag_val": self._to_yaml_obj(obj.value)}
        elif obj is undefined:
            return ["zcbor_undefined"]
        assert not isinstance(obj, bytes)
        return obj

    def from_yaml(self, yaml_str, yaml_compat=False):
        """YAML str => CBOR bytestr"""
        yaml_obj = yaml_load(yaml_str)
        obj = self._from_yaml_obj(yaml_obj) if yaml_compat else yaml_obj
        self.validate_obj(obj)
        return dumps(obj)

    def obj_to_yaml(self, obj, yaml_compat=False):
        """CBOR object => YAML str"""
        self.validate_obj(obj)
        yaml_obj = self._to_yaml_obj(obj) if yaml_compat else obj
        return yaml_dump(yaml_obj)

    def str_to_yaml(self, cbor_str, yaml_compat=False):
        """CBOR bytestring => YAML str"""
        return self.obj_to_yaml(loads(cbor_str), yaml_compat=yaml_compat)

    def from_json(self, json_str, yaml_compat=False):
        """JSON str => CBOR bytestr"""
        json_obj = json_load(json_str)
        obj = self._from_yaml_obj(json_obj) if yaml_compat else json_obj
        self.validate_obj(obj)
        return dumps(obj)

    def obj_to_json(self, obj, yaml_compat=False):
        """CBOR object => JSON str"""
        self.validate_obj(obj)
        json_obj = self._to_yaml_obj(obj) if yaml_compat else obj

        def zfrozendict_to_dict(obj):
            if isinstance(obj, zfrozendict):
                return dict(obj)
            raise TypeError(f"Object of type {type(obj)} is not JSON serializable")

        return json_dump(json_obj, default=zfrozendict_to_dict)

    def str_to_json(self, cbor_str, yaml_compat=False):
        """CBOR bytestring => JSON str"""
        return self.obj_to_json(loads(cbor_str), yaml_compat=yaml_compat)

    def str_to_c_code(self, cbor_str, var_name, columns=0):
        """CBOR bytestring => C code (uint8_t array initialization)"""
        arr = ", ".join(f"0x{c:02x}" for c in cbor_str)
        if columns:
            arr = '\n' + indent("\n".join(wrap(arr, 6 * columns)), '\t') + '\n'
        return f'uint8_t {var_name}[] = {{{arr}}};\n'


class XcoderTuple(NamedTuple):
    body: list
    func_name: str
    type_name: str


class CddlTypes(NamedTuple):
    my_types: dict
    my_control_groups: dict


class CodeGenerator(CddlXcoder):
    """Class for generating C code that encode/decodes CBOR and validates it according to the CDDL.
    """
    def __init__(self, mode, entry_type_names, default_bit_size, *args, **kwargs):
        super(CodeGenerator, self).__init__(*args, **kwargs)
        self.mode = mode
        self.entry_type_names = entry_type_names
        self.default_bit_size = default_bit_size

    @classmethod
    def from_cddl(cddl_class, mode, *args, **kwargs):
        cddl_res = super(CodeGenerator, cddl_class).from_cddl(*args, **kwargs)

        # set access prefix (struct access paths) for all the definitions.
        for my_type in cddl_res.my_types:
            cddl_res.my_types[my_type].set_access_prefix(f"(*{struct_ptr_name(mode)})")

        return cddl_res

    def is_entry_type(self):
        """Whether this element (an OTHER) refers to an entry type."""
        return (self.type == "OTHER") and (self.value in self.entry_type_names)

    def is_cbor(self):
        """Whether to include a "cbor" variable for this element."""
        res = (self.type_name() is not None) and not self.is_entry_type() and (
            (self.type != "OTHER") or self.my_types[self.value].is_cbor())
        return res

    def init_args(self):
        return (self.mode, self.entry_type_names, self.default_bit_size, self.default_max_qty)

    def delegate_type_condition(self):
        """Whether to use the C type of the first child as this type's C type"""
        ret = self.skip_condition() and (self.multi_var_condition()
                                         or self.self_repeated_multi_var_condition()
                                         or self.range_check_condition()
                                         or (self in self.my_types.values()))
        return ret

    def is_delegated_type(self):
        return self.is_delegated

    def present_var(self):
        """Declaration of the "present" variable for this element."""
        return ["bool %s;" % self.present_var_name()]

    def count_var(self):
        """Declaration of the "count" variable for this element."""
        return ["size_t %s;" % self.count_var_name()]

    def anonymous_choice_var(self):
        """Declaration of the "choice" variable for this element."""
        int_vals = self.all_children_int_disambiguated()
        return self.enclose("enum", [val.enum_var(int_vals) + "," for val in self.value])

    def choice_var(self):
        """Declaration of the "choice" variable for this element."""
        var = self.anonymous_choice_var()
        var[-1] += f" {self.choice_var_name()};"
        return var

    def child_declarations(self):
        """Declaration of the variables of all children."""
        decl = [line for child in self.value for line in child.full_declaration()]
        return decl

    def child_single_declarations(self):
        """Declaration of the variables of all children."""
        decl = list()
        for child in self.value:
            if not child.is_unambiguous_repeated():
                decl.extend(child.single_declaration())
        return decl

    def simple_func_condition(self):
        if self.range_check_condition():
            return True
        if self.single_func_impl_condition():
            return True
        if self.type == "OTHER" and self.my_types[self.value].simple_func_condition():
            return True
        return False

    def raw_type_name(self):
        """Base name if this element needs to declare a type."""
        return "struct %s" % self.id()

    def enum_type_name(self):
        return "enum %s" % self.id()

    def bit_size(self):
        """The bit width of the integers as represented in code."""
        bit_size = None
        if self.type in ["UINT", "INT", "NINT"]:
            assert self.default_bit_size in [32, 64], "The default_bit_size must be 32 or 64."
            if self.default_bit_size == 64:
                bit_size = 64
            else:
                bit_size = 32

                for v in [self.value or 0, self.max_value or 0, self.min_value or 0]:
                    if (type(v) is str):
                        if "64" in v:
                            bit_size = 64
                    elif self.type == "UINT":
                        if (v > UINT32_MAX):
                            bit_size = 64
                    else:
                        if (v > INT32_MAX) or (v < INT32_MIN):
                            bit_size = 64
        return bit_size

    def float_type(self):
        """If this is a floating point number, return the C type to use for it."""
        if self.type != "FLOAT":
            return None

        max_size = self.max_size or 8

        if max_size <= 4:
            return "float"
        elif max_size == 8:
            return "double"
        else:
            raise TypeError("Floats must have 4 or 8 bytes of precision.")

    def val_type_name(self):
        """Name of the type of this element's actual value variable."""
        if self.multi_val_condition():
            return self.raw_type_name()

        # Will fail runtime if we don't use lambda for type_name()
        # pylint: disable=unnecessary-lambda
        name = {
            "INT": lambda: f"int{self.bit_size()}_t",
            "UINT": lambda: f"uint{self.bit_size()}_t",
            "NINT": lambda: f"int{self.bit_size()}_t",
            "FLOAT": lambda: self.float_type(),
            "BSTR": lambda: "struct zcbor_string",
            "TSTR": lambda: "struct zcbor_string",
            "BOOL": lambda: "bool",
            "NIL": lambda: None,
            "UNDEF": lambda: None,
            "ANY": lambda: None,
            "LIST": lambda: self.value[0].type_name() if len(self.value) >= 1 else None,
            "MAP": lambda: self.value[0].type_name() if len(self.value) >= 1 else None,
            "GROUP": lambda: self.value[0].type_name() if len(self.value) >= 1 else None,
            "UNION": lambda: self.union_type(),
            "OTHER": lambda: self.my_types[self.value].type_name(),
        }[self.type]()

        return name

    def repeated_type_name(self):
        """Name of the type for the repeated part of this element.

        I.e. the part that happens multiple times if the element has a quantifier.
        not including things like the "count" or "present" variable.
        """
        if self.self_repeated_multi_var_condition():
            name = self.raw_type_name()
            if self.val_type_name() == name:
                name = name + "_r"
        else:
            name = self.val_type_name()
        return name

    def type_name(self):
        """Name of the type for this element."""
        if self.multi_var_condition():
            name = self.raw_type_name()
        else:
            name = self.repeated_type_name()
        return name

    def add_var_name(self, var_type, full=False, anonymous=False):
        """Take a multi member type name and create a variable declaration.

        Make it an array if the element is repeated.
        """
        if var_type:
            assert (var_type[-1][-1] == "}" or len(var_type) == 1), \
                f"Expected single var: {var_type!r}"
            if not anonymous or var_type[-1][-1] != "}":
                var_name = self.var_name()
                array_part = f"[{self.max_qty}]" if full and self.max_qty != 1 else ""
                var_type[-1] += f" {var_name}{array_part}"
            var_type = add_semicolon(var_type)
        return var_type

    def var_type(self):
        """The type for this element as a member variable."""
        if not self.multi_val_condition() and self.val_type_name() is not None:
            return [self.val_type_name()]
        elif self.type == "UNION":
            return self.union_type()
        return []

    def enclose(self, ingress, declaration):
        """Enclose a list of declarations in a block (struct, union or enum)."""
        if declaration:
            return [f"{ingress} {{"] + [indentation + line for line in declaration] + ["}"]
        else:
            return []

    def union_type(self):
        """Type declaration for unions."""
        declaration = self.enclose("union", self.child_single_declarations())
        return declaration

    def single_declaration(self):
        return self.add_var_name(self.single_var_type(), anonymous=True)

    def repeated_declaration(self):
        """Declaration of the repeated part of this element."""
        if self.is_unambiguous_repeated():
            return []

        var_type = self.var_type()
        multi_var = False

        decl = []

        if not self.skip_condition():
            decl += self.add_var_name(var_type, anonymous=(self.type == "UNION"))

        if self.type in ["LIST", "MAP", "GROUP"]:
            decl += self.child_declarations()
            multi_var = len(decl) > 1

        if self.reduced_key_var_condition():
            key_var = self.key.full_declaration()
            decl = key_var + decl
            multi_var = key_var != []

        if self.choice_var_condition():
            choice_var = self.choice_var()
            decl += choice_var
            multi_var = choice_var != []

        if self.cbor_var_condition():
            cbor_var = self.cbor.full_declaration()
            decl += cbor_var
            multi_var = cbor_var != []

        return decl

    def full_declaration(self):
        """Declaration of the full type for this element."""
        multi_var = False

        if self.is_unambiguous():
            return []

        if self.multi_var_condition():
            if self.is_unambiguous_repeated():
                decl = []
            else:
                decl = self.add_var_name(
                    [self.repeated_type_name()]
                    if self.repeated_type_name() is not None else [], full=True)
        else:
            decl = self.repeated_declaration()

        if self.count_var_condition():
            count_var = self.count_var()
            decl += count_var
            multi_var = count_var != []

        if self.present_var_condition():
            present_var = self.present_var()
            decl += present_var
            multi_var = present_var != []

        assert multi_var == self.multi_var_condition()

        return decl

    def single_var_type(self, full=True):
        """Return the type definition of this element.

        If there are multiple variables, wrap them in a
        struct so the function always returns a single type with no name. If full is False, only
        repeated part is used.
        """
        if full and self.multi_member():
            return self.enclose("struct", self.full_declaration())
        elif not full and self.repeated_multi_var_condition():
            return self.enclose("struct", self.repeated_declaration())
        else:
            return self.var_type()

    def type_def(self):
        """Return the type definition of this element, and all its children + key + cbor."""
        ret_val = []
        if self.type in ["LIST", "MAP", "GROUP", "UNION"]:
            ret_val.extend(
                [elem for typedef in [
                    child.type_def() for child in self.value] for elem in typedef])
        if self.bits:
            ret_val.extend(self.my_control_groups[self.bits].type_def_bits())
        if self.cbor_var_condition():
            ret_val.extend(self.cbor.type_def())
        if self.reduced_key_var_condition():
            ret_val.extend(self.key.type_def())
        if self.type == "OTHER":
            ret_val.extend(self.my_types[self.value].type_def())
        if self.repeated_type_def_condition():
            type_def_list = self.single_var_type(full=False)
            if type_def_list:
                ret_val.extend([(self.single_var_type(full=False), self.repeated_type_name())])
        if self.type_def_condition():
            type_def_list = self.single_var_type()
            if type_def_list:
                ret_val.extend([(self.single_var_type(), self.type_name())])
        return ret_val

    def type_def_bits(self):
        tdef = self.anonymous_choice_var()
        return [(tdef, self.enum_type_name())]

    def float_prefix(self):
        if self.type != "FLOAT":
            return ""

        min_size = self.min_size or 2
        max_size = self.max_size or 8

        if max_size == 2:
            return "float16"
        elif min_size == 2 and max_size == 4:
            return "float16_32" if self.mode == "decode" else "float32"
        if min_size == 4 and max_size == 4:
            return "float32"
        elif min_size == 4 and max_size == 8:
            return "float32_64" if self.mode == "decode" else "float64"
        elif min_size == 8 and max_size == 8:
            return "float64"
        elif min_size <= 4 and max_size == 8:
            return "float" if self.mode == "decode" else "float64"
        else:
            raise TypeError("Floats must have 2, 4 or 8 bytes of precision.")

    def single_func_prim_prefix(self):
        if self.type == "OTHER":
            return self.my_types[self.value].single_func_prim_prefix()
        return ({
            "INT": f"zcbor_int{self.bit_size()}",
            "UINT": f"zcbor_uint{self.bit_size()}",
            "NINT": f"zcbor_int{self.bit_size()}",
            "FLOAT": f"zcbor_{self.float_prefix()}",
            "BSTR": f"zcbor_bstr",
            "TSTR": f"zcbor_tstr",
            "BOOL": f"zcbor_bool",
            "NIL": f"zcbor_nil",
            "UNDEF": f"zcbor_undefined",
            "ANY": f"zcbor_any",
        }[self.type])

    def xcode_func_name(self):
        """Name of the encoder/decoder function for this element."""
        return f"{self.mode}_{self.var_name(with_prefix=True, observe_skipped=False)}"

    def repeated_xcode_func_name(self):
        """Name of the encoder/decoder function for the repeated part of this element."""
        return f"{self.mode}_repeated_{self.var_name(with_prefix=True, observe_skipped=False)}"

    def single_func_prim_name(self, union_int=None, ptr_result=False):
        """Function name for xcoding this type, when it is a primitive type"""
        ptr_variant = ptr_result and self.type in ["UINT", "INT", "NINT", "FLOAT", "BOOL"]
        func_prefix = self.single_func_prim_prefix()
        if self.mode == "decode":
            if self.type == "ANY":
                func = "zcbor_any_skip"
            elif not self.is_unambiguous_value():
                func = f"{func_prefix}_decode"
            elif not union_int:
                func = f"{func_prefix}_{'pexpect' if ptr_variant else 'expect'}"
            elif union_int == "EXPECT":
                assert not ptr_variant, \
                       "Programmer error: invalid use of expect_union."
                func = f"{func_prefix}_expect_union"
            elif union_int == "DROP":
                return None
        else:
            if self.type == "ANY":
                func = "zcbor_nil_put"
            elif (not self.is_unambiguous_value()) or self.type in ["TSTR", "BSTR"] or ptr_variant:
                func = f"{func_prefix}_encode"
            else:
                func = f"{func_prefix}_put"
        return func

    def single_func_prim(self, access, union_int=None, ptr_result=False):
        """Return the function name and arguments to call to encode/decode this element.

        Only used when this element DOESN'T define its own encoder/decoder function (when it's a
        primitive type, for which functions already exist, or when the function is defined elsewhere
        ("OTHER"))
        """
        assert self.type not in ["LIST", "MAP"], "Must have wrapper function for list or map."

        if self.type == "GROUP":
            assert len(self.value) == 0, "Group should have no children to get here."
            return (None, None)

        if self.type == "OTHER":
            return self.my_types[self.value].single_func(access, union_int)

        func_name = self.single_func_prim_name(union_int, ptr_result=ptr_result)
        if func_name is None:
            return (None, None)

        if self.type in ["NIL", "UNDEF", "ANY"]:
            arg = "NULL"
        elif not self.is_unambiguous_value():
            arg = deref_if_not_null(access)
        elif self.type in ["BSTR", "TSTR"]:
            arg = tmp_str_or_null(self.value)
        elif self.type in ["UINT", "INT", "NINT", "FLOAT", "BOOL"]:
            value = val_to_str(self.value)
            arg = (f"&({self.val_type_name()}){{{value}}}" if ptr_result else value)
        else:
            assert False, "Should not come here."

        return (func_name, arg)

    def single_func(self, access=None, union_int=None):
        """Return the function name and arguments to call to encode/decode this element."""
        if self.single_func_impl_condition():
            return (self.xcode_func_name(), deref_if_not_null(access or self.var_access()))
        else:
            return self.single_func_prim(access or self.val_access(), union_int)

    def repeated_single_func(self, ptr_result=False):
        """Return the function name and arguments to call to encode/decode the repeated
        part of this element.
        """
        if self.repeated_single_func_impl_condition():
            return (self.repeated_xcode_func_name(), deref_if_not_null(self.repeated_val_access()))
        else:
            return self.single_func_prim(self.repeated_val_access(), ptr_result=ptr_result)

    def has_backup(self):
        return (self.cbor_var_condition() or self.type in ["LIST", "MAP", "UNION"])

    def num_backups(self):
        total = 0
        if self.key:
            total += self.key.num_backups()
        if self.cbor_var_condition():
            total += self.cbor.num_backups()
        if self.type in ["LIST", "MAP", "GROUP", "UNION"]:
            total += max([child.num_backups() for child in self.value] + [0])
        if self.type == "OTHER":
            total += self.my_types[self.value].num_backups()
        if self.has_backup():
            total += 1
        return total

    def depends_on(self):
        """Return a number indicating how many other elements this element depends on.

        Used for putting functions and typedefs in the right order.
        """
        ret_vals = [1]

        if not self.dependsOnCall:
            self.dependsOnCall = True
            if self.cbor_var_condition():
                ret_vals.append(self.cbor.depends_on())
            if self.key:
                ret_vals.append(self.key.depends_on())
            if self.type == "OTHER":
                ret_vals.append(1 + self.my_types[self.value].depends_on())
            if self.type in ["LIST", "MAP", "GROUP", "UNION"]:
                ret_vals.extend(child.depends_on() for child in self.value)
            self.dependsOnCall = False

        return max(ret_vals)

    def xcode_single_func_prim(self, union_int=None):
        """Make a string from the list returned by single_func_prim()"""
        return xcode_statement(*self.single_func_prim(self.val_access(), union_int))

    def list_counts(self):
        """Recursively sum the total minimum and maximum element count for this element."""
        retval = ({
            "INT": lambda: (self.min_qty, self.max_qty),
            "UINT": lambda: (self.min_qty, self.max_qty),
            "NINT": lambda: (self.min_qty, self.max_qty),
            "FLOAT": lambda: (self.min_qty, self.max_qty),
            "BSTR": lambda: (self.min_qty, self.max_qty),
            "TSTR": lambda: (self.min_qty, self.max_qty),
            "BOOL": lambda: (self.min_qty, self.max_qty),
            "NIL": lambda: (self.min_qty, self.max_qty),
            "UNDEF": lambda: (self.min_qty, self.max_qty),
            "ANY": lambda: (self.min_qty, self.max_qty),
            # Lists are their own element
            "LIST": lambda: (self.min_qty, self.max_qty),
            # Maps are their own element
            "MAP": lambda: (self.min_qty, self.max_qty),
            "GROUP": lambda: (self.min_qty * sum((child.list_counts()[0] for child in self.value)),
                              self.max_qty * sum((child.list_counts()[1] for child in self.value))),
            "UNION": lambda: (self.min_qty * min((child.list_counts()[0] for child in self.value)),
                              self.max_qty * max((child.list_counts()[1] for child in self.value))),
            "OTHER": lambda: (self.min_qty * self.my_types[self.value].list_counts()[0],
                              self.max_qty * self.my_types[self.value].list_counts()[1]),
        }[self.type]())
        return retval

    def xcode_list(self):
        """Return the full code needed to encode/decode a "LIST" or "MAP" element with children."""
        start_func = f"zcbor_{self.type.lower()}_start_{self.mode}"
        end_func = f"zcbor_{self.type.lower()}_end_{self.mode}"
        end_func_force = f"zcbor_list_map_end_force_{self.mode}"
        assert start_func in [
            "zcbor_list_start_decode", "zcbor_list_start_encode",
            "zcbor_map_start_decode", "zcbor_map_start_encode"]
        assert end_func in [
            "zcbor_list_end_decode", "zcbor_list_end_encode",
            "zcbor_map_end_decode", "zcbor_map_end_encode"]
        assert self.type in ["LIST", "MAP"], \
            "Expected LIST or MAP type, was %s." % self.type
        _, max_counts = zip(
            *(child.list_counts() for child in self.value)) if self.value else ((0,), (0,))
        count_arg = f', {str(sum(max_counts))}' if self.mode == 'encode' else ''
        with_children = "(%s && ((%s) || (%s, false)) && %s)" % (
            f"{start_func}(state{count_arg})",
            f"{newl_ind}&& ".join(child.full_xcode() for child in self.value),
            f"{end_func_force}(state)",
            f"{end_func}(state{count_arg})")
        without_children = "(%s && %s)" % (
            f"{start_func}(state{count_arg})",
            f"{end_func}(state{count_arg})")
        return with_children if len(self.value) > 0 else without_children

    def xcode_group(self, union_int=None):
        """Return the full code needed to encode/decode a "GROUP" element's children."""
        assert self.type in ["GROUP"], "Expected GROUP type."
        return "(%s)" % (newl_ind + "&& ").join(
            [self.value[0].full_xcode(union_int)]
            + [child.full_xcode() for child in self.value[1:]])

    def xcode_union(self):
        """Return the full code needed to encode/decode a "UNION" element's children."""
        assert self.type in ["UNION"], "Expected UNION type."
        if self.mode == "decode":
            if self.all_children_int_disambiguated():
                lines = []
                lines.extend(
                    ["((%s == %s) && (%s))" %
                        (self.choice_var_access(), child.enum_var_name(),
                            child.full_xcode(union_int="DROP"))
                        for child in self.value])
                bit_size = self.value[0].bit_size()
                func = f"zcbor_uint_{self.mode}" if self.all_children_uint_disambiguated() else \
                       f"zcbor_int_{self.mode}"
                return "((%s) && (%s))" % (
                    f"({func}(state, &{self.choice_var_access()}, "
                    + f"sizeof({self.choice_var_access()})))",
                    "((" + f"{newl_ind}|| ".join(lines)
                         + ") || (zcbor_error(state, ZCBOR_ERR_WRONG_VALUE), false))",)

            child_values = ["(%s && ((%s = %s), true))" %
                            (child.full_xcode(
                                union_int="EXPECT" if child.is_int_disambiguated() else None),
                                self.choice_var_access(), child.enum_var_name())
                            for child in self.value]

            # Reset state for all but the first child.
            for i in range(1, len(child_values)):
                if ((not self.value[i].is_int_disambiguated())
                        and self.value[i - 1].simple_func_condition()):
                    child_values[i] = f"(zcbor_union_elem_code(state) && {child_values[i]})"

            return "(%s && (int_res = (%s), %s, int_res))" \
                % ("zcbor_union_start_code(state)",
                   f"{newl_ind}|| ".join(child_values),
                   "zcbor_union_end_code(state)")
        else:
            return ternary_if_chain(
                self.choice_var_access(),
                [child.enum_var_name() for child in self.value],
                [child.full_xcode() for child in self.value])

    def xcode_bstr(self):
        if self.cbor and not self.cbor.is_entry_type():
            access_arg = f', {deref_if_not_null(self.val_access())}' if self.mode == 'decode' \
                else ''
            res_arg = f', &tmp_str' if self.mode == 'encode' \
                else ''
            xcode_cbor = "(%s)" % ((newl_ind + "&& ").join(
                [f"zcbor_bstr_start_{self.mode}(state{access_arg})",
                 f"(int_res = ({self.cbor.full_xcode()}), "
                 f"zcbor_bstr_end_{self.mode}(state{res_arg}), int_res)"]))
            if self.mode == "decode" or self.is_unambiguous():
                return xcode_cbor
            else:
                return f"({self.val_access()}.value " \
                    f"? (memcpy(&tmp_str, &{self.val_access()}, sizeof(tmp_str)), " \
                    f"{self.xcode_single_func_prim()}) : ({xcode_cbor}))"
        return self.xcode_single_func_prim()

    def xcode_tags(self):
        return [f"zcbor_tag_{'put' if (self.mode == 'encode') else 'expect'}(state, {tag})"
                for tag in self.tags]

    def value_suffix(self, value_str):
        """Appends ULL or LL if a value exceeding 32-bits is used"""
        if not value_str.isdigit():
            return ""
        value = int(value_str)
        if self.type == "INT" or self.type == "NINT":
            if value > INT32_MAX or value <= INT32_MIN:
                return "LL"
        elif self.type == "UINT":
            if value > UINT32_MAX:
                return "ULL"

        return ""

    def range_checks(self, access):
        """Return the code needed to check the size/value bounds of this element."""
        if self.type != "OTHER" and self.value is not None:
            return []

        range_checks = []

        # Remove unneeded checks when the bounds are (U)INT64_(MIN|MAX)
        exc_vals = [UINT64_MAX, 0] if self.type == "UINT" else [INT64_MAX, INT64_MIN]
        min_val = self.min_value if self.min_value not in exc_vals else None
        max_val = self.max_value if self.max_value not in exc_vals else None

        if self.type in ["INT", "UINT", "NINT", "FLOAT", "BOOL"]:
            if min_val is not None and min_val == max_val:
                range_checks.append(f"({access} == {val_to_str(min_val)}"
                                    f"{self.value_suffix(val_to_str(min_val))})")
            else:
                if min_val is not None:
                    range_checks.append(f"({access} >= {val_to_str(min_val)}"
                                        f"{self.value_suffix(val_to_str(min_val))})")
                if max_val is not None:
                    range_checks.append(f"({access} <= {val_to_str(max_val)}"
                                        f"{self.value_suffix(val_to_str(max_val))})")
            if self.bits:
                range_checks.append(
                    f"!({access} & ~("
                    + ' | '.join([f'(1 << {c.enum_var_name()})'
                                 for c in self.my_control_groups[self.bits].value])
                    + "))")
        elif self.type in ["BSTR", "TSTR"]:
            if self.min_size is not None and self.min_size == self.max_size:
                range_checks.append(f"({access}.len == {val_to_str(self.min_size)})")
            else:
                if self.min_size is not None:
                    range_checks.append(f"({access}.len >= {val_to_str(self.min_size)})")
                if self.max_size is not None:
                    range_checks.append(f"({access}.len <= {val_to_str(self.max_size)})")
        elif self.type == "OTHER":
            if not self.my_types[self.value].single_func_impl_condition():
                range_checks.extend(self.my_types[self.value].range_checks(access))

        if range_checks:
            range_checks[0] = "((" + range_checks[0]
            range_checks[-1] = range_checks[-1] \
                + ") || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))"

        return range_checks

    def repeated_xcode(self, union_int=None):
        """Return the full code needed to encode/decode this element.

        Including children, key and cbor, excluding repetitions.
        """
        val_union_int = union_int if not self.key else None  # In maps, only pass union_int to key.
        range_checks = self.range_checks(self.val_access())
        xcoder = {
            "INT": self.xcode_single_func_prim,
            "UINT": lambda: self.xcode_single_func_prim(val_union_int),
            "NINT": lambda: self.xcode_single_func_prim(val_union_int),
            "FLOAT": self.xcode_single_func_prim,
            "BSTR": self.xcode_bstr,
            "TSTR": self.xcode_single_func_prim,
            "BOOL": self.xcode_single_func_prim,
            "NIL": self.xcode_single_func_prim,
            "UNDEF": self.xcode_single_func_prim,
            "ANY": self.xcode_single_func_prim,
            "LIST": self.xcode_list,
            "MAP": self.xcode_list,
            "GROUP": lambda: self.xcode_group(val_union_int),
            "UNION": self.xcode_union,
            "OTHER": lambda: self.xcode_single_func_prim(val_union_int),
        }[self.type]
        xcoders = []
        if self.key:
            xcoders.append(self.key.full_xcode(union_int))
        if self.tags:
            xcoders.extend(self.xcode_tags())
        if self.mode == "decode":
            xcoders.append(xcoder())
            xcoders.extend(range_checks)
        elif self.type == "BSTR" and self.cbor:
            xcoders.append(xcoder())
            xcoders.extend(self.range_checks("tmp_str"))
        else:
            xcoders.extend(range_checks)
            xcoders.append(xcoder())

        return "(%s)" % ((newl_ind + "&& ").join(xcoders),)

    def result_len(self):
        """Code for the size of the repeated part of this element."""
        if self.repeated_type_name() is None or self.is_unambiguous_repeated():
            return "0"
        else:
            return "sizeof(%s)" % self.repeated_type_name()

    def full_xcode(self, union_int=None):
        """Return the full code needed to encode/decode this element.

        Including children, key, cbor, and repetitions.
        """
        if self.present_var_condition():
            if self.mode == "encode":
                func, *arguments = self.repeated_single_func(ptr_result=False)
                return f"(!{self.present_var_access()} || {func}({xcode_args(*arguments)}))"
            else:
                assert self.mode == "decode", \
                    f"This code needs self.mode to be 'decode', not {self.mode}."
                if not self.repeated_single_func_impl_condition():
                    decode_str = self.repeated_xcode(union_int)
                    return f"({self.present_var_access()} = {self.repeated_xcode(union_int)}, 1)"
                func, *arguments = self.repeated_single_func(ptr_result=True)
                return (
                    f"zcbor_present_decode(&(%s), (zcbor_decoder_t *)%s, %s)" %
                    (self.present_var_access(), func, xcode_args(*arguments),))
        elif self.count_var_condition():
            func, arg = self.repeated_single_func(ptr_result=True)

            minmax = "_minmax" if self.mode == "encode" else ""
            mode = self.mode
            return (
                f"zcbor_multi_{mode}{minmax}(%s, %s, &%s, (zcbor_{mode}r_t *)%s, %s, %s)" %
                (self.min_qty,
                 self.max_qty,
                 self.count_var_access(),
                 func,
                 xcode_args("*" + arg if arg != "NULL" and self.result_len() != "0" else arg),
                 self.result_len()))
        else:
            return self.repeated_xcode(union_int)

    def xcode(self):
        """Return the body of the encoder/decoder function for this element."""
        return self.full_xcode()

    def xcoders(self):
        """Recursively return a list of the bodies of the encoder/decoder functions for
        this element and its children + key + cbor.
        """
        if self.type in ["LIST", "MAP", "GROUP", "UNION"]:
            for child in self.value:
                for xcoder in child.xcoders():
                    yield xcoder
        if self.cbor:
            for xcoder in self.cbor.xcoders():
                yield xcoder
        if self.key:
            for xcoder in self.key.xcoders():
                yield xcoder
        if self.type == "OTHER" and self.value not in self.entry_type_names:
            for xcoder in self.my_types[self.value].xcoders():
                yield xcoder
        if self.repeated_single_func_impl_condition():
            yield XcoderTuple(
                self.repeated_xcode(), self.repeated_xcode_func_name(), self.repeated_type_name())
        if (self.single_func_impl_condition()):
            xcode_body = self.xcode()
            yield XcoderTuple(xcode_body, self.xcode_func_name(), self.type_name())

    def public_xcode_func_sig(self):
        type_name = self.type_name() if struct_ptr_name(self.mode) in self.full_xcode() else "void"
        return f"""
int cbor_{self.xcode_func_name()}(
		{"const " if self.mode == "decode" else ""}uint8_t *payload, size_t payload_len,
		{"" if self.mode == "decode" else "const "}{type_name} *{struct_ptr_name(self.mode)},
		{"size_t *payload_len_out"})"""


class CodeRenderer():
    def __init__(self, entry_types, modes, print_time, default_max_qty, git_sha='', file_header=''):
        self.entry_types = entry_types
        self.print_time = print_time
        self.default_max_qty = default_max_qty

        self.sorted_types = dict()
        self.functions = dict()
        self.type_defs = dict()

        # Sort type definitions so the typedefs will come in the correct order in the header file
        # and the function in the correct order in the c file.
        for mode in modes:
            self.sorted_types[mode] = list(sorted(
                self.entry_types[mode], key=lambda _type: _type.depends_on(), reverse=False))

            self.functions[mode] = self.unique_funcs(mode)
            self.functions[mode] = self.used_funcs(mode)
            self.type_defs[mode] = self.unique_types(mode)

        self.version = __version__

        if git_sha:
            self.version += f'-{git_sha}'

        self.file_header = file_header.strip() + "\n\n" if file_header.strip() else ""
        self.file_header += f"""Generated using zcbor version {self.version}
https://github.com/NordicSemiconductor/zcbor{'''
at: ''' + datetime.now().strftime('%Y-%m-%d %H:%M:%S') if self.print_time else ''}
Generated with a --default-max-qty of {self.default_max_qty}"""

    def header_guard(self, file_name):
        return path.basename(file_name).replace(".", "_").replace("-", "_").upper() + "__"

    def unique_types(self, mode):
        """Return a list of typedefs for all defined types, with duplicate typedefs
        removed.
        """
        type_names = {}
        out_types = []
        for mtype in self.sorted_types[mode]:
            for type_def in mtype.type_def():
                type_name = type_def[1]
                if type_name not in type_names.keys():
                    type_names[type_name] = type_def[0]
                    out_types.append(type_def)
                else:
                    assert (''.join(type_names[type_name]) == ''.join(type_def[0])), f"""
Two elements share the type name {type_name}, but their implementations are not identical.
Please change one or both names. They are
{linesep.join(type_names[type_name])}
and
{linesep.join(type_def[0])}"""
        return out_types

    def unique_funcs(self, mode):
        """Return a list of encoder/decoder functions for all defined types, with duplicate
        functions removed.
        """
        func_names = {}
        out_types = []
        for mtype in self.sorted_types[mode]:
            xcoders = list(mtype.xcoders())
            for funcType in xcoders:
                func_xcode = funcType[0]
                func_name = funcType[1]
                if func_name not in func_names.keys():
                    func_names[func_name] = funcType
                    out_types.append(funcType)
                elif func_name in func_names.keys():
                    assert func_names[func_name][0] == func_xcode, \
                        ("Two elements share the function name %s, but their implementations are "
                            + "not identical. Please change one or both names.\n\n%s\n\n%s") % \
                        (func_name, func_names[func_name][0], func_xcode)

        return out_types

    def used_funcs(self, mode):
        """Return a list of encoder/decoder functions for all defined types, with unused
        functions removed.
        """
        mod_entry_types = [
            XcoderTuple(
                func_type.xcode(),
                func_type.xcode_func_name(),
                func_type.type_name()) for func_type in self.entry_types[mode]]
        out_types = [func_type for func_type in mod_entry_types]
        full_code = "".join([func_type[0] for func_type in mod_entry_types])
        for func_type in reversed(self.functions[mode]):
            func_name = func_type[1]
            if func_type not in mod_entry_types and getrp(r"%s\W" % func_name).search(full_code):
                full_code += func_type[0]
                out_types.append(func_type)
        return list(reversed(out_types))

    def render_forward_declaration(self, xcoder, mode):
        """Render a single decoding function with signature and body."""
        return f"""
static bool {xcoder.func_name}(zcbor_state_t *state, {"" if mode == "decode" else "const "}{
            xcoder.type_name
            if struct_ptr_name(mode) in xcoder.body else "void"} *{struct_ptr_name(mode)});
            """.strip()

    def render_function(self, xcoder, mode):
        body = xcoder.body

        # Define the subroutine "paren" that matches parenthesised expressions.
        paren_re = r'(?(DEFINE)(?P<paren>\(((?>[^\(\)]+|(?&paren))*)\)))'
        # This uses "paren" to match a single argument to a function.
        arg_re = rf'([^,\(\)]|(?&paren))+'
        # Match a function pointer argument to a function.
        func_re = rf'\(zcbor_(en|de)coder_t \*\)(?P<func>{arg_re})'
        # Match a triplet of function pointer, state arg, and result arg.
        call_re = rf'{func_re}, (?P<state>{arg_re}), (?P<arg>{arg_re})'
        multi_re = rf'{paren_re}zcbor_multi_(en|de)code\(({arg_re},){{3}} {call_re}'
        present_re = rf'{paren_re}zcbor_present_(en|de)code\({arg_re}, {call_re}\)'
        map_re = rf'{paren_re}zcbor_unordered_map_search\({call_re}\)'
        all_funcs = chain(getrp(multi_re).finditer(body),
                          getrp(present_re).finditer(body),
                          getrp(map_re).finditer(body))
        arg_test = ""
        calls = ("\n		".join(
            (f"{m.group('func')}({m.group('state')}, {m.group('arg')});" for m in (all_funcs))))
        if calls != "":
            arg_test = f"""
	if (false) {{
		/* For testing that the types of the arguments are correct.
		 * A compiler error here means a bug in zcbor.
		 */
		{calls}
	}}
"""
        return f"""
static bool {xcoder.func_name}(
		zcbor_state_t *state, {"" if mode == "decode" else "const "}{
            xcoder.type_name
            if struct_ptr_name(mode) in body else "void"} *{struct_ptr_name(mode)})
{{
	zcbor_log("%s\\r\\n", __func__);
	{"struct zcbor_string tmp_str;" if "tmp_str" in body else ""}
	{"bool int_res;" if "int_res" in body else ""}

	bool res = ({body});
{arg_test}
	log_result(state, res, __func__);
	return res;
}}""".replace("	\n", "")  # call replace() to remove empty lines.

    def render_entry_function(self, xcoder, mode):
        """Render a single entry function (API function) with signature and body."""
        func_name, func_arg = (xcoder.xcode_func_name(), struct_ptr_name(mode))
        return f"""
{xcoder.public_xcode_func_sig()}
{{
	zcbor_state_t states[{xcoder.num_backups() + 2}];

	return zcbor_entry_function(payload, payload_len, (void *){func_arg}, payload_len_out, states,
		(zcbor_decoder_t *){func_name}, sizeof(states) / sizeof(zcbor_state_t), {
            xcoder.list_counts()[1]});
}}"""

    def render_file_header(self, line_prefix):
        lp = line_prefix
        return (f"\n{lp} " + self.file_header.replace("\n", f"\n{lp} ")).replace(" \n", "\n")

    def render_c_file(self, header_file_name, mode):
        """Render the entire generated C file contents."""
        log_result_define = """#define log_result(state, result, func) \
do { \\
	if (!result) { \\
		zcbor_trace_file(state); \\
		zcbor_log("%s error: %s\\r\\n", func, zcbor_error_str(zcbor_peek_error(state))); \\
	} else { \\
		zcbor_log("%s success\\r\\n", func); \\
	} \\
} while(0)"""
        return f"""/*{self.render_file_header(" *")}
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_{mode}.h"
#include "{header_file_name}"
#include "zcbor_print.h"

#if DEFAULT_MAX_QTY != {self.default_max_qty}
#error "The type file was generated with a different default_max_qty than this file"
#endif

{log_result_define}

{linesep.join([self.render_forward_declaration(xcoder, mode) for xcoder in self.functions[mode]])}

{linesep.join([self.render_function(xcoder, mode) for xcoder in self.functions[mode]])}

{linesep.join([self.render_entry_function(xcoder, mode) for xcoder in self.entry_types[mode]])}
"""

    def render_h_file(self, type_def_file, header_guard, mode):
        """Render the entire generated header file contents."""
        return \
            f"""/*{self.render_file_header(" *")}
 */

#ifndef {header_guard}
#define {header_guard}

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "{type_def_file}"

#ifdef __cplusplus
extern "C" {{
#endif

#if DEFAULT_MAX_QTY != {self.default_max_qty}
#error "The type file was generated with a different default_max_qty than this file"
#endif

{(linesep * 2).join([f"{xcoder.public_xcode_func_sig()};" for xcoder in self.entry_types[mode]])}


#ifdef __cplusplus
}}
#endif

#endif /* {header_guard} */
"""

    def render_type_file(self, header_guard, mode):
        body = (
            linesep + linesep).join(
                [f"{typedef[1]} {{{linesep}{linesep.join(typedef[0][1:])};"
                    for typedef in self.type_defs[mode]])
        return \
            f"""/*{self.render_file_header(" *")}
 */

#ifndef {header_guard}
#define {header_guard}

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
{'#include <zcbor_common.h>' if "struct zcbor_string" in body else ""}

#ifdef __cplusplus
extern "C" {{
#endif

/** Which value for --default-max-qty this file was created with.
 *
 *  The define is used in the other generated file to do a build-time
 *  compatibility check.
 *
 *  See `zcbor --help` for more information about --default-max-qty
 */
#define DEFAULT_MAX_QTY {self.default_max_qty}

{body}

#ifdef __cplusplus
}}
#endif

#endif /* {header_guard} */
"""

    def render_cmake_file(self, target_name, h_files, c_files, type_file,
                          output_c_dir, output_h_dir, cmake_dir):
        include_dirs = sorted(set(((Path(output_h_dir)),
                                  (Path(type_file.name).parent),
                                  *((Path(h.name).parent) for h in h_files.values()))))

        def relativify(p):
            try:
                return PurePosixPath(
                    Path("${CMAKE_CURRENT_LIST_DIR}") / path.relpath(Path(p), cmake_dir))
            except ValueError:
                # On Windows, the above will fail if the paths are on different drives.
                return Path(p).absolute().as_posix()
        return \
            f"""\
#{self.render_file_header("#")}
#

add_library({target_name})
target_sources({target_name} PRIVATE
    {relativify(Path(output_c_dir, "zcbor_decode.c"))}
    {relativify(Path(output_c_dir, "zcbor_encode.c"))}
    {relativify(Path(output_c_dir, "zcbor_common.c"))}
    {relativify(Path(output_c_dir, "zcbor_print.c"))}
    {(linesep + "    ").join(((str(relativify(c.name))) for c in c_files.values()))}
    )
target_include_directories({target_name} PUBLIC
    {(linesep + "    ").join(((str(relativify(f)) for f in include_dirs)))}
    )
"""

    def render(self, modes, h_files, c_files, type_file, include_prefix, cmake_file=None,
               output_c_dir=None, output_h_dir=None):
        for mode in modes:
            h_name = Path(include_prefix, Path(h_files[mode].name).name)

            # Create and populate the generated c and h file.
            makedirs(path.dirname(Path(c_files[mode].name).absolute()), exist_ok=True)

            type_def_name = Path(include_prefix, Path(type_file.name).name)

            print("Writing to " + c_files[mode].name)
            c_files[mode].write(self.render_c_file(h_name, mode))

            print("Writing to " + h_files[mode].name)
            h_files[mode].write(self.render_h_file(
                type_def_name,
                self.header_guard(h_files[mode].name), mode))

        print("Writing to " + type_file.name)
        type_file.write(self.render_type_file(self.header_guard(type_file.name), mode))

        if cmake_file:
            print("Writing to " + cmake_file.name)
            cmake_file.write(self.render_cmake_file(
                Path(cmake_file.name).stem, h_files, c_files, type_file,
                output_c_dir, output_h_dir, Path(cmake_file.name).absolute().parent))


def int_or_str(arg):
    try:
        return int(arg)
    except ValueError:
        # print(arg)
        if getrp(r"\A\w+\Z").match(arg) is not None:
            return arg
    raise ArgumentTypeError(
        "Argument must be an integer or a string with only letters, numbers, or '_'.")


def parse_args():

    parent_parser = ArgumentParser(add_help=False)

    parent_parser.add_argument(
        "-c", "--cddl", required=True, type=FileType('r', encoding='utf-8'), action="append",
        help="""Path to one or more input CDDL file(s). Passing multiple files is equivalent to
concatenating them.""")
    parent_parser.add_argument(
        "--no-prelude", required=False, action="store_true", default=False,
        help=f"""Exclude the standard CDDL prelude from the build. The prelude can be viewed at
{PRELUDE_PATH.relative_to(PACKAGE_PATH)} in the repo, or together with the script.""")
    parent_parser.add_argument(
        "-v", "--verbose", required=False, action="store_true", default=False,
        help="Print more information while parsing CDDL and generating code.")

    parser = ArgumentParser(
        description='''Parse a CDDL file and validate/convert between YAML, JSON, and CBOR.
Can also generate C code for validation/encoding/decoding of CBOR.''')

    parser.add_argument(
        "--version", action="version", version=f"zcbor {__version__}")

    subparsers = parser.add_subparsers()
    code_parser = subparsers.add_parser(
        "code", description='''Parse a CDDL file and produce C code that validates and xcodes CBOR.
The output from this script is a C file and a header file. The header file
contains typedefs for all the types specified in the cddl input file, as well
as declarations to xcode functions for the types designated as entry types when
running the script. The c file contains all the code for decoding and validating
the types in the CDDL input file. All types are validated as they are xcoded.

Where a `bstr .cbor <Type>` is specified in the CDDL, AND the Type is an entry
type, the xcoder will not xcode the string, only provide a pointer into the
payload buffer. This is useful to reduce the size of typedefs, or to break up
decoding. Using this mechanism is necessary when the CDDL contains self-
referencing types, since the C type cannot be self referencing.

This script requires 'regex' for lookaround functionality not present in 're'.''',
        formatter_class=RawDescriptionHelpFormatter,
        parents=[parent_parser])

    code_parser.add_argument(
        "--default-max-qty", "--dq", required=False, type=int_or_str, default=3,
        help="""Default maximum number of repetitions when no maximum
is specified. This is needed to construct complete C types.

The default_max_qty can usually be set to a text symbol if desired,
to allow it to be configurable when building the code. This is not always
possible, as sometimes the value is needed for internal computations.
If so, the script will raise an exception.""")
    code_parser.add_argument(
        "--output-c", "--oc", required=False, type=str,
        help="""Path to output C file. If both --decode and --encode are specified, _decode and
_encode will be appended to the filename when creating the two files. If not
specified, the path and name will be based on the --output-cmake file. A 'src'
directory will be created next to the cmake file, and the C file will be
placed there with the same name (except the file extension) as the cmake file.""")
    code_parser.add_argument(
        "--output-h", "--oh", required=False, type=str,
        help="""Path to output header file. If both --decode and --encode are specified, _decode and
_encode will be appended to the filename when creating the two files. If not
specified, the path and name will be based on the --output-cmake file. An 'include'
directory will be created next to the cmake file, and the C file will be
placed there with the same name (except the file extension) as the cmake file.""")
    code_parser.add_argument(
        "--output-h-types", "--oht", required=False, type=str,
        help="""Path to output header file with typedefs (shared between decode and encode).
If not specified, the path and name will be taken from the output header file
(--output-h), with '_types' added to the file name.""")
    code_parser.add_argument(
        "--copy-sources", required=False, action="store_true", default=False,
        help="""Copy the non-generated source files (zcbor_*.c/h) into the same directories as the
generated files.""")
    code_parser.add_argument(
        "--output-cmake", required=False, type=str,
        help="""Path to output CMake file. The filename of the CMake file without '.cmake' is used
as the name of the CMake target in the file.
The CMake file defines a CMake target with the zcbor source files and the
generated file as sources, and the zcbor header files' and generated header
files' folders as include_directories.
Add it to your project via include() in your CMakeLists.txt file, and link the
target to your program.
This option works with or without the --copy-sources option.""")
    code_parser.add_argument(
        "-t", "--entry-types", required=True, type=str, nargs="+",
        help="Names of the types which should have their xcode functions exposed.")
    code_parser.add_argument(
        "-d", "--decode", required=False, action="store_true", default=False,
        help="Generate decoding code. Either --decode or --encode or both must be specified.")
    code_parser.add_argument(
        "-e", "--encode", required=False, action="store_true", default=False,
        help="Generate encoding code. Either --decode or --encode or both must be specified.")
    code_parser.add_argument(
        "--time-header", required=False, action="store_true", default=False,
        help="Put the current time in a comment in the generated files.")
    code_parser.add_argument(
        "--git-sha-header", required=False, action="store_true", default=False,
        help="Put the current git sha of zcbor in a comment in the generated files.")
    code_parser.add_argument(
        "-b", "--default-bit-size", required=False, type=int, default=32, choices=[32, 64],
        help="""Default bit size of integers in code. When integers have no explicit bounds,
assume they have this bit width. Should follow the bit width of the architecture
the code will be running on.""")
    code_parser.add_argument(
        "--include-prefix", default="",
        help="""When #include'ing generated files, add this path prefix to the filename.""")
    code_parser.add_argument(
        "-s", "--short-names", required=False, action="store_true", default=False,
        help="""Attempt to make most generated struct member names shorter. This might make some
names identical which will cause a compile error. If so, tweak the CDDL labels
or layout, or disable this option. This might also make enum names different
from the corresponding union members.""")
    code_parser.add_argument(
        "--file-header", required=False, type=str, default="",
        help="""Header to be included in the comment at the top of generated files, e.g. copyright.
Can be a string or a path to a file. If interpreted as a path to an existing file,
the file's contents will be used.""")
    code_parser.set_defaults(process=process_code)

    validate_parent_parser = ArgumentParser(add_help=False)
    validate_parent_parser.add_argument(
        "-i", "--input", required=True, type=str,
        help='''Input data file. The option --input-as specifies how to interpret the contents.
Use "-" to indicate stdin.''')
    validate_parent_parser.add_argument(
        "--input-as", required=False, choices=["yaml", "json", "cbor", "cborhex"],
        help='''Which format to interpret the input file as.
If omitted, the format is inferred from the file name.
.yaml, .yml => YAML, .json => JSON, .cborhex => CBOR as hex string, everything else => CBOR''')
    validate_parent_parser.add_argument(
        "-t", "--entry-type", required=True, type=str,
        help='''Name of the type (from the CDDL) to interpret the data as.''')
    validate_parent_parser.add_argument(
        "--default-max-qty", "--dq", required=False, type=int, default=0xFFFFFFFF,
        help="""Default maximum number of repetitions when no maximum is specified.
It is only relevant when handling data that will be decoded by generated code.
If omitted, a large number will be used.""")
    validate_parent_parser.add_argument(
        "--yaml-compatibility", required=False, action="store_true", default=False,
        help='''Whether to convert CBOR-only values to YAML-compatible ones
(when converting from CBOR), or vice versa (when converting to CBOR).

When this is enabled, all CBOR data is guaranteed to convert into YAML/JSON.
JSON and YAML do not support all data types that CBOR/CDDL supports.
bytestrings (BSTR), tags, undefined, and maps with non-text keys need
special handling. See the zcbor README for more information.''')

    validate_parser = subparsers.add_parser(
        "validate", description='''Read CBOR, YAML, or JSON data from file or stdin and validate
it against a CDDL schema file.
        ''',
        parents=[parent_parser, validate_parent_parser])

    validate_parser.set_defaults(process=process_validate)

    convert_parser = subparsers.add_parser(
        "convert", description='''Parse a CDDL file and validate/convert between CBOR and YAML/JSON.
The script decodes the CBOR/YAML/JSON data from a file or stdin
and verifies that it conforms to the CDDL description.
The script fails if the data does not conform.
'zcbor validate' can be used if only validate is needed.''',
        parents=[parent_parser, validate_parent_parser])

    convert_parser.add_argument(
        "-o", "--output", required=True, type=str,
        help='''Output data file. The option --output-as specifies how to interpret the contents.
 Use "-" to indicate stdout.''')
    convert_parser.add_argument(
        "--output-as", required=False, choices=["yaml", "json", "cbor", "cborhex", "c_code"],
        help='''Which format to interpret the output file as.
If omitted, the format is inferred from the file name.
.yaml, .yml => YAML, .json => JSON, .c, .h => C code,
.cborhex => CBOR as hex string, everything else => CBOR''')
    convert_parser.add_argument(
        "--c-code-var-name", required=False, type=str,
        help='''Only relevant together with '--output-as c_code' or .c files.''')
    convert_parser.add_argument(
        "--c-code-columns", required=False, type=int, default=0,
        help='''Only relevant together with '--output-as c_code' or .c files.
The number of bytes per line in the variable instantiation. If omitted, the
entire declaration is a single line.''')
    convert_parser.set_defaults(process=process_convert)

    args = parser.parse_args()

    if not args.no_prelude:
        args.cddl.append(open(PRELUDE_PATH, 'r', encoding="utf-8"))

    if hasattr(args, "decode") and not args.decode and not args.encode:
        parser.error("Please specify at least one of --decode or --encode.")

    if hasattr(args, "output_c"):
        if not args.output_c or not args.output_h:
            if not args.output_cmake:
                parser.error(
                    "Please specify both --output-c and --output-h "
                    "unless --output-cmake is specified.")

    return args


def process_code(args):
    modes = list()
    if args.decode:
        modes.append("decode")
    if args.encode:
        modes.append("encode")

    if args.file_header and Path(args.file_header).exists():
        args.file_header = Path(args.file_header).read_text(encoding="utf-8")

    print("Parsing files: " + ", ".join((c.name for c in args.cddl)))

    cddl_contents = linesep.join((c.read() for c in args.cddl))

    cddl_res = dict()
    for mode in modes:
        cddl_res[mode] = CodeGenerator.from_cddl(
            mode, cddl_contents, args.default_max_qty, mode, args.entry_types,
            args.default_bit_size, short_names=args.short_names)

    # Parsing is done, pretty print the result.
    verbose_print(args.verbose, "Parsed CDDL types:")
    for mode in modes:
        verbose_pprint(args.verbose, cddl_res[mode].my_types)

    git_sha = ''
    if args.git_sha_header:
        if "zcbor.py" in sys.argv[0]:
            git_args = ['git', 'rev-parse', '--verify', '--short', 'HEAD']
            git_sha = Popen(
                git_args, cwd=PACKAGE_PATH, stdout=PIPE).communicate()[0].decode('utf-8').strip()
        else:
            git_sha = __version__

    def create_and_open(path):
        Path(path).absolute().parent.mkdir(parents=True, exist_ok=True)
        return Path(path).open('w', encoding='utf-8')

    if args.output_cmake:
        cmake_dir = Path(args.output_cmake).parent
        output_cmake = create_and_open(args.output_cmake)
        filenames = Path(args.output_cmake).parts[-1].replace(".cmake", "")
    else:
        output_cmake = None

    def add_mode_to_fname(filename, mode):
        name = Path(filename).stem + "_" + mode + Path(filename).suffix
        return Path(filename).with_name(name)

    output_c = dict()
    output_h = dict()
    out_c = args.output_c if (len(modes) == 1 and args.output_c) else None
    out_h = args.output_h if (len(modes) == 1 and args.output_h) else None
    for mode in modes:
        output_c[mode] = create_and_open(
            out_c or add_mode_to_fname(
                args.output_c or Path(cmake_dir, 'src', f'{filenames}.c'), mode))
        output_h[mode] = create_and_open(
            out_h or add_mode_to_fname(
                args.output_h or Path(cmake_dir, 'include', f'{filenames}.h'), mode))

    out_c_parent = Path(output_c[modes[0]].name).parent
    out_h_parent = Path(output_h[modes[0]].name).parent

    output_h_types = create_and_open(
        args.output_h_types
        or (args.output_h and Path(args.output_h).with_name(Path(args.output_h).stem + "_types.h"))
        or Path(cmake_dir, 'include', filenames + '_types.h'))

    renderer = CodeRenderer(entry_types={mode: [cddl_res[mode].my_types[entry]
                                         for entry in args.entry_types] for mode in modes},
                            modes=modes, print_time=args.time_header,
                            default_max_qty=args.default_max_qty, git_sha=git_sha,
                            file_header=args.file_header
                            )

    c_code_dir = C_SRC_PATH
    h_code_dir = C_INCLUDE_PATH

    if args.copy_sources:
        new_c_code_dir = out_c_parent
        new_h_code_dir = out_h_parent
        copyfile(Path(c_code_dir, "zcbor_decode.c"), Path(new_c_code_dir, "zcbor_decode.c"))
        copyfile(Path(c_code_dir, "zcbor_encode.c"), Path(new_c_code_dir, "zcbor_encode.c"))
        copyfile(Path(c_code_dir, "zcbor_common.c"), Path(new_c_code_dir, "zcbor_common.c"))
        copyfile(Path(c_code_dir, "zcbor_print.c"), Path(new_c_code_dir, "zcbor_print.c"))
        copyfile(Path(h_code_dir, "zcbor_decode.h"), Path(new_h_code_dir, "zcbor_decode.h"))
        copyfile(Path(h_code_dir, "zcbor_encode.h"), Path(new_h_code_dir, "zcbor_encode.h"))
        copyfile(Path(h_code_dir, "zcbor_common.h"), Path(new_h_code_dir, "zcbor_common.h"))
        copyfile(Path(h_code_dir, "zcbor_tags.h"), Path(new_h_code_dir, "zcbor_tags.h"))
        copyfile(Path(h_code_dir, "zcbor_print.h"), Path(new_h_code_dir, "zcbor_print.h"))
        c_code_dir = new_c_code_dir
        h_code_dir = new_h_code_dir

    renderer.render(modes, output_h, output_c, output_h_types, args.include_prefix,
                    output_cmake, c_code_dir, h_code_dir)


def parse_cddl(args):
    cddl_contents = linesep.join((c.read() for c in args.cddl))
    cddl_res = DataTranslator.from_cddl(cddl_contents, args.default_max_qty)
    return cddl_res.my_types[args.entry_type]


def read_data(args, cddl):
    _, in_file_ext = path.splitext(args.input)
    in_file_format = args.input_as or in_file_ext.strip(".")
    if in_file_format in ["yaml", "yml"]:
        f = sys.stdin if args.input == "-" else open(args.input, "r", encoding="utf-8")
        cbor_str = cddl.from_yaml(f.read(), yaml_compat=args.yaml_compatibility)
    elif in_file_format == "json":
        f = sys.stdin if args.input == "-" else open(args.input, "r", encoding="utf-8")
        cbor_str = cddl.from_json(f.read(), yaml_compat=args.yaml_compatibility)
    elif in_file_format == "cborhex":
        f = sys.stdin if args.input == "-" else open(args.input, "r", encoding="utf-8")
        cbor_str = bytes.fromhex(f.read().replace("\n", ""))
        cddl.validate_str(cbor_str)
    else:
        f = sys.stdin.buffer if args.input == "-" else open(args.input, "rb")
        cbor_str = f.read()
        cddl.validate_str(cbor_str)

    return cbor_str


def write_data(args, cddl, cbor_str):
    _, out_file_ext = path.splitext(args.output)
    out_file_format = args.output_as or out_file_ext.strip(".")
    if out_file_format in ["yaml", "yml"]:
        f = sys.stdout if args.output == "-" else open(args.output, "w", encoding="utf-8")
        f.write(cddl.str_to_yaml(cbor_str, yaml_compat=args.yaml_compatibility))
    elif out_file_format == "json":
        f = sys.stdout if args.output == "-" else open(args.output, "w", encoding="utf-8")
        f.write(cddl.str_to_json(cbor_str, yaml_compat=args.yaml_compatibility))
    elif out_file_format in ["c", "h", "c_code"]:
        f = sys.stdout if args.output == "-" else open(args.output, "w", encoding="utf-8")
        assert args.c_code_var_name is not None, \
            "Must specify --c-code-var-name when outputting c code."
        f.write(cddl.str_to_c_code(cbor_str, args.c_code_var_name, args.c_code_columns))
    elif out_file_format == "cborhex":
        f = sys.stdout if args.output == "-" else open(args.output, "w", encoding="utf-8")
        f.write(getrp(r"(.{1,64})").sub(r"\1\n", cbor_str.hex()))  # Add newlines every 64 chars
    else:
        f = sys.stdout.buffer if args.output == "-" else open(args.output, "wb")
        f.write(cbor_str)


def process_validate(args):
    cddl = parse_cddl(args)
    read_data(args, cddl)


def process_convert(args):
    cddl = parse_cddl(args)
    cbor_str = read_data(args, cddl)
    write_data(args, cddl, cbor_str)


def main():
    args = parse_args()
    args.process(args)


if __name__ == "__main__":
    main()
