/*
 * QuickJS host library shared private helpers
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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
#ifndef QJS_LIBC_INTERNAL_BASE_H
#define QJS_LIBC_INTERNAL_BASE_H

#include <errno.h>
#include <sys/types.h>

#include "../../quickjs.h"

#if defined(__GNUC__) || defined(__clang__)
#define QJS_LIBC_INTERNAL __attribute__((visibility("hidden")))
#else
#define QJS_LIBC_INTERNAL
#endif

static inline ssize_t js_get_errno(ssize_t ret)
{
    if (ret == -1)
        ret = -errno;
    return ret;
}

static inline int get_bool_option(JSContext *ctx, BOOL *pbool,
                                           JSValueConst obj,
                                           const char *option)
{
    JSValue val;

    val = JS_GetPropertyStr(ctx, obj, option);
    if (JS_IsException(val))
        return -1;
    if (!JS_IsUndefined(val))
        *pbool = JS_ToBool(ctx, val);
    JS_FreeValue(ctx, val);
    return 0;
}

#endif /* QJS_LIBC_INTERNAL_BASE_H */
