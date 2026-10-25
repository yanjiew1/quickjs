/* CLDR timezone identity/territory, separate from IANA primary identifiers.
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
#ifndef QJS_INTL_LOCALE_TIMEZONE_IDENTITY_H
#define QJS_INTL_LOCALE_TIMEZONE_IDENTITY_H
#include "type-identity.h"

typedef struct QJSIntlTimeZoneIdentity {
    QJSIntlBytes identifier, short_id;
    uint32_t metadata_index;
    char territory[3]; /* Owned uppercase code or empty, never an internal pointer. */
} QJSIntlTimeZoneIdentity;

/* Match a registered u/tz type or exact alias and follow preferred types.
   The first long alias is the stable CLDR ID; the IANA preferred ID is separate.
   Copy an explicit territory override, otherwise derive the first two letters
   only from a five-character short ID. Other lengths have no associated region.
   Do not maximize locales, query rule backends or infer available service IDs.
   Borrow identifier/short_id from unchanged validated metadata; allocate nothing,
   reset disjoint output on failure. Unknown/variable types are UNSUPPORTED. */
QJSIntlStatus qjs_intl_timezone_identity(const QJSIntlLocaleMetadata *metadata,
                                        QJSIntlBytes spelling, QJSIntlTimeZoneIdentity *out);
/* Count distinct terminal registered timezone types associated with the exact
   uppercase region, without collapsing cross-country IANA links. This is a
   CLDR source fact, not the selected backend's supported-zone list. Allocate
   nothing; reset output on errors. Unknown regions return count zero. */
QJSIntlStatus qjs_intl_timezone_region_count(const QJSIntlLocaleMetadata *metadata,
                                            QJSIntlBytes territory, uint32_t *out);
/* Enumerate the same terminal source identities. Start cursor at zero; advance
   it only on success or exhaustion (UNSUPPORTED). Clear output on errors/end;
   leave cursor unchanged on other errors. No allocation or backend queries. */
QJSIntlStatus qjs_intl_timezone_region_next(const QJSIntlLocaleMetadata *metadata,
                                           QJSIntlBytes territory, uint32_t *cursor,
                                           QJSIntlTimeZoneIdentity *out);
#endif
