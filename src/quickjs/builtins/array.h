/*
 * QuickJS internal array builtin interfaces
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
#ifndef QUICKJS_PRIVATE_BUILTIN_ARRAY_H
#define QUICKJS_PRIVATE_BUILTIN_ARRAY_H

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_SpeciesConstructor(JSContext *ctx, JSValueConst obj,
                                     JSValueConst defaultConstructor);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_get_this(JSContext *ctx,
                           JSValueConst this_val);

/* Internal implementation detail; not part of the public QuickJS API. */
__exception int js_get_length64(JSContext *ctx, int64_t *pres,
                                       JSValueConst obj);

typedef enum JSIteratorKindEnum {
    JS_ITERATOR_KIND_KEY,
    JS_ITERATOR_KIND_VALUE,
    JS_ITERATOR_KIND_KEY_AND_VALUE,
} JSIteratorKindEnum;

/* Internal implementation detail; not part of the public QuickJS API. */
void js_array_iterator_finalizer(JSRuntime *rt, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_array_iterator_mark(JSRuntime *rt, JSValueConst val,
                                   JS_MarkFunc *mark_func);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_iterator_concat_finalizer(JSRuntime *rt, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_iterator_concat_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_iterator_helper_mark(JSRuntime *rt, JSValueConst val,
                                   JS_MarkFunc *mark_func);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_iterator_wrap_finalizer(JSRuntime *rt, JSValue val);

/* Internal implementation detail; not part of the public QuickJS API. */
void js_iterator_wrap_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func);

/* Internal implementation detail; not part of the public QuickJS API. */
void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue *build_arg_list(JSContext *ctx, uint32_t *plen,
                               JSValueConst array_arg);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_array_iterator_next(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv,
                                      BOOL *pdone, int magic);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_create_array_iterator(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic);

#define special_every    0
#define special_some     1
#define special_forEach  2
#define special_map      3
#define special_filter   4
#define special_TA       8

#define special_reduce       0
#define special_reduceRight  1

enum {
    ArrayFind,
    ArrayFindIndex,
    ArrayFindLast,
    ArrayFindLastIndex,
};

/* Internal implementation details; not part of the public QuickJS API. */
JSValue js_array_every(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv, int special);
JSValue js_array_reduce(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv, int special);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_iterator_proto_iterator(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_array_push(JSContext *ctx, JSValueConst this_val,
                      int argc, JSValueConst *argv, int unshift);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_array_includes(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_array_pop(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv, int shift);

typedef struct JSArrayIteratorData {
    JSValue obj;
    JSIteratorKindEnum kind;
    uint32_t idx;
} JSArrayIteratorData;

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_array_constructor(JSContext *ctx, JSValueConst new_target,
                                    int argc, JSValueConst *argv);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_iterator_constructor(JSContext *ctx, JSValueConst new_target,
                                       int argc, JSValueConst *argv);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue js_iterator_constructor_getset(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              int magic,
                                              JSValue *func_data);

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_array_funcs[4];

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_array_proto_funcs[40];

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_iterator_funcs[2];

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_iterator_proto_funcs[13];

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_iterator_concat_proto_funcs[3];

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_iterator_helper_proto_funcs[3];

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_iterator_wrap_proto_funcs[2];

/* Internal implementation detail; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_array_iterator_proto_funcs[2];

#endif /* QUICKJS_PRIVATE_BUILTIN_ARRAY_H */
