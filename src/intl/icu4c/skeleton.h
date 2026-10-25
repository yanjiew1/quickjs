/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_INTL_ICU4C_SKELETON_H
#define QJS_INTL_ICU4C_SKELETON_H
#include "common.h"
#include "../common/decimal.h"

typedef struct QJSICU4CSkeleton {
    UChar text[1024]; /* Resolved option tokens, not unbounded locale/user text. */
    int32_t length;
    QJSIntlStatus error;
} QJSICU4CSkeleton;

void qjs_icu4c_skeleton_append(QJSICU4CSkeleton *, QJSIntlBytes ascii);
void qjs_icu4c_skeleton_token(QJSICU4CSkeleton *, const char *ascii);
QJSIntlStatus qjs_icu4c_digit_skeleton(QJSICU4CSkeleton *, const QJSIntlDecimalOptions *);
#endif
