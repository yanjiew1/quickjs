/* Exact duration component values for the Number service.
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
#ifndef QJS_INTL_DURATION_DECIMAL_H
#define QJS_INTL_DURATION_DECIMAL_H
#include "../common/types.h"
#include "../../temporal/types.h"

/* Convert one field of a previously validated duration to exact decimal text.
   If fractional is nonzero, unit must be second, millisecond or microsecond;
   include all smaller fields through nanoseconds at successive powers of 1000.
   Perform no binary64 addition/division or allocation. The Number service owns
   rounding and sign-display policy, including a displayed negative zero.
   Capacity 64 covers every valid duration component with its sign, decimal
   point and terminating NUL. Output borrows buffer; fresh disjoint storage is
   required. Failure resets *out and leaves buffer unchanged. Zero is positive.
   Check duration validity once at the format entry, not once per component. */
QJSIntlStatus qjs_intl_duration_component_decimal(const QJSTemporalDuration *, QJSTemporalUnit unit,
                                                 int fractional, char *buffer, size_t capacity,
                                                 QJSIntlMathematicalValue *out);
#endif
