# Fuzz testing

Simple setup using AFL++ or libFuzzer.
Use the provided scripts.
They build the fuzz target with ASan and UBSan, and fail if the fuzzer fails, or if any crash or hang is found.

## Test cases

| Test case    | What is fuzzed |
|--------------|----------------|
| `pet`        | Generated decoding and encoding code for `pet.cddl`. Decoded data is encoded and re-decoded to check that the encoding is stable. |
| `manifest12` | Generated decoding code for `manifest12.cddl` (SUIT manifest), including the bstr-wrapped command sequences. |
| `manifest14` | Generated decoding code for `manifest14.cddl` with `--unordered-maps`, i.e. the `zcbor_unordered_map_*()` API with `ZCBOR_MAP_SMART_SEARCH`. |
| `everything` | Generated decoding and encoding code for `everything.cddl`, a list of a union of most CDDL types. Encoding is checked like for `pet`. |
| `fragments`  | The fragmented string API (`ZCBOR_FRAGMENTS`). The payload is split into separately allocated sections, and the result is compared to decoding the whole payload. See `fuzz_fragments.c`. |

In addition to sanitizer errors, the test cases report a crash if zcbor returns an error that indicates a bug in zcbor (e.g. `ZCBOR_ERR_BACKUP_MISMATCH`), or if a check in the test case itself fails.

Pass `-DCANONICAL=ON` as an extra argument to build with `ZCBOR_CANONICAL`.

## libFuzzer

```
./test-libfuzzer.sh <seconds to run> [test case] [extra CMake arguments]
./test-libfuzzer.sh 30 everything
```

Failing inputs are placed in `build-libfuzzer/`.
An input that runs for more than 10 seconds is a hang (`timeout-*`).

## AFL++

```
./test-afl.sh <seconds to run> <bit width (32 or 64)> <test case> [extra CMake arguments]
./test-afl.sh 30 64 everything -DCANONICAL=ON
```

Failing inputs are placed in `build-afl/output/default/crashes/` and `build-afl/output/default/hangs/`.
The script also fails if `afl-fuzz` fails, e.g. because a seed crashes or hangs, or if the target was run fewer than 1000 times.
Set `FUZZ_NO_SANITIZERS=1` to build without sanitizers, e.g. for 32 bit builds when the 32 bit sanitizer runtimes are not installed.

If core dumps are piped to a handler (`/proc/sys/kernel/core_pattern` starts with `|`, e.g. with apport or systemd-coredump), AFL++ can miss crashes or save them as hangs.
The script sets `AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1` so AFL++ runs anyway, since hangs fail the test too.
Set `FUZZ_STRICT=1` to let AFL++ refuse to run in this case instead, as CI does after running `echo core | sudo tee /proc/sys/kernel/core_pattern`.
