zcbor
=====

zcbor is a low footprint [CBOR](https://en.wikipedia.org/wiki/CBOR) library in the C language (C++ compatible), tailored for use in microcontrollers.
It comes with a schema-driven script tool that can validate your data, or even generate code.
The schema language (CDDL) allows creating very advanced and detailed schemas.

The validation and conversion part of the tool works with YAML and JSON data, in addition to CBOR.
It can for example validate a YAML file against a schema and convert it into CBOR.

The code generation part of the tool generates C code based on the given schema.
The generated code performs CBOR encoding and decoding using the C library, while also validating the data against all the rules in the schema.

The schema language used by zcbor is CDDL (Concise Data Definition Language) which is a powerful human-readable data description language defined in [IETF RFC 8610](https://datatracker.ietf.org/doc/rfc8610/).


Features
========

Here are some possible ways zcbor can be used:

 - C code:
   - As a low-footprint CBOR decoding/encoding library similar to TinyCBOR/QCBOR/NanoCBOR. The library can be used independently of the Python script. ([More information](#cbor-decodingencoding-library))
   - To generate C code (using the Python script) for validating and decoding or encoding CBOR, for use in optimized or constrained environments, such as microcontrollers. ([More information](#code-generation))
 - Python script and module ([More information](#python-script-and-module)):
   - Validate a YAML/JSON file and translate it into CBOR e.g. for transmission.
   - Validate a YAML/JSON/CBOR file before processing it with some other tool
   - Decode and validate incoming CBOR data into human-readable YAML/JSON.
   - As part of a python script that processes YAML/JSON/CBOR files.
     - Uses the same internal representation used by the PyYAML/json/cbor2 libraries.
     - Do validation against a CDDL schema.
     - Create a read-only representation via named tuples (with names taken from the CDDL schema).


Getting started
===============

There are samples in the [samples](samples) directory that demonstrate different ways to use zcbor, both the script tool and the C code.

1. The [hello_world sample](samples/hello_world/README.md) is a minimum examples of encoding and decoding using the C library.
2. The [pet sample](samples/pet/README.md) shows a how to use the C library together with generated code, and how to use the script tool to do code generation and data conversion.

The [tests](tests) also demonstrate how to use zcbor in different ways. The [encoding](tests/encode), [decoding](tests/decode), and [unit](tests/unit) tests run using [Zephyr](https://github.com/zephyrproject-rtos/zephyr) (the samples do not use Zephyr).

Should I use code generation or the library directly?
-----------------------------------------------------

The benefit of using code generation is greater for decoding than encoding.
This is because decoding is generally more complex than encoding, since when decoding you have to gracefully handle all possible payloads.
The code generation will provide a number of checks that are tedious to write manually.
These checks ensure that the payload is well-formed.


CBOR decoding/encoding library
==============================

The CBOR library can be found in [include/](include) and [src/](src) and can be used directly, by including the files in your project.
If using zcbor with Zephyr, the library will be available when the [CONFIG_ZCBOR](https://docs.zephyrproject.org/latest/kconfig.html#CONFIG_ZCBOR) config is enabled.

The library is also used by generated code. See the [Code generation](#code-generation) section for more info about code generation.

The C library is C++ compatible.

The zcbor state object
----------------------

To do encoding or decoding with the library, instantiate a `zcbor_state_t` object, which is most easily done using the `ZCBOR_STATE_*()` macros, look below or in the [hello_world](samples/hello_world/src/main.c) sample for example code.

The `elem_count` member refers to the number of encoded objects in the current list or map.
`elem_count` starts again when entering a nested list or map, and is restored when exiting.

`elem_count` is one reason for needing "backup" states (the other is to allow rollback of the payload).
Backups are needed for _decoding_ if there are any lists, maps, or CBOR-encoded strings (`zcbor_bstr_*_decode`) in the data.
Backups are needed for _encoding_ if there are any lists or maps *and* you are using canonical encoding (`ZCBOR_CANONICAL`), or when using the `zcbor_bstr_*_encode` functions.

`n_flags` is used when decoding maps where the order is unknown.
It allows using the `zcbor_unordered_map_search()` function to search for elements.

See the header files for more information.

```c
/** Initialize a decoding state (could include an array of backup states).
 *  After calling this, decode_state[0] is ready to be used with the decoding APIs. */
ZCBOR_STATE_D(decode_state, n, payload, payload_len, elem_count, n_flags);

/** Initialize an encoding state (could include an array of backup states).
 *  After calling this, encode_state[0] is ready to be used with the encoding APIs. */
ZCBOR_STATE_E(encode_state, n, payload, payload_len, 0);
```

Configuration
-------------

The C library has a few compile-time configuration options.
These configuration options can be enabled by adding them as compile definitions to the build.
If using zcbor with Zephyr, use the [Kconfig options](https://github.com/zephyrproject-rtos/zephyr/blob/main/modules/zcbor/Kconfig) instead.

Name                      | Description
------------------------- | -----------
`ZCBOR_CANONICAL`         | Assume canonical encoding (AKA "deterministically encoded CBOR", ch 4.2.1 in RFC8949). When encoding lists and maps, do not use indefinite length encoding. Enabling `ZCBOR_CANONICAL` increases code size and makes the encoding library more often use state backups. When decoding, if `enforce_canonical` is true, ensure that the incoming data conforms to canonical encoding, i.e. no indefinite length encoding, and always using minimal length encoding (e.g. not using 16 bits to encode a value < 256). Enabling `ZCBOR_CANONICAL` changes the default of `enforce_canonical` from `false` to `true` Note: the map ordering constraint in canonical encoding is not checked.
`ZCBOR_VERBOSE`           | Print log messages on encoding/decoding errors (`zcbor_log()`), and also a trace message (`zcbor_trace()`) for each decoded value, and in each generated function (when using code generation).
`ZCBOR_ASSERTS`           | Enable asserts (`zcbor_assert()`). When they fail, the assert statements instruct the current function to return a `ZCBOR_ERR_ASSERTION` error. If `ZCBOR_VERBOSE` is enabled, a message is printed.
`ZCBOR_STOP_ON_ERROR`     | Enable the `stop_on_error` functionality. Note that it also has to be enabled in the state variable (`state->constant_state->stop_on_error`). This makes all zcbor functions abort their execution if called when an error has already happened.
`ZCBOR_BIG_ENDIAN`        | All decoded values are returned as big-endian. The default is little-endian.
`ZCBOR_MAP_SMART_SEARCH`  | Applies to decoding of unordered maps. When enabled, a flag is kept for each element in an array, ensuring it is not processed twice. If disabled, a count is kept for map as a whole. Enabling increases code size and memory usage, and requires the state variable to possess the memory necessary for the flags.

Canonical encoding
------------------

The C library supports some of the rules for canonical encoding (also called "deterministically encoded CBOR"). To enable these, use the `ZCBOR_CANONICAL` config for encoding, or the `enforce_canonical` flag for decoding. Note that enabling `ZCBOR_CANONICAL` changes the default value of `enforce_canonical`.

Some rules are not supported because it would introduce significant code size or complexity.

The following canonical encoding rules are enforced for encoding:

 - integers are minimally encoded (always)
 - lists and maps do not use indefinite length encoding (when `ZCBOR_CANONCAL` is enabled)
 - floats are encoded minimally (when using the function `zcbor_float_min_encode` or `zcbor_float_min_put`).

The following canonical encoding rules are enforced for decoding:

 - integers are minimally encoded (when `enforce_canonical` is enabled)
 - lists and maps do not use indefinite length encoding (when `enforce_canonical` is enabled)

The following canonical encoding rules are not enforced:

 - floats are encoded minimally (during decoding)
 - map keys are sorted by bytewise value


Python script and module
========================

The zcbor.py script can directly read CBOR, YAML, or JSON data and validate it against a CDDL description.
It can also freely convert the data between CBOR/YAML/JSON.
It can also output the data to a C file formatted as a byte array.

Invoking zcbor.py from the command line
---------------------------------------

zcbor.py can be installed via [`pip`](https://pypi.org/project/zcbor/), or alternatively invoked directly from its location in this repo.

Following are some generalized examples for validating, and for converting (which also validates) data from the command line.
The script infers the data format from the file extension, but the format can also be specified explicitly.
See `zcbor validate --help` and `zcbor convert --help` for more information.

```sh
zcbor validate -c <CDDL description file> -t <which CDDL type to expect> -i <input data file>
zcbor convert -c <CDDL description file> -t <which CDDL type to expect> -i <input data file> -o <output data file>
```

Or directly from within the repo.

```sh
python3 <zcbor base>/zcbor/zcbor.py validate -c <CDDL description file> -t <which CDDL type to expect> -i <input data file>
python3 <zcbor base>/zcbor/zcbor.py convert -c <CDDL description file> -t <which CDDL type to expect> -i <input data file> -o <output data file>
```

Importing zcbor in a Python script
----------------------------------

Importing zcbor gives access to the DataTranslator class which is used to implement the command line conversion features.
DataTranslator can be used to programmatically perform the translations, or to manipulate the data.
When accessing the data, you can choose between two internal formats:

 1. The format provided by the [cbor2](https://pypi.org/project/cbor2/), [yaml (PyYAML)](https://pypi.org/project/PyYAML/), and [json](https://docs.python.org/3/library/json.html) packages.
    This is a format where the serialization types (map, list, string, number etc.) are mapped directly to the corresponding Python types.
    This format is common between these packages, which makes translation very simple.
    When returning this format, DataTranslator hides the idiomatic representations for bytestrings, tags, and non-text keys described above.
 2. A custom format which allows accessing the data via the names from the CDDL description file.
    This format is implemented using named tuples, and is immutable, meaning that it can be used for inspecting data, but not for changing or creating data.

Canonical encoding
------------------

CBOR data created by the python script can be made canonical with `--output-canonical`, or by setting the `canonical` argument to `True` in supported `DataTranslator` functions. Note that in CBOR data from the script, integers are always minimally encoded, and indeterminate length lists are never used, even without the `canonical` argument.

Making CBOR YAML-/JSON-compatible
---------------------------------

Since CBOR supports more data types than YAML and JSON, zcbor can optionally use a bespoke format when converting to/from YAML/JSON.
This is controlled with the `--yaml-compatibility` option to `convert` and `validate`.
This is relevant when handling YAML/JSON conversions of data that uses the unsupported features.
The following data types are supported by CBOR, but not by YAML (or JSON which is a subset of YAML):

 1. bytestrings: YAML supports only text strings. In YAML, bytestrings are represented as `{"zcbor_bstr": "<hex-formatted bytestring>"}`, or as `{"zcbor_bstr": <any type>}` if the CBOR bytestring contains CBOR-formatted data, in which the data is decoded into `<any type>`.
 2. map keys other than text string: In YAML, such key value pairs are represented as `{"zcbor_keyval<unique int>": {"key": <key, not text>, "val": <value>}}`.
 3. tags: In cbor2, tags are represented by a special type, `cbor2.CBORTag`. In YAML, these are represented as `{"zcbor_tag": <tag number>, "zcbor_tag_val": <tagged data>}`.
 4. undefined: In cbor2, undefined has its own value `cbor2.types.undefined`. In YAML, undefined is represented as: `["zcbor_undefined"]`.

You can see an example of the conversions in [tests/cases/yaml_compatibility.yaml](tests/cases/yaml_compatibility.yaml) and its CDDL file [tests/cases/yaml_compatibility.cddl](tests/cases/yaml_compatibility.cddl).

Unicode
-------

The zcbor Python script fully supports unicode. All text files read by the script, including CDDL files and YAML/JSON files must be utf-8. All produced text files are utf-8.

Unicode escape sequences
------------------------

String literals in CDDL files can contain unicode escape sequences as described in the [CDDL spec update](https://datatracker.ietf.org/doc/html/rfc9682):

 - 16-bit: "\uXXXX"
 - 32-bit: "\U00XXXXXX"
 - variable-length: "\u{XXXX}"
 - characters, e.g.: "\\ \n \t \""

As an example, the CDDL 'Foo = "\U0001F000\u{72}"' (equivalent to 'Foo = "🀀H"') will expect the CBOR data `65f09f808072`.

In code generation, characters that are escaped in the CDDL will be escaped in the C string literals, while characters that are non-escaped in the CDDL will be non-escaped in the generated C code. In both cases, the characters must be non-escaped in the data.

When using the script to handle YAML/JSON, input YAML/JSON data can contain escaped characters as supported in those standards, which will be handled as equivalent to the non-escaped characters.

Surrogate pairs are allowed in CDDL (as `\uD8xx\uDCxx`), and will be converted to their proper representation internally.

Code generation
===============

Code generation is invoked with the `zcbor code` command:

```sh
zcbor code <--decode or --encode or both> -c <CDDL description file(s)> -t <which CDDL type(s) to expose in the API> --output-cmake <path to place the generated CMake file at>
zcbor code <--decode or --encode or both> -c <CDDL description file(s)> -t <which CDDL type(s) to expose in the API> --oc <path to the generated C file> --oh <path to the generated header file> --oht <path to the generated types header>
```

When you call this, zcbor reads the CDDL files and creates C struct types to match the types described in the CDDL.
It then creates code that uses the C library to decode CBOR data into the structs, and/or encode CBOR from the data in the structs.
Finally, it takes the "entry types" (`-t`) and creates a public API function for each of them.
While doing these things, it will make a number of optimizations, e.g. inlining code for small types and removing unused functions.
It outputs the generated code into header and source files and optionally creates a CMake file to build them.

The `zcbor code` command reads one or more CDDL file(s) and generates some or all of these files:
 - A header file with types (always)
 - A header file with declarations for decoding functions (if `--decode`/`-d` is specified)
 - A C file with decoding functions (if `--decode`/`-d` is specified)
 - A header file with declarations for encoding functions (if `--encode`/`-e` is specified)
 - A C file with encoding functions (if `--encode`/`-e` is specified)
 - A CMake file that creates a library with the generated code and the C library (if `--output-cmake` is specified).

CDDL allows placing restrictions on the members of your data.
Restrictions can be on type (int/string/list/bool etc.), on content (e.g. values/sizes of ints or strings), and repetition (e.g. the number of members in a list).
The generated code will validate the input, which means that it will check all the restriction set in the CDDL description, and fail if a restriction is broken.

There are tests for the code generation in [tests/decode](tests/decode) and [tests/encode](tests/encode).
The tests require [Zephyr](https://github.com/zephyrproject-rtos/zephyr) (if your system is set up to build Zephyr samples, the tests should also build).

The generated C code is C++ compatible.

Build system
------------

### Using Generator

When calling zcbor with the argument `--output-cmake <file path>`, a CMake file will be created at that location.
The generated CMake file creates a target library and adds the generated and non-generated source files as well as required include directories to it.
This CMake file can then be included in your project's `CMakeLists.txt` file, and the target can be linked into your project.
This is demonstrated in the tests, e.g. at [tests/decode/test3_simple/CMakeLists.txt](tests/decode/test3_simple/CMakeLists.txt).
zcbor can be instructed to copy the non-generated sources to the same location as the generated sources with `--copy-sources`.

### Using Standard CMake

The zcbor repository includes a standard CMakeLists.txt file that can be used to build the zcbor library.
The CMake build system provides options to configure the library and install it on your system.

To build and install zcbor:

```
mkdir build && cd build
cmake ..
cmake --build .
cmake --install .
```

The CMake build provides several options to configure the library:

 - `ZCBOR_BUILD_SHARED`: Build zcbor as a shared library (default: OFF)
 - `ZCBOR_ENABLE_PRINT`: Enable zcbor_print module (default: ON)
 - `ZCBOR_ENABLE_TAGS`: Enable zcbor_tags module (default: ON)
 - Configuration options like `ZCBOR_CANONICAL`, `ZCBOR_VERBOSE`, `ZCBOR_ASSERTS`, etc. (see [Configuration](#configuration) section).

Usage Example
=============

There are buildable examples in the [samples](samples) directory.

To see how to use the C library directly, see the [hello_world](samples/hello_world/src/main.c) sample, or the [pet](samples/pet/src/main.c) sample (look for calls to functions prefixed with `zcbor_`).

To see how to use code generation, see the [pet](samples/pet/src/main.c) sample.

Look at the [CMakeLists.txt](samples/pet/CMakeLists.txt) file to see how zcbor is invoked for code generation (and for conversion).

To see how to do conversion, see the [pet](samples/pet/CMakeLists.txt) sample.

Below are some additional examples of how to invoke zcbor for code generation and for converting/validating

Code generation
---------------

```sh
python3 <zcbor base>/zcbor/zcbor.py code -c pet.cddl -d -t Pet --oc pet_decode.c --oh pet_decode.h
# or
zcbor code -c pet.cddl -d -t Pet --oc pet_decode.c --oh pet_decode.h
```

Converting
----------

Here is an example call for converting from YAML to CBOR:

```sh
python3 <zcbor base>/zcbor/zcbor.py convert -c pet.cddl -t Pet -i mypet.yaml -o mypet.cbor
# or
zcbor convert -c pet.cddl -t Pet -i mypet.yaml -o mypet.cbor
```

Which takes a yaml structure from mypet.yaml, validates it against the Pet type in the CDDL description in pet.cddl, and writes binary CBOR data to mypet.cbor.

Validating
----------

Here is an example call for validating a JSON file:

```sh
python3 <zcbor base>/zcbor/zcbor.py validate -c pet.cddl -t Pet --yaml-compatibility -i mypet.json
# or
zcbor validate -c pet.cddl -t Pet --yaml-compatibility -i mypet.json
```

Which takes the json structure in mypet.json, converts any [yaml-compatible](#making-cbor-yaml-json-compatible) values to their original form, and validates that against the Pet type in the CDDL description in pet.cddl.


Running tests
=============

The tests for the generated code are based on the Zephyr ztest library.
These tests can be found in [tests/decode](tests/decode) and [tests/encode](tests/encode).
To set up the environment to run the ztest tests, follow [Zephyr's Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html), or see the workflow in the [`.github`](.github) directory.

Tests for `convert` and `verify` are implemented with the unittest module.
These tests can be found in [tests/scripts/test_zcbor.py](tests/scripts/test_zcbor.py).
In this file there are also tests for code style of all python scripts, using the `pycodestyle` library.

Tests for the docs, samples, etc. can be found in [tests/scripts/test_repo_files.py](tests/scripts/test_repo_files.py).

For running the tests locally, there is [`tests/test.sh`](tests/test.sh) which runs all above tests.


Introduction to CDDL
====================

In CDDL you define types from other types.
Types can be defined from base types, or from other types you define.
Types are declared with '`=`', e.g. `Foo = int` which declares the type `Foo` to be an integer, analogous to `typedef int Foo;` in C.
CDDL defines the following base types (this is not an exhaustive list):

 - `int`: Positive or negative integer
 - `uint`: Positive integer
 - `bstr`: Byte string
 - `tstr`: Text string
 - `bool`: Boolean
 - `nil`: Nil/Null value
 - `float`: Floating point value
 - `any`: Any single element

CDDL allows creating aggregate types:

 - `[]`: List. Elements don't need to have the same type.
 - `{}`: Map. Key/value pairs as are declared as `<key> => <value>` or `<key>: <value>`. Note that `:` is also used for labels.
 - `()`: Groups. Grouping with no enclosing type, which means that e.g. `Foo = [(int, bstr)]` is equivalent to `Foo = [int, bstr]`.
 - `/`: Unions. Analogous to unions in C. E.g. `Foo = int/bstr/Bar` where Foo is either an int, a bstr, or Bar (some custom type).

Literals can be used instead of the base type names:

 - Number: `Foo = 3`, where Foo is a uint with the additional requirement that it must have the value 3.
 - Number range: `Foo = -100..100`, where Foo is an int with value between -100 and 100.
 - Text string: `Foo = "hello"`, where Foo is a tstr with the requirement that it must be "hello".
 - True/False: `Foo = false`, where Foo is a bool which is always false.

Base types can also be restricted in other ways:

 - `.size`: Works for integers and strings. E.g. `Foo = uint .size 4` where Foo is a uint exactly 4 bytes long.
 - `.cbor`/`.cborseq`: E.g. `Foo = bstr .cbor Bar` where Foo is a bstr whose contents must be CBOR data decodable as the Bar type.

An element can be repeated:

 - `?`: 0 or 1 time. E.g. `Foo = [int, ?bstr]`, where Foo is a list with an int possibly followed by a bstr.
 - `*`: 0 or more times. E.g. `Foo = [*tstr]`, where Foo is a list containing 0 or more tstrs.
 - `+`: 1 or more times. E.g. `Foo = [+Bar]`.
 - `x*y`: Between x and y times, inclusive. E.g. `Foo = {4*8(int => bstr)}` where Foo is a map with 4 to 8 key/value pairs where each key is an int and each value is a bstr.

Note that in the zcbor script and its generated code, the number of entries supported via `*` and `+` is affected by the default_max_qty value.

Any element can be labeled with `:`.
The label is only for readability and does not impact the data structure in any way.
E.g. `Foo = [name: tstr, age: uint]` is equivalent to `Foo = [tstr, uint]`.

See [pet.cddl](tests/cases/pet.cddl) for CDDL example code.

Unsupported CDDL features
-------------------------

Not all features outlined in the CDDL specs [RFC8610](https://datatracker.ietf.org/doc/html/rfc8610), [RFC9090](https://datatracker.ietf.org/doc/html/rfc9090), and [RFC9165](https://datatracker.ietf.org/doc/html/rfc9165) are supported by zcbor.
The following is a list of limitations and missing features:

 * Generated code does not support unordered maps.
 * Using `&()` to turn groups into choices (unions). `&()` is supported when used with `.bits`.
 * Representation Types (`#x.y`), except for tags (`#6.y(foo)`) which are supported.
 * Unwrapping (`~`)
 * The control operators `.regexp`, `.ne`, `.default`, and `.within` from RFC8610.
 * The control operators `.sdnv`, `.sdnvseq`, and `.oid` from RFC9090.
 * The control operators `.plus`, `.cat`, `.det`, `.abnf`, `.abnfb`, and `.feature` from RFC9165.
 * Generics (`foo<a, b>`).
 * Using `:` for map keys.
 * Cuts, either via `^` or implicitly via `:`.
 * Most of the "Extended Diagnostic Notation" is unsupported.


Introduction to CBOR
====================

CBOR's format is described well on [Wikipedia](https://en.wikipedia.org/wiki/CBOR), but here's a synopsis:

Encoded CBOR data elements look like this.

```
| Header                       | Value                  | Payload                   |
| 1 byte                       | 0, 1, 2, 4, or 8 bytes | 0 - 2^64-1 bytes/elements |
| 3 bits     | 5 bits          |
| Major Type | Additional Info |
```

The available major types can be seen in `zcbor_major_type_t`.

For all major types, Values 0-23 are encoded directly in the _Additional info_, meaning that the _Value_ field is 0 bytes long.
If _Additional info_ is 24, 25, 26, or 27, the _Value_ field is 1, 2, 4, or 8 bytes long, respectively.

Major types `pint` (0), `nint` (1), `tag` (6), and `simple` (7) elements have no payload, only _Value_.

 * `pint`: Interpret the _Value_ as a positive integer.
 * `nint`: Interpret the _Value_ as a positive integer, then multiply by -1 and subtract 1.
 * `tag`: The _Value_ says something about the next non-tag element.
   See the [CBOR tag documentation](https://www.iana.org/assignments/cbor-tags/cbor-tags.xhtml) for details.
 * `simple`: Different _Additional info_ mean different things:
    * 0-19: Unassigned simple values.
    * 20: `false` simple value
    * 21: `true` simple value
    * 22: `null` simple value
    * 23: `undefined` simple value
    * 24: Interpret the _Value_ as a 1 byte simple value. These simple values are currently unassigned.
    * 25: Interpret the _Value_ as an IEEE 754 float16.
    * 26: Interpret the _Value_ as an IEEE 754 float32.
    * 27: Interpret the _Value_ as an IEEE 754 float64.
    * 31: End of an indefinite-length `list` or `map`.

For `bstr` (2), `tstr` (3), `list` (4), and `map` (5), the _Value_ describes the length of the _Payload_.
For `bstr` and `tstr`, the length is in bytes, for `list`, the length is in number of elements, and for `map`, the length is in number of key/value element pairs.

For `list` and `map`, sub elements are regular CBOR elements with their own _Header_, _Value_ and _Payload_. `list`s and `map`s can be recursively encoded.
If a `list` or `map` has _Additional info_ 31, it is "indefinite-length", which means it has an "unknown" number of elements.
Instead, its end is marked by a `simple` with _Additional info_ 31 (byte value 0xFF).


History
=======

zcbor (then "cddl-gen") was initially conceived as a code generation project.
It was inspired by the need to securely decode the complex manifest data structures in the [IETF SUIT specification](https://datatracker.ietf.org/doc/draft-ietf-suit-manifest/).
This is reflected in the fact that there are multiple zcbor tests that use the CDDL and examples from various revisions of that specification.
Decoding/deserializing data securely requires doing some quite repetitive checks on each data element, to be sure that you are not decoding gibberish.
This is where code generation could pull a lot of weight.
Later it was discovered that the CBOR library that was designed to used by generated code could be useful by itself.
The script was also expanded so it could directly manipulate CBOR data.
Since CBOR, YAML, and JSON are all represented in roughly the same way internally in Python, it was easy to expand that data manipulation to support YAML and JSON.

Some places where zcbor is currently used:
- [MCUboot's serial recovery mechanism](https://github.com/mcu-tools/mcuboot/blob/main/boot/boot_serial/src/boot_serial.c)
- [Zephyr's mcumgr](https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/mgmt/mcumgr/grp/img_mgmt/src/img_mgmt.c)
- [Zephyr's LwM2M SenML](https://github.com/zephyrproject-rtos/zephyr/blob/main/subsys/net/lib/lwm2m/lwm2m_rw_senml_cbor.c)
- [nRF Connect SDK's full modem update mechanism](https://github.com/nrfconnect/sdk-nrf/blob/main/subsys/mgmt/fmfu/src/fmfu_mgmt.c)
- [nRF Connect SDK's nrf_rpc](https://github.com/nrfconnect/sdk-nrfxlib/blob/main/nrf_rpc/nrf_rpc_cbor.c)


Command line documentation
==========================

Added via `add_helptext.py`

zcbor --help
------------

```
usage: zcbor [-h] [--version] {code,validate,convert} ...

Parse a CDDL file and validate/convert between YAML, JSON, and CBOR. Can also
generate C code for validation/encoding/decoding of CBOR.

positional arguments:
  {code,validate,convert}

options:
  -h, --help            show this help message and exit
  --version             show program's version number and exit

```

zcbor code --help
-----------------

```
usage: zcbor code [-h] -c CDDL [--no-prelude] [-v] [-q]
                  [--default-max-qty DEFAULT_MAX_QTY] [--output-c OUTPUT_C]
                  [--output-h OUTPUT_H] [--output-h-types OUTPUT_H_TYPES]
                  [--copy-sources] [--output-cmake OUTPUT_CMAKE]
                  -t ENTRY_TYPES [ENTRY_TYPES ...] [-d] [-e] [--time-header]
                  [--git-sha-header] [-b {8,16,32,64}]
                  [--include-prefix INCLUDE_PREFIX] [-s]
                  [--file-header FILE_HEADER]

Parse a CDDL file and produce C code that validates and xcodes CBOR.
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

This script requires 'regex' for lookaround functionality not present in 're'.

options:
  -h, --help            show this help message and exit
  -c, --cddl CDDL       Path to one or more input CDDL file(s). Passing
                        multiple files is equivalent to concatenating them.
  --no-prelude          Exclude the standard CDDL prelude from the build. The
                        prelude can be viewed at zcbor/prelude.cddl in the
                        repo, or together with the script.
  -v, --verbose         Print more information while parsing CDDL and
                        generating code.
  -q, --quiet           Print less information while parsing CDDL and
                        generating code.
  --default-max-qty, --dq DEFAULT_MAX_QTY
                        Default maximum number of repetitions when no maximum
                        is specified. This is needed to construct complete C
                        types. The default_max_qty can usually be set to a
                        text symbol if desired, to allow it to be configurable
                        when building the code. This is not always possible,
                        as sometimes the value is needed for internal
                        computations. If so, the script will raise an
                        exception. Default: 3
  --output-c, --oc OUTPUT_C
                        Path to output C file. If both --decode and --encode
                        are specified, _decode and _encode will be appended to
                        the filename when creating the two files. If not
                        specified, the path and name will be based on the
                        --output-cmake file. A 'src' directory will be created
                        next to the cmake file, and the C file will be placed
                        there with the same name (except the file extension)
                        as the cmake file.
  --output-h, --oh OUTPUT_H
                        Path to output header file. If both --decode and
                        --encode are specified, _decode and _encode will be
                        appended to the filename when creating the two files.
                        If not specified, the path and name will be based on
                        the --output-cmake file. An 'include' directory will
                        be created next to the cmake file, and the C file will
                        be placed there with the same name (except the file
                        extension) as the cmake file.
  --output-h-types, --oht OUTPUT_H_TYPES
                        Path to output header file with typedefs (shared
                        between decode and encode). If not specified, the path
                        and name will be taken from the output header file
                        (--output-h), with '_types' added to the file name.
  --copy-sources        Copy the non-generated source files (zcbor_*.c/h) into
                        the same directories as the generated files.
  --output-cmake OUTPUT_CMAKE
                        Path to output CMake file. The filename of the CMake
                        file without '.cmake' is used as the name of the CMake
                        target in the file. The CMake file defines a CMake
                        target with the zcbor source files and the generated
                        file as sources, and the zcbor header files' and
                        generated header files' folders as
                        include_directories. Add it to your project via
                        include() in your CMakeLists.txt file, and link the
                        target to your program. This option works with or
                        without the --copy-sources option.
  -t, --entry-types ENTRY_TYPES [ENTRY_TYPES ...]
                        Names of the types which should have their xcode
                        functions exposed.
  -d, --decode          Generate decoding code. Either --decode or --encode or
                        both must be specified.
  -e, --encode          Generate encoding code. Either --decode or --encode or
                        both must be specified.
  --time-header         Put the current time in a comment in the generated
                        files.
  --git-sha-header      Put the current git sha of zcbor in a comment in the
                        generated files.
  -b, --default-bit-size {8,16,32,64}
                        Default bit size of integers in code. When integers
                        have no explicit bounds, assume they have this bit
                        width. Recommended to follow the bit width of the
                        architecture the code will be running on. Default: 32
  --include-prefix INCLUDE_PREFIX
                        When #include'ing generated files, add this path
                        prefix to the filename.
  -s, --short-names     Attempt to make most generated struct member names
                        shorter. This might make some names identical which
                        will cause a compile error. If so, tweak the CDDL
                        labels or layout, or disable this option. This might
                        also make enum names different from the corresponding
                        union members.
  --file-header FILE_HEADER
                        Header to be included in the comment at the top of
                        generated files, e.g. copyright. Can be a string or a
                        path to a file. If interpreted as a path to an
                        existing file, the file's contents will be used.

```

zcbor validate --help
---------------------

```
usage: zcbor validate [-h] -c CDDL [--no-prelude] [-v] [-q] -i INPUT
                      [--input-as {yaml,json,cbor,cborhex}] -t ENTRY_TYPE
                      [--default-max-qty DEFAULT_MAX_QTY]
                      [--yaml-compatibility]

Read CBOR, YAML, or JSON data from file or stdin and validate it against a
CDDL schema file.

options:
  -h, --help            show this help message and exit
  -c, --cddl CDDL       Path to one or more input CDDL file(s). Passing
                        multiple files is equivalent to concatenating them.
  --no-prelude          Exclude the standard CDDL prelude from the build. The
                        prelude can be viewed at zcbor/prelude.cddl in the
                        repo, or together with the script.
  -v, --verbose         Print more information while parsing CDDL and
                        generating code.
  -q, --quiet           Print less information while parsing CDDL and
                        generating code.
  -i, --input INPUT     Input data file. The option --input-as specifies how
                        to interpret the contents. Use "-" to indicate stdin.
  --input-as {yaml,json,cbor,cborhex}
                        Which format to interpret the input file as. If
                        omitted, the format is inferred from the file name.
                        .yaml, .yml => YAML, .json => JSON, .cborhex => CBOR
                        as hex string, everything else => CBOR
  -t, --entry-type ENTRY_TYPE
                        Name of the type (from the CDDL) to interpret the data
                        as.
  --default-max-qty, --dq DEFAULT_MAX_QTY
                        Default maximum number of repetitions when no maximum
                        is specified. It is only relevant when handling data
                        that will be decoded by generated code. If omitted, a
                        large number will be used. Default: 0xffffffff
  --yaml-compatibility  Whether to convert CBOR-only values to YAML-compatible
                        ones (when converting from CBOR), or vice versa (when
                        converting to CBOR). When this is enabled, all CBOR
                        data is guaranteed to convert into YAML/JSON. JSON and
                        YAML do not support all data types that CBOR/CDDL
                        supports. bytestrings (BSTR), tags, undefined, and
                        maps with non-text keys need special handling. See the
                        zcbor README for more information.

```

zcbor convert --help
--------------------

```
usage: zcbor convert [-h] -c CDDL [--no-prelude] [-v] [-q] -i INPUT
                     [--input-as {yaml,json,cbor,cborhex}] -t ENTRY_TYPE
                     [--default-max-qty DEFAULT_MAX_QTY]
                     [--yaml-compatibility] -o OUTPUT
                     [--output-as {yaml,json,cbor,cborhex,c_code}]
                     [--c-code-var-name C_CODE_VAR_NAME]
                     [--c-code-columns C_CODE_COLUMNS] [--output-canonical]

Parse a CDDL file and validate/convert between CBOR and YAML/JSON. The script
decodes the CBOR/YAML/JSON data from a file or stdin and verifies that it
conforms to the CDDL description. The script fails if the data does not
conform. 'zcbor validate' can be used if only validate is needed.

options:
  -h, --help            show this help message and exit
  -c, --cddl CDDL       Path to one or more input CDDL file(s). Passing
                        multiple files is equivalent to concatenating them.
  --no-prelude          Exclude the standard CDDL prelude from the build. The
                        prelude can be viewed at zcbor/prelude.cddl in the
                        repo, or together with the script.
  -v, --verbose         Print more information while parsing CDDL and
                        generating code.
  -q, --quiet           Print less information while parsing CDDL and
                        generating code.
  -i, --input INPUT     Input data file. The option --input-as specifies how
                        to interpret the contents. Use "-" to indicate stdin.
  --input-as {yaml,json,cbor,cborhex}
                        Which format to interpret the input file as. If
                        omitted, the format is inferred from the file name.
                        .yaml, .yml => YAML, .json => JSON, .cborhex => CBOR
                        as hex string, everything else => CBOR
  -t, --entry-type ENTRY_TYPE
                        Name of the type (from the CDDL) to interpret the data
                        as.
  --default-max-qty, --dq DEFAULT_MAX_QTY
                        Default maximum number of repetitions when no maximum
                        is specified. It is only relevant when handling data
                        that will be decoded by generated code. If omitted, a
                        large number will be used. Default: 0xffffffff
  --yaml-compatibility  Whether to convert CBOR-only values to YAML-compatible
                        ones (when converting from CBOR), or vice versa (when
                        converting to CBOR). When this is enabled, all CBOR
                        data is guaranteed to convert into YAML/JSON. JSON and
                        YAML do not support all data types that CBOR/CDDL
                        supports. bytestrings (BSTR), tags, undefined, and
                        maps with non-text keys need special handling. See the
                        zcbor README for more information.
  -o, --output OUTPUT   Output data file. The option --output-as specifies how
                        to interpret the contents. Use "-" to indicate stdout.
  --output-as {yaml,json,cbor,cborhex,c_code}
                        Which format to interpret the output file as. If
                        omitted, the format is inferred from the file name.
                        .yaml, .yml => YAML, .json => JSON, .c, .h => C code,
                        .cborhex => CBOR as hex string, everything else =>
                        CBOR
  --c-code-var-name C_CODE_VAR_NAME
                        Only relevant together with '--output-as c_code' or .c
                        files.
  --c-code-columns C_CODE_COLUMNS
                        Only relevant together with '--output-as c_code' or .c
                        files. The number of bytes per line in the variable
                        instantiation. If omitted, the entire declaration is a
                        single line.
  --output-canonical    If the output data is CBOR, use canonical CBOR rules.
                        (no indefinite length lists, minially encoded floats,
                        map elements sorted by key value).

```
