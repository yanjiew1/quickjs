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
#include "dayperiod-code.h"
#include <string.h>

#define NAME(s) {s, sizeof(s) - 1}
static const QJSIntlBytes codes[QJS_INTL_DAYPERIOD_CODE_COUNT] = {
    NAME("am"), NAME("pm"), NAME("midnight"), NAME("noon"),
    NAME("morning1"), NAME("morning2"), NAME("afternoon1"), NAME("afternoon2"),
    NAME("evening1"), NAME("evening2"), NAME("night1"), NAME("night2")
};
#undef NAME

QJSIntlBytes qjs_intl_dayperiod_code_name(uint32_t code)
{
    return code < QJS_INTL_DAYPERIOD_CODE_COUNT ? codes[code] : (QJSIntlBytes){0};
}

int qjs_intl_dayperiod_code_find(QJSIntlBytes name)
{
    size_t i;
    if (!name.data)
        return -1;
    for (i = 0; i < QJS_INTL_DAYPERIOD_CODE_COUNT; i++) {
        if (name.length == codes[i].length && !memcmp(name.data, codes[i].data, name.length))
            return (int)i;
    }
    return -1;
}
