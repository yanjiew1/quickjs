/* Exact JS coercion shared by numeric Intl bindings.
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
#ifndef QUICKJS_INTL_NUMBER_VALUE_H
#define QUICKJS_INTL_NUMBER_VALUE_H
#include "../../internal/base.h"
#include "../../../intl/common/number-decimal.h"

/* ToIntlMathematicalValue, including one number-hint ToPrimitive. Finite
   strings/BigInts retain exact decimal text; Numbers use shortest decimal.
   String overflow/underflow follows RoundMVResult before preserving text.
   Borrow input. Fresh, disjoint output required; failure resets it. Clear
   successful results with qjs_intl_number_decimal_clear and the runtime's
   js_intl_allocator. No formatter or locale/backend state is involved. */
int js_intl_to_mathematical_value(JSContext *, JSValueConst, QJSIntlNumberDecimal *);
#endif
