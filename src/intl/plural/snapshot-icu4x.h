/* ICU4X-owned exact plural adapter shared by ICU provider composition.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifndef QJS_INTL_PLURAL_SNAPSHOT_ICU4X_H
#define QJS_INTL_PLURAL_SNAPSHOT_ICU4X_H
#include "../common/types.h"
#include "../../icu4x/plural.h"

typedef struct QJSICU4XPluralFamily {
    const char *rules[6];
    uint32_t mask;
    uint8_t ranges[36];
} QJSICU4XPluralFamily;
typedef struct QJSICU4XPluralSource {
    const char *locale;
    const char *rule_locales[2], *range_locale;
    uint16_t families[2];
} QJSICU4XPluralSource;
extern const char qjs_icu4x_plural_source_version[];
extern const QJSICU4XPluralFamily qjs_icu4x_plural_families[];
extern const QJSICU4XPluralSource qjs_icu4x_plural_sources[];
extern const size_t qjs_icu4x_plural_source_count;
/* Locale is a canonical available-locale or resolved source identity.
   Prefer complete public rules. Matching-CLDR source families fill category
   sets lost through the stock bake's root fallback. Both string owners remain
   immutable; no allocation, parsing or unrelated locale negotiation here. */
QJSIntlStatus qjs_intl_icu4x_plural_open(QJSIntlBytes locale, unsigned int ordinal,
                                         QJSICU4XPlural **, QJSICU4XPluralData *);
#endif
