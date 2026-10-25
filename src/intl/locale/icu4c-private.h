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
#ifndef QJS_INTL_LOCALE_ICU4C_PRIVATE_H
#define QJS_INTL_LOCALE_ICU4C_PRIVATE_H
#include "locale.h"
#include "preferences.h"
#include "../icu4c/common.h"
#include <string.h>

typedef struct ICUPreference {
    char *key, *values;
    uint32_t count;
} ICUPreference;

typedef struct ICUWeek {
    char region[4];
    QJSIntlWeekInfo info;
} ICUWeek;

typedef struct ICUType {
    char key[2];
    char *source;
    QJSIntlBytes name; /* ICU's immutable registered-name storage. */
} ICUType;

typedef struct ICUZone {
    char *name;
    char region[3];
} ICUZone;

struct QJSIntlLocaleService {
    QJSIntlAllocator allocator;
    QJSICU4CLocales defaults;
    ICUPreference *hours, *calendars;
    size_t hour_count, calendar_count;
    ICUWeek *weeks;
    size_t week_count;
    ICUType *types;
    size_t type_count, type_capacity;
    ICUZone *zones;
    size_t zone_count;
};

/* Preference records and registered types/zones have independent source
   owners. Their clear operations accept partial construction and empty state. */
QJSIntlStatus qjs_icu4c_locale_preferences_open(QJSIntlLocaleService *);
void qjs_icu4c_locale_preferences_clear(QJSIntlLocaleService *);
QJSIntlStatus qjs_icu4c_locale_registry_open(QJSIntlLocaleService *);
void qjs_icu4c_locale_registry_clear(QJSIntlLocaleService *);

static inline int qjs_icu4c_locale_bytes_order(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t length = a.length < b.length ? a.length : b.length;
    int order = length ? memcmp(a.data, b.data, length) : 0;

    return order ? order : (a.length > b.length) - (a.length < b.length);
}

#endif
