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

#include "internal-string.h"

#define ATOD_ACCEPT_BIN_OCT       (1 << 2)
#define ATOD_ACCEPT_LEGACY_OCTAL  (1 << 4)
#define ATOD_ACCEPT_UNDERSCORES   (1 << 5)
#define ATOD_ACCEPT_SUFFIX        (1 << 6)

static inline JSShapeProperty *qjs_get_shape_prop(JSShape *shape)
{
    return (JSShapeProperty *)((uint32_t *)(shape + 1) +
                               shape->prop_hash_mask + 1);
}

QJS_INTERNAL JSShapeProperty *qjs_find_own_property(JSProperty **ppr,
                                                    JSObject *obj,
                                                    JSAtom atom);
QJS_INTERNAL JSBigInt *qjs_bigint_new(JSContext *ctx, int len);
QJS_INTERNAL JSBigInt *qjs_bigint_set_short(JSBigIntBuf *buf, JSValueConst val);
QJS_INTERNAL JSValue qjs_compact_bigint(JSContext *ctx, JSBigInt *p);
QJS_INTERNAL int qjs_set_object_data(JSContext *ctx, JSValueConst obj,
                                  JSValue val);
QJS_INTERNAL JSValue qjs_to_object(JSContext *ctx, JSValueConst val);
QJS_INTERNAL __exception int qjs_get_length32(JSContext *ctx, uint32_t *pres,
                                             JSValueConst obj);
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
QJS_INTERNAL int qjs_update_property_flags(JSContext *ctx, JSObject *obj,
                                           JSShapeProperty **prop, int flags);
QJS_INTERNAL int qjs_to_digit(int c);
QJS_INTERNAL JSValue qjs_atof(JSContext *ctx, const char *str,
                              const char **end, int radix, int flags);


#endif /* QUICKJS_INTERNAL_OBJECT_H */
