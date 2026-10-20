/* ECMA-402 PartitionNumberRangePattern / FormatApproximately; no collapse.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "range.h"
#include "number.h"
#include "../common/format-builder.h"
#include "../common/range-compose.h"
#include <string.h>

QJSIntlStatus qjs_intl_number_range_separator(QJSIntlBytes pattern, QJSIntlBytes *out)
{
    QJSIntlFormatBuilder validator = {0};
    QJSIntlBytes separator;
    QJSIntlStatus status;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!pattern.data && pattern.length) || pattern.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!pattern.length)
        return QJS_INTL_UNSUPPORTED;
    if (pattern.length < 6 || memcmp(pattern.data, "{0}", 3) ||
        memcmp(pattern.data + pattern.length - 3, "{1}", 3))
        return QJS_INTL_DATA_ERROR;
    separator = (QJSIntlBytes){pattern.data + 3, pattern.length - 6};
    if (!separator.length)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < separator.length; i++) {
        if (separator.data[i] == '{' || separator.data[i] == '}')
            return QJS_INTL_DATA_ERROR;
    }
    validator.limit = SIZE_MAX;
    status = qjs_intl_format_builder_text(&validator, separator, NULL, NULL);
    if (!status)
        *out = separator;
    return status;
}

static int body_valid(const QJSIntlFormatted *value)
{
    size_t i, at = 0;
    if (!value || !value->text || !value->length || !value->parts || !value->part_count ||
        value->length > SIZE_MAX / sizeof(*value->text) ||
        value->part_count > SIZE_MAX / sizeof(*value->parts))
        return 0;
    for (i = 0; i < value->part_count; i++) {
        const QJSIntlPart *part = &value->parts[i];
        if (part->start != at || part->end < at || part->end > value->length ||
            part->type == QJS_INTL_PART_ELEMENT || (unsigned int)part->type >= QJS_INTL_PART_NUMBER_COUNT)
            return 0;
        at = part->end;
    }
    return at == value->length;
}

void qjs_intl_number_range_result_clear(const QJSIntlAllocator *allocator, QJSIntlFormattedRange *out)
{
    qjs_intl_formatted_range_clear(allocator, out);
}

QJSIntlStatus qjs_intl_number_range_compose(const QJSIntlAllocator *allocator, QJSIntlBytes separator,
                                          QJSIntlBytes approximately,
                                          const QJSIntlFormatted *start, const QJSIntlFormatted *end,
                                          size_t limit, QJSIntlFormattedRange *out)
{
    QJSIntlRangePiece pieces[3] = {0};
    size_t count;
    int equal;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free ||
        !separator.data || !separator.length || separator.length > PTRDIFF_MAX ||
        (!approximately.data && approximately.length) || approximately.length > PTRDIFF_MAX ||
        !limit || !body_valid(start) || !body_valid(end))
        return QJS_INTL_INVALID_ARGUMENT;
    if (start->length > limit || end->length > limit)
        return QJS_INTL_OVERFLOW;
    equal = start->length == end->length &&
            !memcmp(start->text, end->text, start->length * sizeof(*start->text));
    if (equal) {
        pieces[0] = (QJSIntlRangePiece){NULL, approximately, QJS_INTL_PART_APPROXIMATELY_SIGN, QJS_INTL_RANGE_SHARED};
        pieces[1] = (QJSIntlRangePiece){start, {0}, 0, QJS_INTL_RANGE_SHARED};
        count = 2;
    } else {
        pieces[0] = (QJSIntlRangePiece){start, {0}, 0, QJS_INTL_RANGE_START};
        pieces[1] = (QJSIntlRangePiece){NULL, separator, QJS_INTL_PART_LITERAL, QJS_INTL_RANGE_SHARED};
        pieces[2] = (QJSIntlRangePiece){end, {0}, 0, QJS_INTL_RANGE_END};
        count = 3;
    }
    return qjs_intl_range_compose(allocator, pieces, count, limit, out);
}
