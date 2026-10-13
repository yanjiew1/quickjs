/*
 * QuickJS C API tests
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
#include "cutils.h"

static void check_eval(JSContext *ctx, const char *source)
{
    JSValue result = JS_Eval(ctx, source, strlen(source),
                             "regression-test", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue error = JS_GetException(ctx);
        const char *text = JS_ToCString(ctx, error);
        fprintf(stderr, "regression script: %s\n", text ? text : "exception");
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, error);
        abort();
    }
    assert(JS_ToBool(ctx, result) == 1);
    JS_FreeValue(ctx, result);
}

typedef struct ExternalBufferData {
    uint8_t byte;
    int free_count;
} ExternalBufferData;

static void free_external(JSRuntime *rt, void *opaque, void *ptr)
{
    ExternalBufferData *data = opaque;
    (void)rt;
    assert(ptr == &data->byte || ptr == NULL);
    data->free_count++;
}

static void check_empty_buffer(JSContext *ctx, JSValueConst buffer)
{
    size_t size = 1;
    assert(!JS_IsException(buffer));
    assert(JS_GetArrayBuffer(ctx, &size, buffer) != NULL);
    assert(size == 0 && !JS_HasException(ctx));
}

static void test_empty_buffer_allocation(void)
{
    static const char *sources[] = {
        "new ArrayBuffer(0)",
        "Uint8Array.fromBase64('').buffer",
        "Uint8Array.fromHex('').buffer",
        "new ArrayBuffer(0, { maxByteLength: 8 })",
        "new ArrayBuffer(4).transfer(0)",
        "new ArrayBuffer(4).transferToFixedLength(0)",
        "(() => { const b = new ArrayBuffer(4, { maxByteLength: 8 });"
        "b.resize(0); b.resize(0); return b; })()",
        "(() => { const b = new ArrayBuffer(0, { maxByteLength: 8 });"
        "const a = new Uint8Array(b); b.resize(0); b.resize(4);"
        "a.fill(42); b.resize(0); b.resize(4);"
        "if (a.length !== 4 || [...a].some(x => x !== 0)) throw Error();"
        "b.resize(0); return b; })()",
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue buffer, array;
    JSValueConst arguments[3];
    ExternalBufferData external = { 42, 0 };
    uint8_t *ptr;
    uint8_t byte = 42;
    const uint8_t *source;
    size_t i, size;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    for (i = 0; i < countof(sources); i++) {
        buffer = JS_Eval(ctx, sources[i], strlen(sources[i]),
                         "empty-buffer", JS_EVAL_TYPE_GLOBAL);
        check_empty_buffer(ctx, buffer);
        JS_FreeValue(ctx, buffer);
    }
    for (i = 0; i < 2; i++) {
        source = i ? &byte : NULL;
        buffer = JS_NewArrayBufferCopy(ctx, source, 0);
        check_empty_buffer(ctx, buffer);
        arguments[0] = buffer;
        arguments[1] = arguments[2] = JS_UNDEFINED;
        array = JS_NewTypedArray(ctx, 3, arguments, JS_TYPED_ARRAY_UINT8);
        JS_FreeValue(ctx, buffer);
        assert(!JS_IsException(array));
        buffer = JS_GetTypedArrayBuffer(ctx, array, NULL, NULL, NULL);
        check_empty_buffer(ctx, buffer);
        JS_FreeValue(ctx, buffer);
        JS_FreeValue(ctx, array);
    }
    buffer = JS_NewArrayBuffer(ctx, NULL, 0, free_external, &external, FALSE);
    check_empty_buffer(ctx, buffer);
    JS_DetachArrayBuffer(ctx, buffer);
    JS_FreeValue(ctx, buffer);
    assert(external.free_count == 0);
    buffer = JS_NewArrayBuffer(ctx, NULL, 4, free_external, &external, FALSE);
    assert(!JS_IsException(buffer));
    ptr = JS_GetArrayBuffer(ctx, &size, buffer);
    assert(ptr != NULL && size == 4 && !JS_HasException(ctx));
    for (i = 0; i < size; i++)
        assert(ptr[i] == 0);
    JS_FreeValue(ctx, buffer);
    assert(external.free_count == 0);
    buffer = JS_NewArrayBuffer(ctx, &external.byte, 0,
                               free_external, &external, FALSE);
    assert(!JS_IsException(buffer));
    assert(JS_GetArrayBuffer(ctx, &size, buffer) == &external.byte && size == 0);
    JS_FreeValue(ctx, buffer);
    assert(external.free_count == 1);
    buffer = JS_NewArrayBuffer(ctx, NULL, 0, NULL, NULL, TRUE);
    check_empty_buffer(ctx, buffer);
    arguments[0] = buffer;
    arguments[1] = arguments[2] = JS_UNDEFINED;
    array = JS_NewTypedArray(ctx, 3, arguments, JS_TYPED_ARRAY_UINT8);
    assert(!JS_IsException(array));
    JS_FreeValue(ctx, array);
    JS_FreeValue(ctx, buffer);
    buffer = JS_Eval(ctx, "new SharedArrayBuffer(0)", strlen("new SharedArrayBuffer(0)"),
                     "empty-shared-buffer", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(buffer));
    assert(JS_GetArrayBuffer(ctx, &size, buffer) != NULL);
    assert(size == 0 && !JS_HasException(ctx));
    JS_FreeValue(ctx, buffer);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

typedef struct SharedBufferData {
    void *ptr;
    size_t size;
    int references;
    int allocations;
    int duplications;
    int releases;
} SharedBufferData;

static void *alloc_shared(void *opaque, size_t size)
{
    SharedBufferData *data = opaque;
    assert(data->ptr == NULL && data->references == 0);
    data->ptr = calloc(1, size);
    assert(data->ptr != NULL);
    data->size = size;
    data->references = 1;
    data->allocations++;
    return data->ptr;
}

static void dup_shared(void *opaque, void *ptr)
{
    SharedBufferData *data = opaque;
    assert(ptr == data->ptr && data->references > 0);
    data->references++;
    data->duplications++;
}

static void free_shared(void *opaque, void *ptr)
{
    SharedBufferData *data = opaque;
    assert(ptr == data->ptr && data->references > 0);
    data->releases++;
    if (--data->references == 0) {
        free(ptr);
        data->ptr = NULL;
    }
}

static void test_shared_buffer_allocation(void)
{
    SharedBufferData data = { 0 };
    JSSharedArrayBufferFunctions functions = {
        alloc_shared, free_shared, dup_shared, &data,
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue buffer, array;
    JSValueConst arguments[3];
    void *ptr;
    size_t size;

    assert(rt);
    JS_SetSharedArrayBufferFunctions(rt, &functions);
    ctx = JS_NewContext(rt);
    assert(ctx);
    buffer = JS_NewArrayBuffer(ctx, NULL, 0, NULL, NULL, TRUE);
    check_empty_buffer(ctx, buffer);
    assert(data.allocations == 1 && data.duplications == 0);
    assert(data.size == 1 && data.references == 1);
    arguments[0] = buffer;
    arguments[1] = arguments[2] = JS_UNDEFINED;
    array = JS_NewTypedArray(ctx, 3, arguments, JS_TYPED_ARRAY_UINT8);
    assert(!JS_IsException(array));
    JS_FreeValue(ctx, buffer);
    assert(data.releases == 0);
    JS_FreeValue(ctx, array);
    assert(data.releases == 1 && data.references == 0);

    ptr = alloc_shared(&data, 1);
    buffer = JS_NewArrayBuffer(ctx, ptr, 0, NULL, NULL, TRUE);
    assert(!JS_IsException(buffer));
    assert(JS_GetArrayBuffer(ctx, &size, buffer) == ptr && size == 0);
    assert(data.allocations == 2 && data.duplications == 1);
    assert(data.references == 2);
    JS_FreeValue(ctx, buffer);
    assert(data.releases == 2 && data.references == 1);
    free_shared(&data, ptr);
    assert(data.releases == 3 && data.references == 0);

    buffer = JS_Eval(ctx, "new SharedArrayBuffer(0, { maxByteLength: 8 })",
                     strlen("new SharedArrayBuffer(0, { maxByteLength: 8 })"),
                     "shared-buffer-allocation", JS_EVAL_TYPE_GLOBAL);
    check_empty_buffer(ctx, buffer);
    assert(data.allocations == 3 && data.duplications == 1);
    assert(data.size == 8 && data.references == 1);
    JS_FreeValue(ctx, buffer);
    assert(data.releases == 4 && data.references == 0);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(data.ptr == NULL);
}

static void run_external_buffer(const char *source, int freed_during_eval)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, buffer;
    ExternalBufferData external = { 0, 0 };
    size_t size;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    buffer = JS_NewArrayBuffer(ctx, &external.byte, 0,
                               free_external, &external, FALSE);
    assert(!JS_IsException(buffer));
    assert(JS_GetArrayBuffer(ctx, &size, buffer) == &external.byte && size == 0);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "external", buffer) >= 0);
    JS_FreeValue(ctx, global);
    check_eval(ctx, source);
    assert(external.free_count == freed_during_eval);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    /* Finalizing a detached buffer may call the callback again with NULL. */
    assert(external.free_count >= 1);
}

static void test_empty_buffer_transfer(void)
{
    run_external_buffer("const grown = external.transfer(1);"
                        "external.detached && grown.byteLength === 1 &&"
                        "new Uint8Array(grown)[0] === 0", 1);
}

int main(int argc, char **argv)
{
    static const struct {
        const char *name;
        void (*run)(void);
    } tests[] = {
        { "buffer-allocation", test_empty_buffer_allocation },
        { "shared-buffer-allocation", test_shared_buffer_allocation },
        { "buffer-transfer", test_empty_buffer_transfer },
    };
    size_t i;
    int ran = 0;
    assert(argc <= 2);
    for (i = 0; i < countof(tests); i++) {
        if (argc == 1 || strcmp(argv[1], tests[i].name) == 0) {
            tests[i].run();
            ran++;
        }
    }
    assert(ran > 0);
    return 0;
}
