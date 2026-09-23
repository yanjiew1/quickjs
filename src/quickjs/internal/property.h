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

#include "base.h"

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

#endif /* QJS_PROPERTY_H */
