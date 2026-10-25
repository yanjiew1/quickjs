/* NumberFormat constructor option records; no formatter or JS object ownership.
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
#ifndef QUICKJS_INTL_NUMBER_FORMAT_OPTIONS_H
#define QUICKJS_INTL_NUMBER_FORMAT_OPTIONS_H
#include "../../internal/base.h"
#include "../../../intl/number/number.h"
#include "number-options.h"

typedef struct JSIntlNumberUnitOptions {
    QJSIntlNumberStyle style;
    QJSIntlNumberCurrencyDisplay currency_display;
    QJSIntlNumberCurrencySign currency_sign;
    QJSIntlNumberUnitDisplay unit_display;
    char currency[4]; /* Set only for currency style, canonical ASCII uppercase. */
    char *unit; /* Set only for unit style, owned with js_free. */
} JSIntlNumberUnitOptions;

/* SetNumberFormatUnitOptions: borrow an already obtained options object.
   Validate supplied currency/unit even for inactive styles; retain only the
   selected style's values. Publish out only on success; zero it on failure. */
int js_intl_number_unit_options(JSContext *ctx, JSValueConst options,
                                 JSIntlNumberUnitOptions *out);

typedef struct JSIntlNumberFormatOptions {
    JSIntlNumberUnitOptions unit;
    JSIntlDigitOptions digits;
    QJSIntlNumberNotation notation;
    QJSIntlNumberGrouping grouping;
    QJSIntlNumberSign sign_display;
    int compact_long;
} JSIntlNumberFormatOptions;

/* Read all non-locale NumberFormat options in constructor order. The service
   supplies CurrencyDigits for standard currency notation only. Borrow inputs;
   release the successful result's unit.unit with js_free. Failure releases
   partial state and zeroes out. Digit records retain absent slots as -1. */
int js_intl_number_format_options(JSContext *ctx, JSValueConst options,
                                   const QJSIntlNumberService *service,
                                   JSIntlNumberFormatOptions *out);
#endif
