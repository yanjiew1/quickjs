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
#ifndef QUICKJS_INTERNAL_GLOBAL_H
#define QUICKJS_INTERNAL_GLOBAL_H

#include "internal-builtin.h"

#define QJS_ATOD_INT_ONLY                 (1 << 0)
#define QJS_ATOD_ACCEPT_PREFIX_AFTER_SIGN (1 << 10)

QJS_INTERNAL int qjs_add_intrinsic_global(JSContext *ctx);
QJS_INTERNAL JSValue js_global_isNaN(JSContext *ctx,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv);
QJS_INTERNAL JSValue js_global_isFinite(JSContext *ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst *argv);
QJS_INTERNAL int string_buffer_putc16(StringBuffer *buf,
                                                 uint32_t c);
QJS_INTERNAL int string_buffer_write8(StringBuffer *buf,
                                                 const uint8_t *str,
                                                 int len);
QJS_INTERNAL JSValue JS_ThrowError(JSContext *ctx,
                                            JSErrorEnum error_num,
                                            const char *fmt, va_list ap);
QJS_INTERNAL int skip_spaces(const char *str);
QJS_INTERNAL JSValue js_atof(JSContext *ctx, const char *str,
                                     const char **end, int radix, int flags);

#endif /* QUICKJS_INTERNAL_GLOBAL_H */
