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
#ifndef QJS_INTL_ICU4C_COMMON_H
#define QJS_INTL_ICU4C_COMMON_H
#include "../common/types.h"
#include <unicode/utypes.h>

typedef struct QJSICU4CLocales {
    QJSIntlAllocator allocator;
    QJSIntlBytes *tags;
    size_t count;
} QJSICU4CLocales;

typedef const char *QJSICU4CLocaleAt(int32_t index);
QJSIntlStatus qjs_icu4c_status(UErrorCode status);
/* All returned strings use the supplied allocator; no ICU buffer is retained.
   Input slices preserve their explicit lengths, including rejection of NUL. */
QJSIntlStatus qjs_icu4c_copy_bytes(const QJSIntlAllocator *, QJSIntlBytes, char **);
QJSIntlStatus qjs_icu4c_language_tag(const QJSIntlAllocator *, const char *id, char **);
QJSIntlStatus qjs_icu4c_locale_id(const QJSIntlAllocator *, QJSIntlBytes tag, char **);
QJSIntlStatus qjs_icu4c_locale_keyword(const QJSIntlAllocator *, const char *base, const char *key,
                                       QJSIntlBytes value, char **);
/* Enumerate each module's actual source inventory, normalize its tags, strip
   extensions, sort and deduplicate. The result owns its strings until clear.
   malloc/free required; no realloc and no native-pack admission is involved. */
QJSIntlStatus qjs_icu4c_locales_open(const QJSIntlAllocator *, int32_t count, QJSICU4CLocaleAt *,
                                     QJSICU4CLocales *);
void qjs_icu4c_locales_clear(QJSICU4CLocales *);
#endif
