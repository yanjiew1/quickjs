/* Native DateTimeFormat ownership and selection over immutable source views.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "datetime.h"
#include "format.h"
#include "basic-match.h"
#include "ordinary-programs.h"
#include "date-clock-programs.h"
#include "style-date.h"
#include "style-hour.h"
#include "interval-data.h"
#include "interval-select.h"
#include "name-select.h"
#include "../common/range-compose.h"
#include "../locale/preference-data.h"
#include "../number/symbol-data.h"
#include "../../calendar/calendar.h"
#include <limits.h>
#include <string.h>

typedef struct DateData {
    QJSIntlData pack;
    QJSIntlLocaleMetadata metadata;
    QJSIntlDateAvailableData available;
    QJSIntlDateCombinationData connectors;
    QJSIntlDateStyleData styles;
    QJSIntlDateIntervalData intervals;
    QJSIntlCalendarNameData names;
    QJSIntlCalendarEraData eras;
    QJSIntlDayPeriodData periods;
    QJSIntlTimeZoneNameData zones;
    QJSIntlTimeZonePresentationData presentation;
    QJSIntlNameData regions;
    QJSIntlNumberingData numbering;
    QJSIntlNumberSymbolData symbols;
} DateData;

typedef struct DateLocale {
    uint32_t source;
    size_t number;
    char territory[4];
    QJSCalendarId calendar;
    QJSIntlDateHourPreferences hours;
} DateLocale;

struct QJSIntlDateTimeService {
    QJSIntlAllocator allocator;
    DateData data;
    QJSIntlNumberService *number;
    QJSIntlBytes *available;
    DateLocale *locales;
    size_t count;
};

typedef struct DateRecord {
    QJSIntlDatePattern pattern;
    QJSIntlDateTimeComponents components;
    const QJSIntlDateIntervalSet *intervals; /* Borrows a formatter-owned set. */
} DateRecord;

/* Views borrow the permanent compiled pack, not the service. Interior format
   and rule pointers are installed after allocation; do not relocate this owner. */
struct QJSIntlDateTimeFormatter {
    QJSIntlAllocator allocator;
    DateData data;
    QJSIntlDateTimeOptions options;
    QJSIntlDateFormatData format;
    QJSIntlDayPeriodRuleList rules;
    QJSIntlDateNumberFormats numbers;
    QJSIntlDateOffset offset;
    QJSTimeZoneIdentifier zone, utc;
    QJSCalendarId calendar;
    QJSIntlBytes system;
    QJSIntlPairPattern range_fallback;
    unsigned int intervals_prepared;
    QJSIntlDateIntervalSet interval_sets[QJS_INTL_DATE_VALUE_KIND_COUNT];
    char territory[4];
    DateRecord records[QJS_INTL_DATE_VALUE_KIND_COUNT];
};

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}

static int compare(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t length = a.length < b.length ? a.length : b.length;
    int order = length ? memcmp(a.data, b.data, length) : 0;
    return order ? order : (a.length > b.length) - (a.length < b.length);
}

static QJSIntlStatus find_locale(const QJSIntlBytes *tags, size_t count,
                                 QJSIntlBytes key, size_t *out)
{
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (compare(tags[mid], key) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == count || !equal(tags[lo], key))
        return QJS_INTL_DATA_ERROR;
    *out = lo;
    return QJS_INTL_OK;
}

static QJSIntlBytes next_word(QJSIntlBytes list, size_t *at)
{
    size_t start = *at, end;
    while (*at < list.length && list.data[*at] != ' ')
        (*at)++;
    end = *at;
    if (*at < list.length)
        (*at)++;
    return (QJSIntlBytes){list.data + start, end - start};
}

static QJSIntlStatus calendar_id(QJSIntlBytes identifier, QJSCalendarId *out)
{
    int error;
    /* Mandatory Gregorian Intl rendering uses ISO core arithmetic even when
       the optional calendar backend does not advertise Gregorian operations. */
    if (equal(identifier, bytes("gregory"))) {
        *out = QJS_CAL_GREGORY;
        return QJS_INTL_OK;
    }
    /* ResolveLocale still admits the deprecated value; the resolved Calendar
       slot uses the proposal's tabular fallback only after negotiation. */
    if (equal(identifier, bytes("islamic")))
        identifier = bytes("islamic-tbla");
    error = qjs_calendar_from_identifier(out, identifier.data, identifier.length);
    if (error || (*out != QJS_CAL_GREGORY && !qjs_calendar_is_supported(*out)))
        return QJS_INTL_UNSUPPORTED;
    return QJS_INTL_OK;
}

static QJSIntlStatus calendar_supported(const DateData *data, QJSIntlBytes identifier,
                                        QJSCalendarId *calendar)
{
    uint32_t source;
    QJSIntlStatus status = calendar_id(identifier, calendar);
    if (status)
        return status;
    identifier = bytes(qjs_calendar_identifier(*calendar));
    status = qjs_intl_date_available_calendar_index(&data->metadata, &data->available, identifier, &source);
    if (!status)
        status = qjs_intl_date_style_calendar_index(&data->metadata, &data->styles, identifier, &source);
    if (!status)
        status = qjs_intl_date_combination_calendar_index(&data->metadata, &data->connectors, identifier, &source);
    if (!status)
        status = qjs_intl_date_interval_calendar_index(&data->metadata, &data->intervals, identifier, &source);
    if (!status)
        status = qjs_intl_date_name_calendar_index(&data->metadata, &data->names, identifier, &source);
    return status;
}

static QJSIntlStatus open_data(const QJSIntlAllocator *allocator, DateData *out)
{
    QJSIntlStatus status = qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, allocator, &out->pack);
    if (!status)
        status = qjs_intl_locale_metadata_open(&out->pack, allocator, &out->metadata);
    if (!status)
        status = qjs_intl_date_available_data_open(&out->pack, allocator, &out->available);
    if (!status)
        status = qjs_intl_date_combination_data_open(&out->pack, allocator, &out->connectors);
    if (!status)
        status = qjs_intl_date_style_data_open(&out->pack, allocator, &out->styles);
    if (!status)
        status = qjs_intl_date_interval_data_open(&out->pack, allocator, &out->intervals);
    if (!status)
        status = qjs_intl_calendar_name_data_open(&out->pack, allocator, &out->names);
    if (!status)
        status = qjs_intl_calendar_era_data_open(&out->pack, allocator, &out->eras);
    if (!status)
        status = qjs_intl_dayperiod_data_open(&out->pack, allocator, &out->periods);
    if (!status)
        status = qjs_intl_timezone_name_data_open(&out->pack, allocator, &out->zones);
    if (!status)
        status = qjs_intl_timezone_presentation_data_open(&out->pack, allocator, &out->presentation);
    if (!status)
        status = qjs_intl_name_data_open(&out->pack, allocator, &out->regions);
    if (!status)
        status = qjs_intl_numbering_data_open(&out->pack, allocator, &out->numbering);
    if (!status)
        status = qjs_intl_number_symbol_data_open(&out->numbering, allocator, &out->symbols);
    return status;
}

static QJSIntlStatus hour_preferences(const QJSIntlLocaleService *identity, QJSIntlBytes locale,
                                      QJSIntlDateHourPreferences *out)
{
    static const char *const names[] = {"h11", "h12", "h23", "h24"};
    QJSIntlBytes list;
    uint32_t count, i;
    size_t at = 0;
    unsigned int seen = 0;
    QJSIntlStatus status = qjs_intl_locale_hour_cycles(identity, locale, &list, &count);
    if (status)
        return status;
    out->twelve = QJS_INTL_DATE_H12;
    out->twenty_four = QJS_INTL_DATE_H23;
    if (!count)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < count; i++) {
        QJSIntlBytes word = next_word(list, &at);
        unsigned int j, family;
        for (j = 0; j < sizeof(names) / sizeof(*names); j++)
            if (equal(word, bytes(names[j])))
                break;
        if (j == sizeof(names) / sizeof(*names))
            return QJS_INTL_DATA_ERROR;
        if (!i)
            out->preferred = j;
        family = j <= QJS_INTL_DATE_H12 ? 1 : 2;
        if (!(seen & family)) {
            if (family == 1)
                out->twelve = j;
            else
                out->twenty_four = j;
            seen |= family;
        }
    }
    return at == list.length ? QJS_INTL_OK : QJS_INTL_DATA_ERROR;
}

static QJSIntlStatus default_calendar(const DateData *data, const QJSIntlLocalePreferences *preferences,
                                      QJSIntlBytes region, QJSCalendarId *out)
{
    QJSIntlBytes list;
    uint32_t count, i;
    size_t at = 0;
    QJSIntlStatus status = qjs_intl_preference_list(preferences, 0, region, &list, &count);
    if (status == QJS_INTL_UNSUPPORTED)
        status = qjs_intl_preference_list(preferences, 0, bytes("001"), &list, &count);
    if (status)
        return status;
    for (i = 0; i < count; i++) {
        QJSIntlLocaleType type;
        QJSIntlBytes word = next_word(list, &at);
        uint32_t index;
        status = qjs_intl_locale_type_identity(&data->metadata, bytes("u"), bytes("ca"), word, &index);
        if (!status)
            status = qjs_intl_locale_type_at(&data->metadata, index, &type);
        if (!status)
            status = calendar_supported(data, type.name, out);
        if (status != QJS_INTL_UNSUPPORTED)
            return status;
    }
    return calendar_supported(data, bytes("gregory"), out);
}

void qjs_intl_datetime_close(QJSIntlDateTimeService *service)
{
    if (service) {
        QJSIntlAllocator allocator = service->allocator;
        qjs_intl_number_close(service->number);
        if (service->available)
            allocator.free(allocator.opaque, service->available);
        if (service->locales)
            allocator.free(allocator.opaque, service->locales);
        allocator.free(allocator.opaque, service);
    }
}

QJSIntlStatus qjs_intl_datetime_open(const QJSIntlAllocator *allocator, QJSIntlDateTimeService **out)
{
    QJSIntlDateTimeService *service;
    QJSIntlLocaleService *identity = NULL;
    QJSIntlLocalePreferences preferences;
    const QJSIntlBytes *numbers;
    size_t number_count, index = 0;
    uint32_t i;
    QJSIntlStatus status;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    service = allocator->malloc(allocator->opaque, sizeof(*service));
    if (!service)
        return QJS_INTL_NO_MEMORY;
    memset(service, 0, sizeof(*service));
    service->allocator = *allocator;
    status = open_data(allocator, &service->data);
    if (!status)
        status = qjs_intl_number_open(allocator, &service->number);
    if (!status)
        status = qjs_intl_locale_open(allocator, &identity);
    if (!status)
        status = qjs_intl_locale_preferences_open(&service->data.pack, allocator, &preferences);
    if (status)
        goto done;
    if (service->data.pack.locales.count < 2) {
        status = QJS_INTL_DATA_ERROR;
        goto done;
    }
    service->count = service->data.pack.locales.count - 1;
    if (service->count > SIZE_MAX / sizeof(*service->available) ||
        service->count > SIZE_MAX / sizeof(*service->locales)) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    service->available = allocator->malloc(allocator->opaque, service->count * sizeof(*service->available));
    if (service->available)
        service->locales = allocator->malloc(allocator->opaque, service->count * sizeof(*service->locales));
    if (!service->available || !service->locales) {
        status = QJS_INTL_NO_MEMORY;
        goto done;
    }
    status = qjs_intl_number_available(service->number, &numbers, &number_count);
    if (status)
        goto done;
    for (i = 0; i < service->data.pack.locales.count; i++) {
        QJSIntlBytes tag;
        QJSIntlRegionPreference region;
        uint32_t parent;
        DateLocale *locale;
        status = qjs_intl_data_locale(&service->data.pack, i, &tag, &parent);
        if (status)
            goto done;
        if (parent == UINT32_MAX)
            continue;
        if (index == service->count) {
            status = QJS_INTL_DATA_ERROR;
            goto done;
        }
        service->available[index] = tag;
        locale = &service->locales[index++];
        memset(locale, 0, sizeof(*locale));
        locale->source = i;
        status = find_locale(numbers, number_count, tag, &locale->number);
        if (!status)
            status = qjs_intl_locale_region_preference(identity, tag, &region);
        if (!status) {
            memcpy(locale->territory, region.override[0] ? region.override : region.region, sizeof(locale->territory));
            status = default_calendar(&service->data, &preferences, bytes(locale->territory), &locale->calendar);
        }
        if (!status)
            status = hour_preferences(identity, tag, &locale->hours);
        if (status)
            goto done;
    }
    if (index != service->count)
        status = QJS_INTL_DATA_ERROR;
done:
    qjs_intl_locale_close(identity);
    if (status)
        qjs_intl_datetime_close(service);
    else
        *out = service;
    return status;
}

QJSIntlStatus qjs_intl_datetime_available(const QJSIntlDateTimeService *service,
                                          const QJSIntlBytes **out, size_t *count)
{
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!service || !out || !count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->available;
    *count = service->count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_datetime_default_calendar(const QJSIntlDateTimeService *service,
                                                 size_t locale, QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out || locale >= service->count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = bytes(qjs_calendar_identifier(service->locales[locale].calendar));
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_datetime_calendar_supported(const QJSIntlDateTimeService *service,
                                                   size_t locale, QJSIntlBytes identifier, int *out)
{
    QJSCalendarId calendar;
    QJSIntlStatus status;
    if (out)
        *out = 0;
    if (!service || !out || locale >= service->count || (!identifier.data && identifier.length))
        return QJS_INTL_INVALID_ARGUMENT;
    status = calendar_supported(&service->data, identifier, &calendar);
    if (status == QJS_INTL_UNSUPPORTED)
        return QJS_INTL_OK;
    if (!status)
        *out = 1;
    return status;
}

QJSIntlStatus qjs_intl_datetime_default_system(const QJSIntlDateTimeService *service,
                                               size_t locale, QJSIntlBytes *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out || locale >= service->count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_number_default_system(service->number, service->locales[locale].number, out);
}

QJSIntlStatus qjs_intl_datetime_system_supported(const QJSIntlDateTimeService *service,
                                                 size_t locale, QJSIntlBytes system, int *out)
{
    if (out)
        *out = 0;
    if (!service || !out || locale >= service->count)
        return QJS_INTL_INVALID_ARGUMENT;
    return qjs_intl_number_system_supported(service->number, service->locales[locale].number, system, out);
}

QJSIntlStatus qjs_intl_datetime_hour_preferences(const QJSIntlDateTimeService *service,
                                                 size_t locale, QJSIntlDateHourPreferences *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!service || !out || locale >= service->count)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = service->locales[locale].hours;
    return QJS_INTL_OK;
}

#define BIT(field) (1U << QJS_INTL_DATETIME_ ## field)
#define DATE_FIELDS (BIT(WEEKDAY) | BIT(YEAR) | BIT(MONTH) | BIT(DAY))
#define TIME_FIELDS (BIT(DAY_PERIOD) | BIT(HOUR) | BIT(MINUTE) | BIT(SECOND) | BIT(FRACTION))
#define ALL_FIELDS ((1U << QJS_INTL_DATETIME_COMPONENT_COUNT) - 1)

typedef struct Programs {
    QJSIntlDateSourceCandidates pool;
    QJSIntlDateAvailableSources append;
    QJSIntlDateOrdinaryPrograms ordinary;
    QJSIntlDatePreparedStyle dates[QJS_INTL_DATE_STYLE_WIDTH_COUNT];
    QJSIntlDateClockPrograms mixed;
    QJSIntlDateNameAdmission names;
    unsigned char *admitted;
    uint32_t style_calendar, connector_calendar;
} Programs;

static void programs_clear(const QJSIntlAllocator *allocator, Programs *programs)
{
    unsigned int i;
    for (i = 0; i < QJS_INTL_DATE_STYLE_WIDTH_COUNT; i++)
        qjs_intl_date_prepared_style_clear(&programs->dates[i]);
    qjs_intl_date_clock_programs_clear(&programs->mixed);
    if (programs->admitted)
        allocator->free(allocator->opaque, programs->admitted);
    qjs_intl_date_ordinary_programs_clear(&programs->ordinary);
    qjs_intl_date_source_candidates_clear(&programs->pool);
    qjs_intl_date_available_sources_clear(&programs->append);
    memset(programs, 0, sizeof(*programs));
}

static QJSIntlStatus inventory(const QJSIntlDateAvailableData *data, uint32_t locale,
                               uint32_t calendar, QJSIntlDateAvailableFamily family,
                               const QJSIntlAllocator *allocator, QJSIntlDateAvailableSources *out)
{
    QJSIntlDateAvailableGroup key = {0};
    uint32_t group, head;
    QJSIntlStatus status;
    key.calendar = calendar;
    key.family = family;
    status = qjs_intl_date_available_find_group(data, &key, &group);
    if (!status)
        status = qjs_intl_date_available_head(data, locale, group, &head);
    if (!status)
        status = qjs_intl_date_available_sources_open(data, head, allocator, out);
    return status;
}

static QJSIntlStatus programs_open(QJSIntlDateTimeFormatter *formatter, uint32_t locale,
                                   QJSIntlBytes decimal, Programs *out)
{
    static const unsigned int required[] = {
        DATE_FIELDS | BIT(HOUR) | BIT(MINUTE) | BIT(SECOND) | BIT(FRACTION),
        DATE_FIELDS | BIT(HOUR) | BIT(MINUTE) | BIT(SECOND), DATE_FIELDS,
        BIT(YEAR) | BIT(MONTH) | BIT(DAY), BIT(YEAR) | BIT(MONTH), BIT(MONTH) | BIT(DAY), BIT(MONTH),
        BIT(HOUR) | BIT(MINUTE) | BIT(SECOND) | BIT(FRACTION), BIT(HOUR) | BIT(MINUTE) | BIT(SECOND),
        BIT(HOUR) | BIT(MINUTE), TIME_FIELDS, TIME_FIELDS & ~BIT(FRACTION),
        BIT(DAY_PERIOD) | BIT(HOUR) | BIT(MINUTE), BIT(DAY_PERIOD) | BIT(HOUR),
    };
    QJSIntlDateAvailableSources patterns = {0};
    QJSIntlDateFormatSources groups = {0};
    const QJSIntlAllocator *allocator = &formatter->allocator;
    const DateData *data = &formatter->data;
    QJSIntlBytes calendar = bytes(qjs_calendar_identifier(formatter->calendar));
    uint32_t available_calendar;
    size_t i, write = 0;
    unsigned int seen = 0;
    QJSIntlStatus status = qjs_intl_date_available_calendar_index(&data->metadata, &data->available,
                                                                calendar, &available_calendar);
    if (!status)
        status = qjs_intl_date_style_calendar_index(&data->metadata, &data->styles, calendar, &out->style_calendar);
    if (!status)
        status = qjs_intl_date_combination_calendar_index(&data->metadata, &data->connectors,
                                                          calendar, &out->connector_calendar);
    if (!status)
        status = inventory(&data->available, locale, available_calendar, QJS_INTL_DATE_AVAILABLE_PATTERN,
                            allocator, &patterns);
    if (!status)
        status = inventory(&data->available, locale, available_calendar, QJS_INTL_DATE_AVAILABLE_APPEND,
                            allocator, &out->append);
    if (!status)
        status = qjs_intl_date_source_candidates_open(&patterns, (QJSIntlBytes){0}, allocator, &out->pool);
    if (status)
        goto done;
    /* Filter owned sources before grouping: an unavailable first L/context or
       year kind must not mask a usable whole M/y program with equal components. */
    for (i = 0; i < out->pool.count; i++) {
        QJSIntlDateSourceCandidate *candidate = &out->pool.records[i];
        status = qjs_intl_date_names_admit(&candidate->pattern, formatter->calendar,
                                          &formatter->format.text, &out->names);
        if (status == QJS_INTL_UNSUPPORTED) {
            qjs_intl_date_pattern_clear(allocator, &candidate->pattern);
            memset(candidate, 0, sizeof(*candidate));
            continue;
        }
        if (status)
            goto done;
        if (write != i) {
            out->pool.records[write] = *candidate;
            memset(candidate, 0, sizeof(*candidate));
        }
        write++;
    }
    out->pool.count = write;
    status = qjs_intl_date_format_sources_open(&out->pool, allocator, &groups);
    if (!status)
        status = qjs_intl_date_ordinary_programs_open(&out->pool, &groups, &out->append, &data->connectors,
                   locale, out->connector_calendar, decimal, allocator, &out->ordinary);
    if (status)
        goto done;
    out->admitted = allocator->malloc(allocator->opaque, out->ordinary.count);
    if (!out->admitted) {
        status = QJS_INTL_NO_MEMORY;
        goto done;
    }
    memset(out->admitted, 0, out->ordinary.count);
    for (i = 0; i < out->ordinary.count; i++) {
        const QJSIntlDateSinglePrograms *single = &out->ordinary.records[i].single;
        size_t j;
        status = qjs_intl_date_names_admit(&single->normal.pattern, formatter->calendar,
                                          &formatter->format.text, &out->names);
        if (!status && single->twelve.pattern.count)
            status = qjs_intl_date_names_admit(&single->twelve.pattern, formatter->calendar,
                                              &formatter->format.text, &out->names);
        if (status == QJS_INTL_UNSUPPORTED)
            continue;
        if (status)
            goto done;
        out->admitted[i] = 1;
        for (j = 0; j < sizeof(required) / sizeof(*required); j++)
            if (qjs_intl_datetime_component_mask(&single->components) == required[j])
                seen |= 1U << j;
    }
    status = seen == (1U << (sizeof(required) / sizeof(*required))) - 1 ? QJS_INTL_OK : QJS_INTL_UNSUPPORTED;
    /* Real date styles are additional whole format records. In particular,
       plain-value style adjustment can retain a wide weekday/month date even
       when the available skeleton inventory contains only abbreviated forms. */
    for (i = 0; !status && i < QJS_INTL_DATE_STYLE_WIDTH_COUNT; i++) {
        QJSIntlDatePreparedStyle *date = &out->dates[i];
        status = qjs_intl_date_style_date_open(&data->styles, locale, out->style_calendar, i, 0,
                                               &out->pool, allocator, date);
        if (!status)
            status = qjs_intl_date_names_admit(&date->style.pattern, formatter->calendar,
                                              &formatter->format.text, &out->names);
        if (status == QJS_INTL_UNSUPPORTED) {
            qjs_intl_date_prepared_style_clear(date);
            status = QJS_INTL_OK;
        }
    }
    if (!status) {
        const QJSIntlDatePattern *dates[QJS_INTL_DATE_STYLE_WIDTH_COUNT] = {0};
        for (i = 0; i < QJS_INTL_DATE_STYLE_WIDTH_COUNT; i++)
            if (out->dates[i].style.pattern.count)
                dates[i] = &out->dates[i].style.pattern;
        /* Both inputs already passed name admission. A literal-only connector
           introduces no new name domain, and the builder verifies the union. */
        status = qjs_intl_date_clock_programs_open(dates, QJS_INTL_DATE_STYLE_WIDTH_COUNT,
                     &out->ordinary, out->admitted, &data->connectors, locale,
                     out->connector_calendar, allocator, &out->mixed);
    }
done:
    qjs_intl_date_format_sources_clear(&groups);
    qjs_intl_date_available_sources_clear(&patterns);
    return status;
}

static QJSIntlStatus copy_record(QJSIntlDateTimeFormatter *formatter,
                                 const QJSIntlDateTimeComponents *components,
                                 const QJSIntlDatePattern *normal, const QJSIntlDatePattern *twelve,
                                 DateRecord *out)
{
    const QJSIntlDatePattern *pattern = components->values[QJS_INTL_DATETIME_HOUR] &&
                                      formatter->options.hour_cycle <= QJS_INTL_DATE_H12 ? twelve : normal;
    QJSIntlStatus status = qjs_intl_date_pattern_copy(&formatter->allocator, pattern, &out->pattern);
    if (!status)
        out->components = *components;
    return status;
}

static QJSIntlStatus match_score(const QJSIntlDateTimeFormatter *formatter,
                                  const QJSIntlDateTimeComponents *request,
                                  const QJSIntlDateTimeComponents *candidate, int *out)
{
    int a = request->values[QJS_INTL_DATETIME_MONTH];
    int b = candidate->values[QJS_INTL_DATETIME_MONTH];
    QJSIntlStatus status = qjs_intl_datetime_basic_score(request, candidate, out);
    /* Native best fit distinguishes numeric/text months more strongly than
       width alone. Preserve source padding: matching skeleton lengths can
       deliberately select a different pattern width under TR35. */
    if (!status && formatter->options.best_fit && a && b &&
        ((a <= QJS_INTL_DATETIME_NUMERIC) != (b <= QJS_INTL_DATETIME_NUMERIC)))
        *out -= 20;
    return status;
}

static QJSIntlStatus match_record(QJSIntlDateTimeFormatter *formatter, const Programs *programs,
                                  const QJSIntlDateTimeComponents *request, DateRecord *out)
{
    /* Both policies retain source order on equal scores. */
    int best = INT_MIN;
    size_t i, selected = SIZE_MAX;
    for (i = 0; i < programs->ordinary.count; i++) {
        const QJSIntlDateSinglePrograms *single = &programs->ordinary.records[i].single;
        QJSIntlStatus status;
        int score;
        if (!programs->admitted[i])
            continue;
        status = match_score(formatter, request, &single->components, &score);
        if (status)
            return status;
        if (score > best) {
            best = score;
            selected = i;
        }
    }
    {
        size_t style = SIZE_MAX, mixed = SIZE_MAX;
        for (i = 0; i < QJS_INTL_DATE_STYLE_WIDTH_COUNT; i++) {
            const QJSIntlDateStyleSource *date = &programs->dates[i].style;
            QJSIntlStatus status;
            int score;
            if (!date->pattern.count)
                continue;
            status = match_score(formatter, request, &date->info.components, &score);
            if (status)
                return status;
            if (score > best) {
                best = score;
                style = i;
            }
        }
        for (i = 0; i < programs->mixed.count; i++) {
            const QJSIntlDateClockProgram *record = &programs->mixed.records[i];
            QJSIntlStatus status;
            int score;
            status = match_score(formatter, request, &record->components, &score);
            if (status)
                return status;
            if (score > best) {
                best = score;
                mixed = i;
            }
        }
        if (mixed != SIZE_MAX) {
            const QJSIntlDateClockProgram *record = &programs->mixed.records[mixed];
            return copy_record(formatter, &record->components, &record->normal, &record->twelve, out);
        }
        if (style != SIZE_MAX) {
            const QJSIntlDateStyleSource *date = &programs->dates[style].style;
            return copy_record(formatter, &date->info.components, &date->pattern, NULL, out);
        }
    }
    if (selected == SIZE_MAX)
        return QJS_INTL_UNSUPPORTED;
    {
        const QJSIntlDateSinglePrograms *single = &programs->ordinary.records[selected].single;
        return copy_record(formatter, &single->components, &single->normal.pattern, &single->twelve.pattern, out);
    }
}

static unsigned int required_mask(QJSIntlDateRequired required)
{
    switch (required) {
    case QJS_INTL_DATE_REQUIRE_DATE: return DATE_FIELDS;
    case QJS_INTL_DATE_REQUIRE_TIME: return TIME_FIELDS;
    case QJS_INTL_DATE_REQUIRE_YEAR_MONTH: return BIT(YEAR) | BIT(MONTH);
    case QJS_INTL_DATE_REQUIRE_MONTH_DAY: return BIT(MONTH) | BIT(DAY);
    default: return DATE_FIELDS | TIME_FIELDS;
    }
}

static unsigned int default_mask(QJSIntlDateDefaults defaults)
{
    switch (defaults) {
    case QJS_INTL_DATE_DEFAULT_DATE: return BIT(YEAR) | BIT(MONTH) | BIT(DAY);
    case QJS_INTL_DATE_DEFAULT_TIME: return BIT(HOUR) | BIT(MINUTE) | BIT(SECOND);
    case QJS_INTL_DATE_DEFAULT_YEAR_MONTH: return BIT(YEAR) | BIT(MONTH);
    case QJS_INTL_DATE_DEFAULT_MONTH_DAY: return BIT(MONTH) | BIT(DAY);
    default: return BIT(YEAR) | BIT(MONTH) | BIT(DAY) | BIT(HOUR) | BIT(MINUTE) | BIT(SECOND);
    }
}

/* GetDateTimeFormat: null relevant records are distinct from an unavailable
   backend. An absent pattern represents null and becomes a frontend TypeError
   only if that Temporal kind is subsequently supplied. */
static QJSIntlStatus get_record(QJSIntlDateTimeFormatter *formatter, const Programs *programs,
                                QJSIntlDateRequired required, QJSIntlDateDefaults defaults,
                                int relevant, DateRecord *out)
{
    const QJSIntlDateTimeComponents *options = &formatter->options.components;
    QJSIntlDateTimeComponents request = relevant ? (QJSIntlDateTimeComponents){{0}} : *options;
    unsigned int fields = required_mask(required), i;
    unsigned int present = qjs_intl_datetime_component_mask(options);
    if (relevant && (required == QJS_INTL_DATE_REQUIRE_DATE ||
                     required == QJS_INTL_DATE_REQUIRE_YEAR_MONTH || required == QJS_INTL_DATE_REQUIRE_ANY))
        request.values[QJS_INTL_DATETIME_ERA] = options->values[QJS_INTL_DATETIME_ERA];
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        if ((fields >> i) & 1)
            request.values[i] = options->values[i];
    if (!(present & fields)) {
        if (relevant && (present & (DATE_FIELDS | TIME_FIELDS)))
            return QJS_INTL_OK;
        fields = default_mask(defaults);
        for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
            if ((fields >> i) & 1)
                request.values[i] = QJS_INTL_DATETIME_NUMERIC;
        if (defaults == QJS_INTL_DATE_DEFAULT_ZONED && !request.values[QJS_INTL_DATETIME_ZONE])
            request.values[QJS_INTL_DATETIME_ZONE] = QJS_INTL_DATETIME_ZONE_SHORT;
    }
    return match_record(formatter, programs, &request, out);
}

static QJSIntlStatus adjust_style(QJSIntlDateTimeFormatter *formatter, const Programs *programs,
                                  const QJSIntlDateStylePrograms *style, unsigned int allowed,
                                  DateRecord *out)
{
    QJSIntlDateTimeComponents request = style->components;
    unsigned int i;
    if (!(qjs_intl_datetime_component_mask(&style->components) & ~allowed))
        return copy_record(formatter, &style->components, &style->normal, &style->twelve, out);
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        if (!((allowed >> i) & 1))
            request.values[i] = 0;
    return match_record(formatter, programs, &request, out);
}

static QJSIntlStatus select_records(QJSIntlDateTimeFormatter *formatter, uint32_t locale, Programs *programs)
{
    QJSIntlDateTimeOptions *options = &formatter->options;
    QJSIntlStatus status;
    if (options->date_style || options->time_style) {
        QJSIntlDatePreparedStyle date = {0};
        QJSIntlDateStyleSource time = {0};
        QJSIntlDateSinglePrograms hours = {0};
        QJSIntlDateStylePrograms style = {0};
        const QJSIntlAllocator *allocator = &formatter->allocator;
        const DateData *data = &formatter->data;
        if (options->date_style) {
            status = qjs_intl_date_style_date_open(&data->styles, locale, programs->style_calendar,
                       options->date_style - 1, 0, &programs->pool, allocator, &date);
            if (status)
                goto style_done;
        }
        if (options->time_style) {
            status = qjs_intl_date_style_source_open(&data->styles, locale, programs->style_calendar,
                       QJS_INTL_DATE_STYLE_TIME, options->time_style - 1, 0, allocator, &time);
            if (!status)
                status = qjs_intl_date_style_time_programs_open(&time, &programs->pool,
                           &programs->append, allocator, &hours);
            if (status)
                goto style_done;
        }
        status = qjs_intl_date_style_programs_open(options->date_style ? &date.style : NULL,
                   options->time_style ? &time : NULL, options->time_style ? &hours : NULL,
                   &data->connectors, locale, programs->connector_calendar, allocator, &style);
        if (!status)
            status = qjs_intl_date_names_admit(&style.normal, formatter->calendar,
                                              &formatter->format.text, &programs->names);
        if (!status && style.twelve.count)
            status = qjs_intl_date_names_admit(&style.twelve, formatter->calendar,
                                              &formatter->format.text, &programs->names);
        if (status)
            goto style_done;
        status = copy_record(formatter, &style.components, &style.normal, &style.twelve,
                               &formatter->records[QJS_INTL_DATE_VALUE_DATE]);
        if (!status)
            status = copy_record(formatter, &style.components, &style.normal, &style.twelve,
                                   &formatter->records[QJS_INTL_DATE_VALUE_INSTANT]);
        if (!status && options->date_style)
            status = adjust_style(formatter, programs, &style, DATE_FIELDS | BIT(ERA),
                                    &formatter->records[QJS_INTL_DATE_VALUE_PLAIN_DATE]);
        if (!status && options->date_style)
            status = adjust_style(formatter, programs, &style, BIT(ERA) | BIT(YEAR) | BIT(MONTH),
                                    &formatter->records[QJS_INTL_DATE_VALUE_PLAIN_YEAR_MONTH]);
        if (!status && options->date_style)
            status = adjust_style(formatter, programs, &style, BIT(MONTH) | BIT(DAY),
                                    &formatter->records[QJS_INTL_DATE_VALUE_PLAIN_MONTH_DAY]);
        if (!status && options->time_style)
            status = adjust_style(formatter, programs, &style, TIME_FIELDS,
                                    &formatter->records[QJS_INTL_DATE_VALUE_PLAIN_TIME]);
        if (!status)
            status = adjust_style(formatter, programs, &style, DATE_FIELDS | BIT(ERA) | TIME_FIELDS,
                                    &formatter->records[QJS_INTL_DATE_VALUE_PLAIN_DATE_TIME]);
style_done:
        qjs_intl_date_style_programs_clear(&style);
        qjs_intl_date_single_programs_clear(&hours);
        qjs_intl_date_style_source_clear(&time);
        qjs_intl_date_prepared_style_clear(&date);
        return status;
    }
    status = get_record(formatter, programs, options->required, options->defaults, 0,
                          &formatter->records[QJS_INTL_DATE_VALUE_DATE]);
    if (!status)
        status = get_record(formatter, programs, QJS_INTL_DATE_REQUIRE_DATE, QJS_INTL_DATE_DEFAULT_DATE, 1,
                              &formatter->records[QJS_INTL_DATE_VALUE_PLAIN_DATE]);
    if (!status)
        status = get_record(formatter, programs, QJS_INTL_DATE_REQUIRE_YEAR_MONTH, QJS_INTL_DATE_DEFAULT_YEAR_MONTH, 1,
                              &formatter->records[QJS_INTL_DATE_VALUE_PLAIN_YEAR_MONTH]);
    if (!status)
        status = get_record(formatter, programs, QJS_INTL_DATE_REQUIRE_MONTH_DAY, QJS_INTL_DATE_DEFAULT_MONTH_DAY, 1,
                              &formatter->records[QJS_INTL_DATE_VALUE_PLAIN_MONTH_DAY]);
    if (!status)
        status = get_record(formatter, programs, QJS_INTL_DATE_REQUIRE_TIME, QJS_INTL_DATE_DEFAULT_TIME, 1,
                              &formatter->records[QJS_INTL_DATE_VALUE_PLAIN_TIME]);
    if (!status)
        status = get_record(formatter, programs, QJS_INTL_DATE_REQUIRE_ANY, QJS_INTL_DATE_DEFAULT_ALL, 1,
                              &formatter->records[QJS_INTL_DATE_VALUE_PLAIN_DATE_TIME]);
    if (!status)
        status = get_record(formatter, programs, QJS_INTL_DATE_REQUIRE_ANY,
                              options->forced_zone ? QJS_INTL_DATE_DEFAULT_ZONED : QJS_INTL_DATE_DEFAULT_ALL, 0,
                              &formatter->records[QJS_INTL_DATE_VALUE_INSTANT]);
    return status;
}

void qjs_intl_datetime_formatter_close(QJSIntlDateTimeFormatter *formatter)
{
    if (formatter) {
        QJSIntlAllocator allocator = formatter->allocator;
        unsigned int i;
        for (i = 0; i < QJS_INTL_DATE_VALUE_KIND_COUNT; i++) {
            qjs_intl_date_interval_set_clear(&formatter->interval_sets[i]);
            qjs_intl_date_pattern_clear(&allocator, &formatter->records[i].pattern);
        }
        for (i = 0; i < 3; i++)
            qjs_intl_number_formatter_close((QJSIntlNumberFormatter *)formatter->numbers.fraction[i]);
        qjs_intl_date_offset_clear(&formatter->offset);
        allocator.free(allocator.opaque, formatter);
    }
}

/* Follow only the selected fallback source chain. An explicit block/empty
   value stops inheritance just as it does in the other sparse source owners. */
static QJSIntlStatus range_fallback_open(QJSIntlDateTimeFormatter *formatter, uint32_t locale)
{
    const QJSIntlDateIntervalData *data = &formatter->data.intervals;
    QJSIntlDateIntervalGroup group = {0};
    QJSIntlDateIntervalNode node;
    QJSIntlDateIntervalEntry entry;
    uint32_t index, head;
    QJSIntlStatus status = qjs_intl_date_interval_calendar_index(&formatter->data.metadata, data,
                                   bytes(qjs_calendar_identifier(formatter->calendar)), &group.calendar);
    group.family = QJS_INTL_DATE_INTERVAL_FALLBACK;
    if (!status)
        status = qjs_intl_date_interval_find_group(data, &group, &index);
    if (!status)
        status = qjs_intl_date_interval_head(data, locale, index, &head);
    if (status)
        return status;
    while (head) {
        status = qjs_intl_date_interval_lookup(data, head, (QJSIntlBytes){0}, (QJSIntlBytes){0}, &entry);
        if (!status) {
            if (entry.blocked || !entry.text.length)
                return QJS_INTL_UNSUPPORTED;
            return qjs_intl_pair_pattern_parse(entry.text, &formatter->range_fallback);
        }
        if (status != QJS_INTL_UNSUPPORTED)
            return status;
        status = qjs_intl_date_interval_node(data, head, &node);
        if (status)
            return status;
        head = node.parent;
    }
    return QJS_INTL_UNSUPPORTED;
}

static QJSIntlStatus validate_options(const QJSIntlDateTimeOptions *options)
{
    int score;
    QJSIntlStatus status;
    if (!options || !options->maximum_output_length ||
        options->date_style > QJS_INTL_DATE_STYLE_WIDTH_COUNT ||
        options->time_style > QJS_INTL_DATE_STYLE_WIDTH_COUNT ||
        (unsigned int)options->hour_cycle > QJS_INTL_DATE_H24 ||
        (unsigned int)options->required > QJS_INTL_DATE_REQUIRE_MONTH_DAY ||
        (unsigned int)options->defaults > QJS_INTL_DATE_DEFAULT_ZONED ||
        options->best_fit > 1 || options->forced_zone > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_datetime_basic_score(&options->components, &options->components, &score);
    if (status)
        return status;
    if ((options->date_style || options->time_style) &&
        qjs_intl_datetime_component_mask(&options->components))
        return QJS_INTL_INVALID_ARGUMENT;
    if ((options->required == QJS_INTL_DATE_REQUIRE_DATE && options->time_style) ||
        (options->required == QJS_INTL_DATE_REQUIRE_TIME && options->date_style))
        return QJS_INTL_INVALID_ARGUMENT;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_datetime_formatter_open(const QJSIntlDateTimeService *service,
                                               size_t locale, QJSIntlBytes calendar,
                                               QJSIntlBytes system, const QJSTimeZoneIdentifier *zone,
                                               const QJSIntlDateTimeOptions *options,
                                               QJSIntlDateTimeFormatter **out)
{
    QJSIntlDateTimeFormatter *formatter;
    QJSIntlNumberOptions number = {0};
    QJSIntlNumberingSystem definition;
    QJSIntlNumberSymbols symbols;
    QJSIntlStatus status;
    const DateLocale *source;
    const QJSIntlAllocator *allocator;
    uint32_t system_index;
    Programs programs = {0};
    unsigned int i;
    int supported;
    if (out)
        *out = NULL;
    if (!out || !service || locale >= service->count || !zone ||
        (!calendar.data && calendar.length) || (!system.data && system.length))
        return QJS_INTL_INVALID_ARGUMENT;
    status = validate_options(options);
    if (status)
        return status;
    allocator = &service->allocator;
    source = &service->locales[locale];
    formatter = allocator->malloc(allocator->opaque, sizeof(*formatter));
    if (!formatter)
        return QJS_INTL_NO_MEMORY;
    memset(formatter, 0, sizeof(*formatter));
    formatter->allocator = *allocator;
    formatter->data = service->data;
    formatter->options = *options;
    status = calendar_supported(&formatter->data, calendar, &formatter->calendar);
    if (!status)
        status = qjs_intl_number_system_supported(service->number, source->number, system, &supported);
    if (status)
        goto done;
    if (!supported) {
        status = QJS_INTL_UNSUPPORTED;
        goto done;
    }
    status = qjs_intl_numbering_system_find(&formatter->data.numbering, system, &system_index);
    if (!status)
        status = qjs_intl_numbering_system_at(&formatter->data.numbering, system_index, &definition);
    if (!status)
        status = qjs_intl_number_symbols_at(&formatter->data.symbols, source->source, system_index, &symbols);
    if (status)
        goto done;
    formatter->system = definition.name;
    if (!memchr(zone->identifier, 0, sizeof(zone->identifier)) ||
        qjs_timezone_parse_identifier(&formatter->zone, zone->identifier, strlen(zone->identifier)) ||
        formatter->zone.is_offset != zone->is_offset ||
        formatter->zone.offset_nanoseconds != zone->offset_nanoseconds || formatter->zone.named != zone->named) {
        status = QJS_INTL_INVALID_ARGUMENT;
        goto done;
    }
    /* The Stage 4 canonicalization text keeps the accepted identifier's
       canonical casing, including aliases. Rule and CLDR display identities
       remain independent; never rewrite this slot to a primary identifier. */
    if (qjs_timezone_parse_identifier(&formatter->utc, "UTC", 3)) {
        status = QJS_INTL_DATA_ERROR;
        goto done;
    }
    number.digits = (QJSIntlDecimalOptions){1, 0, 0, 1, 21, 1, QJS_INTL_ROUND_HALF_EXPAND,
                                             QJS_INTL_ROUND_FRACTION, QJS_INTL_TRAILING_ZERO_AUTO,
                                             options->maximum_output_length};
    number.maximum_output_length = options->maximum_output_length;
    for (i = 0; i < 3; i++) {
        QJSIntlNumberFormatter *digits;
        number.digits.minimum_integer_digits = i + 1;
        status = qjs_intl_number_formatter_open(service->number, source->number, system, &number, &digits);
        if (status)
            goto done;
        formatter->numbers.fraction[i] = digits;
    }
    /* fraction[] owns all three handles; these two convenience slots alias it. */
    formatter->numbers.numeric = formatter->numbers.fraction[0];
    formatter->numbers.two_digit = formatter->numbers.fraction[1];
    /* These typed owners share one validated pack locale catalog. Calendar
       catalogs and NumberFormat's available-locale indexes remain independent. */
    /* With no caller context distinguishing 00:00 from 24:00, use the general
       period at midnight, following TR35's optional midnight-support policy.
       Keep the source rules/names intact; noon still uses its exact point. */
    formatter->format.text = (QJSIntlDateTextData){
        &formatter->data.names, &formatter->data.metadata, &formatter->data.eras,
        &formatter->data.periods, NULL, source->source, 0, 0,
    };
    status = qjs_intl_date_name_calendar_index(&formatter->data.metadata, &formatter->data.names,
                                               bytes(qjs_calendar_identifier(formatter->calendar)),
                                               &formatter->format.text.calendar);
    if (status)
        goto done;
    status = qjs_intl_dayperiod_resolve(&formatter->data.periods, QJS_INTL_DAYPERIOD_FORMAT,
                                        service->available[locale], &formatter->rules);
    if (status == QJS_INTL_UNSUPPORTED)
        status = QJS_INTL_OK;
    else if (!status)
        formatter->format.text.period_rules = &formatter->rules;
    if (status)
        goto done;
    memcpy(formatter->territory, source->territory, sizeof(formatter->territory));
    formatter->format.zones = (QJSIntlDateZoneData){
        &formatter->data.zones, &formatter->data.presentation, &formatter->data.metadata,
        &formatter->data.regions, source->source, source->source, bytes(formatter->territory),
    };
    status = qjs_intl_date_offset_from_data(&formatter->data.zones, source->source, allocator, &formatter->offset);
    if (status)
        goto done;
    formatter->format.offset = &formatter->offset;
    if (symbols.fields[QJS_INTL_SYMBOL_DECIMAL].state != QJS_INTL_SYMBOL_VALUE) {
        status = QJS_INTL_DATA_ERROR;
        goto done;
    }
    status = programs_open(formatter, source->source, symbols.fields[QJS_INTL_SYMBOL_DECIMAL].text, &programs);
    if (!status)
        status = select_records(formatter, source->source, &programs);
    if (!status)
        status = range_fallback_open(formatter, source->source);
done:
    programs_clear(allocator, &programs);
    if (status)
        qjs_intl_datetime_formatter_close(formatter);
    else
        *out = formatter;
    return status;
}

static int same_pattern(const QJSIntlDatePattern *a, const QJSIntlDatePattern *b)
{
    size_t i;
    if (a->count != b->count)
        return 0;
    for (i = 0; i < a->count; i++) {
        const QJSIntlDatePatternToken *x = &a->tokens[i], *y = &b->tokens[i];
        if (x->symbol != y->symbol || x->width != y->width || !equal(x->literal, y->literal))
            return 0;
    }
    return 1;
}

QJSIntlStatus qjs_intl_datetime_formatter_prepare_ranges(QJSIntlDateTimeFormatter *formatter)
{
    QJSIntlDateIntervalSet sets[QJS_INTL_DATE_VALUE_KIND_COUNT] = {0};
    uint8_t owners[QJS_INTL_DATE_VALUE_KIND_COUNT] = {0};
    QJSIntlDateNameAdmission admission = {0};
    QJSIntlStatus status;
    uint32_t calendar;
    size_t i, j;
    if (!formatter)
        return QJS_INTL_INVALID_ARGUMENT;
    if (formatter->intervals_prepared)
        return QJS_INTL_OK;
    status = qjs_intl_date_interval_calendar_index(&formatter->data.metadata, &formatter->data.intervals,
                 bytes(qjs_calendar_identifier(formatter->calendar)), &calendar);
    if (status)
        return status;
    for (i = 0; i < QJS_INTL_DATE_VALUE_KIND_COUNT; i++) {
        if (!formatter->records[i].pattern.count)
            continue;
        /* Several value kinds select the same ordinary program. Their zone
           interpretation differs at rendering, not during interval matching. */
        for (j = 0; j < i; j++)
            if (same_pattern(&formatter->records[i].pattern, &formatter->records[j].pattern))
                break;
        owners[i] = j < i ? owners[j] : i;
        if (j < i)
            continue;
        status = qjs_intl_date_interval_set_open(&formatter->data.intervals, formatter->format.text.locale,
                     calendar, &formatter->records[i].pattern, formatter->options.hour_cycle,
                     formatter->calendar, &formatter->format.text, &admission, &formatter->allocator, &sets[i]);
        if (status)
            goto done;
    }
    /* Publish all kind-specific sets together. A failed preparation leaves a
       usable single-value formatter and no partially installed interval cache. */
    for (i = 0; i < QJS_INTL_DATE_VALUE_KIND_COUNT; i++) {
        formatter->interval_sets[i] = sets[i];
        memset(&sets[i], 0, sizeof(sets[i]));
        if (formatter->records[i].pattern.count)
            formatter->records[i].intervals = &formatter->interval_sets[owners[i]];
    }
    formatter->intervals_prepared = 1;
done:
    for (i = 0; i < QJS_INTL_DATE_VALUE_KIND_COUNT; i++)
        qjs_intl_date_interval_set_clear(&sets[i]);
    return status;
}

QJSIntlStatus qjs_intl_datetime_formatter_info(const QJSIntlDateTimeFormatter *formatter,
                                               QJSIntlBytes *calendar, QJSIntlBytes *system,
                                               const char **zone, QJSIntlDateTimeComponents *components)
{
    if (calendar)
        memset(calendar, 0, sizeof(*calendar));
    if (system)
        memset(system, 0, sizeof(*system));
    if (zone)
        *zone = NULL;
    if (components)
        memset(components, 0, sizeof(*components));
    if (!formatter || !calendar || !system || !zone || !components)
        return QJS_INTL_INVALID_ARGUMENT;
    *calendar = bytes(qjs_calendar_identifier(formatter->calendar));
    *system = formatter->system;
    *zone = formatter->zone.identifier;
    *components = formatter->records[QJS_INTL_DATE_VALUE_DATE].components;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_datetime_format(const QJSIntlDateTimeFormatter *formatter,
                                       QJSIntlDateValueKind kind, int64_t epoch_seconds,
                                       uint32_t nanosecond, QJSIntlFormatted *out)
{
    const QJSTimeZoneIdentifier *zone;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!formatter || !out || (unsigned int)kind >= QJS_INTL_DATE_VALUE_KIND_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!formatter->records[kind].pattern.count)
        return QJS_INTL_UNSUPPORTED;
    zone = kind == QJS_INTL_DATE_VALUE_DATE || kind == QJS_INTL_DATE_VALUE_INSTANT ?
           &formatter->zone : &formatter->utc;
    return qjs_intl_date_format(&formatter->allocator, &formatter->records[kind].pattern,
                                 formatter->calendar, epoch_seconds, nanosecond, formatter->options.hour_cycle,
                                 &formatter->numbers, &formatter->format, zone,
                                 formatter->options.maximum_output_length, out);
}

static QJSIntlStatus range_period(const QJSIntlDateTextData *data, const QJSIntlDatePatternInfo *info,
                                  const QJSIntlDateLocalTime *local, QJSIntlBytes *out)
{
    const QJSIntlDateNumericFields *clock = &local->fields.numeric;
    QJSIntlDayPeriodCode primary, fallback = clock->hour < 12 ? QJS_INTL_DAYPERIOD_AM : QJS_INTL_DAYPERIOD_PM;
    QJSIntlDatePeriodNameSelection name;
    QJSIntlStatus status;
    *out = (QJSIntlBytes){0};
    if (!info->period_symbol || info->period_symbol == 'a')
        return QJS_INTL_OK;
    status = qjs_intl_dayperiod_select(data->periods, data->period_rules, info->period_symbol,
                 (clock->hour * 60 + clock->minute) * 60 + clock->second, local->subsecond, data->use_midnight, &primary);
    if (!status)
        status = qjs_intl_date_period_name_select(data->names, data->locale, data->calendar,
                     info->period_symbol, info->period_width, primary, fallback, &name);
    if (status)
        return status;
    if (name.selected.state != QJS_INTL_DATE_NAME_TEXT)
        return QJS_INTL_UNSUPPORTED;
    *out = name.selected.entry.text;
    return QJS_INTL_OK;
}

static QJSIntlStatus range_difference(const QJSIntlDateTimeFormatter *formatter, const DateRecord *record,
                                        const QJSTimeZoneIdentifier *zone,
                                        int64_t first_seconds, uint32_t first_nanosecond,
                                        int64_t last_seconds, uint32_t last_nanosecond, int *out)
{
    QJSIntlDatePatternInfo info;
    QJSIntlDateLocalTime local[2];
    QJSIntlBytes periods[2];
    QJSIntlStatus status = qjs_intl_date_pattern_info(&record->pattern, &info);
    const QJSTimeZone *named = zone->is_offset ? NULL : zone->named;
    int32_t minutes = zone->is_offset ? zone->offset_nanoseconds / INT64_C(60000000000) : 0;
    int i, finest = qjs_intl_date_range_last_field(&record->components), same = 1;
    unsigned int precision, divisor;
    *out = -1;
    if (!status)
        status = qjs_intl_date_local_time(formatter->calendar, first_seconds, first_nanosecond,
                                           named, minutes, &local[0]);
    if (!status)
        status = qjs_intl_date_local_time(formatter->calendar, last_seconds, last_nanosecond,
                                           named, minutes, &local[1]);
    if (status)
        return status;
    precision = info.components.values[QJS_INTL_DATETIME_FRACTION];
    divisor = precision == 1 ? 100 : precision == 2 ? 10 : 1;
    /* Define fallback records densely through the finest displayed field.
       Missing collapsed CLDR intervals must not make a visible finer field
       disappear from comparison; below displayed precision is irrelevant. */
    for (i = QJS_INTL_DATE_RANGE_ERA; same && i <= finest; i++) {
        const QJSIntlDateCalendarFields *a = &local[0].fields, *b = &local[1].fields;
        switch (i) {
        case QJS_INTL_DATE_RANGE_ERA: same = !strcmp(a->era_code, b->era_code); break;
        case QJS_INTL_DATE_RANGE_YEAR: same = a->arithmetic_year == b->arithmetic_year; break;
        case QJS_INTL_DATE_RANGE_MONTH: same = !strcmp(a->month_code, b->month_code); break;
        case QJS_INTL_DATE_RANGE_DAY: same = a->numeric.day == b->numeric.day; break;
        case QJS_INTL_DATE_RANGE_AM_PM: same = (a->numeric.hour < 12) == (b->numeric.hour < 12); break;
        case QJS_INTL_DATE_RANGE_DAY_PERIOD:
            status = range_period(&formatter->format.text, &info, &local[0], &periods[0]);
            if (!status)
                status = range_period(&formatter->format.text, &info, &local[1], &periods[1]);
            if (status)
                return status;
            same = equal(periods[0], periods[1]);
            break;
        case QJS_INTL_DATE_RANGE_HOUR: same = a->numeric.hour == b->numeric.hour; break;
        case QJS_INTL_DATE_RANGE_MINUTE: same = a->numeric.minute == b->numeric.minute; break;
        case QJS_INTL_DATE_RANGE_SECOND: same = a->numeric.second == b->numeric.second; break;
        case QJS_INTL_DATE_RANGE_FRACTION:
            same = a->numeric.millisecond / divisor == b->numeric.millisecond / divisor;
            break;
        }
        if (!same) {
            *out = i;
            break;
        }
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_datetime_check_value_format(const QJSIntlDateTimeFormatter *formatter,
                                                   QJSIntlDateValueKind kind)
{
    if (!formatter || (unsigned int)kind >= QJS_INTL_DATE_VALUE_KIND_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    return formatter->records[kind].pattern.count ? QJS_INTL_OK : QJS_INTL_UNSUPPORTED;
}

static QJSIntlStatus render_interval(const QJSIntlDateTimeFormatter *formatter,
                                     const QJSIntlDateIntervalProgram *program,
                                     const QJSTimeZoneIdentifier *zone,
                                     int64_t first_seconds, uint32_t first_nanosecond,
                                     int64_t last_seconds, uint32_t last_nanosecond,
                                     QJSIntlFormattedRange *out)
{
    /* Each admitted half has at most the ordinary field/token bound. Group
       consecutive sources so literals and their adjacent fields stay together. */
    enum { MAX_PIECES = 2 * (2 * (QJS_INTL_DATETIME_COMPONENT_COUNT + 2) + 1) };
    QJSIntlFormatted values[MAX_PIECES] = {0};
    QJSIntlRangePiece pieces[MAX_PIECES] = {0};
    QJSIntlStatus status;
    size_t at = 0, count = 0, total = 0;
    if (program->pattern.count > MAX_PIECES)
        return QJS_INTL_DATA_ERROR;
    while (at < program->pattern.count) {
        QJSIntlDatePattern view = {0};
        size_t first = at;
        QJSIntlRangeSource source = program->sources[at];
        int end = source == QJS_INTL_RANGE_END;
        if (total == formatter->options.maximum_output_length) {
            status = QJS_INTL_OVERFLOW;
            goto done;
        }
        do {
            at++;
        } while (at < program->pattern.count && program->sources[at] == source);
        view.tokens = program->pattern.tokens + first;
        view.count = at - first;
        status = qjs_intl_date_format(&formatter->allocator, &view, formatter->calendar,
                     end ? last_seconds : first_seconds, end ? last_nanosecond : first_nanosecond,
                     formatter->options.hour_cycle, &formatter->numbers, &formatter->format, zone,
                     formatter->options.maximum_output_length - total, &values[count]);
        if (status)
            goto done;
        total += values[count].length;
        pieces[count].body = &values[count];
        pieces[count].source = source;
        count++;
    }
    status = qjs_intl_range_compose(&formatter->allocator, pieces, count,
                                   formatter->options.maximum_output_length, out);
done:
    for (at = 0; at < MAX_PIECES; at++)
        qjs_intl_formatted_clear(&formatter->allocator, &values[at]);
    return status;
}

QJSIntlStatus qjs_intl_datetime_format_range(const QJSIntlDateTimeFormatter *formatter,
                                             QJSIntlDateValueKind kind,
                                             int64_t first_seconds, uint32_t first_nanosecond,
                                             int64_t last_seconds, uint32_t last_nanosecond,
                                             QJSIntlFormattedRange *out)
{
    QJSIntlFormatted values[2] = {{0}};
    QJSIntlRangePiece pieces[5] = {{0}};
    const QJSTimeZoneIdentifier *zone;
    const QJSIntlPairPattern *layout;
    QJSIntlStatus status;
    int difference;
    unsigned int first;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_datetime_check_value_format(formatter, kind);
    if (status)
        return status;
    /* Plain inputs encode local ISO fields in UTC. Use UTC for comparison
       too: the PR's formatter-zone comparison can falsely collapse distinct
       plain clocks during a DST overlap, while rendering ignores that zone. */
    zone = kind == QJS_INTL_DATE_VALUE_DATE || kind == QJS_INTL_DATE_VALUE_INSTANT ?
           &formatter->zone : &formatter->utc;
    status = range_difference(formatter, &formatter->records[kind], zone,
                              first_seconds, first_nanosecond, last_seconds, last_nanosecond, &difference);
    if (!status && difference >= 0) {
        const QJSIntlDateIntervalProgram *program =
            qjs_intl_date_interval_set_get(formatter->records[kind].intervals, difference);
        if (program)
            return render_interval(formatter, program, zone, first_seconds, first_nanosecond,
                                    last_seconds, last_nanosecond, out);
    }
    if (!status)
        status = qjs_intl_datetime_format(formatter, kind, first_seconds, first_nanosecond, &values[0]);
    if (!status && difference >= 0)
        status = qjs_intl_datetime_format(formatter, kind, last_seconds, last_nanosecond, &values[1]);
    if (status)
        goto done;
    if (difference < 0) {
        pieces[0] = (QJSIntlRangePiece){&values[0], {0}, 0, QJS_INTL_RANGE_SHARED};
    } else {
        layout = &formatter->range_fallback;
        first = layout->first;
        pieces[0] = (QJSIntlRangePiece){NULL, layout->prefix, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED};
        pieces[1] = (QJSIntlRangePiece){&values[first], {0}, 0, first ? QJS_INTL_RANGE_END : QJS_INTL_RANGE_START};
        pieces[2] = (QJSIntlRangePiece){NULL, layout->separator, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED};
        pieces[3] = (QJSIntlRangePiece){&values[1 - first], {0}, 0, first ? QJS_INTL_RANGE_START : QJS_INTL_RANGE_END};
        pieces[4] = (QJSIntlRangePiece){NULL, layout->suffix, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED};
    }
    status = qjs_intl_range_compose(&formatter->allocator, pieces, difference < 0 ? 1 : 5,
                                   formatter->options.maximum_output_length, out);
done:
    qjs_intl_formatted_clear(&formatter->allocator, &values[0]);
    qjs_intl_formatted_clear(&formatter->allocator, &values[1]);
    return status;
}
