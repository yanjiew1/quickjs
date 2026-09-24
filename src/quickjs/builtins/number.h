/*
 * QuickJS internal Number builtin interfaces
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
#ifndef QUICKJS_PRIVATE_BUILTIN_NUMBER_H
#define QUICKJS_PRIVATE_BUILTIN_NUMBER_H

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_number_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_boolean_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_parseInt(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_parseFloat(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_random_init(JSContext *ctx);

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_number_funcs[14];

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_number_proto_funcs[6];

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_boolean_proto_funcs[2];

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_math_obj[1];

/* Internal implementation detail; not part of the public QuickJS API. */
int js_get_radix(JSContext *ctx, JSValueConst val);

#endif /* QUICKJS_PRIVATE_BUILTIN_NUMBER_H */
