/* Native zone presentation over shared rule and CLDR metadata services.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_ZONE_NAME_H
#define QJS_INTL_DATETIME_ZONE_NAME_H
#include "number-formats.h"
#include "components.h"
#include "zone-offset.h"
#include "timezone-presentation-data.h"
#include "../display/data.h"
#include "../locale/timezone-identity.h"
#include "../../timezone/timezone.h"

typedef struct QJSIntlDateZoneData {
    const QJSIntlTimeZoneNameData *names;
    const QJSIntlTimeZonePresentationData *presentation;
    const QJSIntlLocaleMetadata *metadata;
    const QJSIntlNameData *regions;
    uint32_t names_locale, regions_locale;
    QJSIntlBytes territory; /* Resolved/maximized locale region, or empty for 001. */
} QJSIntlDateZoneData;

/* Format one of the six standard zone-name styles as one owned timeZoneName
   part. All data views and supplied source patterns/number formatters are
   immutable and valid for this allocator during the call. Locale indexes belong
   to their own catalogs. The rule handle is borrowed for the process lifetime.
   NULL selects a fixed minute offset (-1440,1440); named requires minutes=0.
   For fixed zero offsets, name styles try locale-backed Etc/GMT/GMT names;
   explicit offset styles remain numeric. Missing or unusable names retain
   localized numeric fallback. The original timezone identity is not changed.
   Epoch seconds are mathematical floor, with input coercion/range policy owned
   by the frontend. No host default lookup or locale matching occurs here.

   Use CLDR stable identities for names, independently of accepted IANA aliases
   and primary IDs. Respect exact source masks, explicit names, metazone periods,
   offset overrides and source type fallback. Generic qualification uses locale
   territory then 001, country/city and literal CLDR composition templates.
   Missing generic names use location then localized GMT. A standard-name
   substitution requiring seasonal proof is admitted only when the full closed
   +/-184-day window is provably effectively standard; otherwise keep location/
   GMT. This is a conservative native ILD policy, not exhaustive TR35 equivalence
   for constant nonzero daylight-saving amounts absent from TZif.

   Require malloc/free, fresh disjoint output and a nonzero UTF-16 bound. Reset
   errors. Own result independently of all inputs; clear with formatted_clear. */
QJSIntlStatus qjs_intl_date_zone_format(const QJSIntlAllocator *allocator,
                                       const QJSIntlDateZoneData *data,
                                       const QJSIntlDateOffset *offset,
                                       const QJSIntlDateNumberFormats *numbers,
                                       const QJSTimeZone *named, int32_t offset_minutes,
                                       int64_t epoch_seconds, QJSIntlDateTimeZoneStyle style,
                                       size_t maximum_output_length, QJSIntlFormatted *out);
#endif
