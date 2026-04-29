#!/usr/bin/env python3
#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

from pathlib import Path

from .zcbor.zcbor import (
    CddlValidationError,
    DataTranslator,
    zfrozendict,
    main
)
