/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATE_NATIVE_H
#define QJS_INTL_DATE_NATIVE_H
#include "date-pattern.h"
#include "temporal/iso.h"

typedef struct QJSIntlNativeDate QJSIntlNativeDate;
enum { QJS_DATE_H11, QJS_DATE_H12, QJS_DATE_H23, QJS_DATE_H24 };
enum { QJS_DATE_BASIC, QJS_DATE_BEST_FIT };
enum { QJS_DATE_FULL, QJS_DATE_STYLE_LONG, QJS_DATE_MEDIUM, QJS_DATE_STYLE_SHORT };
enum { QJS_DATE_AVAILABLE, QJS_DATE_DATE_STYLE, QJS_DATE_TIME_STYLE, QJS_DATE_JOIN };
enum { QJS_DATE_NAME_ERA, QJS_DATE_NAME_MONTH, QJS_DATE_NAME_WEEKDAY,
       QJS_DATE_NAME_PERIOD, QJS_DATE_NAME_CYCLIC_YEAR, QJS_DATE_NAME_LEAP_TEMPLATE };
/* Name width0 narrow,1 abbreviated,2 wide,3 weekday short (EEEEEE).
 * Context0 format,1 stand-alone. Era context is always0.
 */
enum {
    QJS_DATE_AM, QJS_DATE_PM, QJS_DATE_MIDNIGHT, QJS_DATE_NOON,
    QJS_DATE_MORNING1, QJS_DATE_MORNING2, QJS_DATE_AFTERNOON1,
    QJS_DATE_AFTERNOON2, QJS_DATE_EVENING1, QJS_DATE_EVENING2,
    QJS_DATE_NIGHT1, QJS_DATE_NIGHT2, QJS_DATE_PERIOD_COUNT
};
typedef struct QJSIntlDateOptions {
    QJSIntlBytes calendar;       /* resolved gregory/iso8601, or explicit capability */
    QJSIntlBytes time_zone;      /* frontend validated, canonical public identifier */
    int fields[QJS_DATE_FIELD_COUNT];
    int date_style, time_style;  /* -1 absent, otherwise0..3; exclude fields */
    unsigned int hour_cycle;    /* resolved0..3; hour12 precedence stays frontend */
    unsigned int format_matcher;/* basic0; best fit1 uses documented skeleton subset */
} QJSIntlDateOptions;
typedef struct QJSIntlDatePattern {
    unsigned int kind, style, family;
    QJSIntlBytes skeleton, pattern;
} QJSIntlDatePattern;
typedef struct QJSIntlDateName {
    unsigned int field, context, width, index;
    QJSIntlBytes name;
} QJSIntlDateName;
typedef struct QJSIntlDatePeriodRule {
    unsigned int period, exact, from_second, before_second;
} QJSIntlDatePeriodRule;
typedef struct QJSIntlDateZoneName {
    unsigned int metazone;
    QJSIntlBytes key;
    QJSIntlBytes names[6];       /* short std,dst,generic; long std,dst,generic */
    QJSIntlBytes exemplar;
} QJSIntlDateZoneName;
typedef struct QJSIntlDateMetaPeriod {
    QJSIntlBytes zone, metazone;
    int64_t from_ms, before_ms;  /* half-open UTC interval, INT64 extrema unbounded */
    /* Explicit CLDR stdOffset/dstOffset are absolute offsets for NAME
       classification, independently of negative tzdb SAVE or TZif isdst. */
    int32_t standard_name_offset, daylight_name_offset;
    int has_name_offsets;
} QJSIntlDateMetaPeriod;
/* Localized CLDR generic-location and metazone qualifier snapshot.
 * Empty metazone is the zone's location-only fallback row. A nonempty
 * name_pattern contains exactly one {0}, replaced with the metazone label.
 * An empty pattern means this is the locale's preferred zone.
 */
typedef struct QJSIntlDateZoneFormat {
    QJSIntlBytes zone, metazone, location, name_pattern;
} QJSIntlDateZoneFormat;
typedef struct QJSIntlDateData {
    const QJSIntlDatePattern *patterns;
    size_t pattern_count;
    const QJSIntlDateName *names;
    size_t name_count;
    const QJSIntlDatePeriodRule *periods;
    size_t period_count;
    const QJSIntlDateZoneName *zone_names;
    size_t zone_name_count;
    const QJSIntlDateMetaPeriod *meta_periods;
    size_t meta_period_count;
    uint32_t digits[10];         /* shared numeric section17; never map literals */
    QJSIntlBytes decimal, gmt_format, gmt_zero, hour_positive, hour_negative;
    QJSIntlBytes data_zone;      /* CLDR zone key; explicit alias resolution */
    QJSIntlBytes range_fallback; /* decoded exact CLDR intervalFormatFallback */
    const QJSIntlDateZoneFormat *zone_formats;
    size_t zone_format_count;   /* additive optional section108; copied by open */
} QJSIntlDateData;
typedef struct QJSIntlDateFields {
    int64_t year;               /* positive year in the supplied era */
    unsigned int era, month, day, weekday, hour, minute, second, millisecond;
    /* Calendar adapters can keep numeric ordinal month distinct from the
     * name index selected by monthCode. Zero means use ordinal month.
     */
    unsigned int month_name_index;
    int64_t calendar_year;      /* untransformed local Year for range identity */
    int has_calendar_year;     /* 0 retains the legacy positive-year callback */
    int64_t related_year;      /* Gregorian year containing lunar M01, for r */
    int has_related_year;
    unsigned int year_name_index; /* sexagenary year1..60, for U */
    unsigned int leap_month;   /* Chinese/Dangi monthCode suffix L only */
} QJSIntlDateFields;
typedef struct QJSIntlDateZoneInfo {
    int32_t offset_seconds;     /* exact seconds, never rounded to minutes */
    int daylight;              /* -1 unknown;0 standard;1 daylight */
} QJSIntlDateZoneInfo;
typedef struct QJSIntlDateEnvironment {
    void *opaque;              /* borrowed until close; no process defaults */
    QJSIntlStatus (*zone)(void *, QJSIntlBytes canonical_zone,
                          int64_t floor_epoch_seconds, QJSIntlDateZoneInfo *);
    int (*calendar_supported)(void *, QJSIntlBytes calendar);
    QJSIntlStatus (*calendar)(void *, QJSIntlBytes calendar,
                              const QJSTemporalISODateTime *, QJSIntlDateFields *);
    /* Optional lazy proof for CLDR Type Fallback2. Closed UTC window, only
       called when generic is absent, daylight exists and standard exists.
       Unknown provider policy returns OK with proven=0. */
    QJSIntlStatus (*zone_name_stable)(void *, QJSIntlBytes canonical_zone,
        int64_t from_seconds, int64_t through_seconds, int *proven);
} QJSIntlDateEnvironment;
/* A calendar callback supplies raw calendar_year/has_calendar_year for range
 * identity. Core assigns weekday from the shared ISO epoch-day conversion.
 * The legacy callback can still format a single value, but ranges return
 * UNSUPPORTED when its raw Year is unavailable. Lunar adapters provide
 * related_year, cyclic name index and leap_month from the shared monthCode.
 * Hebrew month_name_index14 identifies CLDR month7@yeartype=leap.
 */
/* open copies all strings, arrays, digits and options before returning.
 * Environment callbacks/opaque are borrowed; source data and view are not.
 * No JS getters/coercions, locale resolution, or runtime activation.
 * Per-type defaults are prepared by the separate typed bank.
 */
QJSIntlStatus qjs_intl_native_date_open(const QJSIntlAllocator *,
    const QJSIntlDateData *, const QJSIntlDateOptions *,
    const QJSIntlDateEnvironment *, QJSIntlNativeDate **);
/* preferred is the locale data preference after applying hour12's
 * suppression of the hc extension, not an ignored hc option. hour12 is -1
 * absent,0 false,1 true; invalid input returns -1. No locale-specific oracle.
 */
int qjs_intl_native_date_hour_cycle(unsigned int preferred, int hour12);
void qjs_intl_native_date_close(QJSIntlNativeDate *);
QJSIntlStatus qjs_intl_native_date_format(QJSIntlNativeDate *, int64_t epoch_ms,
                                         QJSIntlFormatted *);
/* Exact typed provider route. No TimeClip or intermediate Number conversion.
 * is_plain renders with fixed +00:00; the handle's public timezone/calendar
 * remain unchanged. Nonplain input must be a valid Instant epoch. Plain
 * input admits the extended civil/reference-date interval; its brand and
 * per-type civil limits are owned by date-native-bank and the frontend.
 */
QJSIntlStatus qjs_intl_native_date_format_ns(QJSIntlNativeDate *,
    QJSTemporalEpochNs, int is_plain, QJSIntlFormatted *);
QJSIntlStatus qjs_intl_native_date_range_ns(QJSIntlNativeDate *,
    QJSTemporalEpochNs, QJSTemporalEpochNs, int is_plain, QJSIntlFormatted *);
/* Borrowed immutable public calendar identifier, valid until handle close. */
QJSIntlBytes qjs_intl_native_date_calendar(const QJSIntlNativeDate *);
void qjs_intl_native_date_clear(const QJSIntlAllocator *, QJSIntlFormatted *);
void qjs_intl_native_date_resolved_fields(const QJSIntlNativeDate *, int *);
/* Complete-pattern range policy: Default and every optional Table 6 range
 * record use the full selected endpoint pattern. The decoded locale fallback
 * joins those patterns in its exact placeholder order. Equality compares
 * complete local fields (fraction truncated at selected precision), never
 * rendered string equality. Reversed epochs retain argument identity; the
 * pinned specification has no start<=end check. Sources are shared for a
 * collapse and template literals, startRange/endRange for endpoint parts.
 * A missing fallback returns UNSUPPORTED only for noncollapsed ranges.
 */
QJSIntlStatus qjs_intl_native_date_range(QJSIntlNativeDate *, int64_t, int64_t,
                                        QJSIntlFormatted *);
#endif
