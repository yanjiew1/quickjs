#ifndef QUICKJS_BUILTIN_H
#define QUICKJS_BUILTIN_H

#include "cutils.h"
#include "quickjs/def.h"
#include "quickjs/atom.h"
#include "quickjs/value.h"
#include "quickjs/shape.h"
#include "quickjs/object.h"
#include "quickjs/module.h"
#include "quickjs/runtime.h"
#include "quickjs/function.h"
#include "quickjs/string.h"
#include "quickjs/compiler.h"
#include "quickjs/conversion.h"
#include "quickjs/array.h"
#include "quickjs/bigint.h"

/* Shared builtin flags */
#define JS_NEW_CTOR_NO_GLOBAL   (1 << 0) /* don't create a global binding */
#define JS_NEW_CTOR_PROTO_CLASS (1 << 1) /* the prototype class is 'class_id' instead of JS_CLASS_OBJECT */
#define JS_NEW_CTOR_PROTO_EXIST (1 << 2) /* the prototype is already defined */
#define JS_NEW_CTOR_READONLY    (1 << 3) /* read-only constructor field */

/* Shared builtin helper functions */
int check_function(JSContext *ctx, JSValueConst obj);
int check_exception_free(JSContext *ctx, JSValue obj);
JSAtom find_atom(JSContext *ctx, const char *name);
JSValue JS_NewObjectProtoList(JSContext *ctx, JSValueConst proto,
                             const JSCFunctionListEntry *fields, int n_fields);
int JS_SetConstructor2(JSContext *ctx, JSValueConst func_obj, JSValueConst proto,
                       int proto_flags, int ctor_flags);
JSValue JS_NewCConstructor(JSContext *ctx, int class_id, const char *name,
                           JSCFunction *func, int length, JSCFunctionEnum cproto, int magic,
                           JSValueConst parent_ctor,
                           const JSCFunctionListEntry *ctor_fields, int n_ctor_fields,
                           const JSCFunctionListEntry *proto_fields, int n_proto_fields,
                           int flags);
JSValue js_array_includes(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv);
JSValue js_array_push(JSContext *ctx, JSValueConst this_val,
                      int argc, JSValueConst *argv, int unshift);
JSValue js_array_pop(JSContext *ctx, JSValueConst this_val,
                     int argc, JSValueConst *argv, int shift);
JSValue js_object_keys(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv, int kind);
JSValue js_create_array_iterator(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic);
JSValue JS_SpeciesConstructor(JSContext *ctx, JSValueConst obj,
                              JSValueConst defaultConstructor);
JSValue js_get_this(JSContext *ctx, JSValueConst this_val);
JSValue JS_NewRegexp(JSContext *ctx, JSValue pattern, JSValue bc);
JSValue js_compile_regexp(JSContext *ctx, JSValueConst pattern, JSValueConst flags);
int js_is_regexp(JSContext *ctx, JSValueConst obj);
void js_regexp_finalizer(JSRuntime *rt, JSValue val);
void js_regexp_string_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_regexp_string_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
JSValue get_date_string(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv, int magic);
void js_map_finalizer(JSRuntime *rt, JSValue val);
void js_map_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_map_iterator_finalizer(JSRuntime *rt, JSValue val);
void js_map_iterator_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void map_delete_weakrefs(JSRuntime *rt, JSWeakRefHeader *wh);
JSValue js_object_groupBy(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv, int is_map);
BOOL js_weakref_is_target(JSValueConst val);
BOOL js_weakref_is_live(JSValueConst val);
void js_weakref_free(JSRuntime *rt, JSValue val);
JSValue js_weakref_new(JSContext *ctx, JSValueConst val);
void weakref_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
void finrec_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
JSValue *build_arg_list(JSContext *ctx, uint32_t *plen, JSValueConst array_arg);
void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);
JSValue js_object_defineProperty(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int magic);
JSValue js_object_getOwnPropertyDescriptor(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv, int magic);
JSValue js_object_getPrototypeOf(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int magic);
JSValue js_object_isExtensible(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int reflect);
JSValue js_object_preventExtensions(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv, int reflect);
JSValue js_string_constructor(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv);
JSValue js_aggregate_error_constructor(JSContext *ctx, JSValueConst errors);
JSValue JS_CreateAsyncFromSyncIterator(JSContext *ctx, JSValueConst sync_iter);
JSValue js_promise_resolve(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
JSValue js_promise_then(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
__exception int perform_promise_then(JSContext *ctx,
                                    JSValueConst promise,
                                    JSValueConst *resolve_reject,
                                    JSValueConst *cap_resolving_funcs);
JSValue js_iterator_proto_iterator(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv);
JSValue js_array_from_iterator(JSContext *ctx, uint32_t *plen, JSValueConst items, JSValueConst iter);
JSValue js_typed_array___speciesCreate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue JS_ThrowTypeErrorRevokedProxy(JSContext *ctx);
int js_resolve_proxy(JSContext *ctx, JSValueConst *pval, BOOL throw_exception);
int JS_AddIntrinsicReflect(JSContext *ctx);
int JS_AddIntrinsicSymbol(JSContext *ctx);
int JS_AddIntrinsicNumber(JSContext *ctx);
int JS_AddIntrinsicMath(JSContext *ctx);

JSValue js_object_constructor(JSContext *ctx, JSValueConst new_target,
                              int argc, JSValueConst *argv);
JSValue js_object_seal(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv, int magic);
extern const JSCFunctionListEntry js_object_funcs[23];
extern const JSCFunctionListEntry js_object_proto_funcs[11];

JSValue js_function_proto(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv);
extern const JSCFunctionListEntry js_function_proto_funcs[8];

JSValue js_object_toString(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv);
JSValue js_array_constructor(JSContext *ctx, JSValueConst new_target,
                             int argc, JSValueConst *argv);
JSValue js_iterator_constructor(JSContext *ctx, JSValueConst new_target,
                                int argc, JSValueConst *argv);
JSValue js_iterator_constructor_getset(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int magic,
                                      JSValue *func_data);
void js_iterator_wrap_finalizer(JSRuntime *rt, JSValue val);
void js_iterator_wrap_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_iterator_concat_finalizer(JSRuntime *rt, JSValue val);
void js_iterator_concat_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val);
void js_iterator_helper_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);

extern const JSCFunctionListEntry js_array_funcs[4];
extern const JSCFunctionListEntry js_array_proto_funcs[40];
extern const JSCFunctionListEntry js_array_iterator_proto_funcs[2];
extern const JSCFunctionListEntry js_iterator_funcs[2];
extern const JSCFunctionListEntry js_iterator_proto_funcs[13];
extern const JSCFunctionListEntry js_iterator_wrap_proto_funcs[2];
extern const JSCFunctionListEntry js_iterator_concat_proto_funcs[3];
extern const JSCFunctionListEntry js_iterator_helper_proto_funcs[3];

JSValue js_global_isNaN(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv);
JSValue js_global_isFinite(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv);
JSValue js_parseInt(JSContext *ctx, JSValueConst this_val,
                    int argc, JSValueConst *argv);
JSValue js_parseFloat(JSContext *ctx, JSValueConst this_val,
                      int argc, JSValueConst *argv);
int js_add_global_funcs(JSContext *ctx);

/* Intrinsic addition functions */
int JS_AddIntrinsicBasicObjects(JSContext *ctx);
int JS_AddIntrinsicBaseObjects(JSContext *ctx);
int JS_AddIntrinsicDate(JSContext *ctx);
int JS_AddIntrinsicEval(JSContext *ctx);
int JS_AddIntrinsicStringNormalize(JSContext *ctx);
int JS_AddIntrinsicString(JSContext *ctx);
extern const JSClassExoticMethods js_string_exotic_methods;
int JS_AddIntrinsicRegExp(JSContext *ctx);
void JS_AddIntrinsicRegExpCompiler(JSContext *ctx);
int JS_AddIntrinsicJSON(JSContext *ctx);
int JS_AddIntrinsicProxy(JSContext *ctx);
int JS_AddIntrinsicMapSet(JSContext *ctx);
int JS_AddIntrinsicTypedArrays(JSContext *ctx);
int JS_AddIntrinsicPromise(JSContext *ctx);
int JS_AddIntrinsicBigInt(JSContext *ctx);
int JS_AddIntrinsicWeakRef(JSContext *ctx);

#endif /* QUICKJS_BUILTIN_H */

