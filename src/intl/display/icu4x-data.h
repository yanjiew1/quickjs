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
#ifndef QJS_INTL_ICU4X_DISPLAY_DATA_H
#define QJS_INTL_ICU4X_DISPLAY_DATA_H
#include <stddef.h>
#include <stdint.h>

/* One compiled UTF-8 pool and interned final label maps. Missing is no row;
   an explicit empty label has text offset zero. styles contains long/short/
   narrow bits (1/2/4), merging equal labels without erasing width absence.
   No runtime CLDR tree or wire decoder is needed. This private layout can
   evolve with its generator. */
typedef struct QJSICU4XDisplayLabel {
    uint32_t code, text, styles;
} QJSICU4XDisplayLabel;
typedef struct QJSICU4XDisplayMap {
    uint32_t begin, count;
} QJSICU4XDisplayMap;
typedef struct QJSICU4XDisplayLocale {
    uint32_t tag, maps[3]; /* Standalone script overrides, calendars, date fields. */
    uint32_t titlecase;    /* Bits follow QJSIntlDisplayType: language/region/script/calendar. */
} QJSICU4XDisplayLocale;

extern const char qjs_icu4x_display_strings[];
extern const QJSICU4XDisplayLabel qjs_icu4x_display_labels[];
extern const QJSICU4XDisplayMap qjs_icu4x_display_maps[];
extern const QJSICU4XDisplayLocale qjs_icu4x_display_locales[];
extern const size_t qjs_icu4x_display_locale_count;
extern const uint32_t qjs_icu4x_display_version; /* CLDR version string offset. */
#endif
