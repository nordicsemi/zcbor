#!/bin/bash
#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

# Fuzz one test case with libFuzzer, with ASan and UBSan. Fails if a crash or a hang (an input that
# runs for more than 10 seconds) is found. The failing inputs are placed in build-libfuzzer/.

if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <seconds to run> [test case (default: manifest12)] [extra CMake arguments]"
	echo "Test cases: pet, manifest12, manifest14, everything, fragments"
	exit 1
fi

seconds=$1
test_case=${2:-manifest12}
shift
shift

cd "$(dirname "$0")" || exit 1

# The function sanitizer must be off since zcbor deliberately calls decoders through a generic
# function pointer type.
cflags="-fsanitize=fuzzer,address,undefined -fno-sanitize=function -fno-sanitize-recover=all"

if [ -d "build-libfuzzer" ]; then rm -r build-libfuzzer; fi
mkdir build-libfuzzer
pushd build-libfuzzer || exit 1
cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_C_FLAGS="${cflags} -DLIBFUZZER" -DCMAKE_C_COMPILER_WORKS=On -DTEST_CASE=${test_case} "$@"
[[ $? -ne 0 ]] && exit 1
make fuzz_target
[[ $? -ne 0 ]] && exit 1
popd
mkdir build-libfuzzer/corpus

# Inputs are expected to run in milliseconds, so -timeout is generous, but much shorter than the
# default 1200 seconds, which would stall a hanging run for 20 minutes.
libfuzzer_args=(-max_total_time=${seconds} -timeout=10 -artifact_prefix=build-libfuzzer/)
case ${test_case} in
	# Larger inputs are rejected by the test case (see MAX_PAYLOAD in fuzz_fragments.c).
	fragments) libfuzzer_args+=(-max_len=1025);;
esac
./build-libfuzzer/fuzz_target "${libfuzzer_args[@]}" \
	build-libfuzzer/corpus build-libfuzzer/fuzz_input
[[ $? -ne 0 ]] && exit 1
exit 0
