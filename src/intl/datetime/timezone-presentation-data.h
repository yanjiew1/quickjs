/* Supplemental presentation facts, separate from timezone transition rules.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_TIMEZONE_PRESENTATION_DATA_H
#define QJS_INTL_DATETIME_TIMEZONE_PRESENTATION_DATA_H
#include "../native/data.h"

enum {
    QJS_INTL_TIMEZONE_PERIOD_START = 1,
    QJS_INTL_TIMEZONE_PERIOD_END = 2,
    QJS_INTL_TIMEZONE_PERIOD_STANDARD = 4,
    QJS_INTL_TIMEZONE_PERIOD_DAYLIGHT = 8
};

typedef struct QJSIntlTimeZonePeriod {
    int64_t start, end; /* UTC seconds, meaningful only when the flag is set. */
    int32_t standard_offset, daylight_offset; /* Presentation overrides only. */
    uint32_t metazone; /* Zero-based presentation identity index, not a string ID. */
    uint32_t flags;
} QJSIntlTimeZonePeriod;

typedef struct QJSIntlTimeZonePresentationData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *identities, *zones, *periods, *preferred, *primary;
    uint32_t identity_count, zone_count, period_count, preferred_count, primary_count;
} QJSIntlTimeZonePresentationData;

/* Borrow an unchanged validated shared owner. Require malloc/free and release
   all validation scratch before return. Follow pack.h's disjoint-output
   contract; reset non-NULL output on failure. Admit structural/source syntax,
   not cross-section BCP47 identities or formatter capability. */
QJSIntlStatus qjs_intl_timezone_presentation_data_open(const QJSIntlData *data,
                                                       const QJSIntlAllocator *allocator,
                                                       QJSIntlTimeZonePresentationData *out);
/* Queries allocate nothing and borrow pack storage. Indexes are zero-based.
   Missing records return UNSUPPORTED. All non-NULL outputs reset on failure.
   Preferred/dominant lookups are exact; consumers own territory fallback. */
/* Metazone identity, independent of the timezone indexes used by zone/at. */
QJSIntlStatus qjs_intl_timezone_presentation_identity(const QJSIntlTimeZonePresentationData *data,
                                                      uint32_t index, QJSIntlBytes *long_id,
                                                      QJSIntlBytes *short_id);
QJSIntlStatus qjs_intl_timezone_presentation_zone(const QJSIntlTimeZonePresentationData *data,
                                                  uint32_t index, QJSIntlBytes *name,
                                                  uint32_t *start, uint32_t *count);
QJSIntlStatus qjs_intl_timezone_presentation_find_zone(const QJSIntlTimeZonePresentationData *data,
                                                       QJSIntlBytes name, uint32_t *out);
QJSIntlStatus qjs_intl_timezone_presentation_period(const QJSIntlTimeZonePresentationData *data,
                                                    uint32_t index, QJSIntlTimeZonePeriod *out);
/* Half-open membership; gaps and explicit empty zones return UNSUPPORTED. */
QJSIntlStatus qjs_intl_timezone_presentation_at(const QJSIntlTimeZonePresentationData *data,
                                                uint32_t zone, int64_t instant,
                                                QJSIntlTimeZonePeriod *out);
QJSIntlStatus qjs_intl_timezone_presentation_preferred(const QJSIntlTimeZonePresentationData *data,
                                                       uint32_t metazone, QJSIntlBytes territory,
                                                       QJSIntlBytes *out);
QJSIntlStatus qjs_intl_timezone_presentation_primary(const QJSIntlTimeZonePresentationData *data,
                                                     QJSIntlBytes territory, QJSIntlBytes *out);
#endif
