/*
 * QuickJS internal call interfaces
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
#ifndef QUICKJS_PRIVATE_CALL_H
#define QUICKJS_PRIVATE_CALL_H

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_closure2(JSContext *ctx, JSValue func_obj,
                           JSFunctionBytecode *b,
                           JSVarRef **cur_var_refs,
                           JSStackFrame *sf,
                           BOOL is_eval, JSModuleDef *m);


/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_closure(JSContext *ctx, JSValue bfunc,
                          JSVarRef **cur_var_refs,
                          JSStackFrame *sf, BOOL is_eval);


/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                           int argc, JSValueConst *argv);


/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_async_function_call(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags);


/* Internal implementation detail; not part of the public QuickJS API. */
JSValueConst JS_GetActiveFunction(JSContext *ctx);

/* Internal implementation detail; not part of the public QuickJS API. */
BOOL JS_IsCFunction(JSContext *ctx, JSValueConst val, JSCFunction *func, int magic);

typedef struct JSBoundFunction {
    JSValue func_obj;
    JSValue this_val;
    int argc;
    JSValue argv[0];
} JSBoundFunction;

typedef struct JSCFunctionDataRecord {
    JSCFunctionData *func;
    uint8_t length;
    uint8_t data_len;
    uint16_t magic;
    JSValue data[0];
} JSCFunctionDataRecord;

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_EnqueueJob2(JSContext *ctx, JSJobFunc *job_func,
                          int argc, JSValueConst *argv, BOOL no_exception);

/* Internal implementation detail; not part of the public QuickJS API. */
JSContext *JS_GetFunctionRealm(JSContext *ctx, JSValueConst func_obj);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj,
                               JSValueConst method);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                             int argc, JSValueConst *argv);

/* Internal implementation detail; not part of the public QuickJS API. */
int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj,
                            BOOL is_exception_pending);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj,
                                           BOOL *pdone);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
                               JSValueConst method,
                               int argc, JSValueConst *argv, BOOL *pdone);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj,
                                JSValueConst method,
                                int argc, JSValueConst *argv, int *pdone);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_NewCFunction3(JSContext *ctx, JSCFunction *func,
                                const char *name,
                                int length, JSCFunctionEnum cproto, int magic,
                                JSValueConst proto_val, int n_fields);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_create_iterator_result(JSContext *ctx,
                                         JSValue val,
                                         BOOL done);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_function_set_properties(JSContext *ctx, JSValueConst func_obj,
                                       JSAtom name, int len);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_async_function_resolve_call(JSContext *ctx,
                                              JSValueConst func_obj,
                                              JSValueConst this_obj,
                                              int argc, JSValueConst *argv,
                                              int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_async_generator_finalizer(JSRuntime *rt, JSValue obj);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_async_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                                JSValueConst this_obj,
                                                int argc, JSValueConst *argv,
                                                int flags);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_async_generator_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_async_generator_next(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv,
                                       int magic);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_function_proto_fileName(JSContext *ctx,
                                          JSValueConst this_val);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_function_proto_lineNumber(JSContext *ctx,
                                            JSValueConst this_val, int is_col);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_generator_next(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv,
                                 BOOL *pdone, int magic);

#endif /* QUICKJS_PRIVATE_CALL_H */
