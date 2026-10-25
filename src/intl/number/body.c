/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19:
 * PartitionNotationSubPattern (localized digits, grouping and numeric parts).
 * CLDR 49 beta4 minimumGroupingDigits controls AUTO; MIN2 requires two digits
 * beyond the primary group, independently of the locale's AUTO preference.
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
#include "body.h"
#include <string.h>

typedef struct Builder {
    const QJSIntlNumberBody *layout;
    QJSIntlFormatted value;
    size_t length, count;
} Builder;

void qjs_intl_number_body_clear(const QJSIntlAllocator *allocator, QJSIntlFormatted *out)
{
    qjs_intl_formatted_clear(allocator, out);
}

static int slice_valid(QJSIntlUTF16 text)
{
    return text.data || !text.length;
}

static int layout_valid(const QJSIntlNumberBody *layout)
{
    unsigned int i, j;
    if (!layout || !layout->maximum_output_length ||
        (unsigned int)layout->grouping > QJS_INTL_NUMBER_GROUP_MIN2 ||
        layout->primary_grouping > 9 || layout->secondary_grouping > 9 ||
        !layout->minimum_grouping_digits || layout->minimum_grouping_digits > 9 ||
        (!!layout->primary_grouping != !!layout->secondary_grouping) ||
        !slice_valid(layout->decimal) || !slice_valid(layout->group) || !slice_valid(layout->nan) ||
        !slice_valid(layout->infinity))
        return 0;
    for (i = 0; i < 10; i++) {
        uint32_t c = layout->digits[i];
        if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return 0;
        for (j = 0; j < i; j++)
            if (c == layout->digits[j])
                return 0;
    }
    return 1;
}

static QJSIntlStatus reserve(Builder *builder, size_t length)
{
    if (length > builder->layout->maximum_output_length - builder->length)
        return QJS_INTL_OVERFLOW;
    return QJS_INTL_OK;
}

static QJSIntlStatus part(Builder *builder, size_t start, QJSIntlPartType type)
{
    if (builder->count == SIZE_MAX / sizeof(QJSIntlPart))
        return QJS_INTL_OVERFLOW;
    if (builder->value.parts) {
        QJSIntlPart *result = &builder->value.parts[builder->count];
        result->start = start;
        result->end = builder->length;
        result->type = type;
    }
    builder->count++;
    return QJS_INTL_OK;
}

static QJSIntlStatus symbol(Builder *builder, QJSIntlUTF16 text, QJSIntlPartType type)
{
    size_t start = builder->length;
    QJSIntlStatus status;
    if (!text.length)
        return QJS_INTL_DATA_ERROR;
    status = reserve(builder, text.length);
    if (status)
        return status;
    if (builder->value.text)
        memcpy(builder->value.text + start, text.data, text.length * sizeof(uint16_t));
    builder->length += text.length;
    return part(builder, start, type);
}

static QJSIntlStatus digits(Builder *builder, QJSIntlBytes text, QJSIntlPartType type)
{
    size_t i, start = builder->length;
    for (i = 0; i < text.length; i++) {
        uint32_t c = builder->layout->digits[(unsigned char)text.data[i] - '0'];
        size_t width = c > 0xffff ? 2 : 1;
        QJSIntlStatus status = reserve(builder, width);
        if (status)
            return status;
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
    return part(builder, start, type);
}

static QJSIntlStatus emit(Builder *builder, QJSIntlMathematicalKind kind, QJSIntlBytes text,
                          size_t integer)
{
    const QJSIntlNumberBody *layout = builder->layout;
    size_t at = 0, first = integer, minimum = 1;
    QJSIntlStatus status;
    if (kind == QJS_INTL_NAN)
        return symbol(builder, layout->nan, QJS_INTL_PART_NAN);
    if (kind == QJS_INTL_POSITIVE_INFINITY || kind == QJS_INTL_NEGATIVE_INFINITY)
        return symbol(builder, layout->infinity, QJS_INTL_PART_INFINITY);
    if (layout->grouping == QJS_INTL_NUMBER_GROUP_AUTO)
        minimum = layout->minimum_grouping_digits;
    else if (layout->grouping == QJS_INTL_NUMBER_GROUP_MIN2)
        minimum = 2;
    if (layout->grouping != QJS_INTL_NUMBER_GROUP_OFF && layout->primary_grouping &&
        integer >= layout->primary_grouping + minimum)
        first = (integer - layout->primary_grouping - 1) % layout->secondary_grouping + 1;
    do {
        size_t count = at ? (integer - at > layout->primary_grouping ? layout->secondary_grouping
                                                                     : layout->primary_grouping)
                          : first;
        status = digits(builder, (QJSIntlBytes){text.data + at, count}, QJS_INTL_PART_INTEGER);
        if (status)
            return status;
        at += count;
        if (at < integer) {
            status = symbol(builder, layout->group, QJS_INTL_PART_GROUP);
            if (status)
                return status;
        }
    } while (at < integer);
    if (integer < text.length) {
        status = symbol(builder, layout->decimal, QJS_INTL_PART_DECIMAL);
        if (status)
            return status;
        return digits(builder, (QJSIntlBytes){text.data + integer + 1, text.length - integer - 1},
                      QJS_INTL_PART_FRACTION);
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_number_body_format(const QJSIntlAllocator *allocator,
                                          const QJSIntlNumberBody *layout,
                                          QJSIntlMathematicalKind kind, QJSIntlBytes text,
                                          QJSIntlFormatted *out)
{
    Builder builder = {0};
    QJSIntlStatus status;
    size_t i, integer = text.length;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !layout_valid(layout) ||
        (unsigned int)kind > QJS_INTL_NEGATIVE_ZERO || (!text.data && text.length))
        return QJS_INTL_INVALID_ARGUMENT;
    if (kind == QJS_INTL_FINITE || kind == QJS_INTL_NEGATIVE_ZERO) {
        if (!text.length)
            return QJS_INTL_INVALID_ARGUMENT;
        if (text.length > layout->maximum_output_length)
            return QJS_INTL_OVERFLOW;
        for (i = 0; i < text.length; i++) {
            if (text.data[i] >= '0' && text.data[i] <= '9')
                continue;
            if (text.data[i] != '.' || integer != text.length || !i || i + 1 == text.length)
                return QJS_INTL_INVALID_ARGUMENT;
            integer = i;
        }
    } else if (text.length)
        return QJS_INTL_INVALID_ARGUMENT;
    builder.layout = layout;
    status = emit(&builder, kind, text, integer);
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
    status = emit(&builder, kind, text, integer);
    if (status) {
        qjs_intl_number_body_clear(allocator, &builder.value);
        return status;
    }
    *out = builder.value;
    return QJS_INTL_OK;
}
