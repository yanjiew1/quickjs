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
#include "../icu4x/common.h"
#include "../../icu4x/currency-number.h"
#include <limits.h>
#include <string.h>

QJSIntlStatus qjs_intl_currency_boundary(QJSIntlBytes text, QJSIntlCurrencyBoundary *out)
{
    QJSICU4XCurrencyBoundary value = {0};
    QJSIntlStatus error;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!text.data && text.length) || text.length > INT_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    error = qjs_icu4x_status(
        qjs_icu4x_currency_boundary((const uint8_t *)text.data, text.length, &value));
    if (!error)
        *out = (QJSIntlCurrencyBoundary){value.before_number, value.after_number,
                                         value.compact_before_number, value.spacing_before_number,
                                         value.spacing_after_number};
    return error;
}
