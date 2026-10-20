/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "format.h"
#include "../common/format-builder.h"
#include <string.h>

static int zone_symbol(unsigned int symbol)
{
    return symbol == 'z' || symbol == 'v' || symbol == 'O';
}

QJSIntlStatus qjs_intl_date_format(const QJSIntlAllocator *allocator,
                                  const QJSIntlDatePattern *pattern, QJSCalendarId calendar,
                                  int64_t epoch_seconds, uint32_t nanosecond,
                                  QJSIntlDateHourCycle cycle,
                                  const QJSIntlDateNumberFormats *numbers,
                                  const QJSIntlDateFormatData *data,
                                  const QJSTimeZoneIdentifier *zone,
                                  size_t maximum_output_length, QJSIntlFormatted *out)
{
    QJSIntlDatePatternInfo info;
    QJSIntlDateLocalTime local;
    /* pattern_info admits one zone field: text prefix, zone, text suffix. */
    QJSIntlFormatted pieces[3] = {{0}};
    QJSIntlFormatBuilder builder = {0};
    QJSIntlStatus status;
    int32_t offset_minutes = 0;
    const QJSTimeZone *named;
    size_t at = 0, count = 0, total = 0, i, pass;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !data ||
        !numbers || !zone || !maximum_output_length || nanosecond >= 1000000000 ||
        (unsigned int)cycle > QJS_INTL_DATE_H24 || (unsigned int)zone->is_offset > 1)
        return QJS_INTL_INVALID_ARGUMENT;
    named = zone->named;
    if (zone->is_offset) {
        if (zone->offset_nanoseconds % INT64_C(60000000000) ||
            zone->offset_nanoseconds <= -INT64_C(86400000000000) ||
            zone->offset_nanoseconds >= INT64_C(86400000000000))
            return QJS_INTL_INVALID_ARGUMENT;
        offset_minutes = zone->offset_nanoseconds / INT64_C(60000000000);
        named = NULL;
    } else if (!named) {
        return QJS_INTL_INVALID_ARGUMENT;
    }
    status = qjs_intl_date_pattern_info(pattern, &info);
    if (!status)
        status = qjs_intl_date_local_time(calendar, epoch_seconds, nanosecond,
                                           named, offset_minutes, &local);
    if (status)
        return status;
    if (!info.components.values[QJS_INTL_DATETIME_ZONE])
        return qjs_intl_date_text_format(allocator, pattern, &local.fields, local.subsecond,
                   cycle, numbers, &data->text, maximum_output_length, out);
    if (pattern->count == 1)
        return qjs_intl_date_zone_format(allocator, &data->zones, data->offset, numbers,
                   named, offset_minutes, epoch_seconds,
                   info.components.values[QJS_INTL_DATETIME_ZONE], maximum_output_length, out);
    while (at < pattern->count) {
        QJSIntlDatePattern view = {0};
        size_t first = at;
        int is_zone = zone_symbol(pattern->tokens[at].symbol);
        if (count == sizeof(pieces) / sizeof(*pieces)) {
            status = QJS_INTL_DATA_ERROR;
            goto done;
        }
        if (total == maximum_output_length) {
            status = QJS_INTL_OVERFLOW;
            goto done;
        }
        if (is_zone) {
            at++;
            status = qjs_intl_date_zone_format(allocator, &data->zones, data->offset, numbers,
                         named, offset_minutes, epoch_seconds,
                         info.components.values[QJS_INTL_DATETIME_ZONE],
                         maximum_output_length - total, &pieces[count]);
        } else {
            while (at < pattern->count && !zone_symbol(pattern->tokens[at].symbol))
                at++;
            view.tokens = pattern->tokens + first;
            view.count = at - first;
            status = qjs_intl_date_text_format(allocator, &view, &local.fields, local.subsecond,
                         cycle, numbers, &data->text, maximum_output_length - total, &pieces[count]);
        }
        if (status)
            goto done;
        total += pieces[count].length;
        count++;
    }
    builder.limit = maximum_output_length;
    for (pass = 0; pass < 2; pass++) {
        builder.length = builder.count = 0;
        for (i = 0; i < count; i++) {
            status = qjs_intl_format_builder_body(&builder, &pieces[i]);
            if (status)
                goto done;
        }
        if (!pass) {
            if (!builder.length || !builder.count) {
                status = QJS_INTL_DATA_ERROR;
                goto done;
            }
            if (builder.length > SIZE_MAX / sizeof(uint16_t) ||
                builder.count > SIZE_MAX / sizeof(QJSIntlPart)) {
                status = QJS_INTL_OVERFLOW;
                goto done;
            }
            builder.result.length = builder.length;
            builder.result.part_count = builder.count;
            builder.result.text = allocator->malloc(allocator->opaque, builder.length * sizeof(uint16_t));
            if (builder.result.text)
                builder.result.parts = allocator->malloc(allocator->opaque, builder.count * sizeof(QJSIntlPart));
            if (!builder.result.text || !builder.result.parts) {
                status = QJS_INTL_NO_MEMORY;
                goto done;
            }
        }
    }
    *out = builder.result;
    memset(&builder.result, 0, sizeof(builder.result));
done:
    for (i = 0; i < sizeof(pieces) / sizeof(*pieces); i++)
        qjs_intl_formatted_clear(allocator, &pieces[i]);
    qjs_intl_formatted_clear(allocator, &builder.result);
    return status;
}
