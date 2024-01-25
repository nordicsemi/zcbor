#!/bin/bash
#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

if [[ -z "$ZEHPYR_BASE" ]]; then
        ZEPHYR_BASE=$(west topdir)/zephyr
fi

$ZEPHYR_BASE/scripts/twister -M -v -T . -W --platform qemu_malta/qemu_malta/be $*
