/*
 * QuickJS Object Private Interface
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

#ifndef QJS_INTERNAL_OBJECT_API_H
#define QJS_INTERNAL_OBJECT_API_H

#include "internal/object.h"
#include "internal/vm.h"

QJS_INTERNAL int JS_AddBrand(JSContext *ctx, JSValueConst obj, JSValueConst home_obj);
QJS_INTERNAL int JS_AutoInitProperty(JSContext *ctx, JSObject *p, JSAtom prop,
                               JSProperty *pr, JSShapeProperty *prs);
QJS_INTERNAL int JS_CheckBrand(JSContext *ctx, JSValueConst obj, JSValueConst func);
QJS_INTERNAL int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom prop, int flags);
QJS_INTERNAL int JS_CreateDataPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                       int64_t idx, JSValue val, int flags);
QJS_INTERNAL int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj,
                                     JSAtom prop, JSAutoInitIDEnum id,
                                     void *opaque, int flags);
QJS_INTERNAL int JS_DefineObjectName(JSContext *ctx, JSValueConst obj,
                               JSAtom name, int flags);
QJS_INTERNAL int JS_DefineObjectNameComputed(JSContext *ctx, JSValueConst obj,
                                       JSValueConst str, int flags);
QJS_INTERNAL int JS_DefinePrivateField(JSContext *ctx, JSValueConst obj,
                                 JSValueConst name, JSValue val);
QJS_INTERNAL int JS_DefinePropertyValueInt64(JSContext *ctx, JSValueConst this_obj,
                                int64_t idx, JSValue val, int flags);
QJS_INTERNAL int JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj,
                                JSValue prop, JSValue val, int flags);
QJS_INTERNAL int JS_DeleteGlobalVar(JSContext *ctx, JSAtom prop);
QJS_INTERNAL int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, int flags);
QJS_INTERNAL JSFunctionBytecode *JS_GetFunctionBytecode(JSValueConst val);
QJS_INTERNAL int JS_GetGlobalVarRef(JSContext *ctx, JSAtom prop, JSValue *sp);
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
QJS_INTERNAL BOOL JS_IsCFunction(JSContext *ctx, JSValueConst val, JSCFunction *func, int magic);
QJS_INTERNAL JSValue JS_NewCFunction3(JSContext *ctx, JSCFunction *func,
                                const char *name,
                                int length, JSCFunctionEnum cproto, int magic,
                                JSValueConst proto_val, int n_fields);
QJS_INTERNAL JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *sh, JSClassID class_id,
                                     JSProperty *props);
QJS_INTERNAL JSValue JS_NewObjectProtoClassAlloc(JSContext *ctx, JSValueConst proto_val,
                                           JSClassID class_id, int n_alloc_props);
QJS_INTERNAL int JS_OrdinaryIsInstanceOf(JSContext *ctx, JSValueConst val,
                                   JSValueConst obj);
QJS_INTERNAL void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects);
QJS_INTERNAL void JS_SetImmutablePrototype(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL int JS_SetObjectData(JSContext *ctx, JSValueConst obj, JSValue val);
QJS_INTERNAL int JS_SetPrivateField(JSContext *ctx, JSValueConst obj,
                              JSValueConst name, JSValue val);
QJS_INTERNAL int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                               JSValue prop, JSValue val, int flags);
QJS_INTERNAL int JS_SetPrototypeInternal(JSContext *ctx, JSValueConst obj,
                                   JSValueConst proto_val,
                                   BOOL throw_flag);
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
QJS_INTERNAL JSValue JS_ThrowTypeErrorInvalidClass(JSContext *ctx, int class_id);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx,
                                                JSValueConst func_obj);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);
QJS_INTERNAL int __attribute__((format(printf, 3, 4))) JS_ThrowTypeErrorOrFalse(JSContext *ctx, int flags, const char *fmt, ...);
QJS_INTERNAL int JS_ThrowTypeErrorReadOnly(JSContext *ctx, int flags, JSAtom atom);
QJS_INTERNAL int JS_ToBoolFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ToPrimitive(JSContext *ctx, JSValueConst val, int hint);
QJS_INTERNAL JSValue JS_ToPrimitiveFree(JSContext *ctx, JSValue val, int hint);
QJS_INTERNAL int JS_TryGetPropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, JSValue *pval);
QJS_INTERNAL JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowSyntaxErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...);
QJS_INTERNAL void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h,
                          JSGCObjectTypeEnum type);
QJS_INTERNAL JSProperty *add_property(JSContext *ctx,
                                JSObject *p, JSAtom prop, int prop_flags);
QJS_INTERNAL void build_backtrace(JSContext *ctx, JSValueConst error_obj,
                            const char *filename, int line_num, int col_num,
                            int backtrace_flags);
QJS_INTERNAL BOOL check_define_prop_flags(int prop_flags, int flags);
QJS_INTERNAL no_inline __exception int convert_fast_array_to_array(JSContext *ctx,
                                                             JSObject *p);
QJS_INTERNAL void dbuf_put_leb128(DynBuf *s, uint32_t v);
QJS_INTERNAL void dbuf_put_sleb128(DynBuf *s, int32_t v1);
QJS_INTERNAL int delete_property(JSContext *ctx, JSObject *p, JSAtom atom);
QJS_INTERNAL int expand_fast_array(JSContext *ctx, JSObject *p, uint32_t new_len);
QJS_INTERNAL int find_line_num(JSContext *ctx, JSFunctionBytecode *b,
                         uint32_t pc_value, int *pcol_num);
QJS_INTERNAL void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
QJS_INTERNAL void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
QJS_INTERNAL void free_zero_refcount(JSRuntime *rt);
QJS_INTERNAL int get_leb128(uint32_t *pval, const uint8_t *buf,
                      const uint8_t *buf_end);
QJS_INTERNAL const char *get_prop_string(JSContext *ctx, JSValueConst obj, JSAtom prop);
QJS_INTERNAL JSObject *get_proto_obj(JSValueConst proto_val);
QJS_INTERNAL int get_sleb128(int32_t *pval, const uint8_t *buf,
                       const uint8_t *buf_end);
QJS_INTERNAL BOOL is_backtrace_needed(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL JSValue js_allocate_fast_array(JSContext *ctx, int64_t len);
QJS_INTERNAL void js_array_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_array_mark(JSRuntime *rt, JSValueConst val,
                          JS_MarkFunc *mark_func);
QJS_INTERNAL JSAutoInitIDEnum js_autoinit_get_id(JSProperty *pr);
QJS_INTERNAL JSContext *js_autoinit_get_realm(JSProperty *pr);
QJS_INTERNAL void js_bound_function_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_bound_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
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
QJS_INTERNAL BOOL js_class_has_bytecode(JSClassID class_id);
QJS_INTERNAL JSValue js_create_array(JSContext *ctx, int len, JSValueConst *tab);
QJS_INTERNAL JSValue js_create_array_free(JSContext *ctx, int len, JSValue *tab);
QJS_INTERNAL void js_for_in_iterator_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_for_in_iterator_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);
QJS_INTERNAL void js_function_set_properties(JSContext *ctx, JSValueConst func_obj,
                                       JSAtom name, int len);
QJS_INTERNAL void js_method_set_home_object(JSContext *ctx, JSValueConst func_obj,
                                      JSValueConst home_obj);
QJS_INTERNAL int js_method_set_properties(JSContext *ctx, JSValueConst func_obj,
                                    JSAtom name, int flags, JSValueConst home_obj);
QJS_INTERNAL void js_object_data_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_object_data_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
QJS_INTERNAL uint32_t js_string_obj_get_length(JSContext *ctx,
                                         JSValueConst obj);
QJS_INTERNAL int js_update_property_flags(JSContext *ctx, JSObject *p,
                                    JSShapeProperty **pprs, int flags);
QJS_INTERNAL void remove_gc_object(JSGCObjectHeader *h);
QJS_INTERNAL void set_cycle_flag(JSContext *ctx, JSValueConst obj);
QJS_INTERNAL int skip_spaces(const char *pc);

static inline JSShapeProperty *get_shape_prop(JSShape *sh)
{
    return (JSShapeProperty *)((uint32_t *)(sh + 1) + sh->prop_hash_mask + 1);
}

static force_inline JSShapeProperty *find_own_property1(JSObject *p,
                                                        JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *prop;
    intptr_t h;
    sh = p->shape;
    h = (uintptr_t)atom & sh->prop_hash_mask;
    h = sh->hash_table[h];
    prop = get_shape_prop(sh);
    while (h) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            return pr;
        }
        h = pr->hash_next;
    }
    return NULL;
}

static force_inline JSShapeProperty *find_own_property(JSProperty **ppr,
                                                       JSObject *p,
                                                       JSAtom atom)
{
    JSShape *sh;
    JSShapeProperty *pr, *prop;
    intptr_t h;
    sh = p->shape;
    h = (uintptr_t)atom & sh->prop_hash_mask;
    h = sh->hash_table[h];
    prop = get_shape_prop(sh);
    while (h) {
        pr = &prop[h - 1];
        if (likely(pr->atom == atom)) {
            *ppr = &p->prop[h - 1];
            /* the compiler should be able to assume that pr != NULL here */
            return pr;
        }
        h = pr->hash_next;
    }
    *ppr = NULL;
    return NULL;
}

QJS_INTERNAL __exception int __js_poll_interrupts(JSContext *ctx);

static inline __exception int js_poll_interrupts(JSContext *ctx)
{
    if (unlikely(--ctx->interrupt_counter <= 0)) {
        return __js_poll_interrupts(ctx);
    } else {
        return 0;
    }
}

static force_inline BOOL can_extend_fast_array(JSObject *p)
{
    JSObject *proto;
    if (!p->extensible)
        return FALSE;
    proto = p->shape->proto;
    if (!proto)
        return TRUE;
    return proto->is_std_array_prototype;
}

static inline BOOL JS_IsHTMLDDA(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
        return FALSE;
    p = JS_VALUE_GET_OBJ(obj);
    return p->is_HTMLDDA;
}

static inline int to_digit(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'z')
        return c - 'a' + 10;
    else
        return 36;
}

#define JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL (1 << 0)

#define DEFINE_GLOBAL_LEX_VAR (1 << 7)
#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)

#define JS_ThrowTypeErrorAtom(ctx, fmt, atom) __JS_ThrowTypeErrorAtom(ctx, atom, fmt, "")
#define JS_ThrowSyntaxErrorAtom(ctx, fmt, atom) __JS_ThrowSyntaxErrorAtom(ctx, atom, fmt, "")

#endif /* QJS_INTERNAL_OBJECT_API_H */
