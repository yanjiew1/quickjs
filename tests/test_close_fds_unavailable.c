/* Fail safely when no reliable descriptor-closing facility exists.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT
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
#include <assert.h>
#include <sys/syscall.h>

/* Compile the generic fallback even on Linux. */
#undef HAVE_CLOSEFROM
#undef SYS_close_range
#undef SYS_getdents64
#include "../src/quickjs-libc/os.c"

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue args = JS_NewArray(ctx), result;
    int status;

    assert(rt && ctx && !JS_IsException(args));
    assert(JS_SetPropertyUint32(ctx, args, 0, JS_NewString(ctx, "/bin/true")) >= 0);
    result = js_os_exec(ctx, JS_UNDEFINED, 1, &args);
    assert(!JS_IsException(result));
    assert(JS_ToInt32(ctx, &status, result) == 0 && status == 127);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
