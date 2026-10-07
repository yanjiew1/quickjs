/*
 * QuickJS native C function interfaces
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
#ifndef QUICKJS_C_FUNCTION_H
#define QUICKJS_C_FUNCTION_H

#include "base.h"
#include "object.h"

/* Native function names use LP64 padding or a native-only ILP32 tail. */
static inline JSAtom *js_c_function_initial_name(JSObject *p)
{
#ifdef JS_PTR64
    return &p->u.cfunc.initial_name;
#else
    return (JSAtom *)(p + 1);
#endif
}

typedef struct JSCFunctionDataRecord {
    JSCFunctionData *func;
    uint8_t length;
    uint8_t data_len;
    uint16_t magic;
    JSValue data[0];
} JSCFunctionDataRecord;

JSValue JS_NewCFunction3(JSContext *ctx, JSCFunction *func,
                         const char *name,
                         int length, JSCFunctionEnum cproto, int magic,
                         JSValueConst proto_val, int n_fields);
JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj,
                           JSValueConst this_obj,
                           int argc, JSValueConst *argv, int flags);
void js_c_function_finalizer(JSRuntime *rt, JSValue val);
void js_c_function_mark(JSRuntime *rt, JSValueConst val,
                        JS_MarkFunc *mark_func);
JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                JSValueConst this_val,
                                int argc, JSValueConst *argv, int flags);
void js_c_function_data_finalizer(JSRuntime *rt, JSValue val);
void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                             JS_MarkFunc *mark_func);
BOOL JS_IsCFunction(JSContext *ctx, JSValueConst val, JSCFunction *func, int magic);

#endif
