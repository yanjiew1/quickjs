/*
 * QuickJS Javascript Engine
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
#ifndef QUICKJS_INTERNAL_OPERATOR_H
#define QUICKJS_INTERNAL_OPERATOR_H

#include "internal-property.h"

QJS_INTERNAL __exception int qjs_unary_arith_slow(
    JSContext *ctx, JSValue *sp, OPCodeEnum op);
QJS_INTERNAL __exception int qjs_post_inc_slow(
    JSContext *ctx, JSValue *sp, OPCodeEnum op);
QJS_INTERNAL int qjs_not_slow(JSContext *ctx, JSValue *sp);
QJS_INTERNAL __exception int qjs_binary_arith_slow(
    JSContext *ctx, JSValue *sp, OPCodeEnum op);
QJS_INTERNAL __exception int qjs_add_slow(JSContext *ctx, JSValue *sp);
QJS_INTERNAL __exception int qjs_binary_logic_slow(
    JSContext *ctx, JSValue *sp, OPCodeEnum op);
QJS_INTERNAL int qjs_relational_slow(
    JSContext *ctx, JSValue *sp, OPCodeEnum op);
QJS_INTERNAL __exception int qjs_eq_slow(
    JSContext *ctx, JSValue *sp, BOOL is_neq);
QJS_INTERNAL int qjs_shr_slow(JSContext *ctx, JSValue *sp);
QJS_INTERNAL __exception int qjs_operator_in(JSContext *ctx, JSValue *sp);
QJS_INTERNAL __exception int qjs_operator_private_in(
    JSContext *ctx, JSValue *sp);
QJS_INTERNAL __exception int qjs_operator_instanceof(
    JSContext *ctx, JSValue *sp);
QJS_INTERNAL __exception int qjs_operator_typeof(JSContext *ctx,
                                                 JSValueConst value);
QJS_INTERNAL __exception int qjs_operator_delete(JSContext *ctx, JSValue *sp);
QJS_INTERNAL __exception int qjs_has_unscopable(
    JSContext *ctx, JSValueConst obj, JSAtom atom);

#endif /* QUICKJS_INTERNAL_OPERATOR_H */
