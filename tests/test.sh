#!/bin/bash
#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

pushd "scripts"
python3 -m unittest test_zcbor test_repo_files
[[ $? -ne 0 ]] && popd && exit 1
popd

if [[ -z "$ZEHPYR_BASE" ]]; then
        ZEPHYR_BASE=$(west topdir)/zephyr
fi

$ZEPHYR_BASE/scripts/twister -M -v -T . -W --exclude-tag release --platform native_posix --platform native_posix/native/64 --platform mps2/an521/cpu0 $*
