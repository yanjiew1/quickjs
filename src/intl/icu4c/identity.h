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
#ifndef QJS_INTL_ICU4C_IDENTITY_H
#define QJS_INTL_ICU4C_IDENTITY_H
#include "../common/types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum QJSICU4CLocaleOperation {
    QJS_ICU4C_CANONICALIZE,
    QJS_ICU4C_MAXIMIZE,
    QJS_ICU4C_MINIMIZE
} QJSICU4CLocaleOperation;

/* ICU's complete CLDR alias replacement is exposed by its public C++ Locale
   API. Keep that dependency in this private adapter; the engine and common
   locale interface remain C. Results belong to the supplied allocator. */
QJSIntlStatus qjs_icu4c_locale_identity(const QJSIntlAllocator *, QJSIntlBytes,
                                        QJSICU4CLocaleOperation, char **out);
/* Short, validated component operation used by the framing adapter. */
QJSIntlStatus qjs_icu4c_locale_component(const QJSIntlAllocator *, QJSIntlBytes,
                                         QJSICU4CLocaleOperation, char **out);
QJSIntlStatus qjs_icu4c_zone_region(QJSIntlBytes source_zone, char region[4], size_t *length);
#ifdef __cplusplus
}
#endif
#endif
