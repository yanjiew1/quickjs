/* CLDR timezone identity/territory, separate from IANA primary identifiers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
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
#endif
