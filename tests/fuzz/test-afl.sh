#!/bin/bash
#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

# Fuzz one test case with AFL++. Fails if afl-fuzz fails, if the target was hardly run, or if any
# crash or hang is found.
# The target is built with ASan and UBSan, unless FUZZ_NO_SANITIZERS is set (e.g. for 32 bit builds
# when the 32 bit sanitizer runtimes are not installed).

if [[ $# -lt 3 ]]; then
	echo "Usage: $0 <seconds to run> <bit width (32 or 64)> <test case> [extra CMake arguments]"
	echo "Test cases: pet, manifest12, manifest14, everything, fragments"
	exit 1
fi

seconds=$1
bits=$2
test_case=$3
shift 3

cd "$(dirname "$0")" || exit 1

# For convenience (use if afl fails and recommends this):
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

# Don't abort if the CPU frequency scaling governor isn't "performance", as on many CI runners and
# laptops. That only makes fuzzing a bit slower.
export AFL_SKIP_CPUFREQ=1

cflags="-m${bits}"
if [[ -z "$FUZZ_NO_SANITIZERS" ]]; then
	export AFL_USE_ASAN=1
	# UBSan is enabled here instead of with AFL_USE_UBSAN, because AFL++ would add
	# -fsanitize=undefined after -fno-sanitize=function. The function sanitizer must be off since
	# zcbor deliberately calls decoders through a generic function pointer type.
	cflags="${cflags} -fsanitize=undefined -fno-sanitize=function -fsanitize-trap=undefined"
fi

if [ -d "build-afl" ]; then rm -r build-afl; fi
mkdir build-afl
pushd build-afl || exit 1
cmake .. -DCMAKE_C_COMPILER=afl-clang-fast -DCMAKE_C_FLAGS="${cflags}" -DTEST_CASE=${test_case} "$@"
[[ $? -ne 0 ]] && exit 1
make fuzz_target
[[ $? -ne 0 ]] && exit 1
popd
afl-fuzz -i build-afl/fuzz_input -o build-afl/output -V ${seconds} -- ./build-afl/fuzz_target
if [[ $? -ne 0 ]]; then
	echo "afl-fuzz failed."
	exit 1
fi

# Check that the target was actually run, in case afl-fuzz stops early without failing. Calibrating
# the seeds alone takes several executions per seed, and fuzzing typically does thousands per
# second, even with sanitizers. So a run of any length executes the target far more often than this.
min_execs=1000
stats_files=$(find build-afl/output -mindepth 2 -maxdepth 2 -type f -name fuzzer_stats 2> /dev/null)
if [[ -z "${stats_files}" ]]; then
	echo "No fuzzer_stats in build-afl/output/."
	exit 1
fi
for stats in ${stats_files}; do
	execs=$(sed -n 's/^execs_done *: *\([0-9]*\)$/\1/p' "${stats}")
	if [[ -z "${execs}" || ${execs} -lt ${min_execs} ]]; then
		echo "Only ${execs:-0} executions in ${stats} (expected at least ${min_execs})."
		exit 1
	fi
done

# Count the saved inputs of all AFL++ instances (output/<instance>/{crashes,hangs}/id:*). Hangs are
# failures too: they can be infinite loops, and crashes can be saved as hangs if core dumps are
# piped to a handler (see AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES above).
crashes=$(find build-afl/output -mindepth 3 -maxdepth 3 -type f -path "*/crashes/id:*" | wc -l)
hangs=$(find build-afl/output -mindepth 3 -maxdepth 3 -type f -path "*/hangs/id:*" | wc -l)
echo "Crashes: ${crashes}, hangs: ${hangs}"
if [[ ${crashes} -ne 0 || ${hangs} -ne 0 ]]; then
	echo "The failing inputs are in build-afl/output/*/crashes/ and build-afl/output/*/hangs/."
	exit 1
fi
exit 0
