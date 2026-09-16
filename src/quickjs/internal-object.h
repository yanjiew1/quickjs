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
#ifndef QUICKJS_INTERNAL_OBJECT_H
#define QUICKJS_INTERNAL_OBJECT_H

#include "internal-number.h"

QJS_INTERNAL int JS_SetObjectData(JSContext *ctx, JSValueConst obj, JSValue val);
QJS_INTERNAL JSValue JS_ToObject(JSContext *ctx, JSValueConst val);
QJS_INTERNAL JSValue JS_ToObjectFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ToStringFree(JSContext *ctx, JSValue val);
QJS_INTERNAL __exception int js_get_length32(JSContext *ctx, uint32_t *pres,
                                       JSValueConst obj);
QJS_INTERNAL __exception int js_get_length64(JSContext *ctx, int64_t *pres,
                                       JSValueConst obj);
QJS_INTERNAL void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);

typedef struct JSArrayIteratorData {
    JSValue obj;
    JSIteratorKindEnum kind;
    uint32_t idx;
} JSArrayIteratorData;


/* Cross-TU declarations owned by this subsystem. */
extern QJS_INTERNAL JSClassShortDef const js_std_class_def[50];

QJS_INTERNAL __maybe_unused void JS_DumpGCObject(JSRuntime *rt, JSGCObjectHeader *p);

QJS_INTERNAL __maybe_unused void JS_DumpObjectHeader(JSRuntime *rt);

QJS_INTERNAL void JS_RunGCInternal(JSRuntime *rt, BOOL remove_weak_objects);

QJS_INTERNAL JSValue JS_ThrowTypeErrorNotASymbol(JSContext *ctx);

QJS_INTERNAL JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowTypeErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...);

QJS_INTERNAL void gc_decref(JSRuntime *rt);

QJS_INTERNAL void js_autoinit_free(JSRuntime *rt, JSProperty *pr);

QJS_INTERNAL JSAutoInitIDEnum js_autoinit_get_id(JSProperty *pr);

QJS_INTERNAL JSContext *js_autoinit_get_realm(JSProperty *pr);

#endif /* QUICKJS_INTERNAL_OBJECT_H */
