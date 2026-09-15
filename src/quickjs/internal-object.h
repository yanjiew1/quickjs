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

#include "internal-runtime.h"

#define JS_ATOM_TAG_INT (1U << 31)
#define JS_ATOM_MAX_INT (JS_ATOM_TAG_INT - 1)
#define JS_ATOM_MAX     ((1U << 30) - 1)
#define ATOM_GET_STR_BUF_SIZE 64

static inline BOOL qjs_atom_is_tagged_int(JSAtom atom)
{
    return (atom & JS_ATOM_TAG_INT) != 0;
}

static inline JSAtom qjs_atom_from_uint32(uint32_t value)
{
    return JS_ATOM_TAG_INT | value;
}

static inline uint32_t qjs_atom_to_uint32(JSAtom atom)
{
    return atom & ~JS_ATOM_TAG_INT;
}

static inline JSShapeProperty *qjs_get_shape_prop(JSShape *shape)
{
    return (JSShapeProperty *)((uint32_t *)(shape + 1) +
                               shape->prop_hash_mask + 1);
}

QJS_INTERNAL JSShapeProperty *qjs_find_own_property(JSProperty **ppr,
                                                    JSObject *obj,
                                                    JSAtom atom);
QJS_INTERNAL BOOL qjs_atom_is_string(JSContext *ctx, JSAtom atom);
QJS_INTERNAL JSAtom qjs_new_atom_str(JSContext *ctx, JSString *str);
QJS_INTERNAL JSString *qjs_alloc_string(JSContext *ctx, int max_len,
                                       int is_wide_char);
QJS_INTERNAL void qjs_free_string(JSRuntime *rt, JSString *str);
QJS_INTERNAL JSBigInt *qjs_bigint_new(JSContext *ctx, int len);
QJS_INTERNAL JSBigInt *qjs_bigint_set_short(JSBigIntBuf *buf, JSValueConst val);
QJS_INTERNAL JSValue qjs_compact_bigint(JSContext *ctx, JSBigInt *p);
QJS_INTERNAL int qjs_set_object_data(JSContext *ctx, JSValueConst obj,
                                  JSValue val);
QJS_INTERNAL JSValue qjs_to_object(JSContext *ctx, JSValueConst val);
QJS_INTERNAL __exception int qjs_get_length32(JSContext *ctx, uint32_t *pres,
                                             JSValueConst obj);
QJS_INTERNAL const char *qjs_atom_get_str(JSContext *ctx, char *buf,
                                          int buf_size, JSAtom atom);
QJS_INTERNAL int qjs_string_compare(JSContext *ctx, const JSString *left,
                                    const JSString *right);
QJS_INTERNAL JSShapeProperty *qjs_find_own_property1(JSObject *obj,
                                                     JSAtom atom);
QJS_INTERNAL void qjs_free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
QJS_INTERNAL int qjs_get_own_property_names_internal(
    JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen, JSObject *obj,
    int flags);
QJS_INTERNAL JSProperty *qjs_add_property(JSContext *ctx, JSObject *obj,
                                          JSAtom atom, int flags);
QJS_INTERNAL int qjs_define_auto_init_property(JSContext *ctx,
                                               JSValueConst obj,
                                               JSAtom atom,
                                               JSAutoInitIDEnum id,
                                               void *opaque, int flags);
QJS_INTERNAL void qjs_print_atom(JSContext *ctx, JSAtom atom);
QJS_INTERNAL JSValue qjs_throw_duplicate_export(JSContext *ctx, JSAtom atom);


#endif /* QUICKJS_INTERNAL_OBJECT_H */
