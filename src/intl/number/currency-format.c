/* CLDR currency unitPattern substitution after decimal formatting.
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
#include "currency-format.h"
#include "affix.h"
#include <string.h>

QJSIntlStatus qjs_intl_number_currency_name_format(const QJSIntlAllocator *allocator,
                                                    QJSIntlBytes pattern, QJSIntlBytes name,
                                                    const QJSIntlFormatted *number, size_t limit,
                                                    QJSIntlFormatted *out)
{
    QJSIntlNumberPatternToken tokens[5];
    QJSIntlNumberAffixSymbols symbols = {.currency = name};
    size_t i, start = 0, count = 0;
    unsigned int seen = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !number || !limit ||
        (!name.data && name.length) || (!pattern.data && pattern.length) || pattern.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < pattern.length; i++) {
        unsigned int argument;
        if (pattern.data[i] == '}')
            return QJS_INTL_DATA_ERROR;
        if (pattern.data[i] != '{')
            continue;
        if (pattern.length - i < 3 || pattern.data[i + 2] != '}' ||
            (pattern.data[i + 1] != '0' && pattern.data[i + 1] != '1'))
            return QJS_INTL_DATA_ERROR;
        argument = pattern.data[i + 1] - '0';
        if (seen & (1U << argument))
            return QJS_INTL_DATA_ERROR;
        seen |= 1U << argument;
        if (i != start)
            tokens[count++] = (QJSIntlNumberPatternToken){QJS_INTL_NUMBER_TOKEN_LITERAL,
                                                          {pattern.data + start, i - start}};
        tokens[count++] = (QJSIntlNumberPatternToken){argument ? QJS_INTL_NUMBER_TOKEN_CURRENCY
                                                              : QJS_INTL_NUMBER_TOKEN_NUMBER,
                                                      {0}};
        i += 2;
        start = i + 1;
    }
    if (seen != 3)
        return QJS_INTL_DATA_ERROR;
    if (start != pattern.length)
        tokens[count++] = (QJSIntlNumberPatternToken){QJS_INTL_NUMBER_TOKEN_LITERAL,
                                                      {pattern.data + start, pattern.length - start}};
    return qjs_intl_number_affix_format(allocator, tokens, count, &symbols, number, 0, limit, out);
}
