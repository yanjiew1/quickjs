/* CLDR TR35 Localized GMT format; no zone identity or transition policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "zone-offset.h"
#include "../common/format-builder.h"
#include <stdio.h>
#include <string.h>

static QJSIntlStatus split(QJSIntlBytes text, size_t *position)
{
    size_t i, at = SIZE_MAX;
    int quoted = 0;
    for (i = 0; i < text.length; i++) {
        if (text.data[i] == '\'') {
            if (i + 1 < text.length && text.data[i + 1] == '\'')
                i++;
            else
                quoted = !quoted;
        } else if (text.data[i] == ';' && !quoted) {
            if (at != SIZE_MAX)
                return QJS_INTL_DATA_ERROR;
            at = i;
        }
    }
    if (quoted || at == SIZE_MAX || !at || at + 1 == text.length)
        return QJS_INTL_DATA_ERROR;
    *position = at;
    return QJS_INTL_OK;
}

static QJSIntlStatus pattern(const QJSIntlAllocator *allocator, QJSIntlBytes source,
                             QJSIntlDateOffsetPattern *out)
{
    QJSIntlStatus status = qjs_intl_date_pattern_compile(allocator, source, &out->program);
    unsigned int phase = 0;
    size_t i;
    if (status)
        return status;
    for (i = 0; i < out->program.count; i++) {
        const QJSIntlDatePatternToken *token = &out->program.tokens[i];
        if (!token->symbol) {
            QJSIntlBytes *literal = phase == 0 ? &out->prefix : phase == 1 ? &out->separator : &out->suffix;
            *literal = token->literal;
        } else if (!phase && token->symbol == 'H' && token->width <= 2) {
            phase = 1;
        } else if (phase == 1 && token->symbol == 'm' && token->width == 2) {
            phase = 2;
        } else {
            return QJS_INTL_DATA_ERROR;
        }
    }
    return phase == 2 ? QJS_INTL_OK : QJS_INTL_DATA_ERROR;
}

void qjs_intl_date_offset_clear(QJSIntlDateOffset *format)
{
    if (!format)
        return;
    qjs_intl_date_pattern_clear(&format->allocator, &format->positive.program);
    qjs_intl_date_pattern_clear(&format->allocator, &format->negative.program);
    if (format->storage && format->allocator.free)
        format->allocator.free(format->allocator.opaque, format->storage);
    memset(format, 0, sizeof(*format));
}

QJSIntlStatus qjs_intl_date_offset_open(const QJSIntlAllocator *allocator,
                                       QJSIntlBytes hour, QJSIntlBytes wrapper, QJSIntlBytes zero,
                                       QJSIntlDateOffset *out)
{
    QJSIntlDateOffset result = {0};
    QJSIntlFormatBuilder validation = {0};
    QJSIntlStatus status;
    size_t at, length;
    char *copy;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free ||
        (!hour.data && hour.length) || (!wrapper.data && wrapper.length) || (!zero.data && zero.length) ||
        hour.length > PTRDIFF_MAX || wrapper.length > PTRDIFF_MAX || zero.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!hour.length || !wrapper.length)
        return QJS_INTL_UNSUPPORTED;
    result.allocator = *allocator;
    validation.limit = SIZE_MAX;
    status = qjs_intl_format_builder_text(&validation, wrapper, NULL, NULL);
    if (!status)
        status = qjs_intl_format_builder_text(&validation, zero, NULL, NULL);
    if (!status)
        status = split(hour, &at);
    if (!status)
        status = pattern(allocator, (QJSIntlBytes){hour.data, at}, &result.positive);
    if (!status)
        status = pattern(allocator, (QJSIntlBytes){hour.data + at + 1, hour.length - at - 1}, &result.negative);
    if (status)
        goto done;
    if (wrapper.length > SIZE_MAX - zero.length) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    length = wrapper.length + zero.length;
    result.storage = copy = allocator->malloc(allocator->opaque, length);
    if (!copy) {
        status = QJS_INTL_NO_MEMORY;
        goto done;
    }
    memcpy(copy, wrapper.data, wrapper.length);
    if (zero.length)
        memcpy(copy + wrapper.length, zero.data, zero.length);
    result.zero = (QJSIntlBytes){copy + wrapper.length, zero.length};
    status = qjs_intl_unary_pattern_parse((QJSIntlBytes){copy, wrapper.length}, &result.wrapper);
    if (status == QJS_INTL_UNSUPPORTED)
        status = QJS_INTL_DATA_ERROR;
    if (!status) {
        *out = result;
        memset(&result, 0, sizeof(result));
    }
done:
    qjs_intl_date_offset_clear(&result);
    return status;
}

QJSIntlStatus qjs_intl_date_offset_from_data(const QJSIntlTimeZoneNameData *data, uint32_t locale,
                                            const QJSIntlAllocator *allocator, QJSIntlDateOffset *out)
{
    static const char *const codes[] = {"hourFormat", "gmtFormat", "gmtZeroFormat"};
    QJSIntlTimeZoneNameGroup group = {0};
    QJSIntlBytes texts[3];
    QJSIntlStatus status;
    uint32_t index, head;
    unsigned int i;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !data || !data->section.bytes || locale >= data->section.count ||
        !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    group.family = QJS_INTL_TIMEZONE_NAME_FORMAT;
    status = qjs_intl_timezone_name_find_group(data, &group, &index);
    if (!status)
        status = qjs_intl_timezone_name_head(data, locale, index, &head);
    if (status)
        return status;
    for (i = 0; i < 3; i++) {
        QJSIntlTimeZoneNameEntry entry;
        QJSIntlTimeZoneNameNode origin;
        status = qjs_intl_timezone_name_resolve(data, head, (QJSIntlBytes){codes[i], strlen(codes[i])},
                                               QJS_INTL_TIMEZONE_NAME_NO_TYPE, (QJSIntlBytes){0}, &entry, &origin);
        if (i == 2 && status == QJS_INTL_UNSUPPORTED) {
            texts[i] = (QJSIntlBytes){0};
            continue;
        }
        if (status)
            return status;
        if (entry.blocked || !entry.text.length) {
            if (i != 2)
                return QJS_INTL_UNSUPPORTED;
            texts[i] = (QJSIntlBytes){0};
        } else {
            texts[i] = entry.text;
        }
    }
    return qjs_intl_date_offset_open(allocator, texts[0], texts[1], texts[2], out);
}

typedef struct Pieces {
    QJSIntlFormatted hour, minute, second;
    unsigned int minutes, seconds;
} Pieces;

static QJSIntlStatus emit(QJSIntlFormatBuilder *builder, const QJSIntlDateOffset *format,
                          const QJSIntlDateOffsetPattern *source, const Pieces *pieces, int zero)
{
    QJSIntlStatus status;
    if (zero)
        return qjs_intl_format_builder_text(builder, format->zero, NULL, NULL);
    status = qjs_intl_format_builder_text(builder, format->wrapper.prefix, NULL, NULL);
    if (!status)
        status = qjs_intl_format_builder_text(builder, source->prefix, NULL, NULL);
    if (!status)
        status = qjs_intl_format_builder_body(builder, &pieces->hour);
    if (!status && pieces->minutes)
        status = qjs_intl_format_builder_text(builder, source->separator, NULL, NULL);
    if (!status && pieces->minutes)
        status = qjs_intl_format_builder_body(builder, &pieces->minute);
    if (!status && pieces->seconds)
        status = qjs_intl_format_builder_text(builder, source->separator, NULL, NULL);
    if (!status && pieces->seconds)
        status = qjs_intl_format_builder_body(builder, &pieces->second);
    if (!status)
        status = qjs_intl_format_builder_text(builder, source->suffix, NULL, NULL);
    if (!status)
        status = qjs_intl_format_builder_text(builder, format->wrapper.suffix, NULL, NULL);
    return status;
}

static QJSIntlStatus number(const QJSIntlNumberFormatter *formatter, unsigned int value, QJSIntlFormatted *out)
{
    char text[12];
    QJSIntlMathematicalValue input = {0};
    int count = snprintf(text, sizeof(text), "%u", value);
    if (!formatter || count <= 0 || (size_t)count >= sizeof(text))
        return QJS_INTL_INVALID_ARGUMENT;
    input.decimal = (QJSIntlBytes){text, (size_t)count};
    return qjs_intl_number_format(formatter, &input, out);
}

QJSIntlStatus qjs_intl_date_offset_format(const QJSIntlDateOffset *format, int32_t seconds,
                                         unsigned int long_format,
                                         const QJSIntlNumberFormatter *numeric,
                                         const QJSIntlNumberFormatter *two_digit,
                                         size_t maximum_output_length, QJSIntlFormatted *out)
{
    QJSIntlFormatBuilder builder = {0};
    Pieces pieces = {0};
    const QJSIntlDateOffsetPattern *source;
    QJSIntlStatus status;
    unsigned int magnitude;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !format || !format->storage || !format->allocator.malloc || !format->allocator.free ||
        !maximum_output_length || long_format > 1 || seconds <= -86400 || seconds >= 86400)
        return QJS_INTL_INVALID_ARGUMENT;
    source = seconds < 0 ? &format->negative : &format->positive;
    magnitude = seconds < 0 ? (unsigned int)-seconds : (unsigned int)seconds;
    pieces.seconds = magnitude % 60 != 0;
    pieces.minutes = long_format || magnitude % 3600;
    status = QJS_INTL_OK;
    if (magnitude || !format->zero.length) {
        status = number(long_format ? two_digit : numeric, magnitude / 3600, &pieces.hour);
        if (!status && pieces.minutes)
            status = number(two_digit, magnitude / 60 % 60, &pieces.minute);
        if (!status && pieces.seconds)
            status = number(two_digit, magnitude % 60, &pieces.second);
    }
    builder.limit = maximum_output_length;
    if (!status)
        status = emit(&builder, format, source, &pieces, !magnitude && format->zero.length);
    if (status)
        goto done;
    if (builder.length > SIZE_MAX / sizeof(uint16_t)) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    builder.result.text = format->allocator.malloc(format->allocator.opaque, builder.length * sizeof(uint16_t));
    builder.result.parts = format->allocator.malloc(format->allocator.opaque, sizeof(QJSIntlPart));
    if (!builder.result.text || !builder.result.parts) {
        status = QJS_INTL_NO_MEMORY;
        goto done;
    }
    builder.result.length = builder.length;
    builder.result.part_count = 1;
    builder.length = builder.count = 0;
    /* Numeric body subparts are deliberately not copied: the complete localized
       GMT representation is one date timeZoneName field. */
    {
        QJSIntlPart *part = builder.result.parts;
        builder.result.parts = NULL;
        status = emit(&builder, format, source, &pieces, !magnitude && format->zero.length);
        builder.result.parts = part;
    }
    if (!status) {
        builder.result.parts[0] = (QJSIntlPart){0, builder.length, QJS_INTL_PART_TIME_ZONE_NAME};
        *out = builder.result;
        memset(&builder.result, 0, sizeof(builder.result));
    }
done:
    qjs_intl_formatted_clear(&format->allocator, &pieces.hour);
    qjs_intl_formatted_clear(&format->allocator, &pieces.minute);
    qjs_intl_formatted_clear(&format->allocator, &pieces.second);
    qjs_intl_formatted_clear(&format->allocator, &builder.result);
    return status;
}
