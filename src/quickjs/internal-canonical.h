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
#ifndef QUICKJS_INTERNAL_CANONICAL_H
#define QUICKJS_INTERNAL_CANONICAL_H

/* Temporary broad cross-TU interface for the correctness-first split.
   No function bodies or performance remaps belong in this header. */
#include "internal-allocator.h"
#include "internal-array-algorithm.h"
#include "internal-array.h"
#include "internal-async.h"
#include "internal-base.h"
#include "internal-builtin.h"
#include "internal-collection.h"
#include "internal-config.h"
#include "internal-date.h"
#include "internal-frontend.h"
#include "internal-function.h"
#include "internal-global.h"
#include "internal-iterator.h"
#include "internal-json.h"
#include "internal-module.h"
#include "internal-number.h"
#include "internal-object.h"
#include "internal-opcode.h"
#include "internal-operator.h"
#include "internal-primitive.h"
#include "internal-property.h"
#include "internal-proxy.h"
#include "internal-regexp.h"
#include "internal-runtime.h"
#include "internal-string.h"
#include "internal-typed-array.h"
#include "internal-types.h"

extern QJS_INTERNAL const JSMallocFunctions def_malloc_funcs;
extern QJS_INTERNAL const JSCFunctionListEntry js_array_funcs[4];
extern QJS_INTERNAL const JSCFunctionListEntry js_iterator_wrap_proto_funcs[2];
extern QJS_INTERNAL const JSCFunctionListEntry js_iterator_concat_proto_funcs[3];
extern QJS_INTERNAL const JSCFunctionListEntry js_iterator_funcs[2];
extern QJS_INTERNAL const JSCFunctionListEntry js_iterator_proto_funcs[13];
extern QJS_INTERNAL const JSCFunctionListEntry js_iterator_helper_proto_funcs[3];
extern QJS_INTERNAL const JSCFunctionListEntry js_array_proto_funcs[40];
extern QJS_INTERNAL const JSCFunctionListEntry js_array_iterator_proto_funcs[2];
extern QJS_INTERNAL const JSCFunctionListEntry js_generator_function_proto_funcs[1];
extern QJS_INTERNAL const JSCFunctionListEntry js_generator_proto_funcs[4];
extern QJS_INTERNAL const JSCFunctionListEntry js_global_funcs[15];
extern QJS_INTERNAL const JSCFunctionListEntry js_math_obj[1];
extern QJS_INTERNAL const JSCFunctionListEntry js_number_funcs[14];
extern QJS_INTERNAL const JSCFunctionListEntry js_number_proto_funcs[6];
extern QJS_INTERNAL const JSCFunctionListEntry js_boolean_proto_funcs[2];
extern QJS_INTERNAL const JSClassExoticMethods js_string_exotic_methods;
extern QJS_INTERNAL const JSCFunctionListEntry js_string_funcs[3];
extern QJS_INTERNAL const JSCFunctionListEntry js_string_proto_funcs[50];
extern QJS_INTERNAL const JSCFunctionListEntry js_string_iterator_proto_funcs[2];
extern QJS_INTERNAL const JSCFunctionListEntry js_symbol_proto_funcs[5];
extern QJS_INTERNAL const JSCFunctionListEntry js_symbol_funcs[15];
extern QJS_INTERNAL uint8_t const typed_array_size_log2[JS_TYPED_ARRAY_COUNT];
extern QJS_INTERNAL const JSClassExoticMethods js_module_ns_exotic_methods;
extern QJS_INTERNAL JSClassShortDef const js_std_class_def[50];
extern QJS_INTERNAL const JSClassExoticMethods js_arguments_exotic_methods;
extern QJS_INTERNAL const uint16_t func_kind_to_class_id[4];

QJS_INTERNAL int JS_AddBrand(JSContext *ctx, JSValueConst obj, JSValueConst home_obj);
QJS_INTERNAL int JS_AddIntrinsicBasicObjects(JSContext *ctx);
QJS_INTERNAL int JS_AddIntrinsicBigInt(JSContext *ctx);
QJS_INTERNAL JSAtomKindEnum JS_AtomGetKind(JSContext *ctx, JSAtom v);
QJS_INTERNAL const char *JS_AtomGetStr(JSContext *ctx, char *buf, int buf_size, JSAtom atom);
QJS_INTERNAL const char *JS_AtomGetStrRT(JSRuntime *rt, char *buf, int buf_size,
                                   JSAtom atom);
QJS_INTERNAL BOOL JS_AtomIsArrayIndex(JSContext *ctx, uint32_t *pval, JSAtom atom);
QJS_INTERNAL int JS_AtomIsNumericIndex(JSContext *ctx, JSAtom atom);
QJS_INTERNAL JSValue JS_AtomIsNumericIndex1(JSContext *ctx, JSAtom atom);
QJS_INTERNAL BOOL JS_AtomIsString(JSContext *ctx, JSAtom v);
QJS_INTERNAL BOOL JS_AtomSymbolHasDescription(JSContext *ctx, JSAtom v);
QJS_INTERNAL int JS_AutoInitProperty(JSContext *ctx, JSObject *p, JSAtom prop,
                               JSProperty *pr, JSShapeProperty *prs);
QJS_INTERNAL JSValue JS_CallConstructorInternal(JSContext *ctx,
                                          JSValueConst func_obj,
                                          JSValueConst new_target,
                                          int argc, JSValue *argv, int flags);
QJS_INTERNAL JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                           int argc, JSValueConst *argv);
QJS_INTERNAL JSValue JS_CallInternal(JSContext *caller_ctx, JSValueConst func_obj,
                               JSValueConst this_obj, JSValueConst new_target,
                               int argc, JSValue *argv, int flags);
QJS_INTERNAL int JS_CheckBrand(JSContext *ctx, JSValueConst obj, JSValueConst func);
QJS_INTERNAL int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom prop, int flags);
QJS_INTERNAL JSValue JS_CompactBigInt(JSContext *ctx, JSBigInt *p);
QJS_INTERNAL JSValue JS_ConcatString(JSContext *ctx, JSValue op1, JSValue op2);
QJS_INTERNAL JSValue JS_ConcatString3(JSContext *ctx, const char *str1,
                                JSValue str2, const char *str3);
QJS_INTERNAL BOOL JS_ConcatStringInPlace(JSContext *ctx, JSString *p1, JSValueConst op2);
QJS_INTERNAL __exception int JS_CopyDataProperties(JSContext *ctx,
                                             JSValueConst target,
                                             JSValueConst source,
                                             JSValueConst excluded,
                                             BOOL setprop);
QJS_INTERNAL JSValue JS_CreateAsyncFromSyncIterator(JSContext *ctx,
                                              JSValueConst sync_iter);
QJS_INTERNAL int JS_CreateDataPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                       int64_t idx, JSValue val, int flags);
QJS_INTERNAL int JS_CreateProperty(JSContext *ctx, JSObject *p,
                             JSAtom prop, JSValueConst val,
                             JSValueConst getter, JSValueConst setter,
                             int flags);
QJS_INTERNAL int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj,
                                     JSAtom prop, JSAutoInitIDEnum id,
                                     void *opaque, int flags);
QJS_INTERNAL int JS_DefineObjectName(JSContext *ctx, JSValueConst obj,
                               JSAtom name, int flags);
QJS_INTERNAL int JS_DefineObjectNameComputed(JSContext *ctx, JSValueConst obj,
                                       JSValueConst str, int flags);
QJS_INTERNAL int JS_DefinePrivateField(JSContext *ctx, JSValueConst obj,
                                 JSValueConst name, JSValue val);
QJS_INTERNAL int JS_DeleteGlobalVar(JSContext *ctx, JSAtom prop);
QJS_INTERNAL __maybe_unused void JS_DumpAtoms(JSRuntime *rt);
QJS_INTERNAL __maybe_unused void JS_DumpGCObject(JSRuntime *rt, JSGCObjectHeader *p);
QJS_INTERNAL __maybe_unused void JS_DumpObjectHeader(JSRuntime *rt);
QJS_INTERNAL __maybe_unused void JS_DumpShapes(JSRuntime *rt);
QJS_INTERNAL __maybe_unused void JS_DumpString(JSRuntime *rt, const JSString *p);
QJS_INTERNAL __maybe_unused void JS_DumpValue(JSContext *ctx, const char *str, JSValueConst val);
QJS_INTERNAL JSAtom JS_DupAtomRT(JSRuntime *rt, JSAtom v);
QJS_INTERNAL int JS_EnqueueJob2(JSContext *ctx, JSJobFunc *job_func,
                          int argc, JSValueConst *argv, BOOL no_exception);
QJS_INTERNAL JSValue JS_EvalObject(JSContext *ctx, JSValueConst this_obj,
                             JSValueConst val, int flags, int scope_idx);
QJS_INTERNAL void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p);
QJS_INTERNAL JSValueConst JS_GetActiveFunction(JSContext *ctx);
QJS_INTERNAL JSFunctionBytecode *JS_GetFunctionBytecode(JSValueConst val);
QJS_INTERNAL JSContext *JS_GetFunctionRealm(JSContext *ctx, JSValueConst func_obj);
QJS_INTERNAL int JS_GetGlobalVarRef(JSContext *ctx, JSAtom prop, JSValue *sp);
QJS_INTERNAL JSValue JS_GetIterator(JSContext *ctx, JSValueConst obj, BOOL is_async);
QJS_INTERNAL JSValue JS_GetIterator2(JSContext *ctx, JSValueConst obj,
                               JSValueConst method);
QJS_INTERNAL int JS_GetOwnPropertyInternal(JSContext *ctx, JSPropertyDescriptor *desc,
                                     JSObject *p, JSAtom prop);
QJS_INTERNAL int __exception JS_GetOwnPropertyNamesInternal(JSContext *ctx,
                                                      JSPropertyEnum **ptab,
                                                      uint32_t *plen,
                                                      JSObject *p, int flags);
QJS_INTERNAL JSValue JS_GetPrivateField(JSContext *ctx, JSValueConst obj,
                                  JSValueConst name);
QJS_INTERNAL JSValue JS_GetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx);
QJS_INTERNAL JSValue JS_GetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                                   JSValue prop);
QJS_INTERNAL JSValue JS_GetPrototypeFree(JSContext *ctx, JSValue obj);
QJS_INTERNAL int JS_InitAtoms(JSRuntime *rt);
QJS_INTERNAL JSValue JS_InstantiateFunctionListItem2(JSContext *ctx, JSObject *p,
                                               JSAtom atom, void *opaque);
QJS_INTERNAL JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                             int argc, JSValueConst *argv);
QJS_INTERNAL BOOL JS_IsEmptyString(JSValueConst v);
QJS_INTERNAL int JS_IteratorClose(JSContext *ctx, JSValueConst enum_obj,
                            BOOL is_exception_pending);
QJS_INTERNAL JSValue JS_IteratorGetCompleteValue(JSContext *ctx, JSValueConst obj,
                                           BOOL *pdone);
QJS_INTERNAL JSValue JS_IteratorNext(JSContext *ctx, JSValueConst enum_obj,
                               JSValueConst method,
                               int argc, JSValueConst *argv, BOOL *pdone);
QJS_INTERNAL JSValue JS_IteratorNext2(JSContext *ctx, JSValueConst enum_obj,
                                JSValueConst method,
                                int argc, JSValueConst *argv, int *pdone);
QJS_INTERNAL void JS_MarkContext(JSRuntime *rt, JSContext *ctx,
                           JS_MarkFunc *mark_func);
QJS_INTERNAL JSAtom JS_NewAtomInt64(JSContext *ctx, int64_t n);
QJS_INTERNAL JSAtom JS_NewAtomStr(JSContext *ctx, JSString *p);
QJS_INTERNAL JSValue JS_NewCConstructor(JSContext *ctx, int class_id, const char *name,
                                  JSCFunction *func, int length, JSCFunctionEnum cproto, int magic,
                                  JSValueConst parent_ctor,
                                  const JSCFunctionListEntry *ctor_fields, int n_ctor_fields,
                                  const JSCFunctionListEntry *proto_fields, int n_proto_fields,
                                  int flags);
QJS_INTERNAL JSValue JS_NewCFunction3(JSContext *ctx, JSCFunction *func,
                                const char *name,
                                int length, JSCFunctionEnum cproto, int magic,
                                JSValueConst proto_val, int n_fields);
QJS_INTERNAL JSValue JS_NewModuleValue(JSContext *ctx, JSModuleDef *m);
QJS_INTERNAL JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *sh, JSClassID class_id,
                                     JSProperty *props);
QJS_INTERNAL JSValue JS_NewObjectProtoClassAlloc(JSContext *ctx, JSValueConst proto_val,
                                           JSClassID class_id, int n_alloc_props);
QJS_INTERNAL JSValue JS_NewObjectProtoList(JSContext *ctx, JSValueConst proto,
                                     const JSCFunctionListEntry *fields, int n_fields);
QJS_INTERNAL JSValue JS_NewRegexp(JSContext *ctx, JSValue pattern, JSValue bc);
QJS_INTERNAL JSValue JS_NewSymbol(JSContext *ctx, JSString *p, int atom_type);
QJS_INTERNAL JSValue JS_NewSymbolFromAtom(JSContext *ctx, JSAtom descr,
                                    int atom_type);
QJS_INTERNAL int JS_NumberIsInteger(JSContext *ctx, JSValueConst val);
QJS_INTERNAL BOOL JS_NumberIsNegativeOrMinusZero(JSContext *ctx, JSValueConst val);
QJS_INTERNAL int JS_OrdinaryIsInstanceOf(JSContext *ctx, JSValueConst val,
                                   JSValueConst obj);
QJS_INTERNAL void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects);
QJS_INTERNAL int JS_SetConstructor2(JSContext *ctx,
                              JSValueConst func_obj,
                              JSValueConst proto,
                              int proto_flags, int ctor_flags);
QJS_INTERNAL void JS_SetImmutablePrototype(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL int JS_SetObjectData(JSContext *ctx, JSValueConst obj, JSValue val);
QJS_INTERNAL int JS_SetPrivateField(JSContext *ctx, JSValueConst obj,
                              JSValueConst name, JSValue val);
QJS_INTERNAL int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                               JSValue prop, JSValue val, int flags);
QJS_INTERNAL int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj,
                                   JSValueConst proto_val,
                                   BOOL throw_flag);
QJS_INTERNAL JSValue JS_SpeciesConstructor(JSContext *ctx, JSValueConst obj,
                                     JSValueConst defaultConstructor);
QJS_INTERNAL JSValue JS_StringToBigInt(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_StringToBigIntErr(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ThrowError(JSContext *ctx, JSErrorEnum error_num,
                             const char *fmt, va_list ap);
QJS_INTERNAL JSValue JS_ThrowError2(JSContext *ctx, JSErrorEnum error_num,
                              const char *fmt, va_list ap, BOOL add_backtrace);
QJS_INTERNAL void JS_ThrowInterrupted(JSContext *ctx);
QJS_INTERNAL JSValue JS_ThrowReferenceErrorNotDefined(JSContext *ctx, JSAtom name);
QJS_INTERNAL JSValue JS_ThrowReferenceErrorUninitialized(JSContext *ctx, JSAtom name);
QJS_INTERNAL JSValue JS_ThrowReferenceErrorUninitialized2(JSContext *ctx,
                                                    JSFunctionBytecode *b,
                                                    int idx, BOOL is_ref);
QJS_INTERNAL JSValue JS_ThrowStackOverflow(JSContext *ctx);
QJS_INTERNAL JSValue JS_ThrowSyntaxErrorVarRedeclaration(JSContext *ctx, JSAtom prop);
QJS_INTERNAL JSValue JS_ThrowTypeErrorArrayBufferOOB(JSContext *ctx);
QJS_INTERNAL JSValue JS_ThrowTypeErrorDetachedArrayBuffer(JSContext *ctx);
QJS_INTERNAL JSValue JS_ThrowTypeErrorInvalidClass(JSContext *ctx, int class_id);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx,
                                                JSValueConst func_obj);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotASymbol(JSContext *ctx);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);
QJS_INTERNAL int __attribute__((format(printf, 3, 4))) JS_ThrowTypeErrorOrFalse(JSContext *ctx, int flags, const char *fmt, ...);
QJS_INTERNAL int JS_ThrowTypeErrorReadOnly(JSContext *ctx, int flags, JSAtom atom);
QJS_INTERNAL JSValue JS_ThrowTypeErrorRevokedProxy(JSContext *ctx);
QJS_INTERNAL __exception int JS_ToArrayLengthFree(JSContext *ctx, uint32_t *plen,
                                            JSValue val, BOOL is_array_ctor);
QJS_INTERNAL JSValue JS_ToBigInt(JSContext *ctx, JSValueConst val);
QJS_INTERNAL int JS_ToBigInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
QJS_INTERNAL JSValue JS_ToBigIntFree(JSContext *ctx, JSValue val);
QJS_INTERNAL int JS_ToBoolFree(JSContext *ctx, JSValue val);
QJS_INTERNAL int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val);
QJS_INTERNAL int JS_ToInt32Free(JSContext *ctx, int32_t *pres, JSValue val);
QJS_INTERNAL int JS_ToInt64Free(JSContext *ctx, int64_t *pres, JSValue val);
QJS_INTERNAL __maybe_unused JSValue JS_ToIntegerFree(JSContext *ctx, JSValue val);
QJS_INTERNAL __exception int JS_ToLengthFree(JSContext *ctx, int64_t *plen,
                                       JSValue val);
QJS_INTERNAL JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ToNumber(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue JS_ToNumberFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ToNumeric(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue JS_ToObject(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);
QJS_INTERNAL JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint);
QJS_INTERNAL JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue JS_ToStringFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ToStringInternal(JSContext *ctx, JSValueConst val, BOOL is_ToPropertyKey);
QJS_INTERNAL int JS_ToUint32Free(JSContext *ctx, uint32_t *pres, JSValue val);
QJS_INTERNAL int JS_ToUint8ClampFree(JSContext *ctx, int32_t *pres, JSValue val);
QJS_INTERNAL int JS_TryGetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, JSValue *pval);
QJS_INTERNAL JSAtom __JS_AtomFromUInt32(uint32_t v);
QJS_INTERNAL BOOL __JS_AtomIsConst(JSAtom v);
QJS_INTERNAL BOOL __JS_AtomIsTaggedInt(JSAtom v);
QJS_INTERNAL uint32_t __JS_AtomToUInt32(JSAtom atom);
QJS_INTERNAL JSValue __JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                                 const char *input, size_t input_len,
                                 const char *filename, int flags, int scope_idx);
QJS_INTERNAL JSAtom __JS_FindAtom(JSRuntime *rt, const char *str, size_t len,
                            int atom_type);
QJS_INTERNAL JSAtom __JS_NewAtomInit(JSRuntime *rt, const char *str, int len,
                               int atom_type);
QJS_INTERNAL JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowSyntaxErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...);
QJS_INTERNAL JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowTypeErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...);
QJS_INTERNAL __exception int __JS_ToFloat64Free(JSContext *ctx, double *pres,
                                          JSValue val);
QJS_INTERNAL void __async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
QJS_INTERNAL void __js_free(JSMallocContext *s, void *ptr);
QJS_INTERNAL void *__js_malloc(JSMallocContext *s, size_t size);
QJS_INTERNAL no_inline __exception int __js_poll_interrupts(JSContext *ctx);
QJS_INTERNAL void *__js_realloc(JSMallocContext *s, void *ptr, size_t size);
QJS_INTERNAL JSExportEntry *add_export_entry2(JSContext *ctx,
                                        JSParseState *s, JSModuleDef *m,
                                       JSAtom local_name, JSAtom export_name,
                                       JSExportTypeEnum export_type);
QJS_INTERNAL void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type);
QJS_INTERNAL JSProperty *add_property(JSContext *ctx,
                                JSObject *p, JSAtom prop, int prop_flags);
QJS_INTERNAL int add_req_module_entry(JSContext *ctx, JSModuleDef *m,
                                JSAtom module_name);
QJS_INTERNAL int add_shape_property(JSContext *ctx, JSShape **psh,
                              JSObject *p, JSAtom atom, int prop_flags);
QJS_INTERNAL int add_star_export_entry(JSContext *ctx, JSModuleDef *m,
                                 int req_module_idx);
QJS_INTERNAL BOOL array_buffer_is_resizable(const JSArrayBuffer *abuf);
QJS_INTERNAL void async_func_free(JSRuntime *rt, JSAsyncFunctionState *s);
QJS_INTERNAL BOOL atom_is_free(const JSAtomStruct *p);
QJS_INTERNAL JSValue *build_arg_list(JSContext *ctx, uint32_t *plen,
                               JSValueConst array_arg);
QJS_INTERNAL void build_backtrace(JSContext *ctx, JSValueConst error_obj,
                            const char *filename, int line_num, int col_num,
                            int backtrace_flags);
QJS_INTERNAL JSValue build_for_in_iterator(JSContext *ctx, JSValue obj);
QJS_INTERNAL BOOL can_extend_fast_array(JSObject *p);
QJS_INTERNAL BOOL check_define_prop_flags(int prop_flags, int flags);
QJS_INTERNAL int check_exception_free(JSContext *ctx, JSValue obj);
QJS_INTERNAL int check_function(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL void close_lexical_var(JSContext *ctx, JSFunctionBytecode *b,
                              JSStackFrame *sf, int var_idx);
QJS_INTERNAL void close_var_refs(JSRuntime *rt, JSFunctionBytecode *b, JSStackFrame *sf);
QJS_INTERNAL int compact_properties(JSContext *ctx, JSObject *p);
QJS_INTERNAL no_inline __exception int convert_fast_array_to_array(JSContext *ctx,
                                                             JSObject *p);
QJS_INTERNAL void dbuf_put_leb128(DynBuf *s, uint32_t v);
QJS_INTERNAL void dbuf_put_sleb128(DynBuf *s, int32_t v1);
QJS_INTERNAL int delete_property(JSContext *ctx, JSObject *p, JSAtom atom);
QJS_INTERNAL int expand_fast_array(JSContext *ctx, JSObject *p, uint32_t new_len);
QJS_INTERNAL JSExportEntry *find_export_entry(JSContext *ctx, JSModuleDef *m,
                                        JSAtom export_name);
QJS_INTERNAL JSShape *find_hashed_shape_prop(JSRuntime *rt, JSShape *sh,
                                       JSAtom atom, int prop_flags);
QJS_INTERNAL JSShape *find_hashed_shape_proto(JSRuntime *rt, JSObject *proto);
QJS_INTERNAL int find_line_num(JSContext *ctx, JSFunctionBytecode *b,
                         uint32_t pc_value, int *pcol_num);
QJS_INTERNAL JSShapeProperty *find_own_property(JSProperty **ppr,
                                                       JSObject *p,
                                                       JSAtom atom);
QJS_INTERNAL JSShapeProperty *find_own_property1(JSObject *p,
                                                        JSAtom atom);
QJS_INTERNAL void finrec_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);
QJS_INTERNAL void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);
QJS_INTERNAL void free_function_bytecode(JSRuntime *rt, JSFunctionBytecode *b);
QJS_INTERNAL void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
QJS_INTERNAL void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
QJS_INTERNAL void free_zero_refcount(JSRuntime *rt);
QJS_INTERNAL void gc_decref(JSRuntime *rt);
QJS_INTERNAL JSValue get_date_string(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int magic);
QJS_INTERNAL int get_leb128(uint32_t *pval, const uint8_t *buf,
                      const uint8_t *buf_end);
QJS_INTERNAL JSObject *get_proto_obj(JSValueConst proto_val);
QJS_INTERNAL JSShapeProperty *get_shape_prop(JSShape *sh);
QJS_INTERNAL size_t get_shape_size(size_t hash_size, size_t prop_size);
QJS_INTERNAL int get_sleb128(int32_t *pval, const uint8_t *buf,
                       const uint8_t *buf_end);
QJS_INTERNAL JSVarRef *get_var_ref(JSContext *ctx, JSStackFrame *sf, int var_idx,
                             BOOL is_arg);
QJS_INTERNAL uint32_t hash_string(const JSString *str, uint32_t h);
QJS_INTERNAL uint32_t hash_string_rope(JSValueConst val, uint32_t h);
QJS_INTERNAL int init_class_range(JSRuntime *rt, JSClassShortDef const *tab,
                            int start, int count);
QJS_INTERNAL int init_shape_hash(JSRuntime *rt);
QJS_INTERNAL BOOL is_backtrace_needed(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL BOOL is_be(void);
QJS_INTERNAL int is_digit(int c);
QJS_INTERNAL BOOL is_safe_integer(double d);
QJS_INTERNAL BOOL is_strict_mode(JSContext *ctx);
QJS_INTERNAL no_inline __exception int js_add_slow(JSContext *ctx, JSValue *sp);
QJS_INTERNAL JSValue js_aggregate_error_constructor(JSContext *ctx,
                                              JSValueConst errors);
QJS_INTERNAL JSString *js_alloc_string(JSContext *ctx, int max_len, int is_wide_char);
QJS_INTERNAL JSString *js_alloc_string_rt(JSRuntime *rt, int max_len, int is_wide_char);
QJS_INTERNAL JSValue js_allocate_fast_array(JSContext *ctx, int64_t len);
QJS_INTERNAL JSValue js_array_buffer_constructor3(JSContext *ctx,
                                            JSValueConst new_target,
                                            uint64_t len, uint64_t *max_len,
                                            JSClassID class_id,
                                            uint8_t *buf,
                                            JSFreeArrayBufferDataFunc *free_func,
                                            void *opaque, BOOL alloc_flag);
QJS_INTERNAL void js_array_buffer_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr);
QJS_INTERNAL JSValue js_array_constructor(JSContext *ctx, JSValueConst new_target,
                                    int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_array_every(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int special);
QJS_INTERNAL void js_array_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL JSValue js_array_includes(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv);
QJS_INTERNAL void js_array_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_array_iterator_mark(JSRuntime *rt, JSValueConst val,
                                   JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_array_iterator_next(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv,
                                      BOOL *pdone, int magic);
QJS_INTERNAL void js_array_mark(JSRuntime *rt, JSValueConst val,
                          JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_array_pop(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv, int shift);
QJS_INTERNAL JSValue js_array_push(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int unshift);
QJS_INTERNAL JSValue js_array_reduce(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv, int special);
QJS_INTERNAL JSValue js_async_function_call(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags);
QJS_INTERNAL JSValue js_async_function_resolve_call(JSContext *ctx,
                                              JSValueConst func_obj,
                                              JSValueConst this_obj,
                                              int argc, JSValueConst *argv,
                                              int flags);
QJS_INTERNAL void js_async_function_resolve_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_async_function_resolve_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func);
QJS_INTERNAL void js_async_generator_finalizer(JSRuntime *rt, JSValue obj);
QJS_INTERNAL JSValue js_async_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                                JSValueConst this_obj,
                                                int argc, JSValueConst *argv,
                                                int flags);
QJS_INTERNAL void js_async_generator_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_async_generator_next(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv,
                                       int magic);
QJS_INTERNAL JSValue js_atof(JSContext *ctx, const char *str, const char **pp,
                       int radix, int flags);
QJS_INTERNAL JSAtom js_atom_concat_num(JSContext *ctx, JSAtom name, uint32_t n);
QJS_INTERNAL JSAtom js_atom_concat_str(JSContext *ctx, JSAtom name, const char *str1);
QJS_INTERNAL void js_autoinit_free(JSRuntime *rt, JSProperty *pr);
QJS_INTERNAL JSAutoInitIDEnum js_autoinit_get_id(JSProperty *pr);
QJS_INTERNAL JSContext *js_autoinit_get_realm(JSProperty *pr);
QJS_INTERNAL JSBigInt *js_bigint_add(JSContext *ctx, const JSBigInt *a,
                               const JSBigInt *b, int b_neg);
QJS_INTERNAL int js_bigint_cmp(JSContext *ctx, const JSBigInt *a,
                         const JSBigInt *b);
QJS_INTERNAL JSBigInt *js_bigint_divrem(JSContext *ctx, const JSBigInt *a,
                                  const JSBigInt *b, BOOL is_rem);
QJS_INTERNAL JSBigInt *js_bigint_from_float64(JSContext *ctx, int *pres, double a1);
QJS_INTERNAL JSBigInt *js_bigint_from_string(JSContext *ctx,
                                       const char *str, int radix);
QJS_INTERNAL js_slimb_t js_bigint_get_si_sat(const JSBigInt *a);
QJS_INTERNAL JSBigInt *js_bigint_logic(JSContext *ctx, const JSBigInt *a,
                                 const JSBigInt *b, OPCodeEnum op);
QJS_INTERNAL JSBigInt *js_bigint_mul(JSContext *ctx, const JSBigInt *a,
                               const JSBigInt *b);
QJS_INTERNAL JSBigInt *js_bigint_neg(JSContext *ctx, const JSBigInt *a);
QJS_INTERNAL JSBigInt *js_bigint_new(JSContext *ctx, int len);
QJS_INTERNAL JSBigInt *js_bigint_new_di(JSContext *ctx, js_sdlimb_t a);
QJS_INTERNAL JSBigInt *js_bigint_normalize(JSContext *ctx, JSBigInt *a);
QJS_INTERNAL JSBigInt *js_bigint_not(JSContext *ctx, const JSBigInt *a);
QJS_INTERNAL JSBigInt *js_bigint_pow(JSContext *ctx, const JSBigInt *a, JSBigInt *b);
QJS_INTERNAL JSBigInt *js_bigint_set_short(JSBigIntBuf *buf, JSValueConst val);
QJS_INTERNAL JSBigInt *js_bigint_set_si(JSBigIntBuf *buf, js_slimb_t a);
QJS_INTERNAL JSBigInt *js_bigint_shl(JSContext *ctx, const JSBigInt *a,
                               unsigned int shift1);
QJS_INTERNAL JSBigInt *js_bigint_shr(JSContext *ctx, const JSBigInt *a,
                               unsigned int shift1);
QJS_INTERNAL int js_bigint_sign(const JSBigInt *a);
QJS_INTERNAL double js_bigint_to_float64(JSContext *ctx, const JSBigInt *a);
QJS_INTERNAL JSValue js_bigint_to_string(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue js_bigint_to_string1(JSContext *ctx, JSValueConst val, int radix);
QJS_INTERNAL no_inline __exception int js_binary_arith_slow(JSContext *ctx, JSValue *sp,
                                                      OPCodeEnum op);
QJS_INTERNAL no_inline __exception int js_binary_logic_slow(JSContext *ctx,
                                                      JSValue *sp,
                                                      OPCodeEnum op);
QJS_INTERNAL JSValue js_boolean_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);
QJS_INTERNAL void js_bound_function_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_bound_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_build_arguments(JSContext *ctx, int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_build_mapped_arguments(JSContext *ctx, int argc,
                                         JSValueConst *argv,
                                         JSStackFrame *sf, int arg_count);
QJS_INTERNAL void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val,
                                      JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv, int flags);
QJS_INTERNAL void js_c_function_data_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
QJS_INTERNAL void js_c_function_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_c_function_mark(JSRuntime *rt, JSValueConst val,
                               JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst this_obj,
                                      int argc, JSValueConst *argv, int flags);
QJS_INTERNAL JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj,
                                  JSValueConst this_obj,
                                  int argc, JSValueConst *argv, int flags);
QJS_INTERNAL BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size);
QJS_INTERNAL BOOL js_class_has_bytecode(JSClassID class_id);
QJS_INTERNAL JSShape *js_clone_shape(JSContext *ctx, JSShape *sh1);
QJS_INTERNAL JSValue js_closure(JSContext *ctx, JSValue bfunc,
                          JSVarRef **cur_var_refs,
                          JSStackFrame *sf, BOOL is_eval);
QJS_INTERNAL JSValue js_closure2(JSContext *ctx, JSValue func_obj,
                           JSFunctionBytecode *b,
                           JSVarRef **cur_var_refs,
                           JSStackFrame *sf,
                           BOOL is_eval, JSModuleDef *m);
QJS_INTERNAL int js_compare_bigint(JSContext *ctx, OPCodeEnum op,
                             JSValue op1, JSValue op2);
QJS_INTERNAL JSValue js_create_array(JSContext *ctx, int len, JSValueConst *tab);
QJS_INTERNAL JSValue js_create_array_free(JSContext *ctx, int len, JSValue *tab);
QJS_INTERNAL JSValue js_create_array_iterator(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                   int class_id);
QJS_INTERNAL JSValue js_create_iterator_result(JSContext *ctx,
                                         JSValue val,
                                         BOOL done);
QJS_INTERNAL int js_create_module_function(JSContext *ctx, JSModuleDef *m);
QJS_INTERNAL JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical);
QJS_INTERNAL void js_dbuf_bytecode_init(JSContext *ctx, DynBuf *s);
QJS_INTERNAL void js_dbuf_init(JSContext *ctx, DynBuf *s);
QJS_INTERNAL JSValue js_dtoa2(JSContext *ctx,
                        double d, int radix, int n_digits, int flags);
QJS_INTERNAL void js_dump_value_write(void *opaque, const char *buf, size_t len);
QJS_INTERNAL JSShape *js_dup_shape(JSShape *sh);
QJS_INTERNAL JSValue js_dynamic_import(JSContext *ctx, JSValueConst specifier, JSValueConst options);
QJS_INTERNAL no_inline __exception int js_eq_slow(JSContext *ctx, JSValue *sp,
                                            BOOL is_neq);
QJS_INTERNAL JSValue js_error_toString(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_evaluate_module(JSContext *ctx, JSModuleDef *m);
QJS_INTERNAL void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL __exception int js_for_in_prepare_prototype_chain_enum(JSContext *ctx,
                                                              JSValueConst enum_obj);
QJS_INTERNAL void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);
QJS_INTERNAL void js_free_module_def(JSRuntime *rt, JSModuleDef *m);
QJS_INTERNAL void js_free_modules(JSContext *ctx, JSFreeModuleEnum flag);
QJS_INTERNAL void js_free_shape(JSRuntime *rt, JSShape *sh);
QJS_INTERNAL void js_free_shape_null(JSRuntime *rt, JSShape *sh);
QJS_INTERNAL void js_free_string(JSRuntime *rt, JSString *str);
QJS_INTERNAL JSValue js_function_apply(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue js_function_constructor(JSContext *ctx, JSValueConst new_target,
                                       int argc, JSValueConst *argv, int magic);
QJS_INTERNAL void js_function_set_properties(JSContext *ctx, JSValueConst func_obj,
                                       JSAtom name, int len);
QJS_INTERNAL void js_generator_finalizer(JSRuntime *rt, JSValue obj);
QJS_INTERNAL JSValue js_generator_function_call(JSContext *ctx, JSValueConst func_obj,
                                          JSValueConst this_obj,
                                          int argc, JSValueConst *argv,
                                          int flags);
QJS_INTERNAL void js_generator_mark(JSRuntime *rt, JSValueConst val,
                              JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_generator_next(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv,
                                 BOOL *pdone, int magic);
QJS_INTERNAL JSArrayBuffer *js_get_array_buffer(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL JSAtom js_get_atom_index(JSRuntime *rt, JSAtomStruct *p);
QJS_INTERNAL BOOL js_get_fast_array(JSContext *ctx, JSValueConst obj,
                              JSValue **arrpp, uint32_t *countp);
QJS_INTERNAL JSValue js_get_function_name(JSContext *ctx, JSAtom name);
QJS_INTERNAL __exception int js_get_length32(JSContext *ctx, uint32_t *pres,
                                       JSValueConst obj);
QJS_INTERNAL __exception int js_get_length64(JSContext *ctx, int64_t *pres,
                                       JSValueConst obj);
QJS_INTERNAL JSValue js_get_this(JSContext *ctx,
                           JSValueConst this_val);
QJS_INTERNAL JSValue js_global_isFinite(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_global_isNaN(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv);
QJS_INTERNAL void js_global_object_finalizer(JSRuntime *rt, JSValue obj);
QJS_INTERNAL JSVarRef *js_global_object_find_uninitialized_var(JSContext *ctx, JSObject *p,
                                                         JSAtom atom, BOOL is_lexical);
QJS_INTERNAL void js_global_object_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func);
QJS_INTERNAL __exception int js_has_unscopable(JSContext *ctx, JSValueConst obj,
                                         JSAtom atom);
QJS_INTERNAL JSValue js_import_meta(JSContext *ctx);
QJS_INTERNAL JSValue js_instantiate_prototype(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);
QJS_INTERNAL int js_is_regexp(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL void js_iterator_concat_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_concat_mark(JSRuntime *rt, JSValueConst val,
                                    JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_iterator_constructor(JSContext *ctx, JSValueConst new_target,
                                       int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_iterator_constructor_getset(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv,
                                              int magic,
                                              JSValue *func_data);
QJS_INTERNAL __exception int js_iterator_get_value_done(JSContext *ctx, JSValue *sp);
QJS_INTERNAL void js_iterator_helper_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_helper_mark(JSRuntime *rt, JSValueConst val,
                                   JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_iterator_proto_iterator(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv);
QJS_INTERNAL void js_iterator_wrap_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_iterator_wrap_mark(JSRuntime *rt, JSValueConst val,
                                  JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_linearize_string_rope(JSContext *ctx, JSValue rope);
QJS_INTERNAL int js_link_module(JSContext *ctx, JSModuleDef *m);
QJS_INTERNAL void js_malloc_init(JSMallocContext *s);
QJS_INTERNAL void js_map_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_map_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_map_iterator_mark(JSRuntime *rt, JSValueConst val,
                                 JS_MarkFunc *mark_func);
QJS_INTERNAL void js_map_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
QJS_INTERNAL void js_mapped_arguments_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_mapped_arguments_mark(JSRuntime *rt, JSValueConst val,
                                     JS_MarkFunc *mark_func);
QJS_INTERNAL void js_mark_module_def(JSRuntime *rt, JSModuleDef *m,
                               JS_MarkFunc *mark_func);
QJS_INTERNAL void js_method_set_home_object(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst home_obj);
QJS_INTERNAL int js_method_set_properties(JSContext *ctx, JSValueConst func_obj,
                                    JSAtom name, int flags, JSValueConst home_obj);
QJS_INTERNAL JSValue js_module_ns_autoinit(JSContext *ctx, JSObject *p, JSAtom atom,
                                     void *opaque);
QJS_INTERNAL JSModuleDef *js_new_module_def(JSContext *ctx, JSAtom name);
QJS_INTERNAL JSShape *js_new_shape(JSContext *ctx, JSObject *proto);
QJS_INTERNAL no_inline JSShape *js_new_shape2(JSContext *ctx, JSObject *proto,
                                        int hash_size, int prop_size);
QJS_INTERNAL JSShape *js_new_shape_nohash(JSContext *ctx, JSObject *proto,
                                           int hash_size, int prop_size);
QJS_INTERNAL JSValue js_new_string16_len(JSContext *ctx, const uint16_t *buf, int len);
QJS_INTERNAL JSValue js_new_string8(JSContext *ctx, const char *buf);
QJS_INTERNAL JSValue js_new_string8_len(JSContext *ctx, const char *buf, int len);
QJS_INTERNAL JSValue js_new_string_char(JSContext *ctx, uint16_t c);
QJS_INTERNAL no_inline int js_not_slow(JSContext *ctx, JSValue *sp);
QJS_INTERNAL JSValue js_number_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);
QJS_INTERNAL int js_obj_to_desc(JSContext *ctx, JSPropertyDescriptor *d,
                          JSValueConst desc);
QJS_INTERNAL JSValue js_object_groupBy(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int is_map);
QJS_INTERNAL JSValue js_object_keys(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int kind);
QJS_INTERNAL JSValue js_object_toString(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv);
QJS_INTERNAL __exception int js_operator_delete(JSContext *ctx, JSValue *sp);
QJS_INTERNAL __exception int js_operator_in(JSContext *ctx, JSValue *sp);
QJS_INTERNAL __exception int js_operator_instanceof(JSContext *ctx, JSValue *sp);
QJS_INTERNAL __exception int js_operator_private_in(JSContext *ctx, JSValue *sp);
QJS_INTERNAL __exception int js_operator_typeof(JSContext *ctx, JSValueConst op1);
QJS_INTERNAL __attribute__((format(printf, 2, 3))) int js_parse_error(JSParseState *s, const char *fmt, ...);
QJS_INTERNAL __exception int js_poll_interrupts(JSContext *ctx);
QJS_INTERNAL __exception int js_post_inc_slow(JSContext *ctx,
                                        JSValue *sp, OPCodeEnum op);
QJS_INTERNAL double js_pow(double a, double b);
QJS_INTERNAL JSValue js_promise_resolve(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue js_promise_then(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv);
QJS_INTERNAL void js_random_init(JSContext *ctx);
QJS_INTERNAL JSMallocBlockHeader *js_rc(void *ptr);
QJS_INTERNAL void js_regexp_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_regexp_string_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_regexp_string_iterator_mark(JSRuntime *rt, JSValueConst val,
                                           JS_MarkFunc *mark_func);
QJS_INTERNAL no_inline int js_relational_slow(JSContext *ctx, JSValue *sp,
                                        OPCodeEnum op);
QJS_INTERNAL int js_resize_array(JSContext *ctx, void **parray, int elem_size,
                                  int *psize, int req_size);
QJS_INTERNAL int js_resolve_module(JSContext *ctx, JSModuleDef *m);
QJS_INTERNAL int js_resolve_proxy(JSContext *ctx, JSValueConst *pval, BOOL throw_exception);
QJS_INTERNAL BOOL js_same_value(JSContext *ctx, JSValueConst op1, JSValueConst op2);
QJS_INTERNAL BOOL js_same_value_zero(JSContext *ctx, JSValueConst op1, JSValueConst op2);
QJS_INTERNAL void js_shape_hash_link(JSRuntime *rt, JSShape *sh);
QJS_INTERNAL int js_shape_prepare_update(JSContext *ctx, JSObject *p,
                                   JSShapeProperty **pprs);
QJS_INTERNAL no_inline int js_shr_slow(JSContext *ctx, JSValue *sp);
QJS_INTERNAL BOOL js_strict_eq2(JSContext *ctx, JSValueConst op1, JSValueConst op2,
                          JSStrictEqModeEnum eq_mode);
QJS_INTERNAL int js_string_GetSubstitution(JSContext *ctx,
                                     StringBuffer *b,
                                     JSValueConst matched,
                                     JSString *sp,
                                     uint32_t position,
                                     JSValueConst captures_val,
                                     JSValueConst namedCaptures,
                                     JSValueConst rep,
                                     uint8_t **captures,
                                     uint32_t captures_len);
QJS_INTERNAL int js_string_compare(JSContext *ctx,
                             const JSString *p1, const JSString *p2);
QJS_INTERNAL JSValue js_string_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);
QJS_INTERNAL BOOL js_string_eq(JSContext *ctx,
                         const JSString *p1, const JSString *p2);
QJS_INTERNAL int js_string_find_invalid_codepoint(JSString *p);
QJS_INTERNAL int js_string_memcmp(const JSString *p1, int pos1, const JSString *p2,
                            int pos2, int len);
QJS_INTERNAL uint32_t js_string_obj_get_length(JSContext *ctx,
                                         JSValueConst obj);
QJS_INTERNAL int js_string_rope_compare(JSContext *ctx, JSValueConst op1,
                                  JSValueConst op2, BOOL eq_only);
QJS_INTERNAL JSValue js_sub_string(JSContext *ctx, JSString *p, int start, int end);
QJS_INTERNAL JSValue js_symbol_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);
QJS_INTERNAL JSAtom js_symbol_to_atom(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue js_typed_array___speciesCreate(JSContext *ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_typed_array_constructor(JSContext *ctx,
                                          JSValueConst new_target,
                                          int argc, JSValueConst *argv,
                                          int classid);
QJS_INTERNAL void js_typed_array_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL int js_typed_array_get_length_unsafe(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL void js_typed_array_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL no_inline __exception int js_unary_arith_slow(JSContext *ctx,
                                                     JSValue *sp,
                                                     OPCodeEnum op);
QJS_INTERNAL int js_update_property_flags(JSContext *ctx, JSObject *p,
                                    JSShapeProperty **pprs, int flags);
QJS_INTERNAL void json_free_parse_record(JSContext *ctx, JSONParseRecord *pr);
QJS_INTERNAL JSONParseRecord *json_parse_record_add(JSContext *ctx, JSONParseRecord *pr, JSAtom key, int *psize);
QJS_INTERNAL JSONParseRecord *json_parse_record_find(JSONParseRecord *pr, JSAtom key);
QJS_INTERNAL void json_parse_record_init_obj(JSContext *ctx, JSONParseRecord *pr, JSValueConst val);
QJS_INTERNAL void map_delete_weakrefs(JSRuntime *rt, JSWeakRefHeader *wh);
QJS_INTERNAL __exception int perform_promise_then(JSContext *ctx,
                                            JSValueConst promise,
                                            JSValueConst *resolve_reject,
                                            JSValueConst *cap_resolving_funcs);
QJS_INTERNAL __maybe_unused void print_atom(JSContext *ctx, JSAtom atom);
QJS_INTERNAL void remove_gc_object(JSGCObjectHeader *h);
QJS_INTERNAL no_inline int resize_properties(JSContext *ctx, JSShape **psh,
                                       JSObject *p, uint32_t count);
QJS_INTERNAL void set_cycle_flag(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL void set_value(JSContext *ctx, JSValue *pval, JSValue new_val);
QJS_INTERNAL uint32_t shape_hash(uint32_t h, uint32_t val);
QJS_INTERNAL int skip_spaces(const char *pc);
QJS_INTERNAL int64_t string_advance_index(JSString *p, int64_t index, BOOL unicode);
QJS_INTERNAL int string_buffer_concat(StringBuffer *s, const JSString *p,
                                uint32_t from, uint32_t to);
QJS_INTERNAL int string_buffer_concat_value(StringBuffer *s, JSValueConst v);
QJS_INTERNAL int string_buffer_concat_value_free(StringBuffer *s, JSValue v);
QJS_INTERNAL JSValue string_buffer_end(StringBuffer *s);
QJS_INTERNAL int string_buffer_fill(StringBuffer *s, int c, int count);
QJS_INTERNAL void string_buffer_free(StringBuffer *s);
QJS_INTERNAL int string_buffer_init(JSContext *ctx, StringBuffer *s, int size);
QJS_INTERNAL int string_buffer_init2(JSContext *ctx, StringBuffer *s, int size,
                               int is_wide);
QJS_INTERNAL int string_buffer_putc(StringBuffer *s, uint32_t c);
QJS_INTERNAL int string_buffer_putc16(StringBuffer *s, uint32_t c);
QJS_INTERNAL int string_buffer_putc8(StringBuffer *s, uint32_t c);
QJS_INTERNAL int string_buffer_putc_slow(StringBuffer *s, uint32_t c);
QJS_INTERNAL int string_buffer_puts8(StringBuffer *s, const char *str);
QJS_INTERNAL int string_buffer_write8(StringBuffer *s, const uint8_t *p, int len);
QJS_INTERNAL int string_get(const JSString *p, int idx);
QJS_INTERNAL int string_getc(const JSString *p, int *pidx);
QJS_INTERNAL int string_indexof_char(JSString *p, int c, int from);
QJS_INTERNAL int string_rope_get(JSValueConst val, uint32_t idx);
QJS_INTERNAL int to_digit(int c);
QJS_INTERNAL BOOL typed_array_is_oob(JSObject *p);
QJS_INTERNAL void weakref_delete_weakref(JSRuntime *rt, JSWeakRefHeader *wh);

#endif
