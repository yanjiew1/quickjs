/* Context-sensitive Unicode case conversion over original UTF-16 input.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
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
