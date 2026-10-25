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
#include "unary-pattern.h"
#include <string.h>

QJSIntlStatus qjs_intl_unary_pattern_parse(QJSIntlBytes text, QJSIntlUnaryPattern *out)
{
    size_t i, number = 0;
    int seen = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!text.data && text.length) || text.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < text.length; i++) {
        if (text.data[i] == '}')
            return QJS_INTL_DATA_ERROR;
        if (text.data[i] != '{')
            continue;
        if (seen || text.length - i < 3 || text.data[i + 1] != '0' || text.data[i + 2] != '}')
            return QJS_INTL_DATA_ERROR;
        seen = 1;
        number = i;
        i += 2;
    }
    if (!seen)
        return QJS_INTL_UNSUPPORTED;
    out->prefix = (QJSIntlBytes){text.data, number};
    out->suffix = (QJSIntlBytes){text.data + number + 3, text.length - number - 3};
    return QJS_INTL_OK;
}
