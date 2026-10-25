/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19:
 * PartitionNumberPattern and the number/sign/currency/percent part records.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "affix.h"
#include "body.h"
#include "../common/format-builder.h"
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

static QJSIntlStatus text_part(QJSIntlFormatBuilder *builder, QJSIntlBytes text, QJSIntlPartType type)
{
    size_t start = builder->length;
    QJSIntlStatus status = qjs_intl_format_builder_text(builder, text, NULL, NULL);
    if (status)
        return status;
    /* Even an empty symbol contributes its specified part record. */
    return qjs_intl_format_builder_part(builder, start, builder->length, type, 1);
}

static int is_currency(QJSIntlNumberTokenKind kind)
{
    return kind >= QJS_INTL_NUMBER_TOKEN_CURRENCY && kind <= QJS_INTL_NUMBER_TOKEN_CURRENCY_NARROW;
}

static QJSIntlStatus emit(QJSIntlFormatBuilder *builder, const QJSIntlNumberPatternToken *tokens, size_t count,
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
        QJSIntlBytes gap = {0};
        if (i && symbols->currency.length) {
            if (tokens[i].kind == QJS_INTL_NUMBER_TOKEN_NUMBER && is_currency(tokens[i - 1].kind))
                gap = symbols->after_currency;
            else if (is_currency(tokens[i].kind) && tokens[i - 1].kind == QJS_INTL_NUMBER_TOKEN_NUMBER)
                gap = symbols->before_currency;
        }
        if (gap.length) {
            if (!gap.data)
                return QJS_INTL_INVALID_ARGUMENT;
            status = text_part(builder, gap, QJS_INTL_PART_LITERAL);
            if (status)
                return status;
        }
        switch (tokens[i].kind) {
        case QJS_INTL_NUMBER_TOKEN_NUMBER:
            status = qjs_intl_format_builder_body(builder, body);
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
    QJSIntlFormatBuilder builder = {0};
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
            part->type == QJS_INTL_PART_ELEMENT || (unsigned int)part->type >= QJS_INTL_PART_NUMBER_COUNT)
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
    builder.result.length = builder.length;
    builder.result.part_count = builder.count;
    builder.result.text = allocator->malloc(allocator->opaque, builder.length * sizeof(uint16_t));
    if (!builder.result.text)
        return QJS_INTL_NO_MEMORY;
    builder.result.parts = allocator->malloc(allocator->opaque, builder.count * sizeof(QJSIntlPart));
    if (!builder.result.parts) {
        qjs_intl_number_body_clear(allocator, &builder.result);
        return QJS_INTL_NO_MEMORY;
    }
    builder.length = builder.count = 0;
    status = emit(&builder, tokens, count, symbols, body, ensure_plus && !has_plus);
    if (status) {
        qjs_intl_number_body_clear(allocator, &builder.result);
        return status;
    }
    *out = builder.result;
    return QJS_INTL_OK;
}
