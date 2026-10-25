/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "format-builder.h"
#include "text.h"
#include "../../cutils/cutils.h"
#include <limits.h>

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

/* Append scalar text, optionally tracking Unicode nonspace boundaries for
   the caller's semantic affix classification. */
QJSIntlStatus qjs_intl_format_builder_text(QJSIntlFormatBuilder *builder, QJSIntlBytes text,
                                           size_t *first, size_t *last)
{
    size_t at = 0;
    if (!text.data && text.length)
        return QJS_INTL_INVALID_ARGUMENT;
    while (at < text.length) {
        int c = next(text, &at);
        size_t width;
        if (c < 0)
            return QJS_INTL_DATA_ERROR;
        width = c > 0xffff ? 2 : 1;
        if (width > builder->limit - builder->length)
            return QJS_INTL_OVERFLOW;
        if (first && !qjs_intl_text_is_white_space(c)) {
            if (*first == SIZE_MAX)
                *first = builder->length;
            *last = builder->length + width;
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
    return QJS_INTL_OK;
}

