/* CLDR standard, compact and legacy currency boundary predicates.
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
#include "boundary.h"
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include <limits.h>
#include <string.h>

QJSIntlStatus qjs_intl_currency_boundary(QJSIntlBytes symbol, QJSIntlCurrencyBoundary *out)
{
    const uint8_t *at, *end, *next;
    QJSIntlCurrencyBoundary result = {0};
    uint32_t first = 0, last = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!symbol.data && symbol.length) || symbol.length > INT_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!symbol.length)
        return QJS_INTL_OK;
    at = (const uint8_t *)symbol.data;
    end = at + symbol.length;
    while (at < end) {
        int c = unicode_from_utf8(at, (int)(end - at), &next);
        unsigned int letter;
        if (c <= 0 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return QJS_INTL_INVALID_ARGUMENT;
        if (at == (const uint8_t *)symbol.data)
            first = c;
        last = c;
        letter = lre_is_letter(c);
        if (letter || !lre_is_mark(c))
            result.compact_before_number = letter;
        at = next;
    }
    result.before_number = lre_is_letter(last);
    result.after_number = lre_is_letter(first);
    result.spacing_before_number = !lre_is_symbol_or_separator(last);
    result.spacing_after_number = !lre_is_symbol_or_separator(first);
    *out = result;
    return QJS_INTL_OK;
}
