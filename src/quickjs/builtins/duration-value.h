/* Shared ECMAScript duration conversion, without allocating Temporal objects.
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
#ifndef QUICKJS_BUILTINS_DURATION_VALUE_H
#define QUICKJS_BUILTINS_DURATION_VALUE_H

#include "../internal/base.h"
#include "../../temporal/duration.h"

int js_duration_to_integral_field(JSContext *ctx, JSValueConst value,
                                  double *result);
/* Update existing fields in alphabetical property order. Undefined leaves a
   field unchanged; throw if no recognized property has a defined value.
   May partially update on error. */
int js_duration_partial_fields(JSContext *ctx, JSValueConst object,
                               QJSTemporalDuration *fields);
/* ToTemporalDuration's validated slots, with no temporary JS object or
   intrinsic initialization. Copy branded slots when Temporal is enabled.
   Otherwise accept strings and duration-like objects. Leave out unchanged
   on failure; all returned zero fields are positive zero. */
int js_duration_to_record(JSContext *ctx, JSValueConst value,
                          QJSTemporalDuration *out);

#endif
