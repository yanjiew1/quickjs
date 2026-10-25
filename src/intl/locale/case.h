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
#ifndef QJS_INTL_LOCALE_CASE_H
#define QJS_INTL_LOCALE_CASE_H
#include "../common/format.h"

typedef struct QJSIntlCaseResult {
    uint16_t *text;
    size_t length;
} QJSIntlCaseResult;

/* Counted UTF-16 preserves NUL and lone surrogates. Locale-list coercion and
   first-locale selection stay in the JS binding; tag is already canonical.
   Library providers own locale-specific casing here. Native's direct Unicode
   fast path remains independent. malloc/free required; failure resets output.
   A successful result owns its text, released by the creating allocator. */
QJSIntlStatus qjs_intl_case_map(const QJSIntlAllocator *, QJSIntlBytes tag, QJSIntlUTF16,
                                int lower, size_t limit, QJSIntlCaseResult *);
void qjs_intl_case_result_clear(const QJSIntlAllocator *, QJSIntlCaseResult *);
#endif
