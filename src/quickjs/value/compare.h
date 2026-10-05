/*
 * QuickJS value comparison interfaces
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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
#ifndef QUICKJS_VALUE_COMPARE_H
#define QUICKJS_VALUE_COMPARE_H

#include "../internal/base.h"

typedef enum JSStrictEqModeEnum {
    JS_EQ_STRICT,
    JS_EQ_SAME_VALUE,
    JS_EQ_SAME_VALUE_ZERO,
} JSStrictEqModeEnum;

BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2,
                   JSStrictEqModeEnum eq_mode);
BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2);
BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2);

#endif
