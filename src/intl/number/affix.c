/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19:
 * PartitionNumberPattern and the number/sign/currency/percent part records.
 */
#include "affix.h"
#include "body.h"
#include <string.h>

QJSIntlStatus qjs_intl_number_sign_pattern(QJSIntlNumberSign mode,
                                           const QJSIntlRoundedNumber *number, unsigned int *out)
{
    int negative = 0, is_zero = 0;
    if (out)
        *out = QJS_INTL_NUMBER_PATTERN_POSITIVE;
    if (!out || !number || (unsigned int)mode > QJS_INTL_NUMBER_SIGN_NEGATIVE ||
        (unsigned int)number->kind > QJS_INTL_NEGATIVE_ZERO)
        return QJS_INTL_INVALID_ARGUMENT;
    switch (number->kind) {
    case QJS_INTL_FINITE:
        if (!number->number.rounded.digits || !number->number.rounded.length ||
            number->number.rounded.negative > 1)
            return QJS_INTL_INVALID_ARGUMENT;
        negative = number->number.rounded.negative;
        is_zero = number->number.rounded.length == 1 && number->number.rounded.digits[0] == '0';
        break;
    case QJS_INTL_NAN:
        is_zero = 1;
        break;
    case QJS_INTL_NEGATIVE_ZERO:
        is_zero = negative = 1;
        break;
    case QJS_INTL_NEGATIVE_INFINITY:
        negative = 1;
        break;
    case QJS_INTL_POSITIVE_INFINITY:
        break;
    }
    if (mode == QJS_INTL_NUMBER_SIGN_NEVER ||
        (is_zero &&
         (mode == QJS_INTL_NUMBER_SIGN_EXCEPT_ZERO || mode == QJS_INTL_NUMBER_SIGN_NEGATIVE)))
        return QJS_INTL_OK;
    if (negative)
        *out = QJS_INTL_NUMBER_PATTERN_NEGATIVE;
    else if (mode == QJS_INTL_NUMBER_SIGN_ALWAYS || mode == QJS_INTL_NUMBER_SIGN_EXCEPT_ZERO)
        *out = QJS_INTL_NUMBER_PATTERN_PLUS;
    return QJS_INTL_OK;
}

typedef struct Builder {
    QJSIntlFormatted value;
    size_t length, count, limit;
} Builder;

static int scalar(QJSIntlBytes text, size_t *at, uint32_t *out)
{
    const uint8_t *bytes = (const uint8_t *)text.data;
    size_t i = *at, extra;
    uint32_t c, minimum;
    if (i == text.length)
        return 0;
    c = bytes[i++];
    if (c < 0x80) {
        extra = 0;
        minimum = 0;
    } else if (c >= 0xc2 && c <= 0xdf) {
        c &= 31;
        extra = 1;
        minimum = 0x80;
    } else if (c >= 0xe0 && c <= 0xef) {
        c &= 15;
        extra = 2;
        minimum = 0x800;
    } else if (c >= 0xf0 && c <= 0xf4) {
        c &= 7;
        extra = 3;
        minimum = 0x10000;
    } else
        return -1;
    if (extra > text.length - i)
        return -1;
    while (extra--) {
        if ((bytes[i] & 0xc0) != 0x80)
            return -1;
        c = c << 6 | (bytes[i++] & 63);
    }
    if (!c || c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
        return -1;
    *at = i;
    *out = c;
    return 1;
}

static QJSIntlStatus text_part(Builder *builder, QJSIntlBytes text, QJSIntlPartType type)
{
    size_t at = 0, start = builder->length;
    uint32_t c;
    int result;
    while ((result = scalar(text, &at, &c)) > 0) {
        size_t width = c > 0xffff ? 2 : 1;
        if (width > builder->limit - builder->length)
            return QJS_INTL_OVERFLOW;
        if (builder->value.text) {
            uint16_t *out = builder->value.text + builder->length;
            if (width == 2) {
                c -= 0x10000;
                out[0] = 0xd800 + (c >> 10);
                out[1] = 0xdc00 + (c & 1023);
            } else
                out[0] = c;
        }
        builder->length += width;
    }
    if (result < 0)
        return QJS_INTL_DATA_ERROR;
    if (builder->count == SIZE_MAX / sizeof(QJSIntlPart))
        return QJS_INTL_OVERFLOW;
    if (builder->value.parts)
        builder->value.parts[builder->count] = (QJSIntlPart){start, builder->length, type};
    builder->count++;
    return QJS_INTL_OK;
}

static QJSIntlStatus body_part(Builder *builder, const QJSIntlFormatted *body)
{
    size_t i;
    if (body->length > builder->limit - builder->length ||
        body->part_count > SIZE_MAX / sizeof(QJSIntlPart) - builder->count)
        return QJS_INTL_OVERFLOW;
    if (builder->value.text)
        memcpy(builder->value.text + builder->length, body->text, body->length * sizeof(uint16_t));
    if (builder->value.parts) {
        for (i = 0; i < body->part_count; i++) {
            QJSIntlPart *part = &builder->value.parts[builder->count + i];
            *part = body->parts[i];
            part->start += builder->length;
            part->end += builder->length;
        }
    }
    builder->length += body->length;
    builder->count += body->part_count;
    return QJS_INTL_OK;
}

static QJSIntlStatus emit(Builder *builder, const QJSIntlNumberPatternToken *tokens, size_t count,
                          const QJSIntlNumberAffixSymbols *symbols, const QJSIntlFormatted *body,
                          unsigned int prefix_plus)
{
    size_t i;
    QJSIntlStatus status;
    if (prefix_plus) {
        status = text_part(builder, symbols->plus, QJS_INTL_PART_PLUS_SIGN);
        if (status)
            return status;
    }
    for (i = 0; i < count; i++) {
        QJSIntlBytes text = tokens[i].text;
        QJSIntlPartType type = QJS_INTL_PART_LITERAL;
        switch (tokens[i].kind) {
        case QJS_INTL_NUMBER_TOKEN_NUMBER:
            status = body_part(builder, body);
            if (status)
                return status;
            continue;
        case QJS_INTL_NUMBER_TOKEN_MINUS:
            text = symbols->minus;
            type = QJS_INTL_PART_MINUS_SIGN;
            break;
        case QJS_INTL_NUMBER_TOKEN_PLUS:
            text = symbols->plus;
            type = QJS_INTL_PART_PLUS_SIGN;
            break;
        case QJS_INTL_NUMBER_TOKEN_PERCENT:
            text = symbols->percent;
            type = QJS_INTL_PART_PERCENT_SIGN;
            break;
        case QJS_INTL_NUMBER_TOKEN_PER_MILLE:
            text = symbols->per_mille;
            type = QJS_INTL_PART_PERCENT_SIGN;
            break;
        case QJS_INTL_NUMBER_TOKEN_CURRENCY:
        case QJS_INTL_NUMBER_TOKEN_CURRENCY_ISO:
        case QJS_INTL_NUMBER_TOKEN_CURRENCY_NAME:
        case QJS_INTL_NUMBER_TOKEN_CURRENCY_NARROW:
            text = symbols->currency;
            type = QJS_INTL_PART_CURRENCY;
            break;
        case QJS_INTL_NUMBER_TOKEN_LITERAL:
            break;
        default:
            return QJS_INTL_INVALID_ARGUMENT;
        }
        if (!text.data && text.length)
            return QJS_INTL_INVALID_ARGUMENT;
        status = text_part(builder, text, type);
        if (status)
            return status;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_affix_format(const QJSIntlAllocator *allocator,
                                           const QJSIntlNumberPatternToken *tokens, size_t count,
                                           const QJSIntlNumberAffixSymbols *symbols,
                                           const QJSIntlFormatted *body, unsigned int ensure_plus,
                                           size_t maximum_output_length, QJSIntlFormatted *out)
{
    Builder builder = {0};
    size_t i, at = 0, numbers = 0;
    int has_plus = 0;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !tokens || !count ||
        !symbols || !body || !body->text || !body->length || !body->parts || !body->part_count ||
        !maximum_output_length || ensure_plus > 1 || count > SIZE_MAX / sizeof(*tokens) ||
        body->part_count > SIZE_MAX / sizeof(*body->parts))
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < body->part_count; i++) {
        const QJSIntlPart *part = &body->parts[i];
        if (part->start != at || part->end < at || part->end > body->length ||
            part->type == QJS_INTL_PART_ELEMENT || (unsigned int)part->type >= QJS_INTL_PART_COUNT)
            return QJS_INTL_INVALID_ARGUMENT;
        at = part->end;
    }
    if (at != body->length)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < count; i++) {
        if ((unsigned int)tokens[i].kind >= QJS_INTL_NUMBER_TOKEN_COUNT ||
            (tokens[i].kind != QJS_INTL_NUMBER_TOKEN_LITERAL && tokens[i].text.length))
            return QJS_INTL_INVALID_ARGUMENT;
        numbers += tokens[i].kind == QJS_INTL_NUMBER_TOKEN_NUMBER;
        has_plus |= tokens[i].kind == QJS_INTL_NUMBER_TOKEN_PLUS;
    }
    if (numbers != 1)
        return QJS_INTL_INVALID_ARGUMENT;
    builder.limit = maximum_output_length;
    status = emit(&builder, tokens, count, symbols, body, ensure_plus && !has_plus);
    if (status)
        return status;
    if (builder.length > SIZE_MAX / sizeof(uint16_t))
        return QJS_INTL_OVERFLOW;
    builder.value.length = builder.length;
    builder.value.part_count = builder.count;
    builder.value.text = allocator->malloc(allocator->opaque, builder.length * sizeof(uint16_t));
    if (!builder.value.text)
        return QJS_INTL_NO_MEMORY;
    builder.value.parts = allocator->malloc(allocator->opaque, builder.count * sizeof(QJSIntlPart));
    if (!builder.value.parts) {
        qjs_intl_number_body_clear(allocator, &builder.value);
        return QJS_INTL_NO_MEMORY;
    }
    builder.length = builder.count = 0;
    status = emit(&builder, tokens, count, symbols, body, ensure_plus && !has_plus);
    if (status) {
        qjs_intl_number_body_clear(allocator, &builder.value);
        return status;
    }
    *out = builder.value;
    return QJS_INTL_OK;
}
