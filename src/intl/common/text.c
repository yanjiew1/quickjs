/* Scalar UTF-8 spans shared by Intl pattern consumers.
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
#include "text.h"
#include "../../cutils/cutils.h"

QJSIntlStatus qjs_intl_text_trim(QJSIntlBytes text, QJSIntlBytes *out)
{
    size_t at = 0, first = text.length, last = 0;
    if (out)
        *out = (QJSIntlBytes){0};
    if (!out || (!text.data && text.length) || text.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    while (at < text.length) {
        const uint8_t *end;
        size_t start = at, remaining = text.length - at;
        int c = unicode_from_utf8((const uint8_t *)text.data + at,
                                  remaining < 4 ? (int)remaining : 4, &end);
        if (c <= 0 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return QJS_INTL_DATA_ERROR;
        at = (const char *)end - text.data;
        if (!qjs_intl_text_is_white_space(c)) {
            if (!last)
                first = start;
            last = at;
        }
    }
    *out = last ? (QJSIntlBytes){text.data + first, last - first} : (QJSIntlBytes){0};
    return QJS_INTL_OK;
}
