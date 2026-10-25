/*
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

/* Runtime default locale and public embedding failure contracts. */
#include "oom-allocator.h"
#include <stdlib.h>

#ifdef CONFIG_INTL
#include "../src/quickjs/internal/runtime.h"
#include "pool-padding.h"

static void check_default(JSContext *ctx, const char *expected)
{
    JSValue value = JS_GetIntlDefaultLocale(ctx);
    const char *text;
    assert(!JS_IsException(value));
    text = JS_ToCString(ctx, value);
    assert(text && !strcmp(text, expected));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, value);
}

static void test_set_failures(void)
{
    size_t step, calls = 0;
    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContextRaw(rt);
        char *old;
        void *padding;
        int status;
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx));
        assert(!JS_SetIntlDefaultLocale(ctx, "en"));
        old = rt->intl_default_locale;
        /* Drain pooled blocks so every sample reaches the host allocator. */
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        status = JS_SetIntlDefaultLocale(ctx, "EN-u-nu-arab");
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 256 && !status);
        } else {
            assert(allocator.failures == 1 && status < 0);
            assert(rt->intl_default_locale == old);
            oom_test_check_exception(ctx);
        }
        check_default(ctx, "en");
        assert(!JS_SetIntlDefaultLocale(ctx, "en"));
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

static void test_get_failures(void)
{
    size_t step, calls = 0;
    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContextRaw(rt);
        JSValue result;
        void *padding;
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx));
        /* Exhaust the policy/string path with locale metadata already open;
           its independent C tests exhaust cold data-reader allocations. */
        assert(!JS_SetIntlDefaultLocale(ctx, "en"));
        assert(!JS_SetIntlDefaultLocale(ctx, NULL));
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = JS_GetIntlDefaultLocale(ctx);
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 256 && !JS_IsException(result));
        } else {
            assert(allocator.failures == 1 && JS_IsException(result));
            oom_test_check_exception(ctx);
        }
        JS_FreeValue(ctx, result);
        result = JS_GetIntlDefaultLocale(ctx);
        assert(JS_IsString(result) && !JS_HasException(ctx));
        JS_FreeValue(ctx, result);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

#ifndef _WIN32
static void test_host_policy(JSContext *ctx)
{
    const char *old = getenv("LC_ALL");
    char *saved = old ? strdup(old) : NULL;
    JSValue expected;
    const char *text;
    assert(!old || saved);
    assert(!setenv("LC_ALL", "not_a_locale", 1));
    assert(!JS_SetIntlDefaultLocale(ctx, NULL));
    check_default(ctx, "en");
    assert(!setenv("LC_ALL", "zz_ZZ.UTF-8", 1));
    assert(!JS_SetIntlDefaultLocale(ctx, NULL));
    check_default(ctx, "en");
    assert(!setenv("LC_ALL", "en_US.UTF-8", 1));
    check_default(ctx, "en"); /* Host changes do not mutate a snapshot. */
    assert(!JS_SetIntlDefaultLocale(ctx, "en-US"));
    expected = JS_GetIntlDefaultLocale(ctx);
    text = JS_ToCString(ctx, expected);
    assert(text);
    assert(!JS_SetIntlDefaultLocale(ctx, NULL));
    check_default(ctx, text);
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, expected);
    if (saved) {
        assert(!setenv("LC_ALL", saved, 1));
        free(saved);
    } else {
        assert(!unsetenv("LC_ALL"));
    }
}
#endif
#endif

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContextRaw(rt);
    assert(ctx && !JS_AddIntrinsicBaseObjects(ctx));
#ifdef CONFIG_INTL
    {
        JSContext *other = JS_NewContextRaw(rt);
        JSValue value;
        const char *text;
        assert(other && !JS_AddIntrinsicBaseObjects(other));
        assert(!JS_SetIntlDefaultLocale(ctx, "EN-u-ca-gregory-x-private"));
        check_default(ctx, "en");
        check_default(other, "en");
        assert(JS_SetIntlDefaultLocale(ctx, "not_a_locale") < 0);
        JS_FreeValue(ctx, JS_GetException(ctx));
        check_default(ctx, "en");
        assert(JS_SetIntlDefaultLocale(ctx, "zz-ZZ") < 0);
        JS_FreeValue(ctx, JS_GetException(ctx));
        check_default(ctx, "en");
        assert(!JS_SetIntlDefaultLocale(other, NULL));
        assert(!rt->intl_default_locale);
        value = JS_GetIntlDefaultLocale(ctx);
        assert(!JS_IsException(value) && rt->intl_default_locale);
        text = JS_ToCString(ctx, value);
        assert(text);
        check_default(other, text);
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, value);
        JS_FreeContext(other);
    }
#ifndef _WIN32
    test_host_policy(ctx);
#endif
#else
    {
        JSValue value;
        assert(JS_SetIntlDefaultLocale(ctx, "en") < 0 && JS_HasException(ctx));
        JS_FreeValue(ctx, JS_GetException(ctx));
        assert(JS_SetIntlDefaultLocale(ctx, NULL) < 0 && JS_HasException(ctx));
        JS_FreeValue(ctx, JS_GetException(ctx));
        value = JS_GetIntlDefaultLocale(ctx);
        assert(JS_IsException(value) && JS_HasException(ctx));
        JS_FreeValue(ctx, JS_GetException(ctx));
    }
#endif
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
#ifdef CONFIG_INTL
    test_set_failures();
    test_get_failures();
#endif
    return 0;
}
