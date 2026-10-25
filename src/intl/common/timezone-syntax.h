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
#ifndef QJS_INTL_COMMON_TIMEZONE_SYNTAX_H
#define QJS_INTL_COMMON_TIMEZONE_SYNTAX_H
#include "types.h"

/* Source identity grammar, not ECMA primary-zone canonicalization. */
static inline int qjs_intl_timezone_source_identity_valid(QJSIntlBytes text, int metazone)
{
    size_t i;
    unsigned int segment = 0;
    if (!text.length)
        return 0;
    for (i = 0; i < text.length; i++) {
        unsigned int c = (uint8_t)text.data[i];
        int letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (metazone && !i && !letter)
            return 0;
        if (letter || (c >= '0' && c <= '9') || c == '_' || (!metazone && (c == '+' || c == '.' || c == '-')))
            segment = 1;
        else if (!metazone && c == '/' && segment)
            segment = 0;
        else
            return 0;
    }
    return segment;
}
#endif
