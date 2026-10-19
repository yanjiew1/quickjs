/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * CLDR release-49-beta4, reviewed 2026-10-19: Compact Number Formats.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c:
 * PartitionNotationSubPattern compact symbols/names and literal parts.
 */
#include "compact-format.h"
#include "body.h"
#include "../../cutils/cutils.h"
#include <limits.h>
#include <string.h>

typedef struct Builder {
    QJSIntlFormatted result;
    size_t length, count, limit;
} Builder;

static int next(QJSIntlBytes text, size_t *at)
{
    const uint8_t *end;
    int c;
    if (*at == text.length || text.length - *at > INT_MAX)
        return -1;
    c = unicode_from_utf8((const uint8_t *)text.data + *at, (int)(text.length - *at), &end);
    if (c <= 0 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
        return -1;
    *at = (const char *)end - text.data;
    return c;
}

static int white_space(int c)
{
    return c == 0x20 || (c >= 0x9 && c <= 0xd) || c == 0x85 || c == 0xa0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
           c == 0x205f || c == 0x3000;
}

static QJSIntlStatus add_part(Builder *builder, size_t start, size_t end, QJSIntlPartType type)
{
    if (start == end)
        return QJS_INTL_OK;
    if (builder->count == SIZE_MAX / sizeof(QJSIntlPart))
        return QJS_INTL_OVERFLOW;
    if (builder->result.parts)
        builder->result.parts[builder->count] = (QJSIntlPart){start, end, type};
    builder->count++;
    return QJS_INTL_OK;
}

static QJSIntlStatus add_affix(Builder *builder, const QJSIntlCompactData *data,
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
        size_t at = 0;
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
        while (at < text.length) {
            int c = next(text, &at);
            size_t width;
            if (c < 0)
                return QJS_INTL_DATA_ERROR;
            width = c > 0xffff ? 2 : 1;
            if (width > builder->limit - builder->length)
                return QJS_INTL_OVERFLOW;
            if (!white_space(c)) {
                if (first == SIZE_MAX)
                    first = builder->length;
                last = builder->length + width;
            }
            if (builder->result.text) {
                uint16_t *out = builder->result.text + builder->length;
                if (c > 0xffff) {
                    c -= 0x10000;
                    out[0] = 0xd800 + (c >> 10);
                    out[1] = 0xdc00 + (c & 1023);
                } else
                    out[0] = c;
            }
            builder->length += width;
        }
    }
    if (first == SIZE_MAX)
        return add_part(builder, start, builder->length, QJS_INTL_PART_LITERAL);
    status = add_part(builder, start, first, QJS_INTL_PART_LITERAL);
    if (!status)
        status = add_part(builder, first, last, QJS_INTL_PART_COMPACT);
    if (status)
        return status;
    return add_part(builder, last, builder->length, QJS_INTL_PART_LITERAL);
}

static QJSIntlStatus add_body(Builder *builder, const QJSIntlFormatted *body)
{
    size_t i;
    if (body->length > builder->limit - builder->length ||
        body->part_count > SIZE_MAX / sizeof(QJSIntlPart) - builder->count)
        return QJS_INTL_OVERFLOW;
    if (builder->result.text)
        memcpy(builder->result.text + builder->length, body->text, body->length * sizeof(uint16_t));
    if (builder->result.parts) {
        for (i = 0; i < body->part_count; i++) {
            QJSIntlPart *part = &builder->result.parts[builder->count + i];
            *part = body->parts[i];
            part->start += builder->length;
            part->end += builder->length;
        }
    }
    builder->length += body->length;
    builder->count += body->part_count;
    return QJS_INTL_OK;
}

static QJSIntlStatus emit(Builder *builder, const QJSIntlCompactData *data,
                          const QJSIntlCompactPattern *pattern, QJSIntlBytes minus,
                          QJSIntlBytes plus, const QJSIntlFormatted *body)
{
    const QJSIntlNumberAffix *affix = &pattern->forms[QJS_INTL_NUMBER_PATTERN_POSITIVE];
    uint32_t i, start = 0;
    unsigned int numbers = 0;
    QJSIntlStatus status;
    for (i = 0; i < affix->count; i++) {
        QJSIntlNumberPatternToken token;
        status = qjs_intl_compact_token_at(data, affix, i, &token);
        if (status)
            return status;
        if (token.kind == QJS_INTL_NUMBER_TOKEN_NUMBER) {
            if (pattern->mode != QJS_INTL_COMPACT_SCALED || numbers++)
                return QJS_INTL_DATA_ERROR;
            status = add_affix(builder, data, affix, start, i, minus, plus);
            if (!status)
                status = add_body(builder, body);
            if (status)
                return status;
            start = i + 1;
        }
    }
    if (numbers != (pattern->mode == QJS_INTL_COMPACT_SCALED))
        return QJS_INTL_DATA_ERROR;
    return add_affix(builder, data, affix, start, affix->count, minus, plus);
}

QJSIntlStatus qjs_intl_number_compact_format(const QJSIntlAllocator *allocator,
                                             const QJSIntlCompactData *data,
                                             const QJSIntlCompactPattern *pattern,
                                             QJSIntlBytes minus, QJSIntlBytes plus,
                                             const QJSIntlFormatted *body, size_t limit,
                                             QJSIntlFormatted *out)
{
    Builder builder = {0};
    QJSIntlStatus status;
    size_t i, at = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !data ||
        !data->section.bytes || !pattern || !limit ||
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
                (unsigned int)part->type >= QJS_INTL_PART_COUNT)
                return QJS_INTL_INVALID_ARGUMENT;
            at = part->end;
        }
        if (at != body->length)
            return QJS_INTL_INVALID_ARGUMENT;
    }
    builder.limit = limit;
    status = emit(&builder, data, pattern, minus, plus, body);
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
    status = emit(&builder, data, pattern, minus, plus, body);
    if (status) {
        qjs_intl_number_body_clear(allocator, &builder.result);
        return status;
    }
    *out = builder.result;
    return QJS_INTL_OK;
}
