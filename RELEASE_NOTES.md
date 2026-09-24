# zcbor v. 1.0.0 (2026-09-24)

Any new bugs, requests, or missing features should be reported as [Github issues](https://github.com/nordicsemi/zcbor/issues).

## Improvements:

### C libraries:

* Redesign the fragment decoding API and add new fragmented string encoding API
* zcbor_common: Add support for backing up elem_state
* zcbor_common.c: Deprecate the max_elem_count argument in zcbor_process_backup() to fix an issue.
* zcbor_common.h: Introduce the ZCBOR_CAST_FP() macro for casting function pointers (RECOMMENDED, see MIGRATION_GUIDE)
* zcbor_decode: Introduce the 'force' arg to *_(list|map|bstr)_end_decode()
* Add support for encoding and decoding smaller int types (8 and 16 bit).
* Improved backup handling (backups could sometimes be mismatched):
  * src: Explicitly throw an error if backups are mismatched
  * zcbor_decode: Always exit backup if a *_start_decode() fails
* Add zcbor_bstr_end_force_(de|en)code and use them in generated code
* src: Avoid calling memmove/memcpy with NULL pointers
* zcbor_encode.c: Allow size_hint for maps and lists to be smaller than actual size
* zcbor_common.h: Add NULL-checking of the state var in all public APIs
* zcbor_common: Adapt zcbor_entry_function() to be able to create elem_states
* zcbor_decode.c: Gracefully handle calling elem_processed early
* zcbor_common.h: Convert zcbor_flags_to_states() and zcbor_round_up() to macros

### zcbor.py:

* General zcbor.py and Python code improvements:
  * zcbor.py: Make arguments to all __init__() and from_cddl() keyword-only
  * zcbor.py: Make changes to support both versions 5 and 6 of cbor2.
  * Reformat all python files with black
  * Add support for Python up to 3.15, and remove support for defunct Python versions
* CDDL parsing:
  * zcbor.py: Refactor .eq, .default, .size, etc. to accept types, not just numbers
  * zcbor.py: Allow value ranges (x..y) to contain types and floats
  * zcbor.py: Add support for .default for unions
  * zcbor.py: Refactor exceptions and improve error reporting for CDDL parsing
  * zcbor.py: Refactor the CddlParser class to make it able to run by itself
  * zcbor.py: Refactor parsing of "float16", "float32-64" etc
  * zcbor.py: Support unicode escape sequences
* Code generation:
  * zcbor.py: Add support for unordered maps in generated code
  * zcbor.py: Add cli option to generate defines for magic values/numbers
  * zcbor.py: Use ZCBOR_CAST_FP() in generated code
  * zcbor.py: Add basic support for .default in code generation.
  * zcbor.py: Use smaller int types in generated code when possible.
  * zcbor.py: Avoid redundant single-member structs in generated types.
  * zcbor.py: Include tag in var name if using the type for name
  * zcbor.py: Avoid duplicate declarations
  * zcbor.py: Use the DEFAULT_MAX_QTY define in the generated code
  * zcbor.py: Add arg type checking for generated entry functions
* Data translation:
  * zcbor.py: Support canonical encoding when converting to CBOR in script
  * zcbor.py: Add custom float precision support for YAML conversion
  * zcbor.py: Performance improvements in DataTranslator

## Bugfixes:

### C libraries:

* zcbor_decode.c: Fix bug in zcbor_tag_expect() where it changes elem_count if it fails.
* zcbor_decode.c: Fix bug in zcbor_any_skip() where it doesn't catch a string overflow if incorrectly formatted
* zcbor_common: Fix issue where the wrong backup was used for searching in unordered maps
* zcbor_common.c: Fix ubsan (-fsanitize=undefined) and strict-aliasing errors in float conversion 16 <-> 32
* zcbor_print.h: Fix an issue where NULL payload could cause crash

### zcbor.py:

* CDDL parsing:
  * zcbor.py: Fix an issue with optional elements ('?') where a non-present element can leave the decoding in a bad state.
  * zcbor.py: Fix a naming bug causing unfixable CDDL naming collisions.
  * zcbor.py: Fix issue where semicolons in string literals were seen as comments
  * zcbor.py: Fix issue where newlines in string literals were removed
  * zcbor.py: Fix bug in quote parsing when nested with braces.
  * zcbor.py: Fix a small bug for repeated elements in unions, where _count variable could be missing if the element is unambiguous.
  * zcbor.py: Fix bug where .le 1 .ge 1 would cause an issue.
  * zcbor.py: Fix control groups "&()" so they can have a single member
* Code generation:
  * zcbor.py: Fix bug where float range checks could be ignored
* Data translation:
  * zcbor.py: Fix reading of .cbor files


# zcbor v. 0.9.1 (2024-10-17)

Any new bugs, requests, or missing features should be reported as [Github issues](https://github.com/NordicSemiconductor/zcbor/issues).

## Improvements:

 * README improvements:
   * pypi_README.md: Add separate README for use in the pypi package
   * README: Clarify what ZCBOR_STOP_ON_ERROR does
   * RELEASE_NOTES.md: Move "Unsupported CDDL features" to the README

## Bugfixes:

## Unsupported CDDL features

This section has been moved to the README, and this heading will be removed in subsequent releases.


# zcbor v. 0.9.0 (2024-08-27)

Any new bugs, requests, or missing features should be reported as [Github issues](https://github.com/NordicSemiconductor/zcbor/issues).

## Improvements:

 * zcbor.py:
   * Allow --file-header to accept a path to a file with header contents
   * Avoid generated names that are C keywords (capitalize them if present)
   * Whenever generated code uses function pointers, add dummy calls using the function pointers, to verify that the arguments have the right type.
   * Optimize range checks
   * Convert function docs in comments to pep-257 docstrings and add some new docstrings
 * C libraries:
   * Remove zcbor_simple_*() functions from the header file to discourage their use.
   * zcbor_decode: Allow deciding at run-time whether to enforce canonical data (via 'enforce_canonical'), instead of via ZCBOR_CANONICAL.
   * zcbor_print.h: Move function implementations into new file zcbor_print.c
 * Documentation:
   * MIGRATION_GUIDE.md: Add a migration guide file
   * README.md: Clarification on n_flags
   * ARCHITECTURE.md: Small improvements to bring the document up to date

## Bugfixes:

 * zcbor.py:
   * Fixes to support code generation on Windows
   * Handle when files are on different drives in Windows
   * Fix a few bugs in code generation to do with dereferencing
   * Fix bug where NINT literals were not given unique names
   * Fix a bug where empty groups and lists would give an error
   * Fix bstr names based on size
   * Don't omit zcbor_union_elem_code() when there is a range check.
   * Fix bug where paths to generated files could not be absolute
   * Fix a bug where some decoding function were not called
   * Fix/improve deeply nested types
   * Remove outdated assert statement that caused incorrect assert
   * Fix a bug in CDDL parsing to do with nested brackets
 * zcbor_decode:
   * Fix various things related to constant_state
   * Fix a bug with enforce_canonical (ZCBOR_CANONICAL) and floats
 * zcbor_encode.c: Fix a length bug in zcbor_bstr_end_decode()
 * zcbor_tags.h: Typo fix YANK -> YANG

## Unsupported CDDL features
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


# zcbor v. 0.8.1 (2024-01-26)

Any new bugs, requests, or missing features should be reported as [Github issues](https://github.com/NordicSemiconductor/zcbor/issues).

See also the [release notes for 0.8.0](#zcbor-v-080-2024-01-09) below.

## Improvements:

 * zcbor.py: Use zcbor_uint_decode() when decoding only positive enums
 * zcbor_common.h: Add ZCBOR_VERSION macro

## Bugfixes:

 * zcbor_common.h: Add forward declaration for strnlen()
 * zcbor.py: Fix conversion of UINT8_MAX to avoid script errors when using e.g. 255 in CDDL
 * zcbor_print.h: Fix iterator type and print formats to avoid compiler warnings

## Unsupported CDDL features
Not all features outlined in the CDDL specs [RFC8610](https://datatracker.ietf.org/doc/html/rfc8610), [RFC9090](https://datatracker.ietf.org/doc/html/rfc9090), and [RFC9165](https://datatracker.ietf.org/doc/html/rfc9165) are supported by zcbor.
The following is a list of limitations and missing features:

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


# zcbor v. 0.8.0 (2024-01-09)

Any new bugs, requests, or missing features should be reported as [Github issues](https://github.com/NordicSemiconductor/zcbor/issues).

## Improvements:
 * C library: Add support for searching for elements in a map.
 * Overhaul zcbor logging/printing
   * Refactor printing of errors from generated functions
   * zcbor_print.h: Improve zcbor_trace() and other tracing
   * zcbor_print.h: Move from printk to printf
   * Rename zcbor_print() to zcbor_log()
   * zcbor_print.h: Move printing code to a new file zcbor_print.h
   * zcbor_debug.h: Add function for getting error string and printing it
 * Performance improvements
   * zcbor.py: Refactor the regex matching to fix label and remove all_types_regex
   * zcbor.py: Merge regexes for pure types without values
   * zcbor.py: Add a cache for compiled regex patterns
   * zcbor.py: Don't regenerate the big list of regexes every call
   * zcbor_encode: Simplify value_encode() by using the new zcbor_header_len
   * zcbor_decode.c: Streamline value_extract
   * zcbor_common: Refactor zcbor_header_len
   * zcbor.py: Don't use zcbor_present_encode()
   * zcbor_common: Add a zcbor_entry_function helper
 * Code generation name improvements:
   * zcbor.py: Make generated variable names C-compatible.
   * zcbor.py: Change name of generated choice enum members to add a '_c'
   * Avoid leading and trailing as well as repeated underscores
 * API cleanup/improvements:
   * include: Rearrange and improve zcbor_decode.h and zcbor_encode.h
   * zcbor_encode: Change zcbor_tag_encode to take a pointer argument
   * zcbor_common: Rename ZCBOR_FLAG_TRANSFER_PAYLOAD to ZCBOR_FLAG_KEEP_PAYLOAD
   * zcbor_decode: Add _pexpect() functions
 * Internal cleanup:
   * zcbor_encode, zcbor_decode: Move inline functions from header files
   * zcbor_decode.c: Move MAJOR_TYPE() and ADDITIONAL() macros to zcbor_common.h
   * src: Abstract float16 conversion and move it to zcbor_common.c
   * zcbor_common: Add ZCBOR_FLAG_KEEP_DECODE_STATE to zcbor_process_backup()
   * Move zcbor_array_at_end() from zcbor_common to zcbor_decode
 * Move from setup.py to pyproject.toml for creation of zcbor packages
 * zcbor_decode: Add validation that data follows canonical CBOR rules
 * zcbor.py: Change the label regex to accept non-latin characters
 * C library: Change usage of strlen to use strnlen

## Bugfixes:
 * zcbor.py: Fix range_checks for 'OTHER' type
 * zcbor_decode.c: Fix "'num_decode' may be used uninitialized"
 * zcbor.py: Generate cmake files with linux paths on Windows
 * Remove references to uint_fast32_t
 * Use ZCBOR_BIG_ENDIAN instead of CONFIG_BIG_ENDIAN
 * zcbor.py: Fix cborhex generation so it generates newlines instead of spaces
 * zcbor.py: Adjust the lower bound on negative numbers.
 * zcbor_encode, zcbor_decode: fix double promotion warnings
 * zcbor_print.h: Add missing errors to zcbor_error_str()
 * zcbor_print.h: Fix memcmp expression
 * zcbor.py: DataTranslator: fix type checking

## Unsupported CDDL features
Not all features outlined in the CDDL specs [RFC8610](https://datatracker.ietf.org/doc/html/rfc8610), [RFC9090](https://datatracker.ietf.org/doc/html/rfc9090), and [RFC9165](https://datatracker.ietf.org/doc/html/rfc9165) are supported by zcbor.
The following is a list of limitations and missing features:

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


# zcbor v. 0.7.0 (2023-04-01)

Any new bugs, requests, or missing features should be reported as [Github issues](https://github.com/NordicSemiconductor/zcbor/issues).

## Improvements:

### Code generation
 * Add support for float16 encoding and decoding
 * Add support for C++ in generated code
 * Use (U)INT(8|16|32|64)_(MIN|MAX) macros in generated code
 * Add the --c-code-columns option for adding newlines to c_code
 * Refactor CBOR <-> YAML/JSON translation
 * Allow adding file headers to generated files, e.g. for copyright.

### C libraries
 * Add support for float16 encoding and decoding
 * Add helper functions zcbor_compare_strings() and zcbor_header_len()
 * Rename prim/primitive to simple
 * Add new APIs zcbor_uint_(encode/decode)
 * Move tags enum into separate file zcbor_tags.h and add more tags from the IANA list
 * When generating cmake files, make all paths relative to the cmake file.
 * Use the zcbor_compare_strings() function in str_expect()
 * Make the zcbor source files compile with -Wall and -Wconversion
 * Refactor zcbor_any_skip()
 * Change all uint_fast32_t usage to use size_t instead

### Other
 * README: Improve docs, particularly to help new users
 * Add two samples: a hello world with no code generation, and one with code generation and script usage
 * Port decode/encode/unit tests to new ztest API
 * Fix fuzz tests

## Bugfixes:
 * zcbor.py: Fix 'zcbor --version' so it works again
 * zcbor.py: Fix cbor bstrs so they properly check unambiguous values
 * zcbor.py: Fix rendering of entry functions when they have no output
 * zcbor.py: Don't flag cbor bstrs as unambiguous

## Unsupported CDDL features
Not all features outlined in the CDDL specs [RFC8610](https://datatracker.ietf.org/doc/html/rfc8610), [RFC9090](https://datatracker.ietf.org/doc/html/rfc9090), and [RFC9165](https://datatracker.ietf.org/doc/html/rfc9165) are supported by zcbor.
The following is a list of limitations and missing features:

 * Map elements in data must appear in the same order as they appear in the CDDL.
 * C API doesn't support searching through a map.
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


# zcbor v. 0.6.0 (2022-10-12)

Any new bugs, requests, or missing features should be reported as [Github issues](https://github.com/NordicSemiconductor/zcbor/issues).

## Improvements:
 * Refactor zcbor CLI to add 'verify' and put all options after keyword
 * Make several C++ related improvements in the C library
 * Fix Coverity warnings in C library and generated code
 * zcbor_decode: Add zcbor_int[32|64]_expect_union
 * zcbor_common.h: Add new zcbor_assert_state() macro
 * zcbor_common.h: Rename ARRAY_SIZE to ZCBOR_ARRAY_SIZE to avoid #include order problems in Zephyr
 * .github: Refactor Github action and add QEMU testing on mps2_an521 (ARM) and qemu_malta_be (MIPS, big-endian)
 * zcbor.py: Raise argparse error on insufficient --output-* args
 * zcbor_decode.h: Define variables to avoid rvalue address errors
 * Add release dates to all RELEASE_NOTES entries

## Bugfixes:
 * zcbor.py: Fix bug where the union_int optimization decodes twice
 * zcbor_encode.c: Fix error for big-endian architectures

## Unsupported CDDL features
Not all features outlined in the [CDDL spec](https://datatracker.ietf.org/doc/html/rfc8610) are supported by zcbor.
The following is a list of limitations and missing features:

 * Map elements in data must appear in the same order as they appear in the CDDL.
 * C API doesn't support searching through a map.
 * 16-bit floating point numbers (float16).
 * Using `&()` to turn groups into choices (unions). `&()` is supported when used with `.bits`.
 * Representation Types (`#x.y`), except for tags (`#6.y(foo)`) which are supported.
 * Unwrapping (`~`)
 * The control operator `.regexp`.
 * The control operator `.ne`.
 * The control operator `.default`.
 * Generics (`foo<a, b>`).
 * Most of the "Extended Diagnostic Notation" is unsupported.


# zcbor v. 0.5.1 (2022-06-21)

Any new bugs, requests, or missing features should be reported as [Github issues](https://github.com/NordicSemiconductor/zcbor/issues).

## Improvements:
 * requirements.txt: Change PyYAML requirement from >6.0.0 to >5.4.1 to be more widely compatible.
 * zcbor_encode/zcbor_decode: Add new functions zcbor_int_encode() and zcbor_int_decode() which accept the integer size as an argument.
 * zcbor.py: Use zcbor_int_encode() and zcbor_int_decode() when operating directly on _choice enums, since enums can be any size, depending on compiler and project. This removes the restrictions that enums must be 32 bits.

## Bugfixes:
 * tests: Fix an error in manifest3.cddl (encode/test1) (naming collision with 'uri')

## Unsupported CDDL features
Not all features outlined in the [CDDL spec](https://datatracker.ietf.org/doc/html/rfc8610) are supported by zcbor.
The following is a list of limitations and missing features:

 * Map elements in data must appear in the same order as they appear in the CDDL.
 * C API doesn't support searching through a map.
 * 16-bit floating point numbers (float16).
 * Using `&()` to turn groups into choices (unions). `&()` is supported when used with `.bits`.
 * Representation Types (`#x.y`), except for tags (`#6.y(foo)`) which are supported.
 * Unwrapping (`~`)
 * The control operator `.regexp`.
 * The control operator `.ne`.
 * The control operator `.default`.
 * Generics (`foo<a, b>`).
 * Most of the "Extended Diagnostic Notation" is unsupported.


# zcbor v. 0.5.0 (2022-06-13)

Any new bugs, requests, or missing features should be reported as [Github issues](https://github.com/NordicSemiconductor/zcbor/issues).

## Improvements:

### Code generation
 * Add the --short-names option when generating code, which shortens names in struct definitions.
 * Allow --default-max-qty to accept a string label when generating code so that the value can be configured after generation.
 * Update versions of dependencies in pip requirements.

### C Libraries
 * Add const to *ptr argument of cbor_tstr_encode_ptr.
 * Make it easier to initialize zcbor_state_t objects: Allow single states as input to zcbor_new_state() and remove return value.
 * Make zcbor_list/map_end_encode() more robust to wrong max_num arguments (when ZCBOR_CANONICAL is enabled).
 * Add zcbor_peek_error() for reading the error value without clearing it.
 * Add zcbor_size_put/encode/decode/expect functions, to allow directly encoding and decoding size_t type variables.

### Tests
 * Test on multiple Python versions
 * Add tests for using ranges (< or >) with floats in code generation.
 * Add/enable test for signature check of SUIT manifests.


## Bugfixes:
 * zcbor.py: Fix missing size check for repeated bstrs with their own type
 * zcbor_encode.c: Fix encoding of floats equal to 0.0 (or any with leading 0s)
 * zcbor_encode.h: Fix "taking address of rvalue" errors with some compiler configurations, stemming from zcbor_b|tstr_encode_ptr()
 * zcbor.py: Add forward declarations to functions in generated code.
 * zcbor.py: Fix bug when adding tagged (#6.x) items to larger types, e.g. lists.
 * zcbor.py: Fix naming bug for container types with no members.
 * zcbor.py: "union_int" optimization: If all union members start with an integer, the optimization does a single int_decode() and identifies the correct union member by the result.
   * zcbor.py: Fix a bug preventing the "union_int" optimization from being applied.
   * zcbor.py: Expand the "union_uint" optimization to work on all integers, instead of only positive integers.


## Unsupported CDDL features
Not all features outlined in the [CDDL spec](https://datatracker.ietf.org/doc/html/rfc8610) are supported by zcbor.
The following is a list of limitations and missing features:

 * Map elements in data must appear in the same order as they appear in the CDDL.
 * C API doesn't support searching through a map.
 * 16-bit floating point numbers (float16).
 * Using `&()` to turn groups into choices (unions). `&()` is supported when used with `.bits`.
 * Representation Types (`#x.y`), except for tags (`#6.y(foo)`) which are supported.
 * Unwrapping (`~`)
 * The control operator `.regexp`.
 * The control operator `.ne`.
 * The control operator `.default`.
 * Generics (`foo<a, b>`).
 * Most of the "Extended Diagnostic Notation" is unsupported.


# zcbor v. 0.4.0 (2022-03-31)

**cddl-gen has been renamed to zcbor!**

Any new bugs, requests, or missing features should be reported as [Github issues](https://github.com/NordicSemiconductor/zcbor/issues).

## Improvements:

### CDDL language
 * Add support for 32-bit and 64-bit floating point numbers.
 * Add support for `undefined` in CDDL.
 * Add the CDDL prelude (from RFC 8610 Appendix D) to all builds (unless excluded with `--no-prelude`)
 * Add support for more complex `.cbor` and `.cborseq` expressions.


### Command line and Python module
 * Rename script/module to zcbor.
 * Better 64-bit support:
   * New argument: `--default-bit-size` which controls the assumed size of integers. See zcbor code `--help` for more information.
 * Add the `--include-prefix` option for path prefix to generated h files.
 * Allow using both `--encode` and `--decode` together to generate both at the same time.
 * Make changes to generated code to build with `-Wpedantic` and `-Wconversion`.


### C Libraries
 * All public function names have been prefixed with `zcbor_`.
 * Many function names have also been changed slightly.
 * Better API docs.
 * Add error value to state, which is populated when a function fails (returns `false`).
   * Add a `stop_on_error` feature that makes functions abort when an error has occurred (to avoid checking the output of all functions).
 * Add support for 32-bit and 64-bit floating point numbers.
 * Better 64-bit support:
   * New 64-bit integer decoding and encoding functions (in addition to existing 32-bit functions).
   * `uint32_t` usage has been largely transitioned to `size_t` or `uint_fast32_t` which are typically the same size as the architecture (32 or 64 bits).
   * This means that list/map/string sizes can be 64 bits on 64-bit architectures. Note that having 64-bit sizes on 32-bit architectures rarely make sense since they won't fit in the address space.
 * New helper macros for instantiating state variables: `ZCBOR_STATE_D()` and `ZCBOR_STATE_E()` for decoding and encoding respectively.
 * Add support for decoding payloads split over multiple memory chunks.
   * There are new APIs in zcbor_common.h and zcbor_decode.h to handle this. Look for `zcbor_update_state()` and various functions with `fragment` in the name.
   * Strings can be split between chunks, there is a new type `struct zcbor_string_fragment` for this purpose.
   * Data headers can not be split.
   * Backups to previous chunks cannot be restored.
 * Split `zcbor_multi_encode()` into `zcbor_multi_encode()` and `zcbor_multi_encode_minmax()`. The former is more useful for manual use, while the latter is for generated code.
 * Remove `zcbor_any_encode()` since it was just an alias for `zcbor_nil_put()`.
 * Rename `zcbor_string_type_t` to `struct zcbor_string`.
 * Rename `zcbor_state_backups_t` to `struct zcbor_state_constant`.
 * Add support for decoding and encoding `undefined` CBOR values.
 * `zcbor_any_decode()`:
   * Rename to `zcbor_any_skip()`
   * Add support for indeterminate-length arrays.
   * Have `zcbor_any_skip()` consume any tags that are present.
 * Introduce `zcbor_bstr_expect_ptr()`, `zcbor_bstr_put_ptr()`, `zcbor_tstr_expect_ptr()`, and `zcbor_tstr_put_ptr()`
 * zcbor_common.h: Add an enum (`zcbor_rfc8949_tag`) with some tag values.
 * Move CBOR intro text from zcbor_decode.h to README.
 * Control asserts separately from verbose (add ZCBOR_ASSERT option)
 * Introduce `zcbor_list_map_end_force_encode()` and `zcbor_list_map_end_force_decode()`.


### Tests
 * decode: Remove test4 since it hasn't been used or updated.
 * New unit test directory and project. Tests:
   * 64-bit vs 32-bit integers.
   * 64-bit vs 32-bit strings.
   * String encoding and decoding macros (`_lit()`, `_arr()`, `_term()`).
   * Payload chunks and string fragments.
 * Collect boilerplate Cmake code for tests into a new file test_template.cmake.
 * Add tests for draft-ietf-suit-manifest-16.
 * Rename "strange" tests to "corner_cases"
 * Build tests with `-Wpedantic` and `-Wconversion`


## Bugfixes:
 * zcbor.py: Avoid #include-ing the full path of the type file.
 * zcbor.py: Avoid creating functions with "None" result type.
 * src: Fix code so it builds correctly with asserts enabled.
 * zcbor_decode.c: Fix `list_map_start_decode()` to handle elem_count correctly
 * zcbor_decode.c: Fix `zcbor_any_skip()` so it reverts the state correctly on fail.
 * zcbor.py: Fix regex for ints/floats to ignore names with hyphens (-)
 * zcbor.py: Fix error in 'convert' where a bstr dict value was incorrectly translated
 * setup.py: Fix windows install
 * zcbor.py: Add suffix for literal values larger than 32-bits.
 * src: Fix code to build with `-Wpedantic` and `-Wconversion`.
 * zcbor.py: Fix some code generation bugs to do with range checking (min/max size) of ints and CBOR-encoded bstrs.
 * zcbor.py: Fix bug where indirection for ints with range checks was wrong.
 * zcbor.py: Fix bugs in range checks in `bstr`s with `.cbor` statements.

## Unsupported CDDL features

Not all features outlined in the [CDDL spec](https://datatracker.ietf.org/doc/html/rfc8610) are supported by zcbor.
The following is a list of limitations and missing features:

 * Map elements in data must appear in the same order as they appear in the CDDL.
 * C API doesn't support searching through a map.
 * 16-bit floating point numbers (float16).
 * Using `&()` to turn groups into choices (unions). `&()` is supported when used with `.bits`.
 * Representation Types (`#x.y`), except for tags (`#6.y(foo)`) which are supported.
 * Unwrapping (`~`)
 * The control operator `.regexp`.
 * The control operator `.ne`.
 * The control operator `.default`.
 * Generics (`foo<a, b>`).
 * Most of the "Extended Diagnostic Notation" is unsupported.


# cddl-gen v. 0.3.0 (2021-11-17)

Any new bugs, requests, or missing features should be reported as [Github issues](https://github.com/NordicSemiconductor/cddl-gen/issues).

## Improvements:

### CDDL language
 * Add more support for hexadecimal/octal/binary numbers in CDDL. E.g. in size ranges or value limits
 * Add support for ranges with three dots in CDDL files
 * Change CDDL quantifier syntax from `x**y` to `x*y`
 * Add support for `$$` and `$`
 * Add support for the `.bits` operator.

### Command line
 * Change command line name from `cddl_gen` to `cddl-gen`
 * Add `--dq` as a shorthand for `--default-max-qty`
 * Allow passing multiple cddl files via command line
 * Allow adding the current git sha to generated file headers.
 * Various changes to the logic for naming in code generation
   * This can give different names of e.g. struct members compared with previous versions of cddl-gen
 * Generate a cmake file when generating code.
   * The `target_cddl_source()` Cmake function has been removed.
   * See README.md for more info.

### Python module
 * Improve tags support in DataTranslator
 * Improve decoding of cbor bstrs in DataTranslator
   * Provide the raw string along with the decoded object.

### C Libraries
 * Refactor cbor_common.c
 * Add support for indefinite length arrays in cbor_decode.c.

### Tests
 * Move all cddl files into tests/cases
 * Run tests with twister instead of custom bash scripts
 * Add tests for draft-ietf-suit-manifest-14


## Bugfixes:
 * cbor_encode.h: Add missing `.` in `tstrx_put` and `tstrx_put_term`
 * cbor_encode.c: Fix `get_result_len` and add big-endian support
 * cddl_gen.py: Fix bug where range checks were dropped
 * cbor_decode.c: Fix bug where a `nil` could be interpreted as `true`
 * cddl_gen.py: Fix parsing of ranges with no max (`n*` or `n**`)
 * cbor_encode.c: Fix bug where minimum quantity restrictions were ignored
 * cddl_gen: Fix generation of map-within-map
 * cbor_decode.c: Add overflow checks and fix assert in `any_decode()`.


## Unsupported CDDL features

Not all features outlined in the [CDDL spec](https://datatracker.ietf.org/doc/html/rfc8610) are supported by cddl-gen.
The following is a list of limitations and missing features:

 * Map elements in data must appear in the same order as they appear in the CDDL.
 * Floating point numbers.
 * Using `&()` to turn groups into choices (unions). `&()` is supported when used with `.bits`.
 * Representation Types (`#x.y`), except for tags (`#6.y(foo)`) which are supported.
 * Unwrapping (`~`)
 * The control operator `.regexp`.
 * The control operator `.ne`.
 * The control operator `.default`.
 * Generics (`foo<a, b>`).
 * Not all types from the "standard prelude" are supported.
 * Most of the "Extended Diagnostic Notation" is unsupported.
