/*
 * QuickJS Javascript Engine: Value Operations and Conversions Definitions
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

#ifndef QUICKJS_VALUE_JS_VALUE_H
#define QUICKJS_VALUE_JS_VALUE_H

#include "quickjs-internal.h"
#include "compiler/js_opcode.h"

/* Conversions and operations */
JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint);
JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);
void JS_SetIsHTMLDDA(JSContext *ctx, JSValueConst obj);
int JS_ToBoolFree(JSContext *ctx, JSValue val);
int JS_ToBool(JSContext *ctx, JSValueConst val);
int skip_spaces(const char *pc);
JSValue js_atof(JSContext *ctx, const char *str, const char **pp, int radix, int flags);
JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);
JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val);
__exception int __JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val);
int JS_ToFloat64(JSContext *ctx, double *pres, JSValueConst val);
JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val);
int JS_ToInt32Sat(JSContext *ctx, int *pres, JSValueConst val);
int JS_ToInt32Clamp(JSContext *ctx, int *pres, JSValueConst val, int min, int max, int val_if_nan);
int JS_ToInt64Sat(JSContext *ctx, int64_t *pres, JSValueConst val);
int JS_ToInt64Clamp(JSContext *ctx, int64_t *pres, JSValueConst val, int64_t min, int64_t max, int64_t val_if_nan);
int JS_ToInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
int JS_ToInt64(JSContext *ctx, int64_t *pres, JSValueConst val);
int JS_ToInt64Ext(JSContext *ctx, int64_t *pres, JSValueConst val);
int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val);
int JS_ToInt32(JSContext *ctx, int32_t *pres, JSValueConst val);
int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);
__exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen, JSValue val, BOOL is_typed_array);
int JS_ToIndex(JSContext *ctx, uint64_t *plen, JSValueConst val);
__exception int JS_ToLengthFree(JSContext *ctx, int64_t *plen, JSValue val);
int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);
BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val);
JSValue js_dtoa2(JSContext *ctx, double d, int radix, int n_digits, int flags);
JSValue JS_ToString(JSContext *ctx, JSValueConst val);
JSValue JS_ToStringFree(JSContext *ctx, JSValue val);
JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val);
JSValue JS_ToPropertyKey(JSContext *ctx, JSValueConst val);
JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val);
int JS_IsArray(JSContext *ctx, JSValueConst val);

/* Debug and dump functions */
void js_dump_value_write(void *opaque, const char *buf, size_t len);
void print_atom(JSContext *ctx, JSAtom atom);
void JS_DumpAtom(JSContext *ctx, const char *str, JSAtom atom);
void JS_DumpValue(JSContext *ctx, const char *str, JSValueConst val);
void JS_DumpValueRT(JSRuntime *rt, const char *str, JSValueConst val);
void JS_DumpObjectHeader(JSRuntime *rt);
void JS_DumpObject(JSRuntime *rt, JSObject *p);
void JS_DumpGCObject(JSRuntime *rt, JSGCObjectHeader *p);

/* Slow arithmetic fallbacks */
__exception int js_unary_arith_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
__exception int js_post_inc_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
int js_not_slow(JSContext *ctx, JSValue *sp);
__exception int js_binary_arith_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
__exception int js_add_slow(JSContext *ctx, JSValue *sp);
__exception int js_binary_logic_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
int js_relational_slow(JSContext *ctx, JSValue *sp, OPCodeEnum op);
__exception int js_eq_slow(JSContext *ctx, JSValue *sp, BOOL is_neq);
int js_shr_slow(JSContext *ctx, JSValue *sp);

/* Strict equality and SameValue */
BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2, JSStrictEqModeEnum mode);
BOOL js_strict_eq(JSContext *ctx, JSValueConst op1, JSValueConst op2);
BOOL JS_StrictEq(JSContext *ctx, JSValueConst op1, JSValueConst op2);
BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2);
BOOL JS_SameValue(JSContext *ctx, JSValueConst op1, JSValueConst op2);
BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2);
BOOL JS_SameValueZero(JSContext *ctx, JSValueConst op1, JSValueConst op2);

#endif /* QUICKJS_VALUE_JS_VALUE_H */
