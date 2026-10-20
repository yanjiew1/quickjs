/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * CLDR release-49-beta4, reviewed 2026-10-19: Compact Number Formats.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c:
 * PartitionNotationSubPattern compact symbols/names and literal parts.
 */
#include "compact-format.h"
#include "body.h"
#include "../common/format-builder.h"
#include <string.h>

static QJSIntlStatus add_currency(QJSIntlFormatBuilder *builder, QJSIntlBytes text)
{
    size_t start = builder->length;
    QJSIntlStatus status = qjs_intl_format_builder_text(builder, text, NULL, NULL);
    if (status)
        return status;
    /* An explicit empty symbol still has a currency part. */
    if (builder->count == SIZE_MAX / sizeof(QJSIntlPart))
        return QJS_INTL_OVERFLOW;
    if (builder->result.parts)
        builder->result.parts[builder->count] =
            (QJSIntlPart){start, builder->length, QJS_INTL_PART_CURRENCY};
    builder->count++;
    return QJS_INTL_OK;
}

static QJSIntlStatus add_affix(QJSIntlFormatBuilder *builder, const QJSIntlCompactData *data,
                               const QJSIntlNumberAffix *affix, uint32_t first_token,
                               uint32_t end_token, QJSIntlBytes minus, QJSIntlBytes plus)
{
    size_t start = builder->length, first = SIZE_MAX, last = 0;
    uint32_t i;
    QJSIntlStatus status;
    /* An affix can span several source tokens, e.g. yrl's "miliãu-ita".
       Resolve unquoted signs using LDML symbols, but keep the whole compact
       name in one part, including its internal spaces and sign characters. */
    for (i = first_token; i < end_token; i++) {
        QJSIntlNumberPatternToken token;
        QJSIntlBytes text;
        status = qjs_intl_compact_token_at(data, affix, i, &token);
        if (status)
            return status;
        if (token.kind == QJS_INTL_NUMBER_TOKEN_LITERAL)
            text = token.text;
        else if (token.kind == QJS_INTL_NUMBER_TOKEN_MINUS)
            text = minus;
        else if (token.kind == QJS_INTL_NUMBER_TOKEN_PLUS)
            text = plus;
        else
            return QJS_INTL_DATA_ERROR;
        if (!text.data || !text.length)
            return QJS_INTL_INVALID_ARGUMENT;
        status = qjs_intl_format_builder_text(builder, text, &first, &last);
        if (status)
            return status;
    }
    if (first == SIZE_MAX)
        return qjs_intl_format_builder_part(builder, start, builder->length, QJS_INTL_PART_LITERAL, 0);
    status = qjs_intl_format_builder_part(builder, start, first, QJS_INTL_PART_LITERAL, 0);
    if (!status)
        status = qjs_intl_format_builder_part(builder, first, last, QJS_INTL_PART_COMPACT, 0);
    if (status)
        return status;
    return qjs_intl_format_builder_part(builder, last, builder->length, QJS_INTL_PART_LITERAL, 0);
}

static QJSIntlStatus emit(QJSIntlFormatBuilder *builder, const QJSIntlCompactData *data,
                          const QJSIntlCompactPattern *pattern,
                          const QJSIntlNumberAffixSymbols *symbols, const QJSIntlFormatted *body)
{
    const QJSIntlNumberAffix *affix = &pattern->forms[QJS_INTL_NUMBER_PATTERN_POSITIVE];
    uint32_t i, start = 0;
    unsigned int numbers = 0;
    QJSIntlNumberTokenKind previous = QJS_INTL_NUMBER_TOKEN_COUNT;
    QJSIntlStatus status;
    for (i = 0; i < affix->count; i++) {
        QJSIntlNumberPatternToken token;
        status = qjs_intl_compact_token_at(data, affix, i, &token);
        if (status)
            return status;
        if (token.kind == QJS_INTL_NUMBER_TOKEN_NUMBER ||
            (token.kind >= QJS_INTL_NUMBER_TOKEN_CURRENCY &&
             token.kind <= QJS_INTL_NUMBER_TOKEN_CURRENCY_NARROW)) {
            QJSIntlBytes gap = {0};
            size_t gap_start;
            status = add_affix(builder, data, affix, start, i, symbols->minus, symbols->plus);
            if (status)
                return status;
            if (symbols->currency.length) {
                if (token.kind == QJS_INTL_NUMBER_TOKEN_NUMBER &&
                    previous >= QJS_INTL_NUMBER_TOKEN_CURRENCY &&
                    previous <= QJS_INTL_NUMBER_TOKEN_CURRENCY_NARROW)
                    gap = symbols->after_currency;
                else if (token.kind != QJS_INTL_NUMBER_TOKEN_NUMBER &&
                         previous == QJS_INTL_NUMBER_TOKEN_NUMBER)
                    gap = symbols->before_currency;
            }
            gap_start = builder->length;
            status = qjs_intl_format_builder_text(builder, gap, NULL, NULL);
            if (!status)
                status = qjs_intl_format_builder_part(builder, gap_start, builder->length, QJS_INTL_PART_LITERAL, 0);
            if (status)
                return status;
            if (token.kind == QJS_INTL_NUMBER_TOKEN_NUMBER) {
                if (pattern->mode != QJS_INTL_COMPACT_SCALED || numbers++)
                    return QJS_INTL_DATA_ERROR;
                status = qjs_intl_format_builder_body(builder, body);
            } else
                status = add_currency(builder, symbols->currency);
            if (status)
                return status;
            start = i + 1;
        }
        previous = token.kind;
    }
    if (numbers != (pattern->mode == QJS_INTL_COMPACT_SCALED))
        return QJS_INTL_DATA_ERROR;
    return add_affix(builder, data, affix, start, affix->count, symbols->minus, symbols->plus);
}

QJSIntlStatus qjs_intl_number_compact_format(const QJSIntlAllocator *allocator,
                                             const QJSIntlCompactData *data,
                                             const QJSIntlCompactPattern *pattern,
                                             const QJSIntlNumberAffixSymbols *symbols,
                                             const QJSIntlFormatted *body, size_t limit,
                                             QJSIntlFormatted *out)
{
    QJSIntlFormatBuilder builder = {0};
    QJSIntlStatus status;
    size_t i, at = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !data ||
        !data->section.bytes || !pattern || !symbols || !limit ||
        pattern->state != QJS_INTL_NUMBER_PATTERN_VALUE ||
        (pattern->mode != QJS_INTL_COMPACT_SCALED && pattern->mode != QJS_INTL_COMPACT_LITERAL))
        return QJS_INTL_INVALID_ARGUMENT;
    if (pattern->mode == QJS_INTL_COMPACT_SCALED) {
        if (!body || !body->text || !body->length || !body->parts || !body->part_count ||
            body->part_count > SIZE_MAX / sizeof(QJSIntlPart))
            return QJS_INTL_INVALID_ARGUMENT;
        for (i = 0; i < body->part_count; i++) {
            const QJSIntlPart *part = &body->parts[i];
            if (part->start != at || part->end < at || part->end > body->length ||
                part->type == QJS_INTL_PART_ELEMENT ||
                (unsigned int)part->type >= QJS_INTL_PART_NUMBER_COUNT)
                return QJS_INTL_INVALID_ARGUMENT;
            at = part->end;
        }
        if (at != body->length)
            return QJS_INTL_INVALID_ARGUMENT;
    }
    builder.limit = limit;
    status = emit(&builder, data, pattern, symbols, body);
    if (status)
        return status;
    if (!builder.length || !builder.count)
        return QJS_INTL_DATA_ERROR;
    if (builder.length > SIZE_MAX / sizeof(uint16_t))
        return QJS_INTL_OVERFLOW;
    builder.result.text = allocator->malloc(allocator->opaque, builder.length * sizeof(uint16_t));
    if (!builder.result.text)
        return QJS_INTL_NO_MEMORY;
    builder.result.parts =
        allocator->malloc(allocator->opaque, builder.count * sizeof(QJSIntlPart));
    if (!builder.result.parts) {
        qjs_intl_number_body_clear(allocator, &builder.result);
        return QJS_INTL_NO_MEMORY;
    }
    builder.result.length = builder.length;
    builder.result.part_count = builder.count;
    builder.length = builder.count = 0;
    status = emit(&builder, data, pattern, symbols, body);
    if (status) {
        qjs_intl_number_body_clear(allocator, &builder.result);
        return status;
    }
    *out = builder.result;
    return QJS_INTL_OK;
}
