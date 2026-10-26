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
#ifndef QJS_INTL_ICU4X_LOCALE_DATA_H
#define QJS_INTL_ICU4X_LOCALE_DATA_H
#include <stddef.h>
#include <stdint.h>

/* Offsets address one NUL-delimited ASCII pool. These are compiled C tables,
   not a versioned wire format or a second native Intl pack. Keep the matching
   Rust repr(C) records synchronized when changing this private layout. */
typedef struct QJSICU4XLocaleKey {
    uint32_t name, aliases, preferred;
    uint8_t extension, value_type, deprecated, reserved;
} QJSICU4XLocaleKey;
typedef struct QJSICU4XLocaleType {
    uint16_t key;
    uint8_t deprecated, variable;
    uint32_t name, aliases, preferred, iana, region;
} QJSICU4XLocaleType;
typedef struct QJSICU4XLocaleAliases {
    const char *strings;
    uint32_t length, version;
    const QJSICU4XLocaleKey *keys;
    uint32_t key_count;
    const QJSICU4XLocaleType *types;
    uint32_t type_count;
} QJSICU4XLocaleAliases;
typedef struct QJSICU4XWeek {
    char region[4];
    uint8_t first_day, minimum_days, weekend, reserved;
} QJSICU4XWeek;
typedef struct QJSICU4XPreference {
    uint32_t name, value;
} QJSICU4XPreference;

extern const char qjs_icu4x_locale_strings[];
extern const QJSICU4XLocaleAliases qjs_icu4x_locale_aliases;
extern const QJSICU4XWeek qjs_icu4x_locale_weeks[];
extern const size_t qjs_icu4x_locale_week_count;
extern const QJSICU4XPreference qjs_icu4x_locale_hours[];
extern const size_t qjs_icu4x_locale_hour_count;
extern const QJSICU4XPreference qjs_icu4x_locale_calendars[];
extern const size_t qjs_icu4x_locale_calendar_count;
extern const uint32_t qjs_icu4x_locale_unknown_scripts[];
extern const size_t qjs_icu4x_locale_unknown_script_count;
extern const uint32_t qjs_icu4x_locale_registry[];
extern const size_t qjs_icu4x_locale_registry_count;
#endif
