/* Canonical era codes to raw CLDR name keys; no calendar arithmetic.
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
#ifndef QJS_INTL_DATETIME_ERA_DATA_H
#define QJS_INTL_DATETIME_ERA_DATA_H
#include "../native/data.h"

typedef struct QJSIntlCalendarEraData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *rows;
} QJSIntlCalendarEraData;

typedef struct QJSIntlCalendarEraEntry {
    QJSIntlBytes calendar, code, key;
} QJSIntlCalendarEraEntry;

/* Borrow a validated shared owner. Validate the optional section's complete
   schema, identities, ordering and both mapping domains. Require malloc/free
   for temporary row-key and string-domain validation, released before return. Follow
   pack.h's immutable/disjoint-output contract. Missing section is UNSUPPORTED.
   No arithmetic, localized-label or inherited-era capability is inferred. */
QJSIntlStatus qjs_intl_calendar_era_data_open(const QJSIntlData *data,
                                            const QJSIntlAllocator *allocator,
                                            QJSIntlCalendarEraData *out);
/* Allocate nothing; exact case-sensitive raw calendar/canonical code lookup.
   Inherited eras, aliases and arithmetic identities are the caller's policy.
   The unchanged pack outlives borrowed text. Reset outputs on failure. */
QJSIntlStatus qjs_intl_calendar_era_at(const QJSIntlCalendarEraData *data, uint32_t index,
                                     QJSIntlCalendarEraEntry *out);
QJSIntlStatus qjs_intl_calendar_era_find(const QJSIntlCalendarEraData *data,
                                       QJSIntlBytes calendar, QJSIntlBytes code,
                                       QJSIntlBytes *key);
#endif
