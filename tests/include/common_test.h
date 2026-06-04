/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COMMON_TEST_H__
#define COMMON_TEST_H__

#define CONCAT_BYTE(a,b) a ## b

#ifdef TEST_INDEFINITE_LENGTH_ARRAYS
#define LIST(num) 0x9F /* Use short count 31 (indefinite-length). Note that the 'num' argument is ignored */
#define LIST2(num) 0x9F /* Use short count 31 (indefinite-length). Note that the 'num' argument is ignored */
#define LIST3(num) 0x9F /* Use short count 31 (indefinite-length). Note that the 'num' argument is ignored */
#define MAP(num) 0xBF  /* Use short count 31 (indefinite-length). Note that the 'num' argument is ignored */
#define ARR_ERR1 ZCBOR_ERR_WRONG_TYPE
#define ARR_ERR2 ZCBOR_ERR_NO_PAYLOAD
#define ARR_ERR3 ZCBOR_ERR_WRONG_TYPE
#define END 0xFF,
#define STR_LEN(len, lists) (len + lists)
#else
#define LIST(num) CONCAT_BYTE(0x8, num)
#define LIST2(num) CONCAT_BYTE(0x9, num)
#define LIST3(num) 0x98, num
#define MAP(num) CONCAT_BYTE(0xA, num)
#define ARR_ERR1 ZCBOR_ERR_HIGH_ELEM_COUNT
#define ARR_ERR2 ZCBOR_ERR_HIGH_ELEM_COUNT
#define ARR_ERR3 ZCBOR_ERR_NO_PAYLOAD
#define END
#define STR_LEN(len, lists) (len)
#endif

#endif /* COMMON_TEST_H__ */
