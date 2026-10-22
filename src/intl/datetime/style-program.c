/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "style-program.h"
#include <string.h>

static int date_component(size_t field)
{
    return field == QJS_INTL_DATETIME_WEEKDAY || field == QJS_INTL_DATETIME_ERA ||
           field == QJS_INTL_DATETIME_YEAR || field == QJS_INTL_DATETIME_MONTH ||
           field == QJS_INTL_DATETIME_DAY;
}

static int same_components(const QJSIntlDateTimeComponents *a, const QJSIntlDateTimeComponents *b)
{
    size_t i;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        if (a->values[i] != b->values[i])
            return 0;
    }
    return 1;
}

void qjs_intl_date_style_source_clear(QJSIntlDateStyleSource *source)
{
    if (!source)
        return;
    qjs_intl_date_pattern_clear(&source->allocator, &source->pattern);
    memset(source, 0, sizeof(*source));
}

QJSIntlStatus qjs_intl_date_style_preferred_record(const QJSIntlDateStyleData *data,
                                                   uint32_t locale, uint32_t calendar,
                                                   QJSIntlDateStyleFamily family,
                                                   QJSIntlDateStyleWidth width,
                                                   QJSIntlDateStyleField field,
                                                   uint32_t preferred, uint32_t fallback,
                                                   QJSIntlDateStyleRecord *out)
{
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!data || !out || preferred >= data->alternate_count || fallback >= data->alternate_count)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_style_record_at(data, locale, calendar, family, width, field, preferred, out);
    if (!status && (out->state != QJS_INTL_DATE_STYLE_VALUE || !out->text.length) && preferred != fallback)
        status = qjs_intl_date_style_record_at(data, locale, calendar, family, width, field, fallback, out);
    return status;
}

static QJSIntlStatus source_open(const QJSIntlDateStyleData *data,
                                 uint32_t locale, uint32_t calendar,
                                 QJSIntlDateStyleFamily family, QJSIntlDateStyleWidth width,
                                 uint32_t alternate, uint32_t fallback, const QJSIntlAllocator *allocator,
                                 QJSIntlDateStyleSource *out)
{
    QJSIntlDateStyleSource result = {0};
    QJSIntlStatus status;
    size_t i;
    int found = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    result.allocator = *allocator;
    result.family = family;
    result.width = width;
    status = qjs_intl_date_style_preferred_record(data, locale, calendar, family, width,
                                           QJS_INTL_DATE_STYLE_PATTERN, alternate, fallback, &result.source);
    if (status)
        return status;
    if (result.source.state != QJS_INTL_DATE_STYLE_VALUE || !result.source.text.length)
        return QJS_INTL_UNSUPPORTED;
    status = qjs_intl_date_style_preferred_record(data, locale, calendar, family, width,
                                           QJS_INTL_DATE_STYLE_SKELETON, alternate, fallback, &result.skeleton);
    if (status)
        return status;
    status = qjs_intl_date_pattern_compile(allocator, result.source.text, &result.pattern);
    if (status)
        return status;
    qjs_intl_date_pattern_fix_calendar_year(&result.pattern);
    status = qjs_intl_date_pattern_info(&result.pattern, &result.info);
    if (status)
        goto fail;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        if (!result.info.components.values[i])
            continue;
        found = 1;
        if (date_component(i) != (family == QJS_INTL_DATE_STYLE_DATE)) {
            status = QJS_INTL_UNSUPPORTED;
            goto fail;
        }
    }
    if (!found || (family == QJS_INTL_DATE_STYLE_DATE && result.info.period_symbol) ||
        (family == QJS_INTL_DATE_STYLE_TIME && !result.info.hour_symbol)) {
        status = QJS_INTL_UNSUPPORTED;
        goto fail;
    }
    *out = result;
    return QJS_INTL_OK;
fail:
    qjs_intl_date_style_source_clear(&result);
    return status;
}

QJSIntlStatus qjs_intl_date_style_source_open(const QJSIntlDateStyleData *data,
                                            uint32_t locale, uint32_t calendar,
                                            QJSIntlDateStyleFamily family, QJSIntlDateStyleWidth width,
                                            uint32_t alternate, const QJSIntlAllocator *allocator,
                                            QJSIntlDateStyleSource *out)
{
    return source_open(data, locale, calendar, family, width, alternate, alternate, allocator, out);
}

QJSIntlStatus qjs_intl_date_style_source_preferred(const QJSIntlDateStyleData *data,
                                                   uint32_t locale, uint32_t calendar,
                                                   QJSIntlDateStyleFamily family, QJSIntlDateStyleWidth width,
                                                   uint32_t preferred, uint32_t fallback,
                                                   const QJSIntlAllocator *allocator,
                                                   QJSIntlDateStyleSource *out)
{
    QJSIntlStatus status = source_open(data, locale, calendar, family, width, preferred, fallback, allocator, out);
    if (preferred != fallback && (status == QJS_INTL_UNSUPPORTED || status == QJS_INTL_DATA_ERROR)) {
        status = source_open(data, locale, calendar, family, width, fallback, fallback, allocator, out);
        /* Pattern fallback does not discard an independently usable preferred
           skeleton. Source fields retain their own resolved provenance. */
        if (!status) {
            status = qjs_intl_date_style_preferred_record(data, locale, calendar, family, width,
                         QJS_INTL_DATE_STYLE_SKELETON, preferred, fallback, &out->skeleton);
            if (status)
                qjs_intl_date_style_source_clear(out);
        }
    }
    return status;
}

void qjs_intl_date_style_programs_clear(QJSIntlDateStylePrograms *programs)
{
    if (!programs)
        return;
    qjs_intl_date_pattern_clear(&programs->allocator, &programs->normal);
    qjs_intl_date_pattern_clear(&programs->allocator, &programs->twelve);
    memset(programs, 0, sizeof(*programs));
}

static int hour12(unsigned int symbol)
{
    return symbol == 'h' || symbol == 'K';
}

static QJSIntlStatus check_time(const QJSIntlDateStyleSource *time,
                                const QJSIntlDateSinglePrograms *programs)
{
    unsigned int period = time->info.period_symbol == 'a' ? 0 : time->info.period_symbol;
    unsigned int twelve_period = period ? period : 'a';
    QJSIntlDatePatternInfo normal, twelve;
    QJSIntlStatus status;
    if (!programs || !same_components(&programs->components, &time->info.components))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_pattern_info(&programs->normal.pattern, &normal);
    if (!status)
        status = qjs_intl_date_pattern_info(&programs->twelve.pattern, &twelve);
    if (status)
        return status;
    if (!same_components(&normal.components, &time->info.components) ||
        !same_components(&twelve.components, &time->info.components) || !normal.hour_symbol ||
        hour12(normal.hour_symbol) || !hour12(twelve.hour_symbol) ||
        normal.period_symbol != period || twelve.period_symbol != twelve_period ||
        normal.year_kinds || twelve.year_kinds)
        return QJS_INTL_INVALID_ARGUMENT;
    return QJS_INTL_OK;
}

static QJSIntlStatus connector(const QJSIntlDateCombinationData *data,
                               uint32_t locale, uint32_t calendar, QJSIntlDateStyleWidth width,
                               QJSIntlDateCombinationRecord *out)
{
    QJSIntlStatus status;
    unsigned int i;
    for (i = 0; i < 2; i++) {
        status = qjs_intl_date_combination_record_at(data, locale, calendar, width,
                    i ? QJS_INTL_DATE_COMBINATION_STANDARD : QJS_INTL_DATE_COMBINATION_AT_TIME, 0, out);
        if (status)
            return status;
        if (out->state == QJS_INTL_DATE_COMBINATION_VALUE && out->text.length)
            return QJS_INTL_OK;
    }
    return QJS_INTL_UNSUPPORTED;
}

static QJSIntlStatus check_composed(const QJSIntlDatePattern *pattern,
                                    const QJSIntlDateTimeComponents *components, unsigned int years,
                                    unsigned int period)
{
    QJSIntlDatePatternInfo info;
    QJSIntlStatus status = qjs_intl_date_pattern_info(pattern, &info);
    if (status)
        return status;
    return same_components(&info.components, components) && info.year_kinds == years &&
           info.period_symbol == period ?
               QJS_INTL_OK : QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_date_style_programs_open(const QJSIntlDateStyleSource *date,
                                              const QJSIntlDateStyleSource *time,
                                              const QJSIntlDateSinglePrograms *time_programs,
                                              const QJSIntlDateCombinationData *connectors,
                                              uint32_t locale, uint32_t calendar,
                                              const QJSIntlAllocator *allocator,
                                              QJSIntlDateStylePrograms *out)
{
    QJSIntlDateStylePrograms result = {0};
    QJSIntlDatePattern combination = {0};
    QJSIntlStatus status;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!date && !time) || !allocator || !allocator->malloc || !allocator->free ||
        (date && (date->family != QJS_INTL_DATE_STYLE_DATE || !date->pattern.tokens)) ||
        (time && (time->family != QJS_INTL_DATE_STYLE_TIME || !time->pattern.tokens)))
        return QJS_INTL_INVALID_ARGUMENT;
    if (time) {
        status = check_time(time, time_programs);
        if (status)
            return status;
    }
    result.allocator = *allocator;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++)
        result.components.values[i] = date && date_component(i) ? date->info.components.values[i] :
                                       time && !date_component(i) ? time->info.components.values[i] : 0;
    if (date)
        result.date_source = date->source;
    if (time)
        result.time_source = time->source;
    if (date && time) {
        status = connector(connectors, locale, calendar, date->width, &result.connector);
        if (status)
            goto fail;
        status = qjs_intl_date_combination_compile(allocator, result.connector.text, &combination);
        if (status)
            goto fail;
        status = qjs_intl_date_pattern_combine(allocator, &combination, &time_programs->normal.pattern,
                                               &date->pattern, &result.normal);
        if (!status)
            status = qjs_intl_date_pattern_combine(allocator, &combination, &time_programs->twelve.pattern,
                                                   &date->pattern, &result.twelve);
    } else {
        status = qjs_intl_date_pattern_copy(allocator, date ? &date->pattern : &time_programs->normal.pattern,
                                            &result.normal);
        if (!status && time)
            status = qjs_intl_date_pattern_copy(allocator, &time_programs->twelve.pattern, &result.twelve);
    }
    if (!status)
        status = check_composed(&result.normal, &result.components, date ? date->info.year_kinds : 0,
                                 time && time->info.period_symbol != 'a' ? time->info.period_symbol : 0);
    if (!status && time)
        status = check_composed(&result.twelve, &result.components, date ? date->info.year_kinds : 0,
                                 time->info.period_symbol ? time->info.period_symbol : 'a');
    if (status)
        goto fail;
    qjs_intl_date_pattern_clear(allocator, &combination);
    *out = result;
    return QJS_INTL_OK;
fail:
    qjs_intl_date_pattern_clear(allocator, &combination);
    qjs_intl_date_style_programs_clear(&result);
    return status;
}
