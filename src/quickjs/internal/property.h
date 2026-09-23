/*
 * QuickJS Property Internal Interface
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
#ifndef QJS_PROPERTY_H
#define QJS_PROPERTY_H

#include "object.h"

typedef struct JSShapeProperty JSShapeProperty;

typedef struct JSProperty {
    union {
        JSValue value;      /* JS_PROP_NORMAL */
        struct {            /* JS_PROP_GETSET */
            JSObject *getter; /* NULL if undefined */
            JSObject *setter; /* NULL if undefined */
        } getset;
        JSVarRef *var_ref;  /* JS_PROP_VARREF */
        struct {            /* JS_PROP_AUTOINIT */
            /* in order to use only 2 pointers, we compress the realm
               and the init function pointer */
            uintptr_t realm_and_id; /* realm and init_id (JS_AUTOINIT_ID_x)
                                       in the 2 low bits */
            void *opaque;
        } init;
    } u;
} JSProperty;

QJS_INTERNAL BOOL check_define_prop_flags(int prop_flags, int flags);
QJS_INTERNAL int __attribute__((format(printf, 3, 4)))
JS_ThrowTypeErrorOrFalse(JSContext *ctx, int flags, const char *fmt, ...);
QJS_INTERNAL JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);
QJS_INTERNAL __exception int js_get_length64(JSContext *ctx, int64_t *pres,
                                               JSValueConst obj);
QJS_INTERNAL __exception int js_get_length32(JSContext *ctx, uint32_t *pres,
                                               JSValueConst obj);
QJS_INTERNAL JSValue JS_GetPropertyInt64(JSContext *ctx, JSValueConst obj,
                                         int64_t idx);
QJS_INTERNAL JSValue JS_GetPropertyValue(JSContext *ctx, JSValueConst this_obj,
                                         JSValue prop);
QJS_INTERNAL int JS_CreateDataPropertyUint32(JSContext *ctx,
                                              JSValueConst this_obj,
                                              int64_t idx, JSValue val,
                                              int flags);
QJS_INTERNAL int JS_DefinePropertyValueInt64(JSContext *ctx,
                                              JSValueConst this_obj,
                                              int64_t idx, JSValue val,
                                              int flags);
QJS_INTERNAL int __exception JS_GetOwnPropertyNamesInternal(JSContext *ctx,
                                                              JSPropertyEnum **ptab,
                                                              uint32_t *plen,
                                                              JSObject *p,
                                                              int flags);
QJS_INTERNAL int JS_GetOwnPropertyInternal(JSContext *ctx,
                                           JSPropertyDescriptor *desc,
                                           JSObject *p, JSAtom prop);
QJS_INTERNAL void js_free_desc(JSContext *ctx, JSPropertyDescriptor *desc);
QJS_INTERNAL int js_obj_to_desc(JSContext *ctx, JSPropertyDescriptor *d,
                                 JSValueConst desc);
QJS_INTERNAL JSValue JS_GetOwnPropertyNames2(JSContext *ctx,
                                              JSValueConst obj1,
                                              int flags, int kind);

QJS_INTERNAL int JS_SetPropertyValue(JSContext *ctx, JSValueConst this_obj, JSValue prop, JSValue val, int flags);

typedef enum {
    JS_AUTOINIT_ID_PROTOTYPE,
    JS_AUTOINIT_ID_MODULE_NS,
    JS_AUTOINIT_ID_PROP,
} JSAutoInitIDEnum;







QJS_INTERNAL __exception int JS_CopyDataProperties(JSContext *ctx, JSValueConst target, JSValueConst source, JSValueConst excluded, BOOL setprop);
QJS_INTERNAL int JS_DefinePropertyValueValue(JSContext *ctx, JSValueConst this_obj, JSValue prop, JSValue val, int flags);

QJS_INTERNAL int JS_DeletePropertyInt64(JSContext *ctx, JSValueConst obj, int64_t idx, int flags);

QJS_INTERNAL int JS_DefineAutoInitProperty(JSContext *ctx, JSValueConst this_obj, JSAtom prop, JSAutoInitIDEnum id, void *opaque, int flags);

QJS_INTERNAL JSProperty *add_property(JSContext *ctx, JSObject *p, JSAtom prop, int prop_flags);

QJS_INTERNAL int js_update_property_flags(JSContext *ctx, JSObject *p, JSShapeProperty **pprs, int flags);

QJS_INTERNAL int delete_property(JSContext *ctx, JSObject *p, JSAtom atom);
QJS_INTERNAL int JS_AutoInitProperty(JSContext *ctx, JSObject *p, JSAtom prop,
                               JSProperty *pr, JSShapeProperty *prs);
QJS_INTERNAL void free_property(JSRuntime *rt, JSProperty *pr, int prop_flags);
QJS_INTERNAL int JS_CheckDefineGlobalVar(JSContext *ctx, JSAtom prop, int flags);
QJS_INTERNAL int JS_GetGlobalVarRef(JSContext *ctx, JSAtom prop, JSValue *sp);
QJS_INTERNAL int JS_DeleteGlobalVar(JSContext *ctx, JSAtom prop);
QJS_INTERNAL JSValue JS_GetPrivateField(JSContext *ctx, JSValueConst obj,
                                  JSValueConst name);
QJS_INTERNAL int JS_SetPrivateField(JSContext *ctx, JSValueConst obj,
                              JSValueConst name, JSValue val);
QJS_INTERNAL int JS_DefinePrivateField(JSContext *ctx, JSValueConst obj,
                                 JSValueConst name, JSValue val);

#define DEFINE_GLOBAL_LEX_VAR (1 << 7)
#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)

QJS_INTERNAL JSContext *js_autoinit_get_realm(JSProperty *pr);
QJS_INTERNAL JSAutoInitIDEnum js_autoinit_get_id(JSProperty *pr);
QJS_INTERNAL void js_autoinit_free(JSRuntime *rt, JSProperty *pr);
QJS_INTERNAL void js_autoinit_mark(JSRuntime *rt, JSProperty *pr,
                             JS_MarkFunc *mark_func);

QJS_INTERNAL no_inline __exception int convert_fast_array_to_array(JSContext *ctx,
                                                                     JSObject *p);

#endif /* QJS_PROPERTY_H */
