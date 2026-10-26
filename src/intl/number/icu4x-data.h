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
#ifndef QJS_INTL_NUMBER_ICU4X_DATA_H
#define QJS_INTL_NUMBER_ICU4X_DATA_H
#include <stddef.h>
#include <stdint.h>

/* Private target-compiled CLDR supplement; no wire-format/ABI version. One
   translation unit owns the arrays. All text offsets address a shared UTF-8
   NUL-delimited pool; offset zero is explicit empty. Reserved references mean
   absent, blocked or an explicitly empty pattern (not an empty text field). */
#define QJS_ICU4X_NUMBER_ABSENT UINT32_MAX
#define QJS_ICU4X_NUMBER_BLOCKED (UINT32_MAX - 1)
#define QJS_ICU4X_NUMBER_EMPTY (UINT32_MAX - 2)

typedef struct QJSICU4XNumberToken {
    uint32_t kind, text; /* Kinds match QJSIntlNumberTokenKind. */
} QJSICU4XNumberToken;
typedef struct QJSICU4XNumberAffix {
    uint32_t begin, count;
} QJSICU4XNumberAffix;
typedef struct QJSICU4XNumberPattern {
    uint32_t grouping[2]; /* Primary/secondary sizes belong to this style. */
    QJSICU4XNumberAffix forms[3]; /* Positive, negative, explicit plus. */
} QJSICU4XNumberPattern;
typedef struct QJSICU4XNumberRecord {
    /* percent, plus, minus, approximately sign, exponential, per-mille,
       infinity, NaN, range template, approximation template, decimal, group. */
    /* Remaining fields are before/afterCurrency currencyMatch,
       surroundingMatch, insertBetween (three per side). */
    /* Finally optional general currencyDecimal and currencyGroup symbols. */
    uint32_t fields[20];
    uint32_t patterns[9]; /* QJSIntlNumberPatternField order. */
    uint32_t grouping[3]; /* Fallback primary, secondary and minimum group sizes. */
    uint32_t currency_compact; /* Exact locale/system's sparse compact map. */
} QJSICU4XNumberRecord;
enum { QJS_ICU4X_COMPACT_SCALED, QJS_ICU4X_COMPACT_LITERAL, QJS_ICU4X_COMPACT_FALLBACK };
enum { QJS_ICU4X_COMPACT_STANDARD, QJS_ICU4X_COMPACT_ALPHA, QJS_ICU4X_COMPACT_NO_CURRENCY };
typedef struct QJSICU4XNumberCompactPattern {
    uint32_t mode, exponent, zeros, pattern; /* Pattern indexes shared typed affixes. */
} QJSICU4XNumberCompactPattern;
typedef struct QJSICU4XNumberCompactEntry {
    /* Sorted magnitude/count/variant. Counts are 0, 1, zero, one, two, few,
       many, other. Pattern indexes compact descriptors, or BLOCKED/EMPTY;
       an absent leaf has no entry at all. */
    uint32_t magnitude, count, variant, pattern;
} QJSICU4XNumberCompactEntry;
typedef struct QJSICU4XNumberCompactMap {
    uint32_t begin, count;
} QJSICU4XNumberCompactMap;
typedef struct QJSICU4XNumberCurrency {
    uint32_t code, decimal, group, pattern;
} QJSICU4XNumberCurrency;
typedef struct QJSICU4XNumberCurrencyMap {
    uint32_t begin, count;
} QJSICU4XNumberCurrencyMap;
typedef struct QJSICU4XNumberLocale {
    uint32_t tag, row, currencies; /* Row has system_count record indexes. */
} QJSICU4XNumberLocale;
extern const char qjs_icu4x_number_strings[];
extern const uint32_t qjs_icu4x_number_version, qjs_icu4x_number_systems[], qjs_icu4x_number_rows[];
/* The library currently omits 55 mandatory ECMA digit payloads. Keep the
   small complete CLDR numeric repertoire here, avoiding a second hand-pinned
   library capability list. Public payloads remain preferred where present. */
extern const uint32_t qjs_icu4x_number_digits[][10];
extern const QJSICU4XNumberToken qjs_icu4x_number_tokens[];
extern const QJSICU4XNumberPattern qjs_icu4x_number_patterns[];
extern const QJSICU4XNumberRecord qjs_icu4x_number_records[];
extern const QJSICU4XNumberCurrency qjs_icu4x_number_currencies[];
extern const QJSICU4XNumberCurrencyMap qjs_icu4x_number_currency_maps[];
extern const QJSICU4XNumberCompactPattern qjs_icu4x_number_compact_patterns[];
extern const QJSICU4XNumberCompactEntry qjs_icu4x_number_compact_entries[];
extern const QJSICU4XNumberCompactMap qjs_icu4x_number_compact_maps[];
extern const QJSICU4XNumberLocale qjs_icu4x_number_locales[];
extern const size_t qjs_icu4x_number_system_count, qjs_icu4x_number_locale_count;
#endif
