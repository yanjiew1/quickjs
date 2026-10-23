/*
 * QuickJS value conversion interfaces
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
#ifndef QUICKJS_VALUE_CONVERSION_H
#define QUICKJS_VALUE_CONVERSION_H

#include "../internal/base.h"

#define HINT_STRING  JS_HINT_STRING
#define HINT_NUMBER  JS_HINT_NUMBER
#define HINT_NONE    JS_HINT_NONE
#define HINT_FORCE_ORDINARY (1 << 4) // don't try Symbol.toPrimitive

JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint);
int JS_ToBoolFree(JSRuntime *rt, JSValue val);
JSValue JS_ToStringFree(JSContext *ctx, JSValue val);
JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val, int argc,
                              JSValueConst *argv);
JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val);

#endif
