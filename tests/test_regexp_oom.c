/* RegExp construction must release its owner on every allocation failure.
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
#include <stdio.h>
#include <string.h>
#include "quickjs.h"
#include "oom-allocator.h"

int main(void)
{
    char pattern[16000];
    size_t i, used = 0, fail;
    int failures = 0, succeeded = 0;
    for (i = 0; i < 500; i++)
        used += snprintf(pattern + used, sizeof(pattern) - used,
                         "%sword%zu", i ? "|" : "", i);
    for (fail = 1; fail <= 64; fail++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue global, constructor, args[2], value;
        assert(ctx);
        global = JS_GetGlobalObject(ctx);
        constructor = JS_GetPropertyStr(ctx, global, "RegExp");
        args[0] = JS_NewString(ctx, pattern);
        args[1] = JS_NewString(ctx, "u");
        assert(!JS_IsException(args[0]) && !JS_IsException(args[1]));
        oom_test_begin(&allocator, fail);
        value = JS_CallConstructor(ctx, constructor, 2, args);
        oom_test_end(&allocator);
        if (JS_IsException(value)) {
            assert(allocator.failures == 1 && JS_HasException(ctx));
            failures++;
            value = JS_GetException(ctx);
        } else {
            assert(!JS_HasException(ctx));
            succeeded++;
        }
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, constructor);
        JS_FreeValue(ctx, global);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
    assert(failures > 0 && succeeded > 0);
    return 0;
}
