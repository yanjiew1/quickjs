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

typedef struct ExceptionOwner {
    void *pointer;
    int count;
} ExceptionOwner;

static ExceptionOwner owners[64];
static int owner_count, transferred, released;

static JSValue observe_exception(JSContext *ctx)
{
    JSValue value = JS_GetException(ctx);
    int i;

    if (JS_IsObject(value)) {
        void *pointer = JS_VALUE_GET_PTR(value);
        for (i = 0; i < owner_count && owners[i].pointer != pointer; i++)
            continue;
        if (i == owner_count) {
            assert(owner_count < (int)(sizeof(owners) / sizeof(owners[0])));
            owners[owner_count++].pointer = pointer;
        }
        owners[i].count++;
        transferred++;
    }
    return value;
}

static void observe_free(JSContext *ctx, JSValue value)
{
    int i;

    if (JS_IsObject(value)) {
        for (i = 0; i < owner_count; i++) {
            if (owners[i].pointer == JS_VALUE_GET_PTR(value) && owners[i].count) {
                owners[i].count--;
                released++;
                break;
            }
        }
    }
    JS_FreeValue(ctx, value);
}

static void observe_free_runtime(JSRuntime *rt)
{
    int i;

    for (i = 0; i < owner_count; i++)
        assert(owners[i].count == 0);
    JS_FreeRuntime(rt);
}

#define JS_GetException observe_exception
#define JS_FreeValue observe_free
#define JS_FreeRuntime observe_free_runtime
#define LLVMFuzzerTestOneInput fuzz_json_ownership_input
#include "../fuzz/fuzz_json.c"
#undef LLVMFuzzerTestOneInput
#define LLVMFuzzerTestOneInput fuzz_bytecode_ownership_input
#include "../fuzz/fuzz_bytecode.c"
#undef LLVMFuzzerTestOneInput
#define LLVMFuzzerTestOneInput fuzz_module_ownership_input
#include "../fuzz/fuzz_module_export.c"
#undef LLVMFuzzerTestOneInput
#define LLVMFuzzerTestOneInput fuzz_regexp_ownership_input
#include "../fuzz/fuzz_regexp_compile.c"
#undef LLVMFuzzerTestOneInput
#undef JS_GetException
#undef JS_FreeValue
#undef JS_FreeRuntime

typedef int FuzzInput(const uint8_t *data, size_t size);

static void check_input(FuzzInput *input, const uint8_t *data, size_t size)
{
    int previous = transferred;

    memset(owners, 0, sizeof(owners));
    owner_count = 0;
    assert(input(data, size) == 0);
    assert(transferred > previous);
    assert(transferred == released);
}

int main(void)
{
    static const uint8_t json[] = { '{' };
    static const uint8_t bytecode[] = { '@', '@', '@', '@', '@', '@', '@', '@' };
    static const uint8_t module[] = { '!' };
    static const uint8_t regexp[] = { '[', 'g' };

    check_input(fuzz_json_ownership_input, json, sizeof(json));
    check_input(fuzz_bytecode_ownership_input, bytecode, sizeof(bytecode));
    check_input(fuzz_module_ownership_input, module, sizeof(module));
    check_input(fuzz_regexp_ownership_input, regexp, sizeof(regexp));
    return 0;
}
