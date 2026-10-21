/* Context-sensitive Unicode case conversion over original UTF-16 input.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT. */
#ifndef UNICODE_CASE_CONVERSION_H
#define UNICODE_CASE_CONVERSION_H
#include "../cutils/cutils.h"

typedef enum UnicodeCaseLocale {
    UNICODE_CASE_DEFAULT,
    UNICODE_CASE_TURKIC,
    UNICODE_CASE_LITHUANIAN
} UnicodeCaseLocale;

/* Append native-endian UTF-16 units to an initialized buffer. Preserve lone
   surrogates and NUL. All context tests inspect the original input, including
   characters removed by a mapping. Return -1 on allocation/size failure; the
   caller owns and frees the buffer on both success and failure. */
int unicode_case_convert(DynBuf *out, const uint16_t *input, size_t length,
                         BOOL to_lower, UnicodeCaseLocale locale);
#endif
