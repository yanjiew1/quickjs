/* DateTimeFormat service boundary; no JS objects, coercion or host discovery.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_SERVICE_H
#define QJS_INTL_DATETIME_SERVICE_H
#include "components.h"
#include "style-types.h"
#include "../common/format.h"
#include "../../timezone/identifier.h"

typedef struct QJSIntlDateTimeService QJSIntlDateTimeService;
typedef struct QJSIntlDateTimeFormatter QJSIntlDateTimeFormatter;

typedef enum QJSIntlDateRequired {
    QJS_INTL_DATE_REQUIRE_ANY,
    QJS_INTL_DATE_REQUIRE_DATE,
    QJS_INTL_DATE_REQUIRE_TIME,
    QJS_INTL_DATE_REQUIRE_YEAR_MONTH,
    QJS_INTL_DATE_REQUIRE_MONTH_DAY
} QJSIntlDateRequired;

typedef enum QJSIntlDateDefaults {
    QJS_INTL_DATE_DEFAULT_DATE,
    QJS_INTL_DATE_DEFAULT_TIME,
    QJS_INTL_DATE_DEFAULT_ALL,
    QJS_INTL_DATE_DEFAULT_YEAR_MONTH,
    QJS_INTL_DATE_DEFAULT_MONTH_DAY,
    QJS_INTL_DATE_DEFAULT_ZONED
} QJSIntlDateDefaults;

typedef enum QJSIntlDateValueKind {
    QJS_INTL_DATE_VALUE_DATE,
    QJS_INTL_DATE_VALUE_PLAIN_DATE,
    QJS_INTL_DATE_VALUE_PLAIN_YEAR_MONTH,
    QJS_INTL_DATE_VALUE_PLAIN_MONTH_DAY,
    QJS_INTL_DATE_VALUE_PLAIN_TIME,
    QJS_INTL_DATE_VALUE_PLAIN_DATE_TIME,
    QJS_INTL_DATE_VALUE_INSTANT,
    QJS_INTL_DATE_VALUE_KIND_COUNT
} QJSIntlDateValueKind;

typedef struct QJSIntlDateTimeOptions {
    QJSIntlDateTimeComponents components;
    unsigned int date_style, time_style; /* 0 absent, otherwise StyleWidth + 1. */
    QJSIntlDateHourCycle hour_cycle;
    QJSIntlDateRequired required;
    QJSIntlDateDefaults defaults;
    unsigned int best_fit, forced_zone;
    size_t maximum_output_length;
} QJSIntlDateTimeOptions;

typedef struct QJSIntlDateHourPreferences {
    QJSIntlDateHourCycle preferred, twelve, twenty_four;
} QJSIntlDateHourPreferences;

/* Compile-time minimal/native service. Require malloc/realloc/free callbacks;
   copy them; their opaque state outlives all handles/results. Own prepared
   source views, with no mutable
   process cache. Locale inventory is sorted and extension-free.
   Capability queries use this service's own indexes, allocate no result strings,
   and never invoke user code. Opening validates source data, not JS options. */
QJSIntlStatus qjs_intl_datetime_open(const QJSIntlAllocator *allocator,
                                     const struct QJSIntlData *checked_data,
                                     QJSIntlDateTimeService **out);
void qjs_intl_datetime_close(QJSIntlDateTimeService *service);
QJSIntlStatus qjs_intl_datetime_available(const QJSIntlDateTimeService *service,
                                          const QJSIntlBytes **out, size_t *count);
QJSIntlStatus qjs_intl_datetime_default_calendar(const QJSIntlDateTimeService *service,
                                                 size_t locale, QJSIntlBytes *out);
/* Global calendar capability, independent of a matched locale index. */
QJSIntlStatus qjs_intl_datetime_calendar_supported(const QJSIntlDateTimeService *service,
                                                   QJSIntlBytes calendar, int *out);
/* Canonical AvailableCalendars membership, excluding negotiation aliases and
   the generic islamic value that resolves to islamic-tbla only after matching. */
QJSIntlStatus qjs_intl_datetime_calendar_available(const QJSIntlDateTimeService *service,
                                                   QJSIntlBytes calendar, int *out);
QJSIntlStatus qjs_intl_datetime_default_system(const QJSIntlDateTimeService *service,
                                               size_t locale, QJSIntlBytes *out);
QJSIntlStatus qjs_intl_datetime_system_supported(const QJSIntlDateTimeService *service,
                                                 size_t locale, QJSIntlBytes system, int *out);
/* Global capability across this service's admitted locales. No allocation;
   unknown systems are OK/false, operational errors reset output. */
QJSIntlStatus qjs_intl_datetime_system_available(const QJSIntlDateTimeService *service,
                                                 QJSIntlBytes system, int *out);
QJSIntlStatus qjs_intl_datetime_hour_preferences(const QJSIntlDateTimeService *service,
                                                 size_t locale, QJSIntlDateHourPreferences *out);

/* Inputs are fully resolved, including calendar, numeric system, hour cycle
   and parsed named/fixed zone. Copy or prepare all retained state; the formatter
   may outlive the service and input strings. Keep value-kind programs separate
   as required by the era/monthCode proposal. Null per-kind records are reported
   as UNSUPPORTED when formatting, not replaced by the ordinary Date record.
   Output is NULL on failure; partial construction is released. */
QJSIntlStatus qjs_intl_datetime_formatter_open(const QJSIntlDateTimeService *service,
                                               size_t locale, QJSIntlBytes calendar,
                                               QJSIntlBytes system, const QJSTimeZoneIdentifier *zone,
                                               const QJSIntlDateTimeOptions *options,
                                               QJSIntlDateTimeFormatter **out);
void qjs_intl_datetime_formatter_close(QJSIntlDateTimeFormatter *formatter);
/* Complete optional collapsed interval programs before publishing a formatter.
   Requires exclusive access, then formatting stays immutable. Idempotent on
   success; failure preserves single-value/fallback state and permits retry.
   Internal single-value locale methods can omit this construction step. */
QJSIntlStatus qjs_intl_datetime_formatter_prepare_ranges(QJSIntlDateTimeFormatter *formatter);
/* Borrow immutable resolved facts until formatter close. Value-kind records
   may differ; resolvedOptions uses the ordinary Date component record. */
QJSIntlStatus qjs_intl_datetime_formatter_info(const QJSIntlDateTimeFormatter *formatter,
                                               QJSIntlBytes *calendar, QJSIntlBytes *system,
                                               const char **zone, QJSIntlDateTimeComponents *components);
/* Date/Temporal type/range/calendar compatibility and coercion are frontend
   policy. Supply normalized epoch seconds/remainder; for plain values these
   encode the local ISO fields as UTC, and formatter ignores its selected zone.
   Result text/parts are owned, independent of handles, and reset on errors. */
QJSIntlStatus qjs_intl_datetime_format(const QJSIntlDateTimeFormatter *formatter,
                                       QJSIntlDateValueKind kind, int64_t epoch_seconds,
                                       uint32_t nanosecond, QJSIntlFormatted *out);
/* Check the per-kind null record without rendering or allocation. This lets
   the frontend finish the first ValueFormatRecord before checking the second. */
QJSIntlStatus qjs_intl_datetime_check_value_format(const QJSIntlDateTimeFormatter *formatter,
                                                   QJSIntlDateValueKind kind);
/* Endpoints already have the same Date/Temporal kind and passed frontend
   coercion, TimeClip and calendar checks. Descending endpoints are permitted.
   Compare plain wall clocks in UTC, preserve displayed fractional precision,
   and compose fallback endpoints with the locale's real interval template.
   Result owns its text/parts; reset errors and use formatted_range_clear. */
QJSIntlStatus qjs_intl_datetime_format_range(const QJSIntlDateTimeFormatter *formatter,
                                             QJSIntlDateValueKind kind,
                                             int64_t first_seconds, uint32_t first_nanosecond,
                                             int64_t last_seconds, uint32_t last_nanosecond,
                                             QJSIntlFormattedRange *out);
#endif
