/* Scientific/engineering exponent parts over a localized numeric body.
 * ECMA-402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
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
#include "exponent.h"
#include <string.h>

/* Append the scientific separator and exponent before applying any positive
   notation literals (for example Hindi's brackets). The style's sign belongs
   outside that complete notation body. Use the selected digit repertoire for
   ICU/CLDR interoperability; the current PartitionNotationSubPattern text
   omits exponent transliteration (open ECMA-402 issue 627). Source E+000 padding
   does not override the specification's exponent rules. */
QJSIntlStatus qjs_intl_number_exponent_format(const QJSIntlAllocator *allocator,
                                              const QJSIntlFormatted *body,
                                              const uint32_t digits[10], QJSIntlUTF16 separator,
                                              QJSIntlUTF16 minus, int32_t exponent, size_t limit,
                                              QJSIntlFormatted *out)
{
    uint32_t magnitude = exponent < 0 ? (uint32_t)-(int64_t)exponent : (uint32_t)exponent;
    uint32_t reversed[10];
    size_t count = 0, digit_length = 0, length, i, at, part;
    size_t extra_parts = exponent < 0 ? 3 : 2;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !body || !digits ||
        !body->text || !body->parts || !body->length || !body->part_count || !limit ||
        !separator.data || !separator.length || (exponent < 0 && (!minus.data || !minus.length)))
        return QJS_INTL_INVALID_ARGUMENT;
    if (body->length > limit)
        return QJS_INTL_OVERFLOW;

    length = body->length;
    do {
        uint32_t c = digits[magnitude % 10];
        reversed[count++] = c;
        digit_length += c > 0xffff ? 2 : 1;
        magnitude /= 10;
    } while (magnitude);
    if (separator.length > limit - length)
        return QJS_INTL_OVERFLOW;
    length += separator.length;
    if (exponent < 0) {
        if (minus.length > limit - length)
            return QJS_INTL_OVERFLOW;
        length += minus.length;
    }
    if (digit_length > limit - length)
        return QJS_INTL_OVERFLOW;
    length += digit_length;
    if (length > SIZE_MAX / sizeof(uint16_t) ||
        body->part_count > SIZE_MAX / sizeof(QJSIntlPart) - extra_parts)
        return QJS_INTL_OVERFLOW;
    out->text = allocator->malloc(allocator->opaque, length * sizeof(uint16_t));
    if (!out->text)
        return QJS_INTL_NO_MEMORY;
    out->parts = allocator->malloc(allocator->opaque,
                                   (body->part_count + extra_parts) * sizeof(QJSIntlPart));
    if (!out->parts) {
        qjs_intl_formatted_clear(allocator, out);
        return QJS_INTL_NO_MEMORY;
    }
    out->length = length;
    out->part_count = body->part_count + extra_parts;
    memcpy(out->text, body->text, body->length * sizeof(uint16_t));
    memcpy(out->parts, body->parts, body->part_count * sizeof(QJSIntlPart));
    at = body->length;
    part = body->part_count;
    memcpy(out->text + at, separator.data, separator.length * sizeof(uint16_t));
    out->parts[part++] = (QJSIntlPart){at, at + separator.length, QJS_INTL_PART_EXPONENT_SEPARATOR};
    at += separator.length;
    if (exponent < 0) {
        memcpy(out->text + at, minus.data, minus.length * sizeof(uint16_t));
        out->parts[part++] =
            (QJSIntlPart){at, at + minus.length, QJS_INTL_PART_EXPONENT_MINUS_SIGN};
        at += minus.length;
    }
    out->parts[part] = (QJSIntlPart){at, length, QJS_INTL_PART_EXPONENT_INTEGER};
    for (i = count; i; i--) {
        uint32_t c = reversed[i - 1];
        if (c > 0xffff) {
            c -= 0x10000;
            out->text[at++] = 0xd800 + (c >> 10);
            out->text[at++] = 0xdc00 + (c & 1023);
        } else
            out->text[at++] = c;
    }
    return QJS_INTL_OK;
}
