/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "numeric.h"
#include "../common/format-builder.h"
#include <inttypes.h>
#include <stdio.h>

/* Projection permits eleven components, related+cyclic year and implicit AM/PM.
   Coalesced literals alternate with fields, so an admitted ordinary program
   cannot have more tokens than this; do not allocate a per-token heap array. */
#define MAX_TOKENS (2 * (QJS_INTL_DATETIME_COMPONENT_COUNT + 2) + 1)

typedef struct Piece {
    QJSIntlFormatted number;
    size_t cut;
    QJSIntlPartType type;
} Piece;

static QJSIntlStatus value(const QJSIntlDatePatternToken *token,
                           const QJSIntlDateNumericFields *local, QJSIntlDateHourCycle cycle,
                           int64_t *out, QJSIntlPartType *part, unsigned int *digits)
{
    *digits = token->width == 2 ? 2 : 1;
    switch (token->symbol) {
    case 'y':
        *out = local->year <= 0 ? 1 - (int64_t)local->year : local->year;
        *part = QJS_INTL_PART_YEAR;
        break;
    case 'r':
        *out = local->related_year;
        *part = QJS_INTL_PART_RELATED_YEAR;
        *digits = 1;
        break;
    case 'M': case 'L':
        if (token->width > 2)
            return QJS_INTL_UNSUPPORTED;
        *out = local->month;
        *part = QJS_INTL_PART_MONTH;
        break;
    case 'd':
        *out = local->day;
        *part = QJS_INTL_PART_DAY;
        break;
    case 'h': case 'H': case 'K': case 'k':
        *out = local->hour;
        if (cycle == QJS_INTL_DATE_H11 || cycle == QJS_INTL_DATE_H12) {
            *out %= 12;
            if (!*out && cycle == QJS_INTL_DATE_H12)
                *out = 12;
        } else if (!*out && cycle == QJS_INTL_DATE_H24) {
            *out = 24;
        }
        *part = QJS_INTL_PART_HOUR;
        break;
    case 'm':
        *out = local->minute;
        *part = QJS_INTL_PART_MINUTE;
        break;
    case 's':
        *out = local->second;
        *part = QJS_INTL_PART_SECOND;
        break;
    case 'S':
        *out = local->millisecond / (token->width == 1 ? 100 : token->width == 2 ? 10 : 1);
        *digits = (unsigned int)token->width;
        *part = QJS_INTL_PART_FRACTIONAL_SECOND;
        break;
    default:
        return QJS_INTL_UNSUPPORTED;
    }
    return QJS_INTL_OK;
}

/* Find the last two code points in the entire FormatNumeric result. This also
   respects supplementary decimal digits and any locale pattern literals. */
static size_t two_digit_start(const QJSIntlFormatted *number)
{
    size_t at = number->length;
    unsigned int count;
    for (count = 0; count < 2 && at; count++) {
        uint16_t c = number->text[--at];
        if (c >= 0xdc00 && c <= 0xdfff && at &&
            number->text[at - 1] >= 0xd800 && number->text[at - 1] <= 0xdbff)
            --at;
    }
    return at;
}

static QJSIntlStatus emit(QJSIntlFormatBuilder *builder, const QJSIntlDatePattern *pattern,
                          const Piece *pieces)
{
    size_t i;
    for (i = 0; i < pattern->count; i++) {
        const QJSIntlDatePatternToken *token = &pattern->tokens[i];
        const Piece *piece = &pieces[i];
        QJSIntlStatus status;
        if (!token->symbol) {
            size_t start = builder->length;
            status = qjs_intl_format_builder_text(builder, token->literal, NULL, NULL);
            if (!status)
                status = qjs_intl_format_builder_part(builder, start, builder->length,
                                                       QJS_INTL_PART_LITERAL, 0);
        } else {
            QJSIntlPart part = {0, piece->number.length - piece->cut, piece->type};
            QJSIntlFormatted body = {piece->number.text + piece->cut, part.end, &part, 1};
            status = qjs_intl_format_builder_body(builder, &body);
        }
        if (status)
            return status;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_date_numeric_format(const QJSIntlAllocator *allocator,
                                          const QJSIntlDatePattern *pattern,
                                          const QJSIntlDateNumericFields *local,
                                          QJSIntlDateHourCycle cycle,
                                          const QJSIntlDateNumberFormats *numbers,
                                          size_t maximum_output_length, QJSIntlFormatted *out)
{
    Piece pieces[MAX_TOKENS] = {0};
    QJSIntlDatePatternInfo info;
    QJSIntlFormatBuilder builder = {0};
    QJSIntlStatus status;
    size_t i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !local || !numbers ||
        !maximum_output_length || (unsigned int)cycle > QJS_INTL_DATE_H24 ||
        !local->month || local->month > 13 || !local->day || local->day > 31 ||
        local->hour > 23 || local->minute > 59 || local->second > 59 || local->millisecond > 999)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_date_pattern_info(pattern, &info);
    if (status)
        return status;
    if (pattern->count > MAX_TOKENS)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < pattern->count; i++) {
        const QJSIntlDatePatternToken *token = &pattern->tokens[i];
        const QJSIntlNumberFormatter *formatter;
        QJSIntlMathematicalValue mathematical = {0};
        char text[32];
        int64_t field;
        int length;
        unsigned int digits;
        if (!token->symbol)
            continue;
        status = value(token, local, cycle, &field, &pieces[i].type, &digits);
        if (status)
            goto done;
        formatter = token->symbol == 'S' ? numbers->fraction[digits - 1] :
                    digits == 2 ? numbers->two_digit : numbers->numeric;
        if (!formatter) {
            status = QJS_INTL_INVALID_ARGUMENT;
            goto done;
        }
        length = snprintf(text, sizeof(text), "%" PRId64, field);
        if (length <= 0 || (size_t)length >= sizeof(text)) {
            status = QJS_INTL_OVERFLOW;
            goto done;
        }
        mathematical.decimal = (QJSIntlBytes){text, (size_t)length};
        status = qjs_intl_number_format(formatter, &mathematical, &pieces[i].number);
        if (status)
            goto done;
        if (!pieces[i].number.text || !pieces[i].number.length) {
            status = QJS_INTL_DATA_ERROR;
            goto done;
        }
        if (digits == 2 && token->symbol != 'S')
            pieces[i].cut = two_digit_start(&pieces[i].number);
    }
    builder.limit = maximum_output_length;
    status = emit(&builder, pattern, pieces);
    if (status)
        goto done;
    if (builder.length > SIZE_MAX / sizeof(uint16_t)) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    if (builder.length)
        builder.result.text = allocator->malloc(allocator->opaque, builder.length * sizeof(uint16_t));
    if (builder.count)
        builder.result.parts = allocator->malloc(allocator->opaque, builder.count * sizeof(QJSIntlPart));
    if ((builder.length && !builder.result.text) || (builder.count && !builder.result.parts)) {
        status = QJS_INTL_NO_MEMORY;
        goto done;
    }
    builder.result.length = builder.length;
    builder.result.part_count = builder.count;
    builder.length = builder.count = 0;
    status = emit(&builder, pattern, pieces);
    if (!status) {
        *out = builder.result;
        memset(&builder.result, 0, sizeof(builder.result));
    }
done:
    for (i = 0; i < MAX_TOKENS; i++)
        qjs_intl_formatted_clear(allocator, &pieces[i].number);
    qjs_intl_formatted_clear(allocator, &builder.result);
    return status;
}
