/*
 * QuickJS native fuzz harness regression tests
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#include <stdlib.h>
#include <string.h>

#include "quickjs.h"
#include "quickjs-libc.h"
#include "../fuzz/fuzz_common.h"

static int fail_runtime, fail_context, fail_buffer;
static int runtime_calls, context_calls, buffer_calls, init_calls;
static int runtime_frees, context_frees, buffer_frees;

static JSRuntime *probe_new_runtime(void)
{
    runtime_calls++;
    return fail_runtime ? NULL : JS_NewRuntime();
}

static JSContext *probe_new_context(JSRuntime *rt)
{
    assert(rt);
    context_calls++;
    return fail_context ? NULL : JS_NewContext(rt);
}

static void probe_free_runtime(JSRuntime *rt)
{
    assert(rt);
    runtime_frees++;
    JS_FreeRuntime(rt);
}

static void probe_free_context(JSContext *ctx)
{
    assert(ctx);
    context_frees++;
    JS_FreeContext(ctx);
}

static void *probe_malloc(size_t size)
{
    buffer_calls++;
    return fail_buffer ? NULL : malloc(size);
}

static void probe_free(void *ptr)
{
    assert(ptr);
    buffer_frees++;
    free(ptr);
}

static void probe_init(JSRuntime *rt, JSContext *ctx)
{
    assert(rt && ctx);
    init_calls++;
    test_one_input_init(rt, ctx);
}

#define JS_NewRuntime probe_new_runtime
#define JS_NewContext probe_new_context
#define JS_FreeRuntime probe_free_runtime
#define JS_FreeContext probe_free_context
#define malloc probe_malloc
#define free probe_free
#define test_one_input_init probe_init
#define LLVMFuzzerTestOneInput fuzz_eval_allocation_input
#include "../fuzz/fuzz_eval.c"
#undef LLVMFuzzerTestOneInput
#define LLVMFuzzerTestOneInput fuzz_compile_allocation_input
#include "../fuzz/fuzz_compile.c"
#undef LLVMFuzzerTestOneInput
#undef test_one_input_init
#undef free
#undef malloc
#undef JS_FreeContext
#undef JS_FreeRuntime
#undef JS_NewContext
#undef JS_NewRuntime

typedef int FuzzInput(const uint8_t *data, size_t size);

static void check_allocation_failure(FuzzInput *input, int failure)
{
    static const uint8_t source[] = { '0' };

    runtime_calls = context_calls = buffer_calls = init_calls = 0;
    runtime_frees = context_frees = buffer_frees = 0;
    fail_runtime = failure == 1;
    fail_context = failure == 2;
    fail_buffer = failure == 3;
    assert(input(source, sizeof(source)) == 0);
    assert(runtime_calls == 1);
    assert(context_calls == (failure == 1 ? 0 : 1));
    assert(buffer_calls == (failure == 1 || failure == 2 ? 0 : 1));
    assert(init_calls == (failure == 0 ? 1 : 0));
    assert(runtime_frees == (failure == 1 ? 0 : 1));
    assert(context_frees == (failure == 1 || failure == 2 ? 0 : 1));
    assert(buffer_frees == (failure == 0 ? 1 : 0));
}

static void check_entry(FuzzInput *input)
{
    static const uint8_t source[] = { '0' };
    int failure;

    for (failure = 0; failure <= 3; failure++)
        check_allocation_failure(input, failure);
    runtime_calls = context_calls = buffer_calls = init_calls = 0;
    assert(input(source, 0) == 0);
    assert(input(source, SIZE_MAX) == 0);
    assert(runtime_calls == 0 && context_calls == 0 && buffer_calls == 0);
}

int main(void)
{
    check_entry(fuzz_eval_allocation_input);
    check_entry(fuzz_compile_allocation_input);
    return 0;
}
