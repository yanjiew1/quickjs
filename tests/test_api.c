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

/* Public declarations must coexist with function-like inline aliases. */
#define js_malloc(ctx, size) js_malloc_inline((ctx), (size))
#define js_mallocz(ctx, size) js_mallocz_inline(ctx, size)
#define js_free(ctx, ptr) js_free_inline(ctx, ptr)
#define js_realloc(ctx, ptr, size) js_realloc_inline(ctx, ptr, size)
#define js_realloc2(ctx, ptr, size, pslack) js_realloc2_inline(ctx, ptr, size, pslack)
#define js_malloc_usable_size(ctx, ptr) js_malloc_usable_size_inline(ctx, ptr)
#include "quickjs.h"
#undef js_malloc
#undef js_mallocz
#undef js_free
#undef js_realloc
#undef js_realloc2
#undef js_malloc_usable_size
#include "cutils.h"

static uint8_t allocator_test_byte(size_t block, size_t offset)
{
    return block * 17 + (block >> 8) + offset * 31;
}

static void test_allocator_api_entry_point(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    void *(*allocate)(JSContext *, size_t);
    uint8_t *blocks[3];
    size_t i, j;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    blocks[0] = js_malloc(ctx, 17);
#define js_malloc(ctx, size) js_malloc_inline((ctx), (size))
    blocks[1] = (js_malloc)(ctx, 17);
    allocate = js_malloc;
    blocks[2] = allocate(ctx, 17);
#undef js_malloc
    for (i = 0; i < countof(blocks); i++) {
        assert(blocks[i]);
        for (j = 0; j < 17; j++)
            blocks[i][j] = allocator_test_byte(i, j);
    }
    for (i = 0; i < countof(blocks); i++) {
        for (j = 0; j < 17; j++)
            assert(blocks[i][j] == allocator_test_byte(i, j));
        js_free(ctx, blocks[i]);
    }
    blocks[0] = js_mallocz(ctx, 23);
    assert(blocks[0]);
    for (j = 0; j < 23; j++)
        assert(blocks[0][j] == 0);
    memset(blocks[0], 0xa5, 23);
    blocks[0] = js_realloc(ctx, blocks[0], 97);
    assert(blocks[0]);
    for (j = 0; j < 23; j++)
        assert(blocks[0][j] == 0xa5);
    {
        size_t slack, usable;
        blocks[0] = js_realloc2(ctx, blocks[0], 157, &slack);
        assert(blocks[0]);
        usable = js_malloc_usable_size(ctx, blocks[0]);
        assert(usable == 0 || usable >= 157);
        assert(usable == 0 || slack == usable - 157);
        for (j = 0; j < 23; j++)
            assert(blocks[0][j] == 0xa5);
    }
    assert(js_realloc(ctx, blocks[0], 0) == NULL);
    assert(!JS_HasException(ctx));
    blocks[0] = js_mallocz(ctx, 0);
    assert(blocks[0]);
    js_free(ctx, blocks[0]);
    js_free(ctx, NULL);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_allocator_capacity_and_reuse(void)
{
    JSRuntime *rt = JS_NewRuntime();
    uint8_t *blocks[4096];
    size_t sizes[countof(blocks)];
    size_t i, j, usable;

    assert(rt);
    for (i = 0; i < countof(blocks); i++) {
        sizes[i] = i % 1024 + 1;
        blocks[i] = js_malloc_rt(rt, sizes[i]);
        assert(blocks[i]);
        usable = js_malloc_usable_size_rt(rt, blocks[i]);
        assert(usable == 0 || usable >= sizes[i]);
        for (j = 0; j < sizes[i]; j++)
            blocks[i][j] = allocator_test_byte(i, j);
    }
    for (i = 0; i < countof(blocks); i++) {
        for (j = 0; j < sizes[i]; j++)
            assert(blocks[i][j] == allocator_test_byte(i, j));
    }
    for (i = 1; i < countof(blocks); i += 2)
        js_free_rt(rt, blocks[i]);
    for (i = 1; i < countof(blocks); i += 2) {
        sizes[i] = 1025 - sizes[i];
        blocks[i] = js_malloc_rt(rt, sizes[i]);
        assert(blocks[i]);
        usable = js_malloc_usable_size_rt(rt, blocks[i]);
        assert(usable == 0 || usable >= sizes[i]);
        for (j = 0; j < sizes[i]; j++)
            blocks[i][j] = allocator_test_byte(i, j);
    }
    for (i = 0; i < countof(blocks); i++) {
        for (j = 0; j < sizes[i]; j++)
            assert(blocks[i][j] == allocator_test_byte(i, j));
        js_free_rt(rt, blocks[i]);
    }
    JS_FreeRuntime(rt);
}

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
    void *allocation;
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
    data->allocation = data->ptr;
    data->size = size;
    data->references = 1;
    data->allocations++;
    return data->ptr;
}

static void *alloc_misaligned_shared(void *opaque, size_t size)
{
    SharedBufferData *data = opaque;
    assert(data->ptr == NULL && data->references == 0);
    assert(size < SIZE_MAX);
    data->allocation = calloc(1, size + 1);
    assert(data->allocation != NULL);
    data->ptr = (uint8_t *)data->allocation + 1;
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
        free(data->allocation);
        data->ptr = NULL;
        data->allocation = NULL;
    }
}

static void test_misaligned_shared_buffer_length(void)
{
    static const unsigned int capacities[] = { 0, 1, 2, 3, 4, 5, 8, 31, 32 };
    size_t capacity_index;
    int first;

    for (capacity_index = 0; capacity_index < countof(capacities); capacity_index++) {
        for (first = 0; first < 2; first++) {
            SharedBufferData data = { 0 };
            JSSharedArrayBufferFunctions functions = {
                alloc_misaligned_shared, free_shared, dup_shared, &data,
            };
            JSRuntime *runtimes[2] = { JS_NewRuntime(), JS_NewRuntime() };
            JSContext *contexts[2];
            JSValue buffer, clone, global;
            uint8_t *encoded, **pointers;
            size_t encoded_size, pointer_count, length;
            char source[128];
            int i;

            for (i = 0; i < 2; i++) {
                assert(runtimes[i]);
                JS_SetSharedArrayBufferFunctions(runtimes[i], &functions);
                contexts[i] = JS_NewContext(runtimes[i]);
                assert(contexts[i]);
            }
            snprintf(source, sizeof(source),
                     "new SharedArrayBuffer(0, { maxByteLength: %u })",
                     capacities[capacity_index]);
            buffer = JS_Eval(contexts[0], source, strlen(source),
                             "misaligned-shared-length", JS_EVAL_TYPE_GLOBAL);
            assert(!JS_IsException(buffer));
            assert(((uintptr_t)data.ptr & 3) == 1);
            encoded = JS_WriteObject2(contexts[0], &encoded_size, buffer,
                                      JS_WRITE_OBJ_SAB, &pointers, &pointer_count);
            assert(encoded && pointer_count == 1 && pointers[0] == data.ptr);
            clone = JS_ReadObject(contexts[1], encoded, encoded_size, JS_READ_OBJ_SAB);
            assert(!JS_IsException(clone));
            assert(JS_GetArrayBuffer(contexts[1], &length, clone) == data.ptr && length == 0);
            js_free(contexts[0], encoded);
            js_free(contexts[0], pointers);
            global = JS_GetGlobalObject(contexts[0]);
            assert(JS_SetPropertyStr(contexts[0], global, "shared", buffer) >= 0);
            JS_FreeValue(contexts[0], global);
            global = JS_GetGlobalObject(contexts[1]);
            assert(JS_SetPropertyStr(contexts[1], global, "shared", clone) >= 0);
            JS_FreeValue(contexts[1], global);
            check_eval(contexts[0], "shared.grow(shared.maxByteLength) === undefined");
            check_eval(contexts[1], "shared.byteLength === shared.maxByteLength");
            global = JS_GetGlobalObject(contexts[1]);
            clone = JS_GetPropertyStr(contexts[1], global, "shared");
            JS_FreeValue(contexts[1], global);
            assert(JS_GetArrayBuffer(contexts[1], &length, clone) == data.ptr);
            assert(length == capacities[capacity_index]);
            JS_FreeValue(contexts[1], clone);
            JS_FreeContext(contexts[first]);
            JS_FreeRuntime(runtimes[first]);
            assert(data.references == 1);
            check_eval(contexts[1 - first],
                       "shared.grow(shared.maxByteLength) === undefined &&"
                       " shared.byteLength === shared.maxByteLength");
            JS_FreeContext(contexts[1 - first]);
            JS_FreeRuntime(runtimes[1 - first]);
            assert(data.references == 0 && data.ptr == NULL && data.allocation == NULL);
            assert(data.allocations == 1 && data.duplications == 1 && data.releases == 2);
        }
    }
}

static void check_shared_serialization_rejection(JSContext *ctx, JSValueConst buffer)
{
    JSValue exception, name;
    const char *text;
    uint8_t *encoded, **pointers = (uint8_t **)(uintptr_t)1;
    size_t encoded_size = 1, pointer_count = 1;

    encoded = JS_WriteObject2(ctx, &encoded_size, buffer,
                              JS_WRITE_OBJ_SAB | JS_WRITE_OBJ_REFERENCE,
                              &pointers, &pointer_count);
    assert(!encoded && encoded_size == 0 && pointers == NULL && pointer_count == 0);
    exception = JS_GetException(ctx);
    name = JS_GetPropertyStr(ctx, exception, "name");
    text = JS_ToCString(ctx, name);
    assert(text && !strcmp(text, "TypeError"));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, name);
    JS_FreeValue(ctx, exception);
    encoded_size = 1;
    encoded = JS_WriteObject(ctx, &encoded_size, buffer, JS_WRITE_OBJ_SAB);
    assert(!encoded && encoded_size == 0);
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
}

static void test_shared_serialization_lifetime(void)
{
    static const char *sources[] = {
        "new SharedArrayBuffer(0)",
        "new SharedArrayBuffer(8)",
        "new SharedArrayBuffer(0, { maxByteLength: 0 })",
        "new SharedArrayBuffer(1, { maxByteLength: 32 })",
    };
    size_t source_index;
    int first, mode, missing;

    for (source_index = 0; source_index < countof(sources); source_index++) {
        for (mode = 0; mode < 3; mode++) {
            for (first = 0; first < 2; first++) {
                SharedBufferData data = { 0 };
                JSSharedArrayBufferFunctions functions = {
                    alloc_shared, free_shared, dup_shared, &data,
                };
                JSSharedArrayBufferFunctions no_allocator = functions;
                JSRuntime *runtimes[2] = { JS_NewRuntime(), JS_NewRuntime() };
                JSContext *contexts[2];
                JSValue buffer, global;
                int i;

                assert(runtimes[0] && runtimes[1]);
                JS_SetSharedArrayBufferFunctions(runtimes[1], &functions);
                no_allocator.sab_alloc = NULL;
                if (mode == 1)
                    JS_SetSharedArrayBufferFunctions(runtimes[0], &no_allocator);
                for (i = 0; i < 2; i++) {
                    contexts[i] = JS_NewContext(runtimes[i]);
                    assert(contexts[i]);
                }
                buffer = JS_Eval(contexts[0], sources[source_index],
                                 strlen(sources[source_index]),
                                 "shared-serialization-default-owner", JS_EVAL_TYPE_GLOBAL);
                assert(!JS_IsException(buffer));
                if (mode == 2)
                    JS_SetSharedArrayBufferFunctions(runtimes[0], &functions);
                check_shared_serialization_rejection(contexts[0], buffer);
                assert(data.allocations == 0 && data.duplications == 0 &&
                       data.releases == 0 && data.references == 0);
                global = JS_GetGlobalObject(contexts[0]);
                assert(JS_SetPropertyStr(contexts[0], global, "shared", buffer) >= 0);
                JS_FreeValue(contexts[0], global);
                JS_FreeContext(contexts[first]);
                JS_FreeRuntime(runtimes[first]);
                JS_FreeContext(contexts[1 - first]);
                JS_FreeRuntime(runtimes[1 - first]);
                assert(data.allocations == 0 && data.duplications == 0 &&
                       data.releases == 0 && data.references == 0);
            }
        }
        for (missing = 0; missing < 2; missing++) {
            SharedBufferData data = { 0 };
            JSSharedArrayBufferFunctions functions = {
                alloc_shared, free_shared, dup_shared, &data,
            };
            JSSharedArrayBufferFunctions incomplete = functions;
            JSRuntime *rt = JS_NewRuntime();
            JSContext *ctx;
            JSValue buffer;

            assert(rt);
            JS_SetSharedArrayBufferFunctions(rt, &functions);
            ctx = JS_NewContext(rt);
            assert(ctx);
            buffer = JS_Eval(ctx, sources[source_index], strlen(sources[source_index]),
                             "shared-serialization-incomplete-owner", JS_EVAL_TYPE_GLOBAL);
            assert(!JS_IsException(buffer));
            if (missing == 0)
                incomplete.sab_dup = NULL;
            else
                incomplete.sab_free = NULL;
            JS_SetSharedArrayBufferFunctions(rt, &incomplete);
            check_shared_serialization_rejection(ctx, buffer);
            assert(data.allocations == 1 && data.references == 1 &&
                   data.duplications == 0 && data.releases == 0);
            JS_FreeValue(ctx, buffer);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            assert(data.allocations == 1 && data.duplications == 0 &&
                   data.releases == 1 && data.references == 0 && data.ptr == NULL);
        }
        {
            SharedBufferData data = { 0 };
            JSSharedArrayBufferFunctions functions = {
                alloc_shared, NULL, dup_shared, &data,
            };
            JSRuntime *rt = JS_NewRuntime();
            JSContext *ctx;
            char script[256];

            assert(rt);
            JS_SetSharedArrayBufferFunctions(rt, &functions);
            ctx = JS_NewContext(rt);
            assert(ctx);
            snprintf(script, sizeof(script),
                     "(() => { try { (%s); return false; }"
                     " catch (e) { return e instanceof TypeError; } })()",
                     sources[source_index]);
            check_eval(ctx, script);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            assert(data.allocations == 0 && data.references == 0 &&
                   data.duplications == 0 && data.releases == 0);
        }
    }
}

static void test_external_shared_buffer_owner(void)
{
    static const size_t lengths[] = { 0, 8 };
    size_t length_index;
    int first;

    for (length_index = 0; length_index < countof(lengths); length_index++) {
        for (first = 0; first < 2; first++) {
            SharedBufferData data = { 0 };
            JSSharedArrayBufferFunctions functions = {
                NULL, free_shared, dup_shared, &data,
            };
            JSSharedArrayBufferFunctions none = { 0 };
            JSRuntime *runtimes[2] = { JS_NewRuntime(), JS_NewRuntime() };
            JSContext *contexts[2];
            JSValue buffer, clone, global;
            uint8_t *ptr, *encoded, **pointers;
            size_t encoded_size, pointer_count, length;
            int i;

            ptr = alloc_shared(&data, lengths[length_index] ? lengths[length_index] : 1);
            for (i = 0; i < 2; i++) {
                assert(runtimes[i]);
                JS_SetSharedArrayBufferFunctions(runtimes[i], &functions);
                contexts[i] = JS_NewContext(runtimes[i]);
                assert(contexts[i]);
            }
            buffer = JS_NewArrayBuffer(contexts[0], ptr, lengths[length_index],
                                       NULL, NULL, TRUE);
            assert(!JS_IsException(buffer));
            assert(data.references == 2 && data.duplications == 1);
            free_shared(&data, ptr);
            encoded = JS_WriteObject2(contexts[0], &encoded_size, buffer,
                                      JS_WRITE_OBJ_SAB, &pointers, &pointer_count);
            assert(encoded && pointer_count == 1 && pointers[0] == ptr);
            clone = JS_ReadObject(contexts[1], encoded, encoded_size, JS_READ_OBJ_SAB);
            assert(!JS_IsException(clone));
            assert(JS_GetArrayBuffer(contexts[1], &length, clone) == ptr);
            assert(length == lengths[length_index]);
            assert(data.references == 2 && data.duplications == 2 && data.releases == 1);
            js_free(contexts[0], encoded);
            js_free(contexts[0], pointers);
            global = JS_GetGlobalObject(contexts[0]);
            assert(JS_SetPropertyStr(contexts[0], global, "shared", buffer) >= 0);
            JS_FreeValue(contexts[0], global);
            global = JS_GetGlobalObject(contexts[1]);
            assert(JS_SetPropertyStr(contexts[1], global, "shared", clone) >= 0);
            JS_FreeValue(contexts[1], global);
            for (i = 0; i < 2; i++)
                JS_SetSharedArrayBufferFunctions(runtimes[i], &none);
            JS_FreeContext(contexts[first]);
            JS_FreeRuntime(runtimes[first]);
            assert(data.references == 1);
            check_eval(contexts[1 - first],
                       "shared.byteLength === new Uint8Array(shared).length");
            JS_FreeContext(contexts[1 - first]);
            JS_FreeRuntime(runtimes[1 - first]);
            assert(data.allocations == 1 && data.duplications == 2 &&
                   data.releases == 3 && data.references == 0 && data.ptr == NULL);
        }
        {
            SharedBufferData data = { 0 };
            JSSharedArrayBufferFunctions functions = {
                NULL, NULL, dup_shared, &data,
            };
            JSRuntime *rt = JS_NewRuntime();
            JSContext *ctx;
            JSValue buffer, exception;
            uint8_t *ptr = alloc_shared(&data, lengths[length_index] ? lengths[length_index] : 1);

            assert(rt);
            JS_SetSharedArrayBufferFunctions(rt, &functions);
            ctx = JS_NewContext(rt);
            assert(ctx);
            buffer = JS_NewArrayBuffer(ctx, ptr, lengths[length_index], NULL, NULL, TRUE);
            assert(JS_IsException(buffer));
            exception = JS_GetException(ctx);
            JS_FreeValue(ctx, exception);
            assert(data.references == 1 && data.duplications == 0 && data.releases == 0);
            free_shared(&data, ptr);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            assert(data.references == 0 && data.releases == 1);
        }
    }
}

static void test_shared_clone_release_callback(void)
{
    SharedBufferData data = { 0 };
    JSSharedArrayBufferFunctions functions = {
        alloc_shared, free_shared, dup_shared, &data,
    };
    JSRuntime *runtimes[2] = { JS_NewRuntime(), JS_NewRuntime() };
    JSContext *contexts[2];
    JSValue buffer, clone, exception;
    uint8_t *encoded, **pointers;
    size_t encoded_size, pointer_count;
    int i;
    const char *source = "new SharedArrayBuffer(8, { maxByteLength: 32 })";

    for (i = 0; i < 2; i++) {
        assert(runtimes[i]);
        contexts[i] = JS_NewContext(runtimes[i]);
        assert(contexts[i]);
    }
    JS_SetSharedArrayBufferFunctions(runtimes[0], &functions);
    buffer = JS_Eval(contexts[0], source, strlen(source),
                     "shared-clone-missing-release", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(buffer));
    encoded = JS_WriteObject2(contexts[0], &encoded_size, buffer,
                              JS_WRITE_OBJ_SAB, &pointers, &pointer_count);
    assert(encoded && pointer_count == 1);
    for (i = 0; i < 2; i++) {
        JSSharedArrayBufferFunctions incomplete = functions;
        incomplete.sab_alloc = NULL;
        if (i == 0)
            incomplete.sab_free = NULL;
        else
            incomplete.sab_dup = NULL;
        JS_SetSharedArrayBufferFunctions(runtimes[1], &incomplete);
        clone = JS_ReadObject(contexts[1], encoded, encoded_size, JS_READ_OBJ_SAB);
        assert(JS_IsException(clone));
        exception = JS_GetException(contexts[1]);
        JS_FreeValue(contexts[1], exception);
        assert(data.allocations == 1 && data.references == 1 &&
               data.duplications == 0 && data.releases == 0);
    }
    js_free(contexts[0], encoded);
    js_free(contexts[0], pointers);
    JS_FreeValue(contexts[0], buffer);
    for (i = 0; i < 2; i++) {
        JS_FreeContext(contexts[i]);
        JS_FreeRuntime(runtimes[i]);
    }
    assert(data.allocations == 1 && data.references == 0 &&
           data.duplications == 0 && data.releases == 1);
}

static void test_default_shared_buffer_growth(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue buffer, global;
    uint8_t *ptr;
    size_t size;
    const char *script = "new SharedArrayBuffer(1, { maxByteLength: 4096 })";

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    buffer = JS_Eval(ctx, script, strlen(script), "shared-buffer-capacity",
                     JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(buffer));
    ptr = JS_GetArrayBuffer(ctx, &size, buffer);
    assert(ptr && size == 1);
    assert(js_malloc_usable_size(ctx, ptr) >= 4096);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "sharedBuffer", buffer) >= 0);
    JS_FreeValue(ctx, global);
    check_eval(ctx,
        "(() => {"
        " const array = new Uint8Array(sharedBuffer);"
        " const view = new DataView(sharedBuffer);"
        " sharedBuffer.grow(4096);"
        " if (array.length !== 4096 || view.byteLength !== 4096)"
        "   throw Error('shared view length');"
        " if (array[4095] !== 0) throw Error('shared growth zero fill');"
        " array[4095] = 42;"
        " return view.getUint8(4095) === 42;"
        "})()");
    assert(ptr[4095] == 42);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_shared_buffer_clone_views(void)
{
    SharedBufferData data = { 0 };
    JSSharedArrayBufferFunctions functions = {
        alloc_shared, free_shared, dup_shared, &data,
    };
    JSRuntime *runtimes[2] = { JS_NewRuntime(), JS_NewRuntime() };
    JSContext *contexts[2];
    JSValue buffer, clone, global;
    uint8_t *encoded, **pointers;
    size_t encoded_size, pointer_count, length;
    int i;
    const char *source = "new SharedArrayBuffer(8, { maxByteLength: 32 })";

    for (i = 0; i < 2; i++) {
        assert(runtimes[i]);
        JS_SetSharedArrayBufferFunctions(runtimes[i], &functions);
        contexts[i] = JS_NewContext(runtimes[i]);
        assert(contexts[i]);
    }
    buffer = JS_Eval(contexts[0], source, strlen(source),
                     "shared-clone", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(buffer));
    encoded = JS_WriteObject2(contexts[0], &encoded_size, buffer,
                              JS_WRITE_OBJ_SAB | JS_WRITE_OBJ_REFERENCE,
                              &pointers, &pointer_count);
    assert(encoded && pointer_count == 1 && pointers[0] == data.ptr);
    clone = JS_ReadObject(contexts[1], encoded, encoded_size,
                           JS_READ_OBJ_SAB | JS_READ_OBJ_REFERENCE);
    assert(!JS_IsException(clone));
    assert(data.allocations == 1 && data.references == 2);
    assert(JS_GetArrayBuffer(contexts[1], &length, clone) == data.ptr);
    assert(length == 8);
    js_free(contexts[0], encoded);
    js_free(contexts[0], pointers);
    global = JS_GetGlobalObject(contexts[0]);
    assert(JS_SetPropertyStr(contexts[0], global, "shared", buffer) >= 0);
    JS_FreeValue(contexts[0], global);
    global = JS_GetGlobalObject(contexts[1]);
    assert(JS_SetPropertyStr(contexts[1], global, "shared", clone) >= 0);
    JS_FreeValue(contexts[1], global);
    check_eval(contexts[1],
        "globalThis.views = {};"
        "for (const name of ['read', 'write', 'stringWrite', 'keys', 'has',"
        " 'descriptor', 'define', 'delete', 'forin', 'iterator', 'length'])"
        " views[name] = new Uint8Array(shared);"
        "globalThis.fixed = new Uint8Array(shared, 0, 8);"
        "globalThis.atomic = new Int32Array(shared);"
        "globalThis.dataView = new DataView(shared);"
        "globalThis.fixedDataView = new DataView(shared, 0, 8); true");
    check_eval(contexts[0], "shared.grow(24); shared.byteLength === 24");
    check_eval(contexts[1],
        "(() => {"
        " if (shared.byteLength !== 24) throw Error('shared length');"
        " if (views.read[20] !== 0) throw Error('indexed read');"
        " views.write[20] = 42;"
        " if (views.read[20] !== 42) throw Error('indexed write');"
        " views.stringWrite['21'] = 43;"
        " if (views.read[21] !== 43) throw Error('string indexed write');"
        " if (Object.keys(views.keys).length !== 24) throw Error('keys');"
        " if (!(20 in views.has)) throw Error('has');"
        " if (!Object.getOwnPropertyDescriptor(views.descriptor, '20'))"
        "   throw Error('descriptor');"
        " Object.defineProperty(views.define, '22', { value: 44 });"
        " if (views.read[22] !== 44) throw Error('define');"
        " if (Reflect.deleteProperty(views.delete, '20')) throw Error('delete');"
        " let count = 0; for (const key in views.forin) count++;"
        " if (count !== 24) throw Error('for-in');"
        " if ([...views.iterator].length !== 24) throw Error('iterator');"
        " if (views.length.length !== 24 || views.length.byteLength !== 24)"
        "   throw Error('view length');"
        " if (fixed.length !== 8 || fixedDataView.byteLength !== 8)"
        "   throw Error('fixed length');"
        " dataView.setUint8(23, 45);"
        " if (dataView.byteLength !== 24 || dataView.getUint8(23) !== 45)"
        "   throw Error('DataView');"
        " Atomics.store(atomic, 4, 46);"
        " if (Atomics.load(atomic, 4) !== 46) throw Error('Atomics');"
        " try { shared.grow(16); throw Error('grow shrank'); }"
        " catch (e) { if (!(e instanceof RangeError)) throw e; }"
        " shared.grow(32); return shared.byteLength === 32;"
        "})()");
    check_eval(contexts[0], "shared.byteLength === 32");
    JS_FreeContext(contexts[0]);
    JS_FreeRuntime(runtimes[0]);
    assert(data.references == 1);
    check_eval(contexts[1],
        "views.length.length === 32 && dataView.byteLength === 32 &&"
        " views.read[31] === 0 && shared.grow(32) === undefined");
    JS_FreeContext(contexts[1]);
    JS_FreeRuntime(runtimes[1]);
    assert(data.references == 0 && data.ptr == NULL);
    assert(data.allocations == 1 && data.duplications == 1 && data.releases == 2);
}

static void test_shared_buffer_queued_clone(void)
{
    static const char *sources[] = {
        "new SharedArrayBuffer(0, { maxByteLength: 0 })",
        "new SharedArrayBuffer(0, { maxByteLength: 8 })",
        "new SharedArrayBuffer(0)",
    };
    size_t i;
    for (i = 0; i < countof(sources); i++) {
        SharedBufferData data = { 0 };
        JSSharedArrayBufferFunctions functions = {
            alloc_shared, free_shared, dup_shared, &data,
        };
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx;
        JSValue buffer, clone, global;
        uint8_t *encoded, *queued, **pointers;
        size_t encoded_size, pointer_count, length;
        assert(rt);
        JS_SetSharedArrayBufferFunctions(rt, &functions);
        ctx = JS_NewContext(rt);
        assert(ctx);
        buffer = JS_Eval(ctx, sources[i], strlen(sources[i]),
                         "shared-queued-clone", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(buffer));
        encoded = JS_WriteObject2(ctx, &encoded_size, buffer,
                                  JS_WRITE_OBJ_SAB | JS_WRITE_OBJ_REFERENCE,
                                  &pointers, &pointer_count);
        assert(encoded && pointer_count == 1 && pointers[0] == data.ptr);
        queued = malloc(encoded_size);
        assert(queued);
        memcpy(queued, encoded, encoded_size);
        dup_shared(&data, pointers[0]);
        js_free(ctx, encoded);
        js_free(ctx, pointers);
        JS_FreeValue(ctx, buffer);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(data.references == 1);
        rt = JS_NewRuntime();
        assert(rt);
        JS_SetSharedArrayBufferFunctions(rt, &functions);
        ctx = JS_NewContext(rt);
        assert(ctx);
        clone = JS_ReadObject(ctx, queued, encoded_size,
                              JS_READ_OBJ_SAB | JS_READ_OBJ_REFERENCE);
        free(queued);
        assert(!JS_IsException(clone));
        free_shared(&data, data.ptr);
        assert(data.references == 1);
        assert(JS_GetArrayBuffer(ctx, &length, clone) == data.ptr && length == 0);
        global = JS_GetGlobalObject(ctx);
        assert(JS_SetPropertyStr(ctx, global, "shared", clone) >= 0);
        JS_FreeValue(ctx, global);
        if (i == 0) {
            check_eval(ctx, "shared.grow(0) === undefined && shared.byteLength === 0");
        } else if (i == 1) {
            check_eval(ctx, "shared.grow(8) === undefined && shared.byteLength === 8 &&"
                           " [...new Uint8Array(shared)].every(x => x === 0)");
        } else {
            check_eval(ctx, "shared.growable === false && shared.byteLength === 0");
        }
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(data.references == 0 && data.ptr == NULL);
        assert(data.allocations == 1 && data.duplications == 2 && data.releases == 3);
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
    assert(data.size > 8 && data.references == 1);
    JS_FreeValue(ctx, buffer);
    assert(data.releases == 4 && data.references == 0);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(data.ptr == NULL);
}

static void check_typed_array_arguments(JSContext *ctx, int argc,
                                       JSValueConst *argv, size_t expected_length)
{
    JSValue array, buffer;
    size_t offset, length, element_size;

    array = JS_NewTypedArray(ctx, argc, argv, JS_TYPED_ARRAY_UINT8);
    assert(!JS_IsException(array));
    buffer = JS_GetTypedArrayBuffer(ctx, array, &offset, &length, &element_size);
    assert(!JS_IsException(buffer));
    assert(offset == 0 && length == expected_length && element_size == 1);
    JS_FreeValue(ctx, buffer);
    JS_FreeValue(ctx, array);
}

static void test_typed_array_arguments(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue value, buffer, array;
    JSValueConst two_args[2], three_args[3];

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    check_typed_array_arguments(ctx, 0, NULL, 0);
    value = JS_NewInt32(ctx, 0);
    check_typed_array_arguments(ctx, 1, &value, 0);
    value = JS_NewInt32(ctx, 2);
    check_typed_array_arguments(ctx, 1, &value, 2);
    buffer = JS_Eval(ctx, "new ArrayBuffer(2)", strlen("new ArrayBuffer(2)"),
                     "typed-array-arguments", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(buffer));
    check_typed_array_arguments(ctx, 1, &buffer, 2);
    two_args[0] = buffer;
    two_args[1] = JS_UNDEFINED;
    check_typed_array_arguments(ctx, 2, two_args, 2);
    three_args[0] = buffer;
    three_args[1] = JS_NewInt32(ctx, 0);
    three_args[2] = JS_NewInt32(ctx, 1);
    check_typed_array_arguments(ctx, 3, three_args, 1);
    JS_FreeValue(ctx, buffer);
    array = JS_NewTypedArray(ctx, 0, NULL,
                             (JSTypedArrayEnum)(JS_TYPED_ARRAY_FLOAT64 + 1));
    assert(JS_IsException(array) && JS_HasException(ctx));
    JS_FreeValue(ctx, JS_GetException(ctx));
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_empty_atom(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSAtom a, b;
    const char *text;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    a = JS_NewAtomLen(ctx, NULL, 0);
    b = JS_NewAtomLen(ctx, NULL, 0);
    assert(a != JS_ATOM_NULL && a == b);
    text = JS_AtomToCString(ctx, a);
    assert(text && text[0] == '\0');
    JS_FreeCString(ctx, text);
    JS_FreeAtom(ctx, a);
    JS_FreeAtom(ctx, b);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
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

static void run_buffer_without_free_callback(uint8_t *data, size_t len,
                                            const char *source)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, buffer;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    buffer = JS_NewArrayBuffer(ctx, data, len, NULL, NULL, FALSE);
    assert(!JS_IsException(buffer));
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "external", buffer) >= 0);
    JS_FreeValue(ctx, global);
    check_eval(ctx, source);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_buffer_transfer_without_free_callback(void)
{
    static const char *methods[] = { "transfer", "transferToFixedLength" };
    uint8_t data[] = { 17, 34 };
    char source[512];
    size_t i, len, new_len, copy_len;
    int n;

    for (i = 0; i < countof(methods); i++) {
        for (len = 0; len <= sizeof(data); len += sizeof(data)) {
            for (new_len = 0; new_len <= sizeof(data) + 1; new_len++) {
                copy_len = len < new_len ? len : new_len;
                n = snprintf(source, sizeof(source),
                             "const original = new Uint8Array(external);"
                             "const moved = external.%s(%zu);"
                             "const bytes = new Uint8Array(moved);"
                             "external.detached && original.length === 0 &&"
                             "moved.byteLength === %zu &&"
                             "[...bytes].every((value, index) => "
                             "value === (index < %zu ? (index + 1) * 17 : 0))",
                             methods[i], new_len, new_len, copy_len);
                assert(n >= 0 && (size_t)n < sizeof(source));
                run_buffer_without_free_callback(data, len, source);
                assert(data[0] == 17 && data[1] == 34);
            }
        }
    }
}

static int regexp_test_interrupt(JSRuntime *rt, void *opaque)
{
    unsigned int *count = opaque;
    (void)rt;
    return ++*count >= 4;
}

static void test_regexp_interrupt(void)
{
    static const char *const sources[] = {
        "/(?:a+)+b/.test('a'.repeat(64))",
        "/a{65536}/.test('a'.repeat(65536))",
        "/(?:a|b)*c/.test('a'.repeat(65536))",
        "/a*?b/.test('a'.repeat(65536))",
    };
    size_t i;

    for (i = 0; i < countof(sources); i++) {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx;
        JSValue result, exception;
        const char *message;
        unsigned int count = 0;

        assert(rt);
        ctx = JS_NewContext(rt);
        assert(ctx);
        JS_SetInterruptHandler(rt, regexp_test_interrupt, &count);
        result = JS_Eval(ctx, sources[i], strlen(sources[i]),
                         "<regexp-interrupt>", JS_EVAL_TYPE_GLOBAL);
        assert(JS_IsException(result));
        assert(count == 4);
        JS_SetInterruptHandler(rt, NULL, NULL);
        exception = JS_GetException(ctx);
        message = JS_ToCString(ctx, exception);
        assert(message && strstr(message, "interrupted"));
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, exception);
        JS_FreeValue(ctx, result);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
}

static void test_typed_array_external_overlap(void)
{
    uint16_t storage[3];
    uint8_t *data = (uint8_t *)storage;
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, source, target, result;
    size_t i;
    static const char script[] =
        "const target = new Uint16Array(targetBuffer);"
        "target.set(new Uint8Array(sourceBuffer));"
        "if (target[0] !== 2 || target[1] !== 3)"
        "    throw Error('external buffer overlap');";

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    for (i = 0; i < sizeof(storage); i++)
        data[i] = i + 1;
    source = JS_NewArrayBuffer(ctx, data + 1, 2, NULL, NULL, FALSE);
    target = JS_NewArrayBuffer(ctx, data + 2, 4, NULL, NULL, FALSE);
    assert(!JS_IsException(source));
    assert(!JS_IsException(target));
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "sourceBuffer", source) >= 0);
    assert(JS_SetPropertyStr(ctx, global, "targetBuffer", target) >= 0);
    JS_FreeValue(ctx, global);
    result = JS_Eval(ctx, script, sizeof(script) - 1,
                     "<typed-array-overlap>", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_iterator_constructor_realm(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx[2];
    JSValue global, other_global, other_iterator;
    int i;

    assert(rt);
    ctx[0] = JS_NewContext(rt);
    ctx[1] = JS_NewContext(rt);
    assert(ctx[0] && ctx[1]);
    for (i = 0; i < 2; i++) {
        other_global = JS_GetGlobalObject(ctx[1 - i]);
        other_iterator = JS_GetPropertyStr(ctx[1 - i], other_global,
                                           "Iterator");
        JS_FreeValue(ctx[1 - i], other_global);
        assert(!JS_IsException(other_iterator));
        global = JS_GetGlobalObject(ctx[i]);
        assert(JS_SetPropertyStr(ctx[i], global, "otherIterator",
                                 other_iterator) >= 0);
        JS_FreeValue(ctx[i], global);
        check_eval(ctx[i],
            "(() => {"
            " const result = Reflect.construct(Iterator, [], otherIterator);"
            " if (Object.getPrototypeOf(result) !== otherIterator.prototype)"
            "   throw Error('foreign Iterator prototype');"
            " if (result instanceof Iterator)"
            "   throw Error('wrong Iterator realm');"
            " return true;"
            "})()");
    }
    JS_FreeContext(ctx[1]);
    JS_FreeContext(ctx[0]);
    JS_FreeRuntime(rt);
}

static void test_stripped_function_to_string(void)
{
    static const char *const functions[] = {
        "(function named() {})",
        "(function* named() {})",
        "(async function named() {})",
        "(async function* named() {})",
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue function, global;
    size_t i;

    assert(rt);
    JS_SetStripInfo(rt, JS_STRIP_SOURCE);
    ctx = JS_NewContext(rt);
    assert(ctx);
    for (i = 0; i < countof(functions); i++) {
        function = JS_Eval(ctx, functions[i], strlen(functions[i]),
                           "<stripped-function>", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(function));
        global = JS_GetGlobalObject(ctx);
        assert(JS_SetPropertyStr(ctx, global, "strippedFunction", function) >= 0);
        JS_FreeValue(ctx, global);
        check_eval(ctx,
            "(() => {"
            " Object.defineProperty(strippedFunction, 'name', {"
            "   get() { throw Error('source-stripped name read'); }"
            " });"
            " return Function.prototype.toString.call(strippedFunction) ==="
            "   'function () {\\n    [native code]\\n}';"
            "})()");
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue native_name_callback(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    return JS_UNDEFINED;
}

static void test_native_function_initial_name(void)
{
    static const char *const invalid[] = {
        "a b", "x-y", "/* injected */", "a) {}", "get ", "[Symbol.]",
    };
    char name[] = "hostFunction";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, function;
    size_t i;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    global = JS_GetGlobalObject(ctx);
    function = JS_NewCFunction(ctx, native_name_callback, name, 0);
    assert(!JS_IsException(function));
    memset(name, 'x', sizeof(name) - 1);
    assert(JS_SetPropertyStr(ctx, global, "hostFunction", function) >= 0);
    check_eval(ctx,
        "(() => {"
        " Object.defineProperty(hostFunction, 'name', {"
        "   get() { throw Error('host name read'); }"
        " });"
        " return hostFunction.toString() ==="
        "   'function hostFunction() {\\n    [native code]\\n}';"
        "})()");
    for (i = 0; i < countof(invalid); i++) {
        function = JS_NewCFunction(ctx, native_name_callback, invalid[i], 0);
        assert(!JS_IsException(function));
        assert(JS_SetPropertyStr(ctx, global, "hostFunction", function) >= 0);
        check_eval(ctx,
            "hostFunction.toString() ==="
            " 'function () {\\n    [native code]\\n}'");
    }
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_allocator_size_overflow(void)
{
    static const size_t requests[] = { SIZE_MAX, SIZE_MAX - 1, SIZE_MAX - 7 };
    static const size_t initial_sizes[] = { 0, 31, 1024 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    uint8_t *ptr;
    JSValue exception;
    size_t i, j, k;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    for (i = 0; i < countof(requests); i++) {
        assert(js_malloc_rt(rt, requests[i]) == NULL);
        assert(js_realloc_rt(rt, NULL, requests[i]) == NULL);
        assert(js_malloc(ctx, requests[i]) == NULL);
        assert(JS_HasException(ctx));
        exception = JS_GetException(ctx);
        JS_FreeValue(ctx, exception);
        assert(js_mallocz(ctx, requests[i]) == NULL);
        assert(JS_HasException(ctx));
        exception = JS_GetException(ctx);
        JS_FreeValue(ctx, exception);
    }
    for (i = 0; i < countof(initial_sizes); i++) {
        ptr = js_malloc_rt(rt, initial_sizes[i]);
        assert(ptr);
        memset(ptr, 0xa5, initial_sizes[i]);
        for (j = 0; j < countof(requests); j++) {
            assert(js_realloc_rt(rt, ptr, requests[j]) == NULL);
            for (k = 0; k < initial_sizes[i]; k++)
                assert(ptr[k] == 0xa5);
            assert(js_realloc(ctx, ptr, requests[j]) == NULL);
            assert(JS_HasException(ctx));
            exception = JS_GetException(ctx);
            JS_FreeValue(ctx, exception);
            for (k = 0; k < initial_sizes[i]; k++)
                assert(ptr[k] == 0xa5);
        }
        js_free_rt(rt, ptr);
    }
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(int argc, char **argv)
{
    static const struct {
        const char *name;
        void (*run)(void);
    } tests[] = {
        { "allocator-overflow", test_allocator_size_overflow },
        { "native-name", test_native_function_initial_name },
        { "stripped-function", test_stripped_function_to_string },
        { "iterator-realm", test_iterator_constructor_realm },
        { "typed-array-overlap", test_typed_array_external_overlap },
        { "regexp-interrupt", test_regexp_interrupt },
        { "allocator-api", test_allocator_api_entry_point },
        { "allocator-capacity", test_allocator_capacity_and_reuse },
        { "buffer-allocation", test_empty_buffer_allocation },
        { "shared-buffer-allocation", test_shared_buffer_allocation },
        { "shared-buffer-growth", test_default_shared_buffer_growth },
        { "shared-buffer-clone", test_shared_buffer_clone_views },
        { "shared-buffer-queued-clone", test_shared_buffer_queued_clone },
        { "shared-buffer-misaligned-length", test_misaligned_shared_buffer_length },
        { "shared-buffer-serialization-lifetime", test_shared_serialization_lifetime },
        { "shared-buffer-external-owner", test_external_shared_buffer_owner },
        { "shared-buffer-clone-release", test_shared_clone_release_callback },
        { "typed-array-arguments", test_typed_array_arguments },
        { "atom", test_empty_atom },
        { "buffer-transfer", test_empty_buffer_transfer },
        { "buffer-transfer-no-free", test_buffer_transfer_without_free_callback },
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
