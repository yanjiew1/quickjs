/* Bounded whole-source date/clock combinations, without punctuation synthesis.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "date-clock-programs.h"
#include <string.h>

#define BIT(field) (1U << QJS_INTL_DATETIME_ ## field)

static QJSIntlDateStyleWidth connector_width(const QJSIntlDateTimeComponents *components)
{
    int month = components->values[QJS_INTL_DATETIME_MONTH];
    if (month == QJS_INTL_DATETIME_LONG)
        return components->values[QJS_INTL_DATETIME_WEEKDAY] ? QJS_INTL_DATE_STYLE_FULL :
                                                            QJS_INTL_DATE_STYLE_LONG;
    return month == QJS_INTL_DATETIME_SHORT ? QJS_INTL_DATE_STYLE_MEDIUM : QJS_INTL_DATE_STYLE_SHORT;
}

void qjs_intl_date_clock_programs_clear(QJSIntlDateClockPrograms *programs)
{
    size_t i;
    if (!programs)
        return;
    for (i = 0; i < programs->count; i++) {
        qjs_intl_date_pattern_clear(&programs->allocator, &programs->records[i].normal);
        qjs_intl_date_pattern_clear(&programs->allocator, &programs->records[i].twelve);
    }
    if (programs->records)
        programs->allocator.free(programs->allocator.opaque, programs->records);
    memset(programs, 0, sizeof(*programs));
}

QJSIntlStatus qjs_intl_date_clock_programs_open(const QJSIntlDatePattern *const *dates,
                                               size_t date_count,
                                               const QJSIntlDateOrdinaryPrograms *ordinary,
                                               const unsigned char *admitted,
                                               const QJSIntlDateCombinationData *connectors,
                                               uint32_t locale, uint32_t calendar,
                                               const QJSIntlAllocator *allocator,
                                               QJSIntlDateClockPrograms *out)
{
    enum { SHAPES = 5, ZONE_STYLES = 7, CLOCKS = SHAPES * ZONE_STYLES };
    const unsigned int clock = BIT(HOUR) | BIT(MINUTE) | BIT(SECOND);
    const unsigned int date_fields = BIT(WEEKDAY) | BIT(ERA) | BIT(YEAR) | BIT(MONTH) | BIT(DAY);
    size_t indexes[CLOCKS], count = 0, i, j, capacity;
    QJSIntlDateClockPrograms result = {0};
    QJSIntlDatePattern connector = {0};
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!dates && date_count) || !ordinary || !connectors || !allocator ||
        !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < CLOCKS; i++)
        indexes[i] = SIZE_MAX;
    for (i = 0; i < ordinary->count; i++) {
        const QJSIntlDateTimeComponents *components = &ordinary->records[i].single.components;
        unsigned int fields = qjs_intl_datetime_component_mask(components);
        int fraction = components->values[QJS_INTL_DATETIME_FRACTION];
        int zone = components->values[QJS_INTL_DATETIME_ZONE];
        size_t at;
        if (admitted && !admitted[i])
            continue;
        if (zone < 0 || zone >= ZONE_STYLES)
            return QJS_INTL_INVALID_ARGUMENT;
        fields &= ~BIT(ZONE);
        if (fields == (clock & ~BIT(SECOND)))
            at = 0;
        else if (fields == clock)
            at = 1;
        else if (fields == (clock | BIT(FRACTION)) && fraction >= 1 && fraction <= 3)
            at = fraction + 1;
        else
            continue;
        /* Preserve a whole admitted zone-bearing clock for each actual style.
           Empty slots allocate nothing; do not manufacture the Cartesian
           product of every shape and zone style. */
        at += (size_t)zone * SHAPES;
        if (indexes[at] == SIZE_MAX) {
            indexes[at] = i;
            count++;
        }
    }
    if (!count || !date_count)
        return QJS_INTL_OK;
    capacity = 0;
    for (i = 0; i < date_count; i++)
        capacity += dates[i] != NULL;
    if (!capacity)
        return QJS_INTL_OK;
    if (capacity > SIZE_MAX / sizeof(*result.records) / count)
        return QJS_INTL_OVERFLOW;
    capacity *= count;
    result.allocator = *allocator;
    result.records = allocator->malloc(allocator->opaque, capacity * sizeof(*result.records));
    if (!result.records)
        return QJS_INTL_NO_MEMORY;
    memset(result.records, 0, capacity * sizeof(*result.records));
    for (i = 0; i < date_count; i++) {
        QJSIntlDatePatternInfo date;
        QJSIntlDateCombinationRecord layout;
        QJSIntlDateStyleWidth width;
        if (!dates[i])
            continue;
        status = qjs_intl_date_pattern_info(dates[i], &date);
        if (status)
            goto fail;
        if (!(qjs_intl_datetime_component_mask(&date.components) & (BIT(YEAR) | BIT(MONTH) | BIT(DAY))) ||
            (qjs_intl_datetime_component_mask(&date.components) & ~date_fields)) {
            status = QJS_INTL_INVALID_ARGUMENT;
            goto fail;
        }
        width = connector_width(&date.components);
        status = qjs_intl_date_combination_record_at(connectors, locale, calendar, width,
                                                     QJS_INTL_DATE_COMBINATION_STANDARD, 0, &layout);
        if (status)
            goto fail;
        if (layout.state != QJS_INTL_DATE_COMBINATION_VALUE || !layout.text.length)
            continue;
        status = qjs_intl_date_combination_compile(allocator, layout.text, &connector);
        if (status)
            goto fail;
        for (j = 0; j < CLOCKS; j++) {
            const QJSIntlDateSinglePrograms *time;
            QJSIntlDateClockProgram *record;
            QJSIntlDatePatternInfo info[2];
            size_t field;
            if (indexes[j] == SIZE_MAX)
                continue;
            time = &ordinary->records[indexes[j]].single;
            /* Include a partly built row in cleanup before either allocation. */
            record = &result.records[result.count++];
            record->date_index = i;
            record->clock_index = indexes[j];
            record->connector = layout;
            record->connector_width = width;
            record->components = date.components;
            for (field = 0; field < QJS_INTL_DATETIME_COMPONENT_COUNT; field++)
                if (time->components.values[field])
                    record->components.values[field] = time->components.values[field];
            status = qjs_intl_date_pattern_combine(allocator, &connector, &time->normal.pattern, dates[i],
                                                   &record->normal);
            if (!status)
                status = qjs_intl_date_pattern_combine(allocator, &connector, &time->twelve.pattern, dates[i],
                                                       &record->twelve);
            if (!status)
                status = qjs_intl_date_pattern_info(&record->normal, &info[0]);
            if (!status)
                status = qjs_intl_date_pattern_info(&record->twelve, &info[1]);
            if (status)
                goto fail;
            for (field = 0; field < QJS_INTL_DATETIME_COMPONENT_COUNT; field++) {
                if (info[0].components.values[field] != record->components.values[field] ||
                    info[1].components.values[field] != record->components.values[field]) {
                    status = QJS_INTL_DATA_ERROR;
                    goto fail;
                }
            }
            if (info[0].year_kinds != date.year_kinds || info[1].year_kinds != date.year_kinds) {
                status = QJS_INTL_DATA_ERROR;
                goto fail;
            }
        }
        qjs_intl_date_pattern_clear(allocator, &connector);
    }
    if (!result.count)
        qjs_intl_date_clock_programs_clear(&result);
    *out = result;
    return QJS_INTL_OK;
fail:
    qjs_intl_date_pattern_clear(allocator, &connector);
    qjs_intl_date_clock_programs_clear(&result);
    return status;
}
