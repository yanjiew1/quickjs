/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "program-adapt.h"
#include <string.h>

static unsigned int width_flag(size_t field)
{
    return field == QJS_INTL_DATETIME_HOUR ? QJS_INTL_DATE_ADAPT_HOUR_WIDTH :
           field == QJS_INTL_DATETIME_MINUTE ? QJS_INTL_DATE_ADAPT_MINUTE_WIDTH :
           field == QJS_INTL_DATETIME_SECOND ? QJS_INTL_DATE_ADAPT_SECOND_WIDTH : 0;
}

static int numeric(int value)
{
    return value == QJS_INTL_DATETIME_NUMERIC || value == QJS_INTL_DATETIME_TWO_DIGIT;
}

QJSIntlStatus qjs_intl_date_program_adapt(const QJSIntlAllocator *allocator,
                                        const QJSIntlDatePattern *source,
                                        const QJSIntlDateTimeComponents *components,
                                        unsigned int allowed_widths, unsigned int twelve,
                                        QJSIntlDatePattern *out, unsigned int *adaptations)
{
    QJSIntlDatePatternInfo before, after;
    QJSIntlDatePattern result = {0};
    QJSIntlStatus status;
    unsigned int changed = 0;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (adaptations)
        *adaptations = 0;
    if (!out || !adaptations || !components || !allocator || !allocator->malloc || !allocator->free ||
        twelve > 1 || (allowed_widths & ~(QJS_INTL_DATE_ADAPT_HOUR_WIDTH |
                                         QJS_INTL_DATE_ADAPT_MINUTE_WIDTH | QJS_INTL_DATE_ADAPT_SECOND_WIDTH)))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_pattern_info(source, &before);
    if (status)
        return status;
    for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
        if (before.components.values[i] != components->values[i] &&
            (!(width_flag(i) & allowed_widths) || !numeric(before.components.values[i]) || !numeric(components->values[i])))
            return QJS_INTL_UNSUPPORTED;
    }
    status = qjs_intl_date_pattern_copy(allocator, source, &result);
    if (status)
        return status;
    for (i = 0; i < result.count; i++) {
        QJSIntlDatePatternToken *token = &result.tokens[i];
        size_t field, width;
        int hour = token->symbol == 'h' || token->symbol == 'H' || token->symbol == 'K' || token->symbol == 'k';
        if (!hour && token->symbol != 'm' && token->symbol != 's')
            continue;
        field = hour ? QJS_INTL_DATETIME_HOUR : token->symbol == 'm' ? QJS_INTL_DATETIME_MINUTE : QJS_INTL_DATETIME_SECOND;
        width = components->values[field] == QJS_INTL_DATETIME_TWO_DIGIT ? 2 : 1;
        if (token->width != width) {
            token->width = width;
            changed |= width_flag(field);
        }
        if (hour && (unsigned int)(token->symbol == 'h' || token->symbol == 'K') != twelve) {
            token->symbol = twelve ? 'h' : 'H';
            changed |= QJS_INTL_DATE_ADAPT_HOUR_FAMILY;
        }
    }
    status = qjs_intl_date_pattern_info(&result, &after);
    if (!status) {
        for (i = 0; i < QJS_INTL_DATETIME_COMPONENT_COUNT; i++) {
            if (after.components.values[i] != components->values[i]) {
                status = QJS_INTL_DATA_ERROR;
                break;
            }
        }
        if (after.year_kinds != before.year_kinds || after.period_symbol != before.period_symbol)
            status = QJS_INTL_DATA_ERROR;
    }
    if (status) {
        qjs_intl_date_pattern_clear(allocator, &result);
        return status;
    }
    *out = result;
    *adaptations = changed;
    return QJS_INTL_OK;
}
