/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Generated using zcbor version 1.0.0
 * https://github.com/nordicsemi/zcbor
 */

#ifndef PET_TYPES_H__
#define PET_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <zcbor_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Which value for --default-max-qty this file was created with.
 *
 *  This can be safely edited.
 *
 *  See `zcbor --help` for more information about --default-max-qty
 */
#define ZCBOR_PET_DEFAULT_MAX_QTY 3

struct Pet {
	struct zcbor_string names[ZCBOR_PET_DEFAULT_MAX_QTY];
	size_t names_count;
	struct zcbor_string birthday;
	enum {
		Pet_species_cat_c = 1,
		Pet_species_dog_c = 2,
		Pet_species_other_c = 3,
	} species_choice;
};

#ifdef __cplusplus
}
#endif

#endif /* PET_TYPES_H__ */
