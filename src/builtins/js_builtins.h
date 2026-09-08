/*
 * QuickJS Javascript Engine: Builtin Definitions
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

#ifndef QUICKJS_BUILTINS_JS_BUILTINS_H
#define QUICKJS_BUILTINS_JS_BUILTINS_H

#include "quickjs-internal.h"

/* Intrinsic initializers */
int JS_AddIntrinsicBasicObjects(JSContext *ctx);
int JS_AddIntrinsicBaseObjects(JSContext *ctx);
int JS_AddIntrinsicDate(JSContext *ctx);
int JS_AddIntrinsicEval(JSContext *ctx);
int JS_AddIntrinsicStringNormalize(JSContext *ctx);
void JS_AddIntrinsicRegExpCompiler(JSContext *ctx);
int JS_AddIntrinsicRegExp(JSContext *ctx);
int JS_AddIntrinsicJSON(JSContext *ctx);
int JS_AddIntrinsicProxy(JSContext *ctx);
int JS_AddIntrinsicMapSet(JSContext *ctx);
int JS_AddIntrinsicTypedArrays(JSContext *ctx);
int JS_AddIntrinsicPromise(JSContext *ctx);
int JS_AddIntrinsicWeakRef(JSContext *ctx);

#define JS_NEW_CTOR_NO_GLOBAL   (1 << 0) /* don't create a global binding */
#define JS_NEW_CTOR_PROTO_CLASS (1 << 1) /* the prototype class is 'class_id' instead of JS_CLASS_OBJECT */
#define JS_NEW_CTOR_PROTO_EXIST (1 << 2) /* the prototype is already defined */
#define JS_NEW_CTOR_READONLY    (1 << 3) /* read-only constructor field */

JSValue JS_NewCConstructor(JSContext *ctx, int class_id, const char *name,
                           JSCFunction *func, int length, JSCFunctionEnum cproto, int magic,
                           JSValueConst parent_ctor,
                           const JSCFunctionListEntry *ctor_fields, int n_ctor_fields,
                           const JSCFunctionListEntry *proto_fields, int n_proto_fields,
                           int flags);
JSValue get_date_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);

/* Proxy & Reflect */
void js_proxy_finalizer(JSRuntime *rt, JSValue val);
void js_proxy_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue JS_ThrowTypeErrorRevokedProxy(JSContext *ctx);
int js_resolve_proxy(JSContext *ctx, JSValueConst *pval, BOOL throw_exception);
extern const JSCFunctionListEntry js_reflect_obj[1];

/* Collections & WeakRef */
void js_map_finalizer(JSRuntime *rt, JSValue val);
void js_map_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_map_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_map_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void map_delete_weakrefs(JSRuntime *rt, JSWeakRefHeader *wh);
void weakref_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
void finrec_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
JSValue js_object_groupBy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int is_map);
JSValue JS_NewObjectProtoList(JSContext *ctx, JSValueConst proto, const JSCFunctionListEntry *fields, int n_fields);

/* Number, Math & BigInt */
JSValue js_number_constructor(JSContext *ctx, JSValueConst new_target,
                             int argc, JSValueConst *argv);
extern const JSCFunctionListEntry js_number_funcs[14];
extern const JSCFunctionListEntry js_number_proto_funcs[6];
JSValue js_parseInt(JSContext *ctx, JSValueConst this_val,
                    int argc, JSValueConst *argv);
JSValue js_parseFloat(JSContext *ctx, JSValueConst this_val,
                      int argc, JSValueConst *argv);
void js_random_init(JSContext *ctx);
extern const JSCFunctionListEntry js_math_obj[1];
int JS_AddIntrinsicBigInt(JSContext *ctx);

/* Global functions in quickjs.c used by js_builtin_number.c */
JSValue js_global_isNaN(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv);
JSValue js_global_isFinite(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv);

/* Iterator */
void js_iterator_concat_finalizer(JSRuntime *rt, JSValue val);
void js_iterator_concat_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val);
void js_iterator_helper_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_iterator_wrap_finalizer(JSRuntime *rt, JSValue val);
void js_iterator_wrap_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
int js_iterator_init(JSContext *ctx);

/* Promise & Async */
int JS_SetConstructor2(JSContext *ctx, JSValueConst func_obj, JSValueConst proto,
                       int proto_flags, int ctor_flags);
JSValue JS_NewPromiseCapability(JSContext *ctx, JSValue *resolving_funcs);
JSValue js_promise_resolve(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv, int magic);
JSValue js_promise_then(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv);
__exception int perform_promise_then(JSContext *ctx,
                                     JSValueConst promise,
                                     JSValueConst *resolve_reject,
                                     JSValueConst *cap_resolving_funcs);

/* Functions in quickjs.c used by js_builtin_promise.c */
JSValue js_function_constructor(JSContext *ctx, JSValueConst new_target,
                                int argc, JSValueConst *argv, int magic);
void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val);
void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_async_function_call(JSContext *ctx, JSValueConst func_obj,
                              JSValueConst this_obj, int argc, JSValueConst *argv,
                              int flags);
void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val);
void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_async_function_resolve_call(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj, int argc, JSValueConst *argv,
                                      int flags);
void js_async_generator_finalizer(JSRuntime *rt, JSValue obj);
void js_async_generator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_async_generator_next(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int magic);
JSValue js_async_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                        JSValueConst this_obj, int argc, JSValueConst *argv,
                                        int flags);
JSValue js_aggregate_error_constructor(JSContext *ctx, JSValueConst errors);
JSValue JS_CreateAsyncFromSyncIterator(JSContext *ctx, JSValueConst sync_iter);

/* TypedArray / ArrayBuffer builtins */
void js_array_buffer_finalizer(JSRuntime *rt, JSValue val);
void js_typed_array_finalizer(JSRuntime *rt, JSValue val);
void js_typed_array_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);

JSValue js_array_buffer_constructor3(JSContext *ctx,
                                    JSValueConst new_target,
                                    uint64_t len, uint64_t *max_len,
                                    JSClassID class_id,
                                    uint8_t *buf,
                                    JSFreeArrayBufferDataFunc *free_func,
                                    void *opaque, BOOL alloc_flag);
void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);
JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);
BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf);
JSValue js_typed_array_constructor(JSContext *ctx,
                                  JSValueConst this_val,
                                  int argc, JSValueConst *argv,
                                  int classid);
BOOL typed_array_is_oob(JSObject *p);
int js_typed_array_get_length_unsafe(JSContext *ctx, JSValueConst obj);
JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);
JSValue JS_ThrowTypeErrorArrayBufferOOB(JSContext *ctx);
JSValue js_typed_array___speciesCreate(JSContext *ctx,
                                      JSValueConst this_val,
                                      int argc, JSValueConst *argv);
JSValue js_create_array_iterator(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic);


/* Shared Array and TypedArray helpers */
#define special_every        0
#define special_some         1
#define special_forEach      2
#define special_map          3
#define special_filter       4
#define special_TA           8

#define special_reduce       0
#define special_reduceRight  1

enum {
    ArrayFind,
    ArrayFindIndex,
    ArrayFindLast,
    ArrayFindLastIndex,
};

JSValue js_array_every(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv, int special);
JSValue js_array_reduce(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv, int special);

/* String, RegExp & Symbol */
extern const JSClassExoticMethods js_string_exotic_methods;
void js_regexp_finalizer(JSRuntime *rt, JSValue val);
void js_regexp_string_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_regexp_string_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue JS_NewRegexp(JSContext *ctx, JSValue pattern, JSValue bc);
int JS_AddIntrinsicString(JSContext *ctx);
int JS_AddIntrinsicSymbol(JSContext *ctx);

/* Array */
void js_array_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_array_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue js_array_iterator_next(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv,
                               BOOL *pdone, int magic);
int js_array_init(JSContext *ctx);
int js_array_iterator_init(JSContext *ctx);

/* Core builtins (Object, Function, Error, Boolean) */
JSValue js_object_seal(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv, int freeze_flag);
int js_object_init_proto(JSContext *ctx);
int js_function_init_proto(JSContext *ctx);
int js_error_init(JSContext *ctx);
int js_object_init_ctor(JSContext *ctx);
int js_function_init_ctor(JSContext *ctx);
int js_boolean_init(JSContext *ctx);

#endif /* QUICKJS_BUILTINS_JS_BUILTINS_H */
