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
#ifndef QUICKJS_INTERNAL_DATE_H
#define QUICKJS_INTERNAL_DATE_H

#include "internal-builtin.h"

#define QJS_DATE_HINT_STRING         0
#define QJS_DATE_HINT_NUMBER         1
#define QJS_DATE_HINT_NONE           2
#define QJS_DATE_HINT_FORCE_ORDINARY (1 << 4)

static inline int qjs_date_string_get(const JSString *str, int index)
{
    return str->is_wide_char ? str->u.str16[index] : str->u.str8[index];
}

QJS_INTERNAL JSValue qjs_date_get_string(JSContext *ctx,
                                         JSValueConst this_val,
                                         int argc, JSValueConst *argv,
                                         int magic);
QJS_INTERNAL JSValue qjs_date_new_string8(JSContext *ctx, const char *str);
QJS_INTERNAL JSValue qjs_date_throw_type_error_not_object(JSContext *ctx);
QJS_INTERNAL JSValue qjs_date_to_primitive(JSContext *ctx, JSValueConst value,
                                           int hint);
QJS_INTERNAL int qjs_date_to_float64_free(JSContext *ctx, double *result,
                                          JSValue value);
QJS_INTERNAL JSValue qjs_date_create_from_ctor(JSContext *ctx,
                                               JSValueConst ctor,
                                               JSClassID class_id);
QJS_INTERNAL JSValue qjs_date_new_c_constructor(
    JSContext *ctx, int class_id, const char *name, JSCFunction *func,
    int length, JSCFunctionEnum cproto, int magic, JSValueConst parent_ctor,
    const JSCFunctionListEntry *ctor_fields, int ctor_field_count,
    const JSCFunctionListEntry *proto_fields, int proto_field_count, int flags);

#endif /* QUICKJS_INTERNAL_DATE_H */
