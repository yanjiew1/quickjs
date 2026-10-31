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
#include <limits.h>
#include <math.h>
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
#include "quickjs-libc.h"
#undef js_malloc
#undef js_mallocz
#undef js_free
#undef js_realloc
#undef js_realloc2
#undef js_malloc_usable_size
#include "cutils.h"
#include "oom-allocator.h"
#include "pool-padding.h"
#include "qjsc-allocation-probe.h"

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

typedef struct ReentrantBufferData {
    JSContext *ctx;
    JSValueConst buffer;
    int free_count;
    int saw_detached;
} ReentrantBufferData;

static void reentrant_buffer_free(JSRuntime *rt, void *opaque, void *ptr)
{
    ReentrantBufferData *state = opaque;
    size_t size = SIZE_MAX;
    uint8_t *data;

    (void)rt;
    (void)ptr;
    state->free_count++;
    if (state->free_count != 1)
        return;
    data = JS_GetArrayBuffer(state->ctx, &size, state->buffer);
    state->saw_detached = data == NULL && size == 0 &&
                          JS_HasException(state->ctx);
    if (JS_HasException(state->ctx))
        JS_FreeValue(state->ctx, JS_GetException(state->ctx));
    JS_DetachArrayBuffer(state->ctx, state->buffer);
}

static void test_reentrant_buffer_free(void)
{
    int transfer;

    for (transfer = 0; transfer < 2; transfer++) {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx;
        JSValue buffer, global, result;
        uint8_t byte = 42;
        ReentrantBufferData state = { 0 };

        assert(rt);
        ctx = JS_NewContext(rt);
        assert(ctx);
        state.ctx = ctx;
        buffer = JS_NewArrayBuffer(ctx, &byte, 1, reentrant_buffer_free,
                                   &state, FALSE);
        assert(!JS_IsException(buffer));
        state.buffer = buffer;
        if (transfer) {
            global = JS_GetGlobalObject(ctx);
            assert(JS_SetPropertyStr(ctx, global, "external",
                                     JS_DupValue(ctx, buffer)) >= 0);
            result = JS_Eval(ctx, "external.transfer(2)",
                             sizeof("external.transfer(2)") - 1,
                             "buffer-reentrant-free", JS_EVAL_TYPE_GLOBAL);
            assert(!JS_IsException(result));
            JS_FreeValue(ctx, result);
            assert(JS_SetPropertyStr(ctx, global, "external", JS_UNDEFINED) >= 0);
            JS_FreeValue(ctx, global);
        } else {
            JS_DetachArrayBuffer(ctx, buffer);
        }
        assert(state.free_count == 1 && state.saw_detached);
        JS_FreeValue(ctx, buffer);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
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

static void *fail_alloc_shared(void *opaque, size_t size)
{
    SharedBufferData *data = opaque;

    assert(size >= 1);
    data->allocations++;
    return NULL;
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

static void test_shared_buffer_length_alignment(void)
{
    static const unsigned int capacities[] = { 0, 1, 2, 3, 4, 5, 8, 31, 32 };
    size_t capacity_index;
    int first;

    for (capacity_index = 0; capacity_index < countof(capacities); capacity_index++) {
        for (first = 0; first < 2; first++) {
            SharedBufferData data = { 0 };
            JSSharedArrayBufferFunctions functions = {
                alloc_shared, free_shared, dup_shared, &data,
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
                             "shared-length-alignment", JS_EVAL_TYPE_GLOBAL);
            assert(!JS_IsException(buffer));
            assert(((uintptr_t)data.ptr & 3) == 0);
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

static void check_type_error(JSContext *ctx)
{
    JSValue exception = JS_GetException(ctx);
    JSValue name = JS_GetPropertyStr(ctx, exception, "name");
    const char *text = JS_ToCString(ctx, name);
    assert(text && !strcmp(text, "TypeError"));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, name);
    JS_FreeValue(ctx, exception);
}

static void count_unclaimed_shared_free(JSRuntime *rt, void *opaque, void *ptr)
{
    int *count = opaque;
    (*count)++;
}

typedef struct SwitchingSharedAllocation {
    SharedBufferData data;
    JSRuntime *rt;
    JSSharedArrayBufferFunctions replacement;
} SwitchingSharedAllocation;

static void *alloc_misaligned_and_switch(void *opaque, size_t size)
{
    SwitchingSharedAllocation *owner = opaque;
    void *ptr = alloc_misaligned_shared(&owner->data, size);
    JS_SetSharedArrayBufferFunctions(owner->rt, &owner->replacement);
    return ptr;
}

static void free_original_shared_allocation(void *opaque, void *ptr)
{
    SwitchingSharedAllocation *owner = opaque;
    free_shared(&owner->data, ptr);
}

static void test_shared_buffer_alignment_rejection(void)
{
    static const char *sources[] = {
        "new SharedArrayBuffer(0)",
        "new SharedArrayBuffer(8)",
        "new SharedArrayBuffer(0, { maxByteLength: 0 })",
        "new SharedArrayBuffer(3, { maxByteLength: 31 })",
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, atomics, buffer;
    JSSharedArrayBufferFunctions none = { 0 };
    SharedBufferData replacement = { 0 };
    SwitchingSharedAllocation owner = { 0 };
    size_t i;
    int mode;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    global = JS_GetGlobalObject(ctx);
    atomics = JS_GetPropertyStr(ctx, global, "Atomics");
    JS_FreeValue(ctx, global);
    if (JS_IsUndefined(atomics)) {
        JS_FreeValue(ctx, atomics);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return;
    }
    assert(!JS_IsException(atomics));
    JS_FreeValue(ctx, atomics);
    owner.rt = rt;
    owner.replacement = (JSSharedArrayBufferFunctions) {
        alloc_shared, free_shared, dup_shared, &replacement,
    };
    for (i = 0; i <= countof(sources); i++) {
        JSSharedArrayBufferFunctions functions = {
            alloc_misaligned_and_switch, free_original_shared_allocation,
            NULL, &owner,
        };
        JS_SetSharedArrayBufferFunctions(rt, &functions);
        if (i == countof(sources)) {
            buffer = JS_NewArrayBuffer(ctx, NULL, 0, NULL, NULL, TRUE);
        } else {
            buffer = JS_Eval(ctx, sources[i], strlen(sources[i]),
                             "shared-alignment-rejection", JS_EVAL_TYPE_GLOBAL);
        }
        assert(JS_IsException(buffer));
        check_type_error(ctx);
        assert(owner.data.allocations == (int)i + 1);
        assert(owner.data.releases == (int)i + 1);
        assert(owner.data.references == 0 && owner.data.ptr == NULL &&
               owner.data.allocation == NULL && owner.data.duplications == 0);
        assert(replacement.allocations == 0 && replacement.releases == 0 &&
               replacement.references == 0 && replacement.duplications == 0);
    }
    for (mode = 0; mode < 4; mode++) {
        SharedBufferData external = { 0 };
        JSSharedArrayBufferFunctions functions = {
            NULL, free_shared, dup_shared, &external,
        };
        uint8_t *ptr = alloc_misaligned_shared(&external, 8);
        int free_count = 0;

        JS_SetSharedArrayBufferFunctions(rt, mode & 1 ? &functions : &none);
        buffer = JS_NewArrayBuffer(ctx, ptr, mode >= 2 ? 0 : 8,
                                   count_unclaimed_shared_free,
                                   &free_count, TRUE);
        assert(JS_IsException(buffer));
        check_type_error(ctx);
        assert(free_count == 0 && external.references == 1 &&
               external.duplications == 0 && external.releases == 0 &&
               external.ptr == ptr);
        free_shared(&external, ptr);
        assert(external.references == 0 && external.releases == 1);
    }
    JS_SetSharedArrayBufferFunctions(rt, &none);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(owner.data.releases == (int)countof(sources) + 1);
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

/* A rejected serialization must not poison the next write's cycle check. */
static void test_shared_write_retry(void)
{
    SharedBufferData data = { 0 };
    JSSharedArrayBufferFunctions functions = {
        alloc_shared, free_shared, dup_shared, &data,
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue buffer, error;
    uint8_t *encoded;
    size_t size;

    assert(rt);
    JS_SetSharedArrayBufferFunctions(rt, &functions);
    ctx = JS_NewContext(rt);
    assert(ctx);
    buffer = JS_NewArrayBuffer(ctx, NULL, 0, NULL, NULL, TRUE);
    assert(!JS_IsException(buffer));
    encoded = JS_WriteObject(ctx, &size, buffer, 0);
    assert(!encoded && !size && JS_HasException(ctx));
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    encoded = JS_WriteObject(ctx, &size, buffer, JS_WRITE_OBJ_SAB);
    assert(encoded && size && !JS_HasException(ctx));
    js_free(ctx, encoded);
    JS_FreeValue(ctx, buffer);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(data.references == 0 && data.releases == 1);
}

static void check_shared_write_cleanup(JSContext *ctx, JSValueConst array,
                                       int reject)
{
    uint8_t *encoded, **pointers = (uint8_t **)(uintptr_t)1;
    size_t encoded_size = 1, pointer_count = 1;
    JSValue exception;

    if (reject) {
        encoded = JS_WriteObject2(ctx, &encoded_size, array, JS_WRITE_OBJ_SAB,
                                  &pointers, &pointer_count);
        assert(!encoded && encoded_size == 0 && pointers == NULL && pointer_count == 0);
        exception = JS_GetException(ctx);
        assert(JS_IsError(ctx, exception));
        JS_FreeValue(ctx, exception);
    } else {
        encoded = JS_WriteObject(ctx, &encoded_size, array, JS_WRITE_OBJ_SAB);
        assert(encoded && encoded_size > 0 && !JS_HasException(ctx));
        js_free(ctx, encoded);
    }
}

static void run_shared_write_cleanup(int reject)
{
    SharedBufferData data = { 0 };
    JSSharedArrayBufferFunctions functions = {
        alloc_shared, free_shared, dup_shared, &data,
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue local, shared, array;
    JSMemoryUsage before, after;
    uint8_t *encoded, **pointers;
    size_t encoded_size, pointer_count, i;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    local = JS_NewArrayBuffer(ctx, NULL, 0, NULL, NULL, TRUE);
    assert(!JS_IsException(local));
    JS_SetSharedArrayBufferFunctions(rt, &functions);
    shared = JS_NewArrayBuffer(ctx, NULL, 0, NULL, NULL, TRUE);
    assert(!JS_IsException(shared));
    array = JS_NewArray(ctx);
    assert(!JS_IsException(array));
    /* A table beyond the small allocation pool is visible in malloc_count. */
    for (i = 0; i < 65; i++)
        assert(JS_SetPropertyUint32(ctx, array, i, JS_DupValue(ctx, shared)) >= 0);
    if (reject)
        assert(JS_SetPropertyUint32(ctx, array, 65, JS_DupValue(ctx, local)) >= 0);
    else {
        encoded = JS_WriteObject2(ctx, &encoded_size, array, JS_WRITE_OBJ_SAB,
                                  &pointers, &pointer_count);
        assert(encoded && pointer_count == 65);
        for (i = 0; i < pointer_count; i++)
            assert(pointers[i] == data.ptr);
        js_free(ctx, encoded);
        js_free(ctx, pointers);
    }
    check_shared_write_cleanup(ctx, array, reject);
    JS_ComputeMemoryUsage(rt, &before);
    for (i = 0; i < 16; i++)
        check_shared_write_cleanup(ctx, array, reject);
    JS_ComputeMemoryUsage(rt, &after);
    assert(after.malloc_count == before.malloc_count);
    assert(data.references == 1 && data.duplications == 0 && data.releases == 0);
    JS_FreeValue(ctx, array);
    JS_FreeValue(ctx, shared);
    JS_FreeValue(ctx, local);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(data.references == 0 && data.allocations == 1 && data.releases == 1);
}

static void test_shared_write_cleanup(void)
{
    run_shared_write_cleanup(FALSE);
}

static void test_shared_write_failure_cleanup(void)
{
    run_shared_write_cleanup(TRUE);
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

static void test_shared_slice_clone_species(void)
{
    static const char *sources[] = {
        "new SharedArrayBuffer(0)",
        "new SharedArrayBuffer(8)",
        "new SharedArrayBuffer(0, { maxByteLength: 0 })",
        "new SharedArrayBuffer(8, { maxByteLength: 32 })",
    };
    size_t i;

    for (i = 0; i < countof(sources); i++) {
        SharedBufferData data = { 0 };
        JSSharedArrayBufferFunctions functions = {
            alloc_shared, free_shared, dup_shared, &data,
        };
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx;
        JSValue source, clone, global;
        uint8_t *encoded, **pointers;
        size_t encoded_size, pointer_count;

        assert(rt);
        JS_SetSharedArrayBufferFunctions(rt, &functions);
        ctx = JS_NewContext(rt);
        assert(ctx);
        source = JS_Eval(ctx, sources[i], strlen(sources[i]),
                         "shared-slice-source", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(source));
        encoded = JS_WriteObject2(ctx, &encoded_size, source, JS_WRITE_OBJ_SAB,
                                  &pointers, &pointer_count);
        assert(encoded && pointer_count == 1 && pointers[0] == data.ptr);
        clone = JS_ReadObject(ctx, encoded, encoded_size, JS_READ_OBJ_SAB);
        assert(!JS_IsException(clone));
        js_free(ctx, encoded);
        js_free(ctx, pointers);
        global = JS_GetGlobalObject(ctx);
        assert(JS_SetPropertyStr(ctx, global, "source", source) >= 0);
        assert(JS_SetPropertyStr(ctx, global, "clone", clone) >= 0);
        JS_FreeValue(ctx, global);
        check_eval(ctx,
            "(() => {"
            " if (source === clone) throw Error('same SAB object');"
            " source.constructor = { [Symbol.species]: function() { return clone; } };"
            " try { source.slice(0, 0); }"
            " catch (e) { return e instanceof TypeError; }"
            " return false;"
            "})()");
        check_eval(ctx,
            "(() => {"
            " if (source.byteLength === 0) return true;"
            " const bytes = new Uint8Array(source);"
            " for (let i = 0; i < bytes.length; i++) bytes[i] = i + 1;"
            " const expected = Array.from(bytes);"
            " for (const [start, end] of [[0, bytes.length], [1, bytes.length - 1]]) {"
            "   if (end <= start) throw Error('empty nonempty-slice fixture');"
            "   let rejected = false;"
            "   try { source.slice(start, end); }"
            "   catch (e) { if (!(e instanceof TypeError)) throw e; rejected = true; }"
            "   if (!rejected) throw Error('aliased nonempty SAB slice accepted');"
            "   if (bytes.length !== expected.length ||"
            "       bytes.some((value, index) => value !== expected[index]))"
            "     throw Error('shared source bytes changed');"
            " }"
            " return true;"
            "})()");
        assert(data.references == 2 && data.duplications == 1 && data.releases == 0);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(data.references == 0 && data.allocations == 1 &&
               data.duplications == 1 && data.releases == 2);
    }
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

static void test_shared_buffer_allocator_failure(void)
{
    SharedBufferData data = { 0 };
    JSSharedArrayBufferFunctions functions = {
        fail_alloc_shared, free_shared, dup_shared, &data,
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue result, error;
    static const char source[] = "new SharedArrayBuffer(8)";

    assert(rt);
    JS_SetSharedArrayBufferFunctions(rt, &functions);
    ctx = JS_NewContext(rt);
    assert(ctx);
    result = JS_Eval(ctx, source, sizeof(source) - 1,
                     "shared-allocator-failure", JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsException(result) && JS_HasException(ctx));
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    assert(data.allocations == 1 && data.releases == 0 &&
           data.references == 0 && data.ptr == NULL);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
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

static void check_typed_buffer_length(JSContext *ctx, JSValueConst array,
                                     size_t expected_length, size_t buffer_length)
{
    JSValue buffer;
    size_t offset, length, element_size, backing_length;

    buffer = JS_GetTypedArrayBuffer(ctx, array, &offset, &length, &element_size);
    assert(!JS_IsException(buffer));
    assert(offset == 2 && length == expected_length && element_size == 2);
    assert(JS_GetArrayBuffer(ctx, &backing_length, buffer) != NULL);
    assert(backing_length == buffer_length && offset + length <= backing_length);
    JS_FreeValue(ctx, buffer);
}

static void test_typed_buffer_resized_length(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue array, fixed, buffer, global, result, exception;
    const char *source = "new ArrayBuffer(8, { maxByteLength: 16 })";

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    buffer = JS_Eval(ctx, source, strlen(source), "typed-buffer-rab", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(buffer));
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "rab", buffer) >= 0);
    JS_FreeValue(ctx, global);
    source = "new Uint16Array(rab, 2)";
    array = JS_Eval(ctx, source, strlen(source), "typed-buffer-tracking", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(array));
    source = "new Uint16Array(rab, 2, 2)";
    fixed = JS_Eval(ctx, source, strlen(source), "typed-buffer-fixed", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(fixed));
    check_typed_buffer_length(ctx, array, 6, 8);
    check_typed_buffer_length(ctx, fixed, 4, 8);
    check_eval(ctx, "rab.resize(5) === undefined");
    check_typed_buffer_length(ctx, array, 2, 5);
    result = JS_GetTypedArrayBuffer(ctx, fixed, NULL, NULL, NULL);
    assert(JS_IsException(result));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    check_eval(ctx, "rab.resize(9) === undefined");
    check_typed_buffer_length(ctx, array, 6, 9);
    check_typed_buffer_length(ctx, fixed, 4, 9);
    check_eval(ctx, "rab.resize(2) === undefined");
    check_typed_buffer_length(ctx, array, 0, 2);
    check_eval(ctx, "rab.resize(1) === undefined");
    result = JS_GetTypedArrayBuffer(ctx, array, NULL, NULL, NULL);
    assert(JS_IsException(result));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    check_eval(ctx, "rab.resize(8) === undefined");
    check_typed_buffer_length(ctx, array, 6, 8);
    check_eval(ctx, "rab.transfer().byteLength === 8");
    result = JS_GetTypedArrayBuffer(ctx, array, NULL, NULL, NULL);
    assert(JS_IsException(result));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, fixed);
    JS_FreeValue(ctx, array);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
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
    assert(external.free_count == 1);
}

static void test_external_buffer_callback_lifetime(void)
{
    static const char *sources[] = {
        "external",
        "external.transfer()",
        "external.transfer(0)",
        "external.transfer(2)",
        "external.transferToFixedLength()",
        "external.transferToFixedLength(0)",
        "external.transferToFixedLength(2)",
        ("try { external.transfer(-1); throw Error('accepted'); }"
         "catch (e) { if (!(e instanceof RangeError)) throw e; } external"),
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, buffer, result;
    ExternalBufferData external;
    size_t i, len;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    global = JS_GetGlobalObject(ctx);
    for (len = 0; len <= 1; len++) {
        for (i = 0; i < countof(sources); i++) {
            external.byte = 42;
            external.free_count = 0;
            buffer = JS_NewArrayBuffer(ctx, &external.byte, len,
                                       free_external, &external, FALSE);
            assert(!JS_IsException(buffer));
            assert(JS_SetPropertyStr(ctx, global, "external",
                                     JS_DupValue(ctx, buffer)) >= 0);
            result = JS_Eval(ctx, sources[i], strlen(sources[i]),
                             "external-buffer-callback", JS_EVAL_TYPE_GLOBAL);
            assert(!JS_IsException(result));
            assert(JS_SetPropertyStr(ctx, global, "external", JS_UNDEFINED) >= 0);
            JS_FreeValue(ctx, buffer);
            JS_FreeValue(ctx, result);
            assert(external.free_count == 1);
        }
    }
    external.free_count = 0;
    buffer = JS_NewArrayBuffer(ctx, &external.byte, 1,
                               free_external, &external, FALSE);
    assert(!JS_IsException(buffer));
    JS_DetachArrayBuffer(ctx, buffer);
    JS_DetachArrayBuffer(ctx, buffer);
    assert(external.free_count == 1);
    JS_FreeValue(ctx, buffer);
    assert(external.free_count == 1);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
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

static JSValue unused_job(JSContext *ctx, int argc, JSValueConst *argv)
{
    return JS_UNDEFINED;
}

static void test_invalid_job_argc(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue error;

    assert(rt && ctx);
    assert(JS_EnqueueJob(ctx, unused_job, -1, NULL) < 0);
    assert(!JS_IsJobPending(rt));
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
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

static JSValue array_interrupt_callback(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    return JS_FALSE;
}

static int array_test_interrupt(JSRuntime *rt, void *opaque)
{
    int *count = opaque;
    (*count)++;
    return 1;
}

static void test_array_native_interrupt(void)
{
    static const struct {
        const char *name;
        int initial;
    } cases[] = {
        { "every", 0 }, { "some", 0 }, { "forEach", 0 },
        { "map", 0 }, { "filter", 0 },
        { "includes", 0 }, { "indexOf", 0 }, { "lastIndexOf", 0 },
        { "find", 0 }, { "findIndex", 0 },
        { "findLast", 0 }, { "findLastIndex", 0 },
        { "reduce", 0 }, { "reduceRight", 0 },
        { "reduce", 1 }, { "reduceRight", 1 },
    };
    size_t i;

    for (i = 0; i < countof(cases); i++) {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx;
        JSValue global, array, prototype, method, input, args[2], result, error;
        const char *message;
        int count = 0;

        assert(rt);
        ctx = JS_NewContext(rt);
        assert(ctx);
        global = JS_GetGlobalObject(ctx);
        array = JS_GetPropertyStr(ctx, global, "Array");
        prototype = JS_GetPropertyStr(ctx, array, "prototype");
        method = JS_GetPropertyStr(ctx, prototype, cases[i].name);
        assert(JS_IsFunction(ctx, method));
        input = JS_NewObject(ctx);
        assert(!JS_IsException(input));
        assert(JS_SetPropertyStr(ctx, input, "length", JS_NewInt32(ctx, 100000)) >= 0);
        args[0] = JS_NewCFunction(ctx, array_interrupt_callback, "callback", 0);
        assert(!JS_IsException(args[0]));
        args[1] = JS_NewInt32(ctx, 0);
        /* Prime the VM budget so call entry itself cannot satisfy the test. */
        result = JS_Eval(ctx, "0", 1, "prime-array-interrupt", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(result));
        JS_FreeValue(ctx, result);
        JS_SetInterruptHandler(rt, array_test_interrupt, &count);
        result = JS_Call(ctx, method, input, cases[i].initial ? 2 : 1, args);
        JS_SetInterruptHandler(rt, NULL, NULL);
        assert(JS_IsException(result));
        assert(count == 1);
        error = JS_GetException(ctx);
        message = JS_ToCString(ctx, error);
        assert(message && strstr(message, "interrupted"));
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, error);
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, input);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, prototype);
        JS_FreeValue(ctx, array);
        JS_FreeValue(ctx, global);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
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

static void int64_property_value_free(JSRuntime *rt, void *opaque, void *ptr)
{
    int *releases = opaque;
    (*releases)++;
}

static void test_int64_property_api(void)
{
    static const struct {
        int64_t index;
        const char *key;
    } cases[] = {
        { 0, "0" },
        { -1, "-1" },
        { INT64_C(2147483648), "2147483648" },
        { INT64_C(4294967296), "4294967296" },
        { INT64_C(9007199254740993), "9007199254740993" },
        { -INT64_C(9007199254740993), "-9007199254740993" },
        { INT64_MIN, "-9223372036854775808" },
        { INT64_MAX, "9223372036854775807" },
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue object, value, exception;
    JSAtom atom;
    const char *key;
    int32_t number;
    size_t i;
    uint8_t external = 0;
    int releases = 0;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    object = JS_NewObject(ctx);
    assert(!JS_IsException(object));
    for (i = 0; i < countof(cases); i++) {
        assert(JS_DefinePropertyValueInt64(ctx, object, cases[i].index,
                                          JS_NewInt32(ctx, 100 + (int)i),
                                          JS_PROP_C_W_E) == 1);
        value = JS_GetPropertyInt64(ctx, object, cases[i].index);
        assert(JS_ToInt32(ctx, &number, value) == 0 && number == 100 + (int)i);
        JS_FreeValue(ctx, value);
        value = JS_GetPropertyStr(ctx, object, cases[i].key);
        assert(JS_ToInt32(ctx, &number, value) == 0 && number == 100 + (int)i);
        JS_FreeValue(ctx, value);
        assert(JS_SetPropertyInt64(ctx, object, cases[i].index,
                                  JS_NewInt32(ctx, 200 + (int)i)) == 1);
        value = JS_GetPropertyInt64(ctx, object, cases[i].index);
        assert(JS_ToInt32(ctx, &number, value) == 0 && number == 200 + (int)i);
        JS_FreeValue(ctx, value);
        assert(JS_DeletePropertyInt64(ctx, object, cases[i].index,
                                     JS_PROP_THROW) == 1);
        atom = JS_NewAtomInt64(ctx, cases[i].index);
        assert(atom != JS_ATOM_NULL && JS_HasProperty(ctx, object, atom) == 0);
        key = JS_AtomToCString(ctx, atom);
        assert(key && strcmp(key, cases[i].key) == 0);
        JS_FreeCString(ctx, key);
        JS_FreeAtom(ctx, atom);
    }

    assert(JS_PreventExtensions(ctx, object) == 1);
    value = JS_NewArrayBuffer(ctx, &external, 1, int64_property_value_free,
                              &releases, FALSE);
    assert(!JS_IsException(value));
    assert(JS_DefinePropertyValueInt64(ctx, object, INT64_MAX, value,
                                      JS_PROP_C_W_E | JS_PROP_THROW) == -1);
    assert(releases == 1 && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    value = JS_NewArrayBuffer(ctx, &external, 1, int64_property_value_free,
                              &releases, FALSE);
    assert(!JS_IsException(value));
    assert(JS_DefinePropertyValueInt64(ctx, JS_NULL, INT64_MIN, value,
                                      JS_PROP_C_W_E | JS_PROP_THROW) == -1);
    assert(releases == 2 && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, object);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(releases == 2);
}

static void check_typed_array_copy_api(JSContext *ctx)
{
    uint8_t input[] = { 0, 17, 255 };
    JSValue array, buffer;
    uint8_t *bytes;
    size_t offset, length, element_size, backing_length;

    array = JS_NewUint8ArrayCopy(ctx, input, sizeof(input));
    assert(!JS_IsException(array));
    assert(JS_GetTypedArrayType(array) == JS_TYPED_ARRAY_UINT8);
    input[1] = 99;
    buffer = JS_GetTypedArrayBuffer(ctx, array, &offset, &length, &element_size);
    assert(!JS_IsException(buffer));
    assert(offset == 0 && length == sizeof(input) && element_size == 1);
    bytes = JS_GetArrayBuffer(ctx, &backing_length, buffer);
    assert(bytes && backing_length == sizeof(input));
    assert(bytes[0] == 0 && bytes[1] == 17 && bytes[2] == 255);
    bytes[0] = 42;
    assert(input[0] == 0);
    JS_FreeValue(ctx, buffer);
    JS_FreeValue(ctx, array);

    array = JS_NewUint8ArrayCopy(ctx, NULL, 0);
    assert(!JS_IsException(array));
    assert(JS_GetTypedArrayType(array) == JS_TYPED_ARRAY_UINT8);
    buffer = JS_GetTypedArrayBuffer(ctx, array, &offset, &length, &element_size);
    assert(!JS_IsException(buffer) && offset == 0 && length == 0 && element_size == 1);
    assert(JS_GetArrayBuffer(ctx, &backing_length, buffer) != NULL);
    assert(backing_length == 0);
    JS_DetachArrayBuffer(ctx, buffer);
    assert(JS_GetTypedArrayType(array) == JS_TYPED_ARRAY_UINT8);
    JS_FreeValue(ctx, buffer);
    JS_FreeValue(ctx, array);
}

static void test_typed_array_public_api(void)
{
    static const char *other_objects[] = {
        "({ [Symbol.toStringTag]: 'Uint8Array' })",
        "new DataView(new ArrayBuffer(0))",
        "new Proxy(new Uint8Array(0), { get() { throw Error('trap'); } })",
    };
    static const char oob_source[] =
        "(() => { let b = new ArrayBuffer(8, { maxByteLength: 8 });"
        " let a = new Uint16Array(b, 4, 2); b.resize(2); return a; })()";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx, *raw;
    JSValue zero, array, exception;
    int (*get_type)(JSValueConst) = JS_GetTypedArrayType;
    int type;
    size_t i;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    assert(get_type(JS_NULL) == -1 && get_type(JS_UNDEFINED) == -1);
    assert(get_type(JS_NewInt32(ctx, 0)) == -1);
    zero = JS_NewInt32(ctx, 0);
    for (type = JS_TYPED_ARRAY_UINT8C; type <= JS_TYPED_ARRAY_FLOAT64; type++) {
        array = JS_NewTypedArray(ctx, 1, (JSValueConst *)&zero,
                                 (JSTypedArrayEnum)type);
        assert(!JS_IsException(array) && get_type(array) == type);
        JS_FreeValue(ctx, array);
    }
    for (i = 0; i < countof(other_objects); i++) {
        array = JS_Eval(ctx, other_objects[i], strlen(other_objects[i]),
                        "typed-array-brand-api", JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(array) && get_type(array) == -1);
        assert(!JS_HasException(ctx));
        JS_FreeValue(ctx, array);
    }
    array = JS_Eval(ctx, oob_source, sizeof(oob_source) - 1,
                    "typed-array-oob-brand-api", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(array) && get_type(array) == JS_TYPED_ARRAY_UINT16);
    assert(!JS_HasException(ctx));
    JS_ThrowTypeError(ctx, "preserved exception");
    assert(get_type(array) == JS_TYPED_ARRAY_UINT16 && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, array);
    check_typed_array_copy_api(ctx);

    raw = JS_NewContextRaw(rt);
    assert(raw);
    assert(JS_AddIntrinsicBaseObjects(raw) == 0);
    assert(JS_AddIntrinsicTypedArrays(raw) == 0);
    check_typed_array_copy_api(raw);
    array = JS_NewTypedArray(raw, 1, (JSValueConst *)&zero, JS_TYPED_ARRAY_FLOAT16);
    assert(!JS_IsException(array) && get_type(array) == JS_TYPED_ARRAY_FLOAT16);
    JS_FreeValue(raw, array);
    JS_FreeContext(raw);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static int well_known_dispose_calls;

static JSValue well_known_dispose_data(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv,
                                      int magic, JSValue *data)
{
    int32_t increment;
    JSValue value;

    assert(argc == 0 && magic == 0);
    value = JS_GetPropertyStr(ctx, data[0], "increment");
    assert(JS_ToInt32(ctx, &increment, value) == 0);
    JS_FreeValue(ctx, value);
    well_known_dispose_calls += increment;
    return JS_UNDEFINED;
}

static void test_well_known_symbol_api(void)
{
    static const struct {
        JSWellKnownSymbolEnum selector;
        const char *name;
    } cases[] = {
        { JS_WELL_KNOWN_SYMBOL_ASYNC_ITERATOR, "asyncIterator" },
        { JS_WELL_KNOWN_SYMBOL_HAS_INSTANCE, "hasInstance" },
        { JS_WELL_KNOWN_SYMBOL_IS_CONCAT_SPREADABLE, "isConcatSpreadable" },
        { JS_WELL_KNOWN_SYMBOL_ITERATOR, "iterator" },
        { JS_WELL_KNOWN_SYMBOL_MATCH, "match" },
        { JS_WELL_KNOWN_SYMBOL_MATCH_ALL, "matchAll" },
        { JS_WELL_KNOWN_SYMBOL_REPLACE, "replace" },
        { JS_WELL_KNOWN_SYMBOL_SEARCH, "search" },
        { JS_WELL_KNOWN_SYMBOL_SPECIES, "species" },
        { JS_WELL_KNOWN_SYMBOL_SPLIT, "split" },
        { JS_WELL_KNOWN_SYMBOL_TO_PRIMITIVE, "toPrimitive" },
        { JS_WELL_KNOWN_SYMBOL_TO_STRING_TAG, "toStringTag" },
        { JS_WELL_KNOWN_SYMBOL_UNSCOPABLES, "unscopables" },
        { JS_WELL_KNOWN_SYMBOL_DISPOSE, "dispose" },
        { JS_WELL_KNOWN_SYMBOL_ASYNC_DISPOSE, "asyncDispose" },
    };
    static const char source[] =
        "Symbol = {}; { using resource = hostResource; }";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx, *raw;
    JSValue first, second, other, global, symbol_ctor, property;
    JSValue resource, hook, capture, result, exception;
    JSAtom atom;
    size_t i;

    assert(rt);
    ctx = JS_NewContextRaw(rt);
    raw = JS_NewContextRaw(rt);
    assert(ctx && raw);
    global = JS_GetGlobalObject(raw);
    symbol_ctor = JS_GetPropertyStr(raw, global, "Symbol");
    assert(JS_IsUndefined(symbol_ctor));
    JS_FreeValue(raw, symbol_ctor);
    JS_FreeValue(raw, global);
    for (i = 0; i < countof(cases); i++) {
        first = JS_GetWellKnownSymbol(ctx, cases[i].selector);
        second = JS_GetWellKnownSymbol(ctx, cases[i].selector);
        other = JS_GetWellKnownSymbol(raw, cases[i].selector);
        assert(JS_IsSymbol(first) && JS_IsSymbol(second) && JS_IsSymbol(other));
        assert(JS_StrictEq(ctx, first, second) && JS_StrictEq(ctx, first, other));
        JS_FreeValue(ctx, first);
        assert(JS_IsSymbol(second) && JS_StrictEq(ctx, second, other));
        JS_FreeValue(ctx, second);
        JS_FreeValue(raw, other);
    }
    assert(JS_AddIntrinsicBaseObjects(ctx) == 0);
    assert(JS_AddIntrinsicEval(ctx) == 0);
    global = JS_GetGlobalObject(ctx);
    symbol_ctor = JS_GetPropertyStr(ctx, global, "Symbol");
    assert(JS_IsFunction(ctx, symbol_ctor));
    for (i = 0; i < countof(cases); i++) {
        first = JS_GetWellKnownSymbol(ctx, cases[i].selector);
        property = JS_GetPropertyStr(ctx, symbol_ctor, cases[i].name);
        assert(JS_StrictEq(ctx, first, property));
        JS_FreeValue(ctx, property);
        JS_FreeValue(ctx, first);
    }
    JS_FreeValue(ctx, symbol_ctor);
    result = JS_GetWellKnownSymbol(ctx, (JSWellKnownSymbolEnum)-1);
    assert(JS_IsException(result) && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    property = JS_GetPropertyStr(ctx, exception, "name");
    {
        const char *name = JS_ToCString(ctx, property);
        assert(name && strcmp(name, "RangeError") == 0);
        JS_FreeCString(ctx, name);
    }
    JS_FreeValue(ctx, property);
    JS_FreeValue(ctx, exception);
    result = JS_GetWellKnownSymbol(ctx, (JSWellKnownSymbolEnum)countof(cases));
    assert(JS_IsException(result) && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);

    first = JS_GetWellKnownSymbol(ctx, JS_WELL_KNOWN_SYMBOL_DISPOSE);
    second = JS_NewSymbol(ctx, "Symbol.dispose", FALSE);
    assert(JS_IsSymbol(second) && !JS_StrictEq(ctx, first, second));
    JS_FreeValue(ctx, second);
    JS_ThrowTypeError(ctx, "preserved exception");
    second = JS_GetWellKnownSymbol(ctx, JS_WELL_KNOWN_SYMBOL_DISPOSE);
    assert(JS_StrictEq(ctx, first, second) && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, second);
    resource = JS_NewObject(ctx);
    assert(!JS_IsException(resource));
    capture = JS_NewObject(ctx);
    assert(!JS_IsException(capture));
    assert(JS_SetPropertyStr(ctx, capture, "increment", JS_NewInt32(ctx, 3)) == 1);
    hook = JS_NewCFunctionData(ctx, well_known_dispose_data, 0, 0, 1,
                               (JSValueConst *)&capture);
    assert(!JS_IsException(hook));
    JS_FreeValue(ctx, capture);
    atom = JS_ValueToAtom(ctx, first);
    assert(atom != JS_ATOM_NULL);
    assert(JS_DefinePropertyValue(ctx, resource, atom, hook,
                                  JS_PROP_C_W_E | JS_PROP_THROW) == 1);
    JS_FreeAtom(ctx, atom);
    assert(JS_SetPropertyStr(ctx, global, "hostResource", resource) == 1);
    JS_FreeValue(ctx, global);
    JS_RunGC(rt);
    well_known_dispose_calls = 0;
    result = JS_Eval(ctx, source, sizeof(source) - 1,
                     "well-known-symbol-host-disposal", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result) && well_known_dispose_calls == 3);
    JS_FreeValue(ctx, result);
    second = JS_GetWellKnownSymbol(ctx, JS_WELL_KNOWN_SYMBOL_DISPOSE);
    assert(JS_StrictEq(ctx, first, second));
    JS_FreeValue(ctx, second);
    JS_FreeValue(ctx, first);
    assert(!JS_HasException(ctx) && !JS_HasException(raw));
    JS_FreeContext(raw);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue utf16_api_eval(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "utf16-api",
                            JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(value));
    return value;
}

static void test_utf16_public_api(void)
{
    static const uint16_t expected[] = { 'A', 0, 0xe9, 0xd834, 0xdf06,
                                        0xd800, 'Z', 0xdc00 };
    uint16_t input[countof(expected)];
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue value, marker, exception;
    const uint16_t *wide;
    const char *bytes;
    size_t length, i;

    assert(rt);
    ctx = JS_NewContextRaw(rt);
    assert(ctx);
    assert(JS_AddIntrinsicBaseObjects(ctx) == 0);
    assert(JS_AddIntrinsicEval(ctx) == 0);
    memcpy(input, expected, sizeof(input));
    value = JS_NewStringUTF16(ctx, input, countof(input));
    assert(!JS_IsException(value));
    memset(input, 0, sizeof(input));
    wide = JS_ToCStringLenUTF16(ctx, &length, value);
    assert(wide && length == countof(expected));
    JS_FreeValue(ctx, value);
    JS_RunGC(rt);
    assert(!memcmp(wide, expected, sizeof(expected)));
    JS_FreeCStringUTF16(ctx, wide);

    value = JS_NewStringUTF16(ctx, NULL, 0);
    assert(!JS_IsException(value));
    wide = JS_ToCStringLenUTF16(ctx, &length, value);
    assert(wide && length == 0);
    JS_FreeCStringUTF16(ctx, wide);
    JS_FreeValue(ctx, value);
    value = JS_NewStringLen(ctx, "a\0\xc3\xa9", 4);
    assert(!JS_IsException(value));
    wide = JS_ToCStringLenUTF16(ctx, &length, value);
    assert(wide && length == 3 && wide[0] == 'a' && wide[1] == 0 &&
           wide[2] == 0xe9);
    JS_FreeCStringUTF16(ctx, wide);
    JS_FreeValue(ctx, value);

    {
        static const char malformed[] = { (char)0x80, 'A' };

        value = JS_NewStringLen(ctx, malformed, sizeof(malformed));
        assert(!JS_IsException(value));
        wide = JS_ToCStringLenUTF16(ctx, &length, value);
        assert(wide && length == 2 && wide[0] == 0xfffd && wide[1] == 'A');
        JS_FreeCStringUTF16(ctx, wide);
        JS_FreeValue(ctx, value);
        value = JS_NewStringLen(ctx, NULL, 0);
        assert(!JS_IsException(value));
        wide = JS_ToCStringLenUTF16(ctx, &length, value);
        assert(wide && length == 0);
        JS_FreeCStringUTF16(ctx, wide);
        JS_FreeValue(ctx, value);
    }

    value = utf16_api_eval(ctx, "'x'.repeat(10000) + '\\ud800\\u0000\\udc00'");
    assert(JS_VALUE_GET_TAG(value) == JS_TAG_STRING_ROPE);
    wide = JS_ToCStringLenUTF16(ctx, &length, value);
    assert(wide && length == 10003);
    for (i = 0; i < 10000; i++)
        assert(wide[i] == 'x');
    assert(wide[10000] == 0xd800 && wide[10001] == 0 && wide[10002] == 0xdc00);
    JS_FreeValue(ctx, value);
    JS_FreeCStringUTF16(ctx, wide);

    value = utf16_api_eval(ctx,
        "globalThis.utf16Calls = 0; ({ [Symbol.toPrimitive](hint) {"
        " if (hint !== 'string') throw Error('wrong hint');"
        " utf16Calls++; return '\\ud800\\u0000'; } })");
    wide = JS_ToCStringUTF16(ctx, value);
    assert(wide && wide[0] == 0xd800 && wide[1] == 0);
    JS_FreeCStringUTF16(ctx, wide);
    JS_FreeValue(ctx, value);
    marker = utf16_api_eval(ctx, "utf16Calls === 1");
    assert(JS_ToBool(ctx, marker) == 1);
    JS_FreeValue(ctx, marker);

    value = JS_NewSymbol(ctx, "UTF16 TypeError", 0);
    assert(!JS_IsException(value));
    length = 123;
    assert(JS_ToCStringLenUTF16(ctx, &length, value) == NULL && length == 0);
    assert(JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, value);
    value = utf16_api_eval(ctx, "({ toString() { throw 'utf16-marker'; } })");
    length = 123;
    assert(JS_ToCStringLenUTF16(ctx, &length, value) == NULL && length == 0);
    exception = JS_GetException(ctx);
    bytes = JS_ToCString(ctx, exception);
    assert(bytes && !strcmp(bytes, "utf16-marker"));
    JS_FreeCString(ctx, bytes);
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, value);
    value = JS_NewStringUTF16(ctx, expected, SIZE_MAX);
    assert(JS_IsException(value) && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);

    value = JS_NewStringUTF16(ctx, expected, countof(expected));
    assert(!JS_IsException(value));
    wide = JS_ToCStringLenUTF16(ctx, &length, value);
    assert(wide && length == countof(expected));
    JS_FreeValue(ctx, value);
    value = JS_NewStringLen(ctx, "lifetime\0x", 10);
    assert(!JS_IsException(value));
    bytes = JS_ToCStringLen(ctx, &length, value);
    assert(bytes && length == 10);
    JS_FreeValue(ctx, value);
    JS_FreeContext(ctx);
    JS_RunGC(rt);
    assert(!memcmp(wide, expected, sizeof(expected)));
    assert(!memcmp(bytes, "lifetime\0x", 10));
    JS_FreeCStringRT_UTF16(rt, wide);
    JS_FreeCStringRT(rt, bytes);
    JS_FreeCStringRT_UTF16(rt, NULL);
    JS_FreeCStringRT(rt, NULL);
    JS_FreeRuntime(rt);
}

static void test_utf16_public_api_oom(void)
{
    uint16_t *input = malloc(50000 * sizeof(*input));
    char *narrow = malloc(50000);
    int operation;

    assert(input && narrow);
    memset(input, 0x61, 50000 * sizeof(*input));
    memset(narrow, 'a', 50000);
    for (operation = 0; operation < 3; operation++) {
        AllocationProbe probe = { 0 };
        JSRuntime *rt = JS_NewRuntime2(&probe_functions, &probe);
        JSContext *ctx;
        JSValue source = JS_UNDEFINED, result, exception;
        const uint16_t *wide;
        size_t length = 77;

        assert(rt);
        ctx = JS_NewContextRaw(rt);
        assert(ctx && JS_AddIntrinsicBaseObjects(ctx) == 0);
        assert(JS_AddIntrinsicEval(ctx) == 0);
        if (operation == 1) {
            source = JS_NewStringLen(ctx, narrow, 50000);
            assert(!JS_IsException(source));
        } else if (operation == 2) {
            source = utf16_api_eval(ctx, "'x'.repeat(10000) + 'y'.repeat(10000)");
            assert(JS_VALUE_GET_TAG(source) == JS_TAG_STRING_ROPE);
        }
        probe.attempts = 0;
        probe.failure_at = 1;
        if (operation == 0) {
            result = JS_NewStringUTF16(ctx, input, 50000);
            assert(JS_IsException(result));
        } else {
            wide = JS_ToCStringLenUTF16(ctx, &length, source);
            assert(wide == NULL && length == 0);
        }
        assert(probe.failed && JS_HasException(ctx));
        probe.failure_at = 0;
        exception = JS_GetException(ctx);
        JS_FreeValue(ctx, exception);
        if (operation != 0) {
            wide = JS_ToCStringLenUTF16(ctx, &length, source);
            assert(wide && length == (operation == 1 ? 50000 : 20000));
            JS_FreeCStringUTF16(ctx, wide);
        }
        JS_FreeValue(ctx, source);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(probe.live == 0);
    }
    free(narrow);
    free(input);
}

static void test_module_attribute_value_ownership(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue attributes, exception;
    JSMemoryUsage before, after;
    double number;
    const char *source = "({ get type() { throw 42; } })";

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_RunGC(rt);
    JS_ComputeMemoryUsage(rt, &before);
    attributes = JS_NewObject(ctx);
    assert(!JS_IsException(attributes));
    assert(JS_SetPropertyStr(ctx, attributes, "type", JS_NewObject(ctx)) >= 0);
    assert(js_module_test_json(ctx, attributes) == 0);
    assert(!JS_HasException(ctx));
    JS_FreeValue(ctx, attributes);
    JS_RunGC(rt);
    JS_ComputeMemoryUsage(rt, &after);
    /* The helper's temporary reference must not keep the non-string alive. */
    assert(after.obj_count == before.obj_count);

    attributes = JS_Eval(ctx, source, strlen(source), "<attribute-getter>", 0);
    assert(!JS_IsException(attributes));
    assert(js_module_test_json(ctx, attributes) == 0);
    assert(JS_HasException(ctx));
    exception = JS_GetException(ctx);
    assert(JS_IsNumber(exception));
    assert(JS_ToFloat64(ctx, &number, exception) == 0 && number == 42);
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, attributes);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

typedef struct ModuleAttributeIdentityState {
    int normalize_calls;
    int loader_calls;
    int shared_calls;
    int self_calls;
    int cycle_a_calls;
    int cycle_b_calls;
    JSModuleDef *shared;
} ModuleAttributeIdentityState;

static char *module_attribute_identity_normalize(JSContext *ctx,
                                                  const char *base_name,
                                                  const char *name,
                                                  void *opaque)
{
    ModuleAttributeIdentityState *state = opaque;
    (void)base_name;
    state->normalize_calls++;
    return js_strdup(ctx, name);
}

static JSValue module_attribute_identity_compile_named(JSContext *ctx,
                                                         const char *filename,
                                                         const char *module_name);

static JSModuleDef *module_attribute_identity_load(JSContext *ctx,
                                                    const char *name,
                                                    void *opaque,
                                                    JSValueConst attributes)
{
    ModuleAttributeIdentityState *state = opaque;
    const char *source;
    JSValue kind, compiled;
    JSModuleDef *module;

    state->loader_calls++;
    if (strcmp(name, "attribute-self") == 0 ||
        strcmp(name, "attribute-cycle-a") == 0 ||
        strcmp(name, "attribute-cycle-b") == 0) {
        const char *filename;
        if (strcmp(name, "attribute-self") == 0) {
            state->self_calls++;
            filename = "tests/fixture_module_attributes_host_self.js";
        } else if (strcmp(name, "attribute-cycle-a") == 0) {
            state->cycle_a_calls++;
            filename = "tests/fixture_module_attributes_host_cycle_a.js";
        } else {
            state->cycle_b_calls++;
            filename = "tests/fixture_module_attributes_host_cycle_b.js";
        }
        /* Active nested loader frames must keep their owned values alive. */
        JS_RunGC(JS_GetRuntime(ctx));
        compiled = module_attribute_identity_compile_named(ctx, filename, name);
        module = JS_VALUE_GET_PTR(compiled);
        JS_FreeValue(ctx, compiled);
        return module;
    }
    if (strcmp(name, "attribute-shared") == 0) {
        state->shared_calls++;
        if (state->shared)
            return state->shared;
        source = "export default 'shared';";
    } else if (strcmp(name, "attribute-empty") == 0) {
        source = "export default 'empty';";
    } else {
        assert(strcmp(name, "attribute-target") == 0);
        kind = JS_GetPropertyStr(ctx, attributes, "other");
        if (JS_IsString(kind)) {
            const char *text = JS_ToCString(ctx, kind);
            assert(text && strcmp(text, "first") == 0);
            JS_FreeCString(ctx, text);
            source = "export default 'alternate';";
        } else {
            assert(JS_IsUndefined(kind));
            JS_FreeValue(ctx, kind);
            kind = JS_GetPropertyStr(ctx, attributes, "kind");
            assert(JS_IsString(kind));
            const char *text = JS_ToCString(ctx, kind);
            assert(text);
            if (strcmp(text, "first") == 0)
                source = "export default 'first';";
            else {
                assert(strcmp(text, "second") == 0);
                source = "export default 'second';";
            }
            JS_FreeCString(ctx, text);
        }
        JS_FreeValue(ctx, kind);
    }
    compiled = JS_Eval(ctx, source, strlen(source), name,
                        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    module = JS_VALUE_GET_PTR(compiled);
    JS_FreeValue(ctx, compiled);
    if (strcmp(name, "attribute-shared") == 0)
        state->shared = module;
    return module;
}

static JSValue module_attribute_identity_compile_named(JSContext *ctx,
                                                         const char *filename,
                                                         const char *module_name)
{
    FILE *file = fopen(filename, "rb");
    char *source;
    long length;
    JSValue compiled;

    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file);
    assert(length > 0 && length < 65536);
    assert(fseek(file, 0, SEEK_SET) == 0);
    source = malloc((size_t)length + 1);
    assert(source);
    assert(fread(source, 1, (size_t)length, file) == (size_t)length);
    assert(fclose(file) == 0);
    source[length] = '\0';
    compiled = JS_Eval(ctx, source, (size_t)length, module_name,
                        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    free(source);
    assert(!JS_IsException(compiled));
    return compiled;
}

static JSValue module_attribute_identity_compile(JSContext *ctx,
                                                   const char *filename)
{
    return module_attribute_identity_compile_named(ctx, filename, filename);
}

/* Consumes the compiled module, then observes its asynchronous completion. */
static void module_attribute_identity_evaluate(JSContext *ctx, JSValue compiled)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSContext *job_ctx;
    JSValue promise, result;
    int ret;

    JS_RunGC(rt);
    promise = JS_EvalFunction(ctx, compiled);
    assert(!JS_IsException(promise));
    while ((ret = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
        JS_RunGC(rt);
    assert(ret == 0);
    result = JS_PromiseResult(ctx, promise);
    if (JS_PromiseState(ctx, promise) != JS_PROMISE_FULFILLED) {
        const char *text = JS_ToCString(ctx, result);
        fprintf(stderr, "module attribute identity: %s\n", text ? text : "pending");
        JS_FreeCString(ctx, text);
    }
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, promise);
    assert(!JS_HasException(ctx));
}

static void test_module_attribute_identity(void)
{
    ModuleAttributeIdentityState state = { 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue compiled;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetModuleLoaderFunc2(rt, module_attribute_identity_normalize,
                             module_attribute_identity_load, NULL, &state);
    compiled = module_attribute_identity_compile(ctx,
                  "tests/fixture_module_attributes_host.js");
    /* Equal requests deduplicate; unequal requests reach the host. */
    assert(state.normalize_calls == 6 && state.loader_calls == 6);
    assert(state.shared_calls == 2);
    module_attribute_identity_evaluate(ctx, compiled);
    compiled = module_attribute_identity_compile(ctx,
                  "tests/fixture_module_attributes_host_dynamic.js");
    module_attribute_identity_evaluate(ctx, compiled);
    assert(state.normalize_calls == 15);
    /* Only the unequal shared request needs another loader call. */
    assert(state.loader_calls == 7 && state.shared_calls == 3);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_module_attribute_bytecode(void)
{
    ModuleAttributeIdentityState state = { 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue compiled;
    uint8_t *bytecode, *copy;
    size_t bytecode_size;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetModuleLoaderFunc2(rt, module_attribute_identity_normalize,
                             module_attribute_identity_load, NULL, &state);
    compiled = module_attribute_identity_compile(ctx,
                  "tests/fixture_module_attributes_host.js");
    assert(state.normalize_calls == 6 && state.loader_calls == 6);
    bytecode = JS_WriteObject(ctx, &bytecode_size, compiled, JS_WRITE_OBJ_BYTECODE);
    assert(bytecode && bytecode_size > 0);
    copy = malloc(bytecode_size);
    assert(copy);
    memcpy(copy, bytecode, bytecode_size);
    js_free(ctx, bytecode);
    JS_FreeValue(ctx, compiled);
    JS_FreeContext(ctx);
    JS_RunGC(rt);

    memset(&state, 0, sizeof(state));
    ctx = JS_NewContext(rt);
    assert(ctx);
    compiled = JS_ReadObject(ctx, copy, bytecode_size, JS_READ_OBJ_BYTECODE);
    free(copy);
    assert(!JS_IsException(compiled));
    {
        const char *pollution = "Object.prototype.kind = 'first';";
        JSValue value = JS_Eval(ctx, pollution, strlen(pollution),
                                 "attribute-prototype-pollution",
                                 JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(value));
        JS_FreeValue(ctx, value);
    }
    assert(JS_ResolveModule(ctx, compiled) == 0);
    assert(state.normalize_calls == 6 && state.loader_calls == 6);
    assert(state.shared_calls == 2);
    module_attribute_identity_evaluate(ctx, compiled);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_module_attribute_cycles(void)
{
    ModuleAttributeIdentityState state = { 0 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue compiled;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetModuleLoaderFunc2(rt, module_attribute_identity_normalize,
                             module_attribute_identity_load, NULL, &state);
    compiled = module_attribute_identity_compile(ctx,
                  "tests/fixture_module_attributes_host_cycles.js");
    module_attribute_identity_evaluate(ctx, compiled);
    assert(state.self_calls == 1 && state.cycle_a_calls == 1);
    assert(state.cycle_b_calls == 1 && state.loader_calls == 3);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

/* Public module normalization callbacks choose the cache key before lookup. */
typedef struct ModuleNormalizeState {
    int normalize1_calls, normalize2_calls, loader1_calls, loader2_calls;
    int checker_calls, reject_json, source_text;
    const char *old_prefix, *throw_reason;
    char last_name[128];
    JSValue retained_attributes;
} ModuleNormalizeState;

static void module_normalize_drain(JSRuntime *rt)
{
    JSContext *job_ctx;
    int status, jobs = 0;

    while ((status = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
        assert(++jobs < 100);
    assert(status == 0);
}

static void module_normalize_run(JSContext *ctx, const char *source,
                                  const char *filename, int flags)
{
    JSValue promise = JS_Eval(ctx, source, strlen(source), filename, flags);

    assert(!JS_IsException(promise));
    module_normalize_drain(JS_GetRuntime(ctx));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, promise);
    assert(!JS_HasException(ctx));
}

static void module_normalize_rejection(JSContext *ctx, const char *source,
                                        const char *filename, int flags,
                                        const char *reason)
{
    JSValue result, error;
    const char *text;

    result = JS_Eval(ctx, source, strlen(source), filename, flags);
    if (JS_IsException(result)) {
        assert(JS_HasException(ctx));
        error = JS_GetException(ctx);
    } else {
        module_normalize_drain(JS_GetRuntime(ctx));
        assert(JS_PromiseState(ctx, result) == JS_PROMISE_REJECTED);
        error = JS_PromiseResult(ctx, result);
    }
    text = JS_ToCString(ctx, error);
    assert(text && strcmp(text, reason) == 0);
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, result);
    assert(!JS_HasException(ctx));
}

static int module_normalize_check_attributes(JSContext *ctx, void *opaque,
                                               JSValueConst attributes)
{
    ModuleNormalizeState *state = opaque;
    JSPropertyEnum *properties;
    uint32_t length, i;
    int status = 0;

    state->checker_calls++;
    if (JS_GetOwnPropertyNames(ctx, &properties, &length, attributes,
                               JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0)
        return -1;
    for (i = 0; i < length; i++) {
        const char *key = JS_AtomToCString(ctx, properties[i].atom);
        if (!key) {
            status = -1;
            break;
        }
        if (strcmp(key, "type") && strcmp(key, "flavor")) {
            JS_ThrowTypeError(ctx, "unsupported test attribute");
            status = -1;
        }
        JS_FreeCString(ctx, key);
        if (status < 0)
            break;
    }
    JS_FreePropertyEnum(ctx, properties, length);
    return status;
}

/* Imported attributes are own enumerable data properties, regardless of proto. */
static JSValue module_normalize_own_attribute(JSContext *ctx,
                                               JSValueConst attributes,
                                               const char *name)
{
    JSPropertyDescriptor descriptor;
    JSAtom atom;
    int status;

    if (!JS_IsObject(attributes))
        return JS_UNDEFINED;
    atom = JS_NewAtom(ctx, name);
    if (atom == JS_ATOM_NULL)
        return JS_EXCEPTION;
    status = JS_GetOwnProperty(ctx, &descriptor, attributes, atom);
    JS_FreeAtom(ctx, atom);
    if (status < 0)
        return JS_EXCEPTION;
    if (status == 0)
        return JS_UNDEFINED;
    JS_FreeValue(ctx, descriptor.getter);
    JS_FreeValue(ctx, descriptor.setter);
    if (!(descriptor.flags & JS_PROP_ENUMERABLE)) {
        JS_FreeValue(ctx, descriptor.value);
        return JS_UNDEFINED;
    }
    return descriptor.value;
}

static char *module_normalize_legacy(JSContext *ctx, const char *base_name,
                                      const char *name, void *opaque)
{
    ModuleNormalizeState *state = opaque;
    char key[128];
    int length;

    (void)base_name;
    state->normalize1_calls++;
    length = snprintf(key, sizeof(key), "%s:%s", state->old_prefix, name);
    assert(length >= 0 && (size_t)length < sizeof(key));
    return js_strdup(ctx, key);
}

static char *module_normalize_with_attributes(JSContext *ctx,
                                                const char *base_name,
                                                const char *name,
                                                JSValueConst attributes,
                                                void *opaque)
{
    ModuleNormalizeState *state = opaque;
    JSValue type = JS_UNDEFINED;
    const char *text = NULL;
    char key[128];
    int length;

    (void)base_name;
    state->normalize2_calls++;
    if (state->throw_reason) {
        JS_Throw(ctx, JS_NewString(ctx, state->throw_reason));
        return NULL;
    }
    if (JS_IsObject(attributes)) {
        if (JS_IsUndefined(state->retained_attributes))
            state->retained_attributes = JS_DupValue(ctx, attributes);
        /* The borrowed input remains live while the callback runs GC. */
        JS_RunGC(JS_GetRuntime(ctx));
        type = module_normalize_own_attribute(ctx, attributes, "type");
        if (JS_IsException(type))
            return NULL;
        if (!JS_IsUndefined(type)) {
            assert(JS_IsString(type));
            text = JS_ToCString(ctx, type);
            if (!text) {
                JS_FreeValue(ctx, type);
                return NULL;
            }
        }
    } else {
        assert(JS_IsUndefined(attributes));
    }
    if (text && state->reject_json && !strcmp(text, "json")) {
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, type);
        JS_Throw(ctx, JS_NewString(ctx, "cached type=json rejected"));
        return NULL;
    }
    /* The host chooses a name; the core may also distinguish attributes. */
    length = snprintf(key, sizeof(key), "%s%s%s", name, text ? "#" : "",
                       text ? text : "");
    assert(length >= 0 && (size_t)length < sizeof(key));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, type);
    return js_strdup(ctx, key);
}

static int module_normalize_module_init(JSContext *ctx, JSModuleDef *module)
{
    return JS_SetModuleExport(ctx, module, "default",
                              JS_GetModulePrivateValue(ctx, module));
}

static JSModuleDef *module_normalize_load(JSContext *ctx, const char *name,
                                           ModuleNormalizeState *state)
{
    JSModuleDef *module;
    JSValue value;

    assert(strlen(name) < sizeof(state->last_name));
    strcpy(state->last_name, name);
    if (state->source_text) {
        static const char source[] = "export default 'source-text';";
        value = JS_Eval(ctx, source, sizeof(source) - 1, name,
                        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        if (JS_IsException(value))
            return NULL;
        module = JS_VALUE_GET_PTR(value);
        JS_FreeValue(ctx, value);
        return module;
    }
    value = JS_NewString(ctx, name);
    if (JS_IsException(value))
        return NULL;
    module = JS_NewCModule(ctx, name, module_normalize_module_init);
    if (!module) {
        JS_FreeValue(ctx, value);
        return NULL;
    }
    JS_SetModulePrivateValue(ctx, module, value);
    if (JS_AddModuleExport(ctx, module, "default") < 0)
        return NULL;
    return module;
}

static JSModuleDef *module_normalize_loader1(JSContext *ctx, const char *name,
                                               void *opaque)
{
    ModuleNormalizeState *state = opaque;

    state->loader1_calls++;
    return module_normalize_load(ctx, name, state);
}

static JSModuleDef *module_normalize_loader2(JSContext *ctx, const char *name,
                                               void *opaque,
                                               JSValueConst attributes)
{
    ModuleNormalizeState *state = opaque;

    assert(JS_IsUndefined(attributes) || JS_IsObject(attributes));
    state->loader2_calls++;
    return module_normalize_load(ctx, name, state);
}

static void test_module_normalize_attributes(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx, *other;
    JSValue global, other_global, first, second, type;
    ModuleNormalizeState state = { .retained_attributes = JS_UNDEFINED };
    JSModuleNormalizeFunc2 *normalize = module_normalize_with_attributes;
    void (*install)(JSRuntime *, JSModuleNormalizeFunc2 *) =
        JS_SetModuleNormalizeFunc2;
    int calls, loads;
    const char *text;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetModuleLoaderFunc2(rt, NULL, module_normalize_loader2,
                            module_normalize_check_attributes, &state);
    install(rt, normalize);
    module_normalize_run(ctx,
        "import * as a from 'virtual' with {type:'variant-a'};"
        "import * as again from 'virtual' with {type:'variant-a'};"
        "globalThis.moduleNormalA=a;"
        "if(a!==again || a.default!=='virtual#variant-a')"
        "throw Error('static identity');",
        "attribute-identity.js", JS_EVAL_TYPE_MODULE);
    assert(state.loader2_calls == 1 && state.normalize2_calls >= 1);
    calls = state.normalize2_calls;
    module_normalize_run(ctx,
        "(async()=>{"
        "const a=await import('virtual',{with:{type:'variant-a'}});"
        "const b=await import('virtual',{with:{type:'variant-b'}});"
        "const again=await import('virtual',{with:{type:'variant-b'}});"
        "if(a!==moduleNormalA || a===b || again!==b ||"
        "b.default!=='virtual#variant-b')"
        "throw Error('host identity'); globalThis.moduleNormalB=b;"
        "})()", "attribute-two-types.js", JS_EVAL_TYPE_GLOBAL);
    assert(state.normalize2_calls == calls + 3 && state.loader2_calls == 2);
    calls = state.normalize2_calls;
    module_normalize_run(ctx,
        "(async()=>{"
        "const x=await import('virtual',"
        "{with:{type:'variant-a',flavor:'stable'}});"
        "const y=await import('virtual',"
        "{with:{flavor:'stable',type:'variant-a'}});"
        "let reads=0; const options={with:{get type(){reads++;"
        "return 'variant-a'}}};"
        "const z=await import('virtual',options);"
        "if(x!==y || z!==moduleNormalA || reads!==1)"
        "throw Error('cache/getters');"
        "})()", "attribute-repeat.js", JS_EVAL_TYPE_GLOBAL);
    assert(state.normalize2_calls == calls + 3);
    loads = state.loader2_calls;
    calls = state.normalize2_calls;
    module_normalize_run(ctx,
        "(async()=>{try {await import('virtual',{with:{type:7}})}"
        "catch(e){if(e instanceof TypeError)return; throw e}"
        "throw Error('non-string accepted')})()",
        "attribute-invalid.js", JS_EVAL_TYPE_GLOBAL);
    assert(state.normalize2_calls == calls && state.loader2_calls == loads);
    module_normalize_run(ctx,
        "(async()=>{try {await import('virtual',{with:{unsupported:'yes'}})}"
        "catch(e){if(e instanceof TypeError)return; throw e}"
        "throw Error('unsupported accepted')})()",
        "attribute-unsupported.js", JS_EVAL_TYPE_GLOBAL);
    assert(state.normalize2_calls == calls && state.loader2_calls == loads);
    module_normalize_run(ctx,
        "import * as a from 'empty';"
        "import * as b from 'empty' with {};"
        "if(a!==b || a.default!=='empty') throw Error('static empty');"
        "globalThis.moduleEmpty=a;",
        "attribute-empty.js", JS_EVAL_TYPE_MODULE);
    module_normalize_run(ctx,
        "(async()=>{const a=await import('empty');"
        "const b=await import('empty',{with:{}});"
        "if(a!==moduleEmpty || b!==a) throw Error('dynamic empty')})()",
        "attribute-empty-dynamic.js", JS_EVAL_TYPE_GLOBAL);
    assert(state.loader2_calls == loads + 1);
    other = JS_NewContext(rt);
    assert(other);
    module_normalize_run(other,
        "import * as a from 'virtual' with {type:'variant-a'};"
        "globalThis.moduleNormalA=a;", "other-context.js", JS_EVAL_TYPE_MODULE);
    assert(state.loader2_calls == loads + 2);
    global = JS_GetGlobalObject(ctx);
    other_global = JS_GetGlobalObject(other);
    first = JS_GetPropertyStr(ctx, global, "moduleNormalA");
    second = JS_GetPropertyStr(other, other_global, "moduleNormalA");
    assert(JS_IsObject(first) && JS_IsObject(second));
    assert(!JS_StrictEq(ctx, first, second));
    JS_FreeValue(ctx, first);
    JS_FreeValue(other, second);
    JS_FreeValue(ctx, global);
    JS_FreeValue(other, other_global);
    JS_FreeContext(other);
    JS_RunGC(rt);
    type = module_normalize_own_attribute(ctx, state.retained_attributes, "type");
    text = JS_ToCString(ctx, type);
    assert(text && !strcmp(text, "variant-a"));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, type);
    JS_FreeValue(ctx, state.retained_attributes);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_module_normalize_rejection(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    ModuleNormalizeState state = {
        .reject_json = 1, .source_text = 1,
        .retained_attributes = JS_UNDEFINED,
    };
    int calls;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetModuleLoaderFunc2(rt, NULL, module_normalize_loader2,
                            module_normalize_check_attributes, &state);
    JS_SetModuleNormalizeFunc2(rt, module_normalize_with_attributes);
    module_normalize_run(ctx,
        "import source from 'cached'; if(source!=='source-text')"
        "throw Error('fixture');", "cache-source.js", JS_EVAL_TYPE_MODULE);
    assert(state.loader2_calls == 1);
    calls = state.normalize2_calls;
    module_normalize_rejection(ctx,
        "import source from 'cached' with {type:'json'};",
        "cache-reject-static.js", JS_EVAL_TYPE_MODULE,
        "cached type=json rejected");
    assert(state.normalize2_calls == calls + 1 && state.loader2_calls == 1);
    module_normalize_rejection(ctx, "import('cached',{with:{type:'json'}})",
        "cache-reject-dynamic.js", JS_EVAL_TYPE_GLOBAL,
        "cached type=json rejected");
    assert(state.normalize2_calls == calls + 2 && state.loader2_calls == 1);
    state.throw_reason = "normalizer sentinel";
    module_normalize_rejection(ctx, "import 'uncached-static';",
        "throw-static.js", JS_EVAL_TYPE_MODULE, "normalizer sentinel");
    module_normalize_rejection(ctx, "import('uncached-dynamic')",
        "throw-dynamic.js", JS_EVAL_TYPE_GLOBAL, "normalizer sentinel");
    assert(state.normalize2_calls == calls + 4 && state.loader2_calls == 1);
    state.throw_reason = NULL;
    module_normalize_rejection(ctx, "import('reverse',{with:{type:'json'}})",
        "reverse-json.js", JS_EVAL_TYPE_GLOBAL, "cached type=json rejected");
    assert(state.loader2_calls == 1);
    module_normalize_run(ctx, "import('reverse')", "reverse-source.js",
                          JS_EVAL_TYPE_GLOBAL);
    assert(state.loader2_calls == 2 && !strcmp(state.last_name, "reverse"));
    JS_FreeValue(ctx, state.retained_attributes);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_module_normalize_setters(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    ModuleNormalizeState first = {
        .old_prefix = "one", .retained_attributes = JS_UNDEFINED,
    }, second = {
        .old_prefix = "two", .retained_attributes = JS_UNDEFINED,
    };
    int checks, normalizers, loads;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetModuleLoaderFunc2(rt, module_normalize_legacy, module_normalize_loader2,
                            module_normalize_check_attributes, &first);
    JS_SetModuleNormalizeFunc2(rt, module_normalize_with_attributes);
    module_normalize_run(ctx, "import('first',{with:{type:'variant-a'}})",
                          "setters.js", JS_EVAL_TYPE_GLOBAL);
    assert(first.normalize1_calls == 0 && first.normalize2_calls == 1);
    assert(first.loader2_calls == 1 && first.checker_calls == 1);
    assert(!strcmp(first.last_name, "first#variant-a"));
    JS_SetModuleLoaderFunc2(rt, module_normalize_legacy, module_normalize_loader2,
                            module_normalize_check_attributes, &second);
    module_normalize_run(ctx, "import('second',{with:{type:'variant-a'}})",
                          "setters.js", JS_EVAL_TYPE_GLOBAL);
    assert(second.normalize1_calls == 1 && second.normalize2_calls == 0);
    assert(second.loader2_calls == 1 && second.checker_calls == 1);
    assert(!strcmp(second.last_name, "two:second"));
    JS_SetModuleNormalizeFunc2(rt, module_normalize_with_attributes);
    module_normalize_run(ctx, "import('third',{with:{type:'variant-a'}})",
                          "setters.js", JS_EVAL_TYPE_GLOBAL);
    assert(second.normalize2_calls == 1 && second.loader2_calls == 2);
    assert(second.checker_calls == 2);
    assert(!strcmp(second.last_name, "third#variant-a"));
    checks = first.checker_calls;
    JS_SetModuleLoaderFunc(rt, module_normalize_legacy,
                           module_normalize_loader1, &first);
    module_normalize_run(ctx, "import('fourth',{with:{unsupported:'yes'}})",
                          "setters.js", JS_EVAL_TYPE_GLOBAL);
    assert(first.normalize1_calls == 1 && first.normalize2_calls == 1);
    assert(first.loader1_calls == 1 && first.checker_calls == checks);
    assert(!strcmp(first.last_name, "one:fourth"));
    JS_SetModuleNormalizeFunc2(rt, module_normalize_with_attributes);
    module_normalize_run(ctx, "import('fifth',{with:{type:'variant-b'}})",
                          "setters.js", JS_EVAL_TYPE_GLOBAL);
    assert(first.normalize2_calls == 2 && first.loader1_calls == 2);
    assert(first.checker_calls == checks);
    assert(!strcmp(first.last_name, "fifth#variant-b"));
    normalizers = first.normalize1_calls + first.normalize2_calls;
    JS_SetModuleNormalizeFunc2(rt, NULL);
    module_normalize_run(ctx, "import('./target')", "dir/entry.js",
                          JS_EVAL_TYPE_GLOBAL);
    assert(!strcmp(first.last_name, "dir/target"));
    assert(first.normalize1_calls + first.normalize2_calls == normalizers);
    loads = first.loader1_calls;
    JS_SetModuleLoaderFunc(rt, NULL, NULL, &second);
    module_normalize_run(ctx, "import('./target')", "dir/entry.js",
                          JS_EVAL_TYPE_GLOBAL);
    module_normalize_rejection(ctx, "import('./missing')", "dir/entry.js",
        JS_EVAL_TYPE_GLOBAL,
        "ReferenceError: could not load module 'dir/missing'");
    assert(first.loader1_calls == loads && second.loader1_calls == 0);
    JS_SetModuleLoaderFunc2(rt, NULL, NULL,
                            module_normalize_check_attributes, &second);
    JS_SetModuleNormalizeFunc2(rt, NULL);
    checks = second.checker_calls;
    module_normalize_run(ctx, "import('./target',{with:{}})",
                          "dir/entry.js", JS_EVAL_TYPE_GLOBAL);
    module_normalize_rejection(ctx,
        "import('./missing',{with:{type:'variant-b'}})", "dir/entry.js",
        JS_EVAL_TYPE_GLOBAL,
        "ReferenceError: could not load module 'dir/missing'");
    assert(second.checker_calls == checks + 2 && second.loader2_calls == 2);
    assert(second.normalize1_calls == 1 && second.normalize2_calls == 1);
    JS_FreeValue(ctx, first.retained_attributes);
    JS_FreeValue(ctx, second.retained_attributes);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}
/* Bytecode restoration can give the attribute object Object.prototype. */
static void test_module_normalize_bytecode(void)
{
    static const struct {
        const char *source, *expected_name, *expected_type;
    } cases[] = {
        {
            "import value from 'restored-flavor' with {flavor:'stable'};"
            "globalThis.moduleNormalRestored=value;",
            "restored-flavor", NULL,
        },
        {
            "import value from 'restored-own' with "
            "{type:'variant-a',flavor:'stable'};"
            "globalThis.moduleNormalRestored=value;",
            "restored-own#variant-a", "variant-a",
        },
    };
    JSRuntime *rt = JS_NewRuntime();
    size_t i;

    assert(rt);
    for (i = 0; i < countof(cases); i++) {
        ModuleNormalizeState compile_state = {
            .retained_attributes = JS_UNDEFINED,
        }, state = {
            .reject_json = 1, .retained_attributes = JS_UNDEFINED,
        };
        JSContext *compile_ctx = JS_NewContext(rt), *ctx;
        JSValue compiled, restored, promise, global, value, type, flavor;
        uint8_t *bytecode;
        size_t bytecode_size;
        const char *text;

        assert(compile_ctx);
        JS_SetModuleLoaderFunc2(rt, NULL, module_normalize_loader2,
                                module_normalize_check_attributes, &compile_state);
        JS_SetModuleNormalizeFunc2(rt, module_normalize_with_attributes);
        compiled = JS_Eval(compile_ctx, cases[i].source, strlen(cases[i].source),
                           "normalizer-bytecode.js",
                           JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        assert(!JS_IsException(compiled));
        bytecode = JS_WriteObject(compile_ctx, &bytecode_size, compiled,
                                   JS_WRITE_OBJ_BYTECODE);
        assert(bytecode && bytecode_size > 0);
        JS_FreeValue(compile_ctx, compiled);
        JS_FreeValue(compile_ctx, compile_state.retained_attributes);
        ctx = JS_NewContext(rt);
        assert(ctx);
        JS_SetModuleLoaderFunc2(rt, NULL, module_normalize_loader2,
                                module_normalize_check_attributes, &state);
        JS_SetModuleNormalizeFunc2(rt, module_normalize_with_attributes);
        restored = JS_ReadObject(ctx, bytecode, bytecode_size, JS_READ_OBJ_BYTECODE);
        js_free(compile_ctx, bytecode);
        assert(!JS_IsException(restored));
        JS_FreeContext(compile_ctx);
        /* An inherited type must neither choose a key nor reject this request. */
        check_eval(ctx, "Object.prototype.type = 'json'; true");
        assert(state.normalize2_calls == 0 && state.loader2_calls == 0);
        JS_RunGC(rt);
        assert(JS_ResolveModule(ctx, restored) == 0);
        assert(state.normalize2_calls == 1 && state.loader2_calls == 1);
        assert(!strcmp(state.last_name, cases[i].expected_name));
        assert(JS_IsObject(state.retained_attributes));
        promise = JS_EvalFunction(ctx, restored);
        assert(!JS_IsException(promise));
        module_normalize_drain(rt);
        assert(JS_PromiseState(ctx, promise) == JS_PROMISE_FULFILLED);
        JS_FreeValue(ctx, promise);
        global = JS_GetGlobalObject(ctx);
        value = JS_GetPropertyStr(ctx, global, "moduleNormalRestored");
        text = JS_ToCString(ctx, value);
        assert(text && !strcmp(text, cases[i].expected_name));
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, global);
        /* The callback duplicated the borrowed object; inspect it after GC. */
        JS_RunGC(rt);
        type = module_normalize_own_attribute(ctx, state.retained_attributes, "type");
        assert(!JS_IsException(type));
        if (cases[i].expected_type) {
            text = JS_ToCString(ctx, type);
            assert(text && !strcmp(text, cases[i].expected_type));
            JS_FreeCString(ctx, text);
        } else {
            assert(JS_IsUndefined(type));
        }
        JS_FreeValue(ctx, type);
        flavor = module_normalize_own_attribute(ctx, state.retained_attributes,
                                                  "flavor");
        text = JS_ToCString(ctx, flavor);
        assert(text && !strcmp(text, "stable"));
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, flavor);
        check_eval(ctx, "delete Object.prototype.type; true");
        JS_FreeValue(ctx, state.retained_attributes);
        assert(!JS_HasException(ctx));
        JS_FreeContext(ctx);
        JS_RunGC(rt);
    }
    JS_FreeRuntime(rt);
}

static void test_array_from_async_raw_context(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx[2];
    JSValue global, array, method;
    int i, phase;

    assert(rt);
    for (i = 0; i < 2; i++) {
        ctx[i] = JS_NewContextRaw(rt);
        assert(ctx[i]);
        assert(JS_AddIntrinsicBaseObjects(ctx[i]) == 0);
        for (phase = 0; phase < 2; phase++) {
            global = JS_GetGlobalObject(ctx[i]);
            assert(!JS_IsException(global));
            array = JS_GetPropertyStr(ctx[i], global, "Array");
            assert(!JS_IsException(array));
            method = JS_GetPropertyStr(ctx[i], array, "fromAsync");
            assert(!JS_IsException(method));
            if (phase)
                assert(JS_IsFunction(ctx[i], method));
            else
                assert(JS_IsUndefined(method));
            JS_FreeValue(ctx[i], method);
            JS_FreeValue(ctx[i], array);
            JS_FreeValue(ctx[i], global);
            if (!phase)
                assert(JS_AddIntrinsicPromise(ctx[i]) == 0);
        }
    }
    JS_RunGC(rt);
    JS_FreeContext(ctx[1]);
    JS_FreeContext(ctx[0]);
    JS_FreeRuntime(rt);
}

/* JS realm tests cover behavior. Keep native control of host context release
   and collection after every job to check suspended builtin state roots. */
static void test_async_realm_job_roots(void)
{
    static const char source[] =
        "(async function() {"
        " const make = foreign.Function('return Promise.resolve(42)');"
        " const values = foreign.Array.fromAsync({"
        "   0: Promise.resolve(42), length: 1"
        " });"
        " const stack = new foreign.AsyncDisposableStack();"
        " stack.defer(make);"
        " const disposal = stack.disposeAsync();"
        " globalThis.foreign = null;"
        " const result = await values;"
        " await disposal;"
        " return result.length === 1 && result[0] === 42;"
        "})()";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx, *foreign, *job_ctx;
    JSValue global, result, value;
    int ret;

    assert(rt);
    ctx = JS_NewContext(rt);
    foreign = JS_NewContext(rt);
    assert(ctx && foreign);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "foreign",
                             JS_GetGlobalObject(foreign)) >= 0);
    JS_FreeValue(ctx, global);
    result = JS_Eval(ctx, source, sizeof(source) - 1, "async-realm-job-roots",
                     JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result));
    JS_FreeContext(foreign);
    JS_RunGC(rt);
    while ((ret = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
        JS_RunGC(rt);
    assert(ret == 0);
    assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
    value = JS_PromiseResult(ctx, result);
    assert(JS_IsBool(value) && JS_ToBool(ctx, value));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue disposal_return_primitive(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    assert(argc == 0);
    return JS_NewInt32(ctx, 42);
}

static void test_async_iterator_disposal_raw_context(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue iterator, prototype, next_prototype, dispose = JS_UNDEFINED;
    JSValue function, name, receiver, promise, result;
    JSPropertyEnum *properties;
    uint32_t count, i;
    JSContext *job_ctx;
    int status;
    const char *string;
    const char *source = "(async function*() {})()";

    assert(rt);
    ctx = JS_NewContextRaw(rt);
    assert(ctx);
    /* Exercise Promise initialization without base or Symbol intrinsics. */
    assert(JS_AddIntrinsicPromise(ctx) == 0);
    assert(JS_AddIntrinsicEval(ctx) == 0);
    iterator = JS_Eval(ctx, source, strlen(source), "raw-context.js",
                       JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(iterator));
    prototype = JS_DupValue(ctx, iterator);
    for (i = 0; i < 3; i++) {
        next_prototype = JS_GetPrototype(ctx, prototype);
        JS_FreeValue(ctx, prototype);
        prototype = next_prototype;
        assert(JS_IsObject(prototype));
    }
    assert(JS_GetOwnPropertyNames(ctx, &properties, &count, prototype,
                                  JS_GPN_SYMBOL_MASK) == 0);
    for (i = 0; i < count; i++) {
        function = JS_GetProperty(ctx, prototype, properties[i].atom);
        assert(!JS_IsException(function));
        name = JS_GetPropertyStr(ctx, function, "name");
        assert(!JS_IsException(name));
        string = JS_ToCString(ctx, name);
        assert(string);
        if (!strcmp(string, "[Symbol.asyncDispose]"))
            dispose = JS_DupValue(ctx, function);
        JS_FreeCString(ctx, string);
        JS_FreeValue(ctx, name);
        JS_FreeValue(ctx, function);
        JS_FreeAtom(ctx, properties[i].atom);
    }
    js_free(ctx, properties);
    assert(JS_IsFunction(ctx, dispose));
    receiver = JS_NewObject(ctx);
    assert(!JS_IsException(receiver));
    promise = JS_Call(ctx, dispose, receiver, 0, NULL);
    assert(!JS_IsException(promise));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_FULFILLED);
    result = JS_PromiseResult(ctx, promise);
    assert(JS_IsUndefined(result));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, promise);
    promise = JS_Call(ctx, dispose, JS_NULL, 0, NULL);
    assert(!JS_IsException(promise));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED);
    assert(!JS_HasException(ctx));
    JS_FreeValue(ctx, promise);
    assert(JS_SetPropertyStr(ctx, receiver, "return",
                             JS_NewCFunction(ctx, disposal_return_primitive, "return", 0)) == 1);
    promise = JS_Call(ctx, dispose, receiver, 0, NULL);
    assert(!JS_IsException(promise));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_PENDING);
    while ((status = JS_ExecutePendingJob(rt, &job_ctx)) > 0)
        ;
    assert(status == 0);
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_FULFILLED);
    result = JS_PromiseResult(ctx, promise);
    assert(JS_IsUndefined(result));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, promise);
    JS_FreeValue(ctx, receiver);
    JS_FreeValue(ctx, dispose);
    JS_FreeValue(ctx, prototype);
    JS_FreeValue(ctx, iterator);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_conversion_public_api(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue object, primitive, result, method, error;
    const char *text;
    int value;
    int64_t value64;
    unsigned i;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    primitive = JS_NewInt32(ctx, 7);
    object = JS_ToObject(ctx, primitive);
    assert(JS_IsObject(object));
    method = JS_GetPropertyStr(ctx, object, "valueOf");
    result = JS_Call(ctx, method, object, 0, NULL);
    assert(JS_StrictEq(ctx, result, primitive));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, method);
    result = JS_ToObject(ctx, object);
    assert(JS_StrictEq(ctx, result, object));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, object);
    result = JS_ToObject(ctx, JS_NULL);
    assert(JS_IsException(result));
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);

    object = utf16_api_eval(ctx,
        "({ [Symbol.toPrimitive](hint) { "
        "if (hint === 'string') return 'string-hint'; "
        "if (hint === 'number') return 41; return 42; } })");
    result = JS_ToPrimitive(ctx, object, JS_HINT_STRING);
    text = JS_ToCString(ctx, result);
    assert(text && !strcmp(text, "string-hint"));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, result);
    result = JS_ToPrimitive(ctx, object, JS_HINT_NUMBER);
    assert(JS_ToInt32(ctx, &value, result) == 0 && value == 41);
    JS_FreeValue(ctx, result);
    result = JS_ToPrimitive(ctx, object, JS_HINT_NONE);
    assert(JS_ToInt32(ctx, &value, result) == 0 && value == 42);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, object);
    object = utf16_api_eval(ctx, "({ [Symbol.toPrimitive]() { throw 17; } })");
    result = JS_ToPrimitive(ctx, object, JS_HINT_STRING);
    assert(JS_IsException(result));
    error = JS_GetException(ctx);
    assert(JS_ToInt32(ctx, &value, error) == 0 && value == 17);
    JS_FreeValue(ctx, error);
    assert(JS_ToInt32Sat(ctx, &value, object) < 0);
    error = JS_GetException(ctx);
    assert(JS_ToInt32(ctx, &value, error) == 0 && value == 17);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, object);

    assert(JS_ToInt32Sat(ctx, &value, JS_NewFloat64(ctx, 1e300)) == 0);
    assert(value == INT32_MAX);
    assert(JS_ToInt32Sat(ctx, &value, JS_NewFloat64(ctx, -1e300)) == 0);
    assert(value == INT32_MIN);
    assert(JS_ToInt32Sat(ctx, &value, JS_NAN) == 0 && value == 0);
    assert(JS_ToInt32Sat(ctx, &value, JS_NewFloat64(ctx, -3.75)) == 0);
    assert(value == -3);
    {
        const double values[] = { -0x1p63, -0x1.8p64, 0x1p63, 0x1.8p64 };
        for (i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
            assert(JS_ToInt64(ctx, &value64, JS_NewFloat64(ctx, values[i])) == 0);
            assert(value64 == INT64_MIN);
        }
    }
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_string_conversion_public_api(void)
{
    static const uint16_t units[] = { 'A', 0, 0xd83d, 0xde00, 0xd800 };
    static const uint32_t expected[] = { 'A', 0, 0x1f600, 0xd800 };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue value, error;
    uint32_t *buffer, sentinel;
    const char *text;
    int length, marker;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    value = JS_NewString(ctx, "plain");
    text = JS_ToCStringNZ(ctx, value);
    assert(text && !strcmp(text, "plain"));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, value);

    value = JS_NewStringUTF16(ctx, units, countof(units));
    assert(JS_ToCStringNZ(ctx, value) == NULL);
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    buffer = NULL;
    length = JS_ToUTF32String(ctx, &buffer, value);
    assert(length == countof(expected) && buffer);
    assert(!memcmp(buffer, expected, sizeof(expected)));
    js_free(ctx, buffer);
    JS_FreeValue(ctx, value);

    value = JS_NewString(ctx, "");
    length = JS_ToUTF32String(ctx, &buffer, value);
    assert(length == 0 && buffer);
    js_free(ctx, buffer);
    text = JS_ToCStringNZ(ctx, value);
    assert(text && !*text);
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, value);

    value = utf16_api_eval(ctx, "({ [Symbol.toPrimitive]() { throw 23; } })");
    buffer = &sentinel;
    assert(JS_ToUTF32String(ctx, &buffer, value) == -1);
    assert(buffer == NULL);
    error = JS_GetException(ctx);
    assert(JS_ToInt32(ctx, &marker, error) == 0 && marker == 23);
    JS_FreeValue(ctx, error);
    assert(JS_ToCStringNZ(ctx, value) == NULL);
    error = JS_GetException(ctx);
    assert(JS_ToInt32(ctx, &marker, error) == 0 && marker == 23);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, value);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_object_construction_public_api(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSClassID class_id = 0;
    JSClassDef def = { .class_name = "EmbeddingObject" };
    JSValue ctor, object, proto, expected, error;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_NewClassID(&class_id);
    assert(JS_NewClass(rt, class_id, &def) == 0);
    expected = JS_NewObject(ctx);
    JS_SetClassProto(ctx, class_id, JS_DupValue(ctx, expected));
    ctor = utf16_api_eval(ctx, "(function EmbeddingConstructor() {})");
    object = JS_CreateFromConstructor(ctx, ctor, class_id);
    assert(JS_IsObject(object) && JS_GetClassID(object) == class_id);
    proto = JS_GetPrototype(ctx, object);
    JS_FreeValue(ctx, expected);
    expected = JS_GetPropertyStr(ctx, ctor, "prototype");
    assert(JS_StrictEq(ctx, proto, expected));
    JS_FreeValue(ctx, proto);
    assert(JS_OrdinaryIsInstanceOf(ctx, object, ctor) == 1);
    check_eval(ctx,
        "globalThis.instanceOverride = function () {}; "
        "Object.defineProperty(instanceOverride, Symbol.hasInstance, "
        "{ value() { return true; } });");
    proto = utf16_api_eval(ctx, "instanceOverride");
    assert(JS_IsInstanceOf(ctx, object, proto) == 1);
    assert(JS_OrdinaryIsInstanceOf(ctx, object, proto) == 0);
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, object);

    object = JS_CreateFromConstructor(ctx, JS_UNDEFINED, class_id);
    proto = JS_GetPrototype(ctx, object);
    {
        JSValue fallback = JS_GetClassProto(ctx, class_id);
        assert(JS_StrictEq(ctx, proto, fallback));
        JS_FreeValue(ctx, fallback);
    }
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, object);
    JS_FreeValue(ctx, expected);
    JS_FreeValue(ctx, ctor);

    ctor = utf16_api_eval(ctx,
        "new Proxy(function () {}, { get(target, name) { "
        "if (name === 'prototype') throw 29; return Reflect.get(target, name); } })");
    object = JS_CreateFromConstructor(ctx, ctor, class_id);
    assert(JS_IsException(object));
    error = JS_GetException(ctx);
    assert(JS_StrictEq(ctx, error, JS_NewInt32(ctx, 29)));
    JS_FreeValue(ctx, error);
    assert(JS_OrdinaryIsInstanceOf(ctx, JS_NULL, ctor) == 0);
    JS_FreeValue(ctx, ctor);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_date_value_public_api(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue date, value, error;
    double milliseconds;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    date = utf16_api_eval(ctx,
        "(() => { const date = new Date(37); "
        "date.valueOf = date.getTime = () => { throw 31; }; return date; })()");
    assert(JS_ThisDateValue(ctx, &milliseconds, date) == 0);
    assert(milliseconds == 37);
    JS_FreeValue(ctx, date);
    date = JS_NewDate(ctx, NAN);
    assert(!JS_IsException(date));
    assert(JS_ThisDateValue(ctx, &milliseconds, date) == 0);
    assert(isnan(milliseconds));
    JS_FreeValue(ctx, date);
    value = utf16_api_eval(ctx, "({ valueOf() { throw 31; } })");
    assert(JS_ThisDateValue(ctx, &milliseconds, value) < 0);
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, value);
    value = utf16_api_eval(ctx, "new Proxy(new Date(37), {})");
    assert(JS_ThisDateValue(ctx, &milliseconds, value) < 0);
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, value);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_iterator_public_api(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue iterable, iter, next, value, error;
    JS_BOOL done;
    int i;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    iterable = utf16_api_eval(ctx, "[1, 2]");
    iter = JS_GetIterator(ctx, iterable, 0);
    assert(!JS_IsException(iter));
    next = JS_GetPropertyStr(ctx, iter, "next");
    for (i = 1; i <= 3; i++) {
        value = JS_IteratorNext(ctx, iter, next, &done);
        assert(!JS_IsException(value));
        assert(done == (i == 3));
        if (done)
            assert(JS_IsUndefined(value));
        else
            assert(JS_StrictEq(ctx, value, JS_NewInt32(ctx, i)));
        JS_FreeValue(ctx, value);
    }
    JS_FreeValue(ctx, next);
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, iterable);
    iter = utf16_api_eval(ctx,
        "({ next() { if (arguments.length) throw 11; "
        "return { done: true, get value() { throw 13; } }; } })");
    next = JS_GetPropertyStr(ctx, iter, "next");
    value = JS_IteratorNext(ctx, iter, next, &done);
    assert(done && JS_IsUndefined(value));
    JS_FreeValue(ctx, next);
    JS_FreeValue(ctx, iter);
    iterable = utf16_api_eval(ctx,
        "({ [Symbol.asyncIterator]() { return { next() {} }; }, "
        "get [Symbol.iterator]() { throw 17; } })");
    iter = JS_GetIterator(ctx, iterable, 1);
    assert(JS_IsObject(iter));
    JS_FreeValue(ctx, iter);
    JS_FreeValue(ctx, iterable);
    iter = utf16_api_eval(ctx, "({ return() { throw 37; } })");
    JS_Throw(ctx, JS_NewInt32(ctx, 19));
    assert(JS_IteratorClose(ctx, iter, 1) < 0);
    error = JS_GetException(ctx);
    assert(JS_StrictEq(ctx, error, JS_NewInt32(ctx, 19)));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, iter);
    iter = utf16_api_eval(ctx, "({ return() { return 1; } })");
    assert(JS_IteratorClose(ctx, iter, 0) < 0);
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, iter);
    iter = utf16_api_eval(ctx, "({ return: null })");
    assert(JS_IteratorClose(ctx, iter, 0) == 0);
    JS_FreeValue(ctx, iter);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue flags_api_callback(JSContext *ctx, JSValueConst func_obj,
                                  JSValueConst this_val, int argc,
                                  JSValueConst *argv, int flags)
{
    JSValue result = JS_NewObject(ctx);

    if (JS_IsException(result))
        return result;
    assert(JS_SetPropertyStr(ctx, result, "func", JS_DupValue(ctx, func_obj)) >= 0);
    assert(JS_SetPropertyStr(ctx, result, "receiver", JS_DupValue(ctx, this_val)) >= 0);
    assert(JS_SetPropertyStr(ctx, result, "construct",
                            JS_NewBool(ctx, flags & JS_CALL_FLAG_CONSTRUCTOR)) >= 0);
    assert(JS_SetPropertyStr(ctx, result, "argc", JS_NewInt32(ctx, argc)) >= 0);
    assert(JS_SetPropertyStr(ctx, result, "arg", JS_DupValue(ctx, argv[0])) >= 0);
    return result;
}

static void test_callback_flags_public_api(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSCFunctionType callback = { .constructor_or_func_flags = flags_api_callback };
    JSValue func, alt, result, value, arg;
    int i;

    assert(JS_CFUNC_f_f == 6 && JS_CFUNC_iterator_next == 12);
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    func = JS_NewCFunction2(ctx, callback.generic, "EmbeddingCallback", 1,
                           JS_CFUNC_constructor_or_func_flags, 0);
    assert(JS_IsConstructor(ctx, func));
    alt = utf16_api_eval(ctx, "(function AlternateTarget() {})");
    arg = JS_NewInt32(ctx, 9);
    for (i = 0; i < 3; i++) {
        if (i == 0)
            result = JS_Call(ctx, func, JS_NewInt32(ctx, 7), 0, NULL);
        else if (i == 1)
            result = JS_CallConstructor(ctx, func, 1, &arg);
        else
            result = JS_CallConstructor2(ctx, func, alt, 1, &arg);
        assert(JS_IsObject(result));
        value = JS_GetPropertyStr(ctx, result, "func");
        assert(JS_StrictEq(ctx, value, func));
        JS_FreeValue(ctx, value);
        value = JS_GetPropertyStr(ctx, result, "receiver");
        assert(JS_StrictEq(ctx, value, i == 0 ? JS_NewInt32(ctx, 7) :
                          i == 1 ? func : alt));
        JS_FreeValue(ctx, value);
        value = JS_GetPropertyStr(ctx, result, "construct");
        assert(JS_ToBool(ctx, value) == (i != 0));
        JS_FreeValue(ctx, value);
        value = JS_GetPropertyStr(ctx, result, "argc");
        assert(JS_StrictEq(ctx, value, JS_NewInt32(ctx, i != 0)));
        JS_FreeValue(ctx, value);
        value = JS_GetPropertyStr(ctx, result, "arg");
        assert(i == 0 ? JS_IsUndefined(value) : JS_StrictEq(ctx, value, arg));
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, alt);
    JS_FreeValue(ctx, func);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static JSValue property_list_api_method(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    return JS_NewInt32(ctx, 41);
}

static void test_property_list_definitions(void)
{
    static const JSCFunctionListEntry fields[] = {
        JS_CFUNC_DEF("method", 0, property_list_api_method),
        JS_PROP_STRING_DEF("label", "new", JS_PROP_CONFIGURABLE),
        JS_PROP_INT32_DEF("number", 42, JS_PROP_CONFIGURABLE),
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue obj, method, value, error;
    int i;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    obj = JS_NewObject(ctx);
    for (i = 0; i < 2; i++) {
        assert(JS_SetPropertyFunctionList(ctx, obj, fields, countof(fields)) == 0);
        method = JS_GetPropertyStr(ctx, obj, "method");
        value = JS_Call(ctx, method, obj, 0, NULL);
        assert(JS_StrictEq(ctx, value, JS_NewInt32(ctx, 41)));
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, method);
    }
    JS_FreeValue(ctx, obj);
    obj = utf16_api_eval(ctx, "Object.preventExtensions({})");
    assert(JS_SetPropertyFunctionList(ctx, obj, fields, 1) < 0);
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, obj);
    obj = utf16_api_eval(ctx, "Object.freeze({ number: 1 })");
    assert(JS_SetPropertyFunctionList(ctx, obj, fields + 2, 1) < 0);
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, obj);
    obj = utf16_api_eval(ctx,
        "new Proxy({}, { defineProperty() { throw 43; } })");
    assert(JS_SetPropertyFunctionList(ctx, obj, fields, 1) < 0);
    error = JS_GetException(ctx);
    assert(JS_StrictEq(ctx, error, JS_NewInt32(ctx, 43)));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, obj);
    assert(JS_SetPropertyFunctionList(ctx, JS_NULL, fields, 1) < 0);
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    {
        static const JSCFunctionListEntry index_field[] = {
            JS_CFUNC_DEF("0", 0, property_list_api_method),
        };
        static const JSCFunctionListEntry length_field[] = {
            JS_PROP_INT32_DEF("length", 0, JS_PROP_WRITABLE),
        };
        obj = JS_NewArray(ctx);
        assert(JS_SetPropertyFunctionList(ctx, obj, index_field, 1) == 0);
        value = JS_GetPropertyStr(ctx, obj, "length");
        assert(JS_StrictEq(ctx, value, JS_NewInt32(ctx, 1)));
        JS_FreeValue(ctx, value);
        assert(JS_SetPropertyFunctionList(ctx, obj, length_field, 1) == 0);
        value = JS_GetPropertyStr(ctx, obj, "0");
        assert(JS_IsUndefined(value));
        JS_FreeValue(ctx, value);
        assert(JS_SetPropertyFunctionList(ctx, obj, fields, 1) == 0);
        JS_FreeValue(ctx, obj);
        obj = utf16_api_eval(ctx, "new String('a')");
        assert(JS_SetPropertyFunctionList(ctx, obj, index_field, 1) < 0);
        error = JS_GetException(ctx);
        assert(JS_IsError(ctx, error));
        JS_FreeValue(ctx, error);
        value = JS_GetPropertyStr(ctx, obj, "0");
        method = JS_NewString(ctx, "a");
        assert(JS_StrictEq(ctx, value, method));
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, method);
        assert(JS_SetPropertyFunctionList(ctx, obj, fields, 1) == 0);
        JS_FreeValue(ctx, obj);
    }
    {
        static const JSCFunctionListEntry global_field[] = {
            JS_CFUNC_DEF("installedApiGlobal", 0, property_list_api_method),
        };
        check_eval(ctx, "globalThis.readApiGlobal = () => installedApiGlobal(); "
                        "try { readApiGlobal(); } catch (e) {}; true;");
        obj = JS_GetGlobalObject(ctx);
        assert(JS_SetPropertyFunctionList(ctx, obj, global_field, 1) == 0);
        value = utf16_api_eval(ctx, "readApiGlobal()");
        assert(JS_StrictEq(ctx, value, JS_NewInt32(ctx, 41)));
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, obj);
    }
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_function_property_oom(void)
{
    int operation;

    for (operation = 0; operation < 3; operation++) {
        size_t failure_at;
        int failures = 0;

        for (failure_at = 1; failure_at < 64; failure_at++) {
            AllocationProbe probe = { 0 };
            JSRuntime *rt = JS_NewRuntime2(&probe_functions, &probe);
            JSContext *ctx;
            JSValue functions[128], error, compiled = JS_UNDEFINED, expected;
            size_t count = 0, i;

            assert(rt);
            ctx = JS_NewContext(rt);
            assert(ctx);
            if (operation != 0) {
                const char *source = operation == 1 ?
                    "(function named() {})" : "(function* named() {})";
                compiled = JS_Eval(ctx, source, strlen(source), "function-oom.js",
                                   JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
                assert(!JS_IsException(compiled));
            }
            expected = JS_NewString(ctx, operation == 0 ? "123" : "named");
            assert(!JS_IsException(expected));
            probe.attempts = 0;
            probe.failure_at = failure_at;
            /* Retain enough functions to exhaust pooled blocks too. */
            for (i = 0; i < countof(functions); i++) {
                JSValue function;
                if (operation == 0)
                    function = JS_NewCFunction(ctx, property_list_api_method, "123", 0);
                else
                    function = JS_EvalFunction(ctx, JS_DupValue(ctx, compiled));
                if (JS_IsException(function)) {
                    assert(probe.failed && JS_HasException(ctx));
                    failures++;
                    break;
                }
                assert(!JS_HasException(ctx));
                functions[count++] = function;
                if (probe.failed)
                    break;
            }
            probe.failure_at = 0;
            if (JS_HasException(ctx)) {
                error = JS_GetException(ctx);
                JS_FreeValue(ctx, error);
            }
            for (i = 0; i < count; i++) {
                JSValue name = JS_GetPropertyStr(ctx, functions[i], "name");
                assert(JS_StrictEq(ctx, name, expected));
                JS_FreeValue(ctx, name);
                JS_FreeValue(ctx, functions[i]);
            }
            JS_FreeValue(ctx, expected);
            JS_FreeValue(ctx, compiled);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            assert(probe.live == 0);
            if (!probe.failed)
                break;
        }
        assert(failures > 0);
    }
}

static JSValue factory_api_callback(JSContext *ctx, JSValueConst new_target,
                                    int argc, JSValueConst *argv)
{
    return JS_NewObject(ctx);
}

static JSValue factory_api_create(JSContext *ctx, int class_id,
                                  JSValueConst parent, int flags)
{
    static const JSCFunctionListEntry ctor_fields[] = {
        JS_PROP_INT32_DEF("version", 5, JS_PROP_CONFIGURABLE),
    };
    static const JSCFunctionListEntry proto_fields[] = {
        JS_CFUNC_DEF("method", 0, property_list_api_method),
    };
    return JS_NewCConstructor(ctx, class_id, "EmbeddingFactory",
                              factory_api_callback, 0, JS_CFUNC_constructor, 0,
                              parent, ctor_fields, countof(ctor_fields),
                              proto_fields, countof(proto_fields), flags);
}

static void test_native_constructor_public_api(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSClassID class_id = 0;
    JSClassDef def = { .class_name = "EmbeddingFactoryPrototype" };
    JSValue ctor, proto, parent, value, old_proto, error, global;
    JSPropertyDescriptor desc;
    JSAtom atom;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_NewClassID(&class_id);
    assert(JS_NewClass(rt, class_id, &def) == 0);
    ctor = factory_api_create(ctx, class_id, JS_UNDEFINED,
                              JS_NEW_CTOR_NO_GLOBAL | JS_NEW_CTOR_PROTO_CLASS);
    assert(JS_IsConstructor(ctx, ctor));
    proto = JS_GetClassProto(ctx, class_id);
    assert(JS_GetClassID(proto) == class_id);
    value = JS_GetPropertyStr(ctx, ctor, "prototype");
    assert(JS_StrictEq(ctx, proto, value));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, ctor);

    parent = utf16_api_eval(ctx, "(function ParentFactory() {})");
    ctor = factory_api_create(ctx, class_id, parent, JS_NEW_CTOR_READONLY);
    assert(JS_IsConstructor(ctx, ctor));
    value = JS_GetPrototype(ctx, ctor);
    assert(JS_StrictEq(ctx, value, parent));
    JS_FreeValue(ctx, value);
    proto = JS_GetClassProto(ctx, class_id);
    value = JS_GetPrototype(ctx, proto);
    old_proto = JS_GetPropertyStr(ctx, parent, "prototype");
    assert(JS_StrictEq(ctx, value, old_proto));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, old_proto);
    atom = JS_NewAtom(ctx, "constructor");
    assert(JS_GetOwnProperty(ctx, &desc, proto, atom) == 1);
    assert(!(desc.flags & JS_PROP_WRITABLE));
    assert(JS_StrictEq(ctx, desc.value, ctor));
    JS_FreeValue(ctx, desc.value);
    JS_FreeValue(ctx, desc.getter);
    JS_FreeValue(ctx, desc.setter);
    JS_FreeAtom(ctx, atom);
    global = JS_GetGlobalObject(ctx);
    value = JS_GetPropertyStr(ctx, global, "EmbeddingFactory");
    assert(JS_StrictEq(ctx, value, ctor));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, parent);
    JS_FreeValue(ctx, proto);

    JS_FreeValue(ctx, global);

    ctor = factory_api_create(ctx, -1, JS_UNDEFINED, JS_NEW_CTOR_NO_GLOBAL);
    assert(JS_IsObject(ctor));
    JS_FreeValue(ctx, ctor);
    ctor = factory_api_create(ctx, -1, JS_UNDEFINED, JS_NEW_CTOR_PROTO_EXIST);
    assert(JS_IsException(ctor));
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    ctor = factory_api_create(ctx, INT_MAX, JS_UNDEFINED, JS_NEW_CTOR_NO_GLOBAL);
    assert(JS_IsException(ctor));
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    {
        static const JSCFunctionListEntry field[] = {
            JS_PROP_INT32_DEF("value", 1, JS_PROP_CONFIGURABLE),
        };
        ctor = JS_NewCConstructor(ctx, -1, "OversizedFactory",
                                  factory_api_callback, 0, JS_CFUNC_constructor, 0,
                                  JS_UNDEFINED, NULL, 0, field, INT_MAX,
                                  JS_NEW_CTOR_NO_GLOBAL);
        assert(JS_IsException(ctor));
        error = JS_GetException(ctx);
        assert(JS_IsError(ctx, error));
        JS_FreeValue(ctx, error);
    }
    parent = utf16_api_eval(ctx, "({ prototype: 1 })");
    ctor = factory_api_create(ctx, -1, parent, JS_NEW_CTOR_NO_GLOBAL);
    assert(JS_IsException(ctor));
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, parent);
    parent = utf16_api_eval(ctx, "({ get prototype() { throw 59; } })");
    ctor = factory_api_create(ctx, -1, parent, JS_NEW_CTOR_NO_GLOBAL);
    assert(JS_IsException(ctor));
    error = JS_GetException(ctx);
    assert(JS_StrictEq(ctx, error, JS_NewInt32(ctx, 59)));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, parent);
    assert(!JS_HasException(ctx));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_constructor_link_failure(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue ctor, proto, error;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    proto = utf16_api_eval(ctx, "Object.freeze({})");
    ctor = JS_NewCFunction(ctx, property_list_api_method, "FrozenPrototype", 0);
    assert(!JS_IsException(ctor));
    assert(JS_SetConstructor(ctx, ctor, proto) < 0);
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, proto);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_promise_constructor_bit(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue global, ctor, result, funcs[2];
    const char *source =
        "(() => { for (const create of [() => Promise.withResolvers(),"
        " () => Promise.resolve(1), () => Promise.reject(1), () => Promise.all([])]) {"
        " let caught = false; try { create(); } catch (e) {"
        " if (!(e instanceof TypeError)) throw e; caught = true; }"
        " if (!caught) throw Error('nonconstructor accepted'); } return true; })()";

    assert(rt && ctx);
    global = JS_GetGlobalObject(ctx);
    ctor = JS_GetPropertyStr(ctx, global, "Promise");
    assert(JS_IsConstructor(ctx, ctor));
    assert(JS_SetConstructorBit(ctx, ctor, 0));
    result = JS_Eval(ctx, source, strlen(source), "promise-constructor-bit.js",
                     JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsBool(result) && JS_ToBool(ctx, result));
    JS_FreeValue(ctx, result);
    /* The C API's internal intrinsic sentinel does not use this public bit. */
    result = JS_NewPromiseCapability(ctx, funcs);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, funcs[0]);
    JS_FreeValue(ctx, funcs[1]);
    JS_FreeValue(ctx, result);
    assert(JS_SetConstructorBit(ctx, ctor, 1));
    source = "Promise.withResolvers().promise instanceof Promise";
    result = JS_Eval(ctx, source, strlen(source), "promise-constructor-bit.js",
                     JS_EVAL_TYPE_GLOBAL);
    assert(JS_IsBool(result) && JS_ToBool(ctx, result));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_proxy_property_failures(int descriptor)
{
    static const char source[] =
        "new Proxy({}, { defineProperty() { return true; } })";
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue anchor, target = JS_UNDEFINED, handler = JS_UNDEFINED;
        JSValue proxy, method = JS_UNDEFINED, result = JS_UNDEFINED;
        JSAtom key = JS_ATOM_NULL;
        void *padding;
        int status;

        assert(ctx);
        anchor = JS_NewObject(ctx); /* Keep an empty shape shared. */
        if (descriptor) {
            proxy = JS_Eval(ctx, source, sizeof(source) - 1,
                            "proxy-descriptor-oom", JS_EVAL_TYPE_GLOBAL);
            key = JS_NewAtom(ctx, "x");
            assert(key != JS_ATOM_NULL);
        } else {
            JSValue global = JS_GetGlobalObject(ctx);
            JSValue constructor = JS_GetPropertyStr(ctx, global, "Proxy");

            method = JS_GetPropertyStr(ctx, constructor, "revocable");
            target = JS_NewObject(ctx);
            handler = JS_NewObject(ctx);
            proxy = constructor;
            JS_FreeValue(ctx, global);
        }
        assert(!JS_IsException(anchor) && !JS_IsException(proxy) &&
               !JS_IsException(method) && !JS_IsException(target) &&
               !JS_IsException(handler));
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step ? step : (size_t)-1);
        if (descriptor) {
            status = JS_DefineProperty(ctx, proxy, key, JS_NewInt32(ctx, 1),
                                       JS_UNDEFINED, JS_UNDEFINED,
                                       JS_PROP_C_W_E | JS_PROP_HAS_VALUE |
                                       JS_PROP_HAS_WRITABLE | JS_PROP_HAS_ENUMERABLE |
                                       JS_PROP_HAS_CONFIGURABLE);
        } else {
            JSValueConst arguments[2] = { target, handler };

            result = JS_Call(ctx, method, proxy, 2, arguments);
            status = JS_IsException(result) ? -1 : 1;
        }
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 128 && status > 0 && !JS_HasException(ctx));
        } else {
            assert(allocator.failures == 1);
            if (status < 0) {
                assert(JS_HasException(ctx));
                JS_FreeValue(ctx, JS_GetException(ctx));
            } else {
                assert(!JS_HasException(ctx));
            }
        }
        if (!descriptor && status > 0) {
            JSValue value = JS_GetPropertyStr(ctx, result, "proxy");
            JSValue revoke = JS_GetPropertyStr(ctx, result, "revoke");

            assert(JS_IsProxy(value) && JS_IsFunction(ctx, revoke));
            JS_FreeValue(ctx, value);
            JS_FreeValue(ctx, revoke);
        }
        JS_FreeValue(ctx, result);
        if (key != JS_ATOM_NULL)
            JS_FreeAtom(ctx, key);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, proxy);
        JS_FreeValue(ctx, target);
        JS_FreeValue(ctx, handler);
        JS_FreeValue(ctx, anchor);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

static void test_proxy_property_oom(void)
{
    test_proxy_property_failures(0);
    test_proxy_property_failures(1);
}

static void test_bound_function_oom(void)
{
    static const char source[] = "(function target(a, b) { return a + b; })";
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue target, bind, result, value, anchor;
        JSValueConst args[41];
        void *padding;
        int i;

        assert(ctx);
        target = JS_Eval(ctx, source, sizeof(source) - 1, "bind-oom",
                         JS_EVAL_TYPE_GLOBAL);
        bind = JS_GetPropertyStr(ctx, target, "bind");
        anchor = JS_NewObject(ctx);
        assert(!JS_IsException(target) && !JS_IsException(bind) &&
               !JS_IsException(anchor));
        args[0] = JS_UNDEFINED;
        for (i = 1; i < 41; i++)
            args[i] = JS_NewInt32(ctx, i);
        /* Reach bound-data and property allocations despite allocator pools. */
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = JS_Call(ctx, bind, target, 41, args);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 128);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(result)) {
            assert(step);
            oom_test_check_exception(ctx);
        } else {
            assert(!JS_HasException(ctx) && JS_IsFunction(ctx, result));
            value = JS_Call(ctx, result, JS_UNDEFINED, 0, NULL);
            assert(JS_StrictEq(ctx, value, JS_NewInt32(ctx, 3)));
            JS_FreeValue(ctx, value);
            value = JS_GetPropertyStr(ctx, result, "length");
            assert(JS_StrictEq(ctx, value, JS_NewInt32(ctx, 0)));
            JS_FreeValue(ctx, value);
            value = JS_GetPropertyStr(ctx, result, "name");
            {
                const char *name = JS_ToCString(ctx, value);

                assert(name && !strcmp(name, "bound target"));
                JS_FreeCString(ctx, name);
            }
            JS_FreeValue(ctx, value);
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, target);
        JS_FreeValue(ctx, bind);
        JS_FreeValue(ctx, anchor);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

static int kept_target_finalized;

static void kept_target_finalizer(JSRuntime *rt, JSValue value)
{
    kept_target_finalized++;
}

static JSValue clear_kept_during_job(JSContext *ctx, JSValueConst self,
                                     int argc, JSValueConst *argv)
{
    JS_ClearKeptObjects(JS_GetRuntime(ctx));
    return JS_UNDEFINED;
}

static void test_weakref_checkpoints(void)
{
    static const JSClassDef target_class = {
        "KeptTarget", .finalizer = kept_target_finalizer,
    };
    static JSClassID target_class_id;
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, constructor, target, weak, deref, result;
    JSMemoryUsage before, after;
    int i;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_NewClassID(&target_class_id);
    assert(JS_NewClass(rt, target_class_id, &target_class) == 0);
    global = JS_GetGlobalObject(ctx);
    constructor = JS_GetPropertyStr(ctx, global, "WeakRef");
    assert(JS_SetPropertyStr(ctx, global, "clearDuringJob",
                             JS_NewCFunction(ctx, clear_kept_during_job,
                                             "clearDuringJob", 0)) >= 0);
    target = JS_NewObjectClass(ctx, target_class_id);
    assert(!JS_IsException(target));
    assert(JS_SetPropertyStr(ctx, target, "self", JS_DupValue(ctx, target)) >= 0);
    weak = JS_CallConstructor(ctx, constructor, 1, (JSValueConst *)&target);
    assert(!JS_IsException(weak));
    deref = JS_GetPropertyStr(ctx, weak, "deref");
    assert(!JS_IsException(deref));
    /* Repeated deref must not grow the kept set or require new allocations. */
    JS_ComputeMemoryUsage(rt, &before);
    JS_SetMemoryLimit(rt, 0);
    for (i = 0; i < 1000; i++) {
        result = JS_Call(ctx, deref, weak, 0, NULL);
        assert(JS_StrictEq(ctx, result, target));
        JS_FreeValue(ctx, result);
    }
    JS_SetMemoryLimit(rt, (size_t)-1);
    JS_ComputeMemoryUsage(rt, &after);
    assert(before.memory_used_size == after.memory_used_size);
    JS_FreeValue(ctx, deref);
    JS_FreeValue(ctx, weak);
    JS_FreeValue(ctx, target);
    kept_target_finalized = 0;
    JS_RunGC(rt);
    assert(kept_target_finalized == 0);
    JS_ClearKeptObjects(rt);
    JS_RunGC(rt);
    assert(kept_target_finalized == 1);
    JS_ClearKeptObjects(rt); /* An idle checkpoint is harmless. */

    /* A later successful deref must establish a new kept root. */
    target = JS_NewObjectClass(ctx, target_class_id);
    weak = JS_CallConstructor(ctx, constructor, 1, (JSValueConst *)&target);
    assert(!JS_IsException(weak));
    deref = JS_GetPropertyStr(ctx, weak, "deref");
    assert(!JS_IsException(deref));
    JS_ClearKeptObjects(rt);
    result = JS_Call(ctx, deref, weak, 0, NULL);
    assert(JS_StrictEq(ctx, result, target));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, target);
    JS_RunGC(rt);
    assert(kept_target_finalized == 1);
    JS_ClearKeptObjects(rt);
    JS_RunGC(rt);
    assert(kept_target_finalized == 2);
    result = JS_Call(ctx, deref, weak, 0, NULL);
    assert(JS_IsUndefined(result));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, deref);
    JS_FreeValue(ctx, weak);

    check_eval(ctx, "globalThis.symbolWeak = new WeakRef(Symbol('kept'));"
                    "globalThis.objectWeak = new WeakRef({});"
                    "clearDuringJob();"
                    "symbolWeak.deref() !== undefined && objectWeak.deref() !== undefined");
    JS_RunGC(rt);
    check_eval(ctx, "symbolWeak.deref() !== undefined && objectWeak.deref() !== undefined");
    JS_ClearKeptObjects(rt);
    JS_RunGC(rt);
    check_eval(ctx, "symbolWeak.deref() === undefined && objectWeak.deref() === undefined");

    JS_FreeValue(ctx, constructor);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_weakref_oom(int dereference)
{
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue global, constructor, target, weak = JS_UNDEFINED;
        JSValue method = JS_UNDEFINED, result;
        void *padding;

        assert(ctx);
        global = JS_GetGlobalObject(ctx);
        constructor = JS_GetPropertyStr(ctx, global, "WeakRef");
        target = JS_NewObject(ctx);
        assert(!JS_IsException(constructor) && !JS_IsException(target));
        if (dereference) {
            weak = JS_CallConstructor(ctx, constructor, 1, (JSValueConst *)&target);
            assert(!JS_IsException(weak));
            method = JS_GetPropertyStr(ctx, weak, "deref");
            assert(!JS_IsException(method));
            JS_ClearKeptObjects(rt);
        }
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = dereference ? JS_Call(ctx, method, weak, 0, NULL) :
            JS_CallConstructor(ctx, constructor, 1, (JSValueConst *)&target);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 64);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(result)) {
            assert(step);
            oom_test_check_exception(ctx);
        } else {
            assert(!JS_HasException(ctx));
            assert(dereference ? JS_StrictEq(ctx, result, target) : JS_IsWeakRef(result));
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, weak);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, target);
        JS_FreeValue(ctx, constructor);
        JS_FreeValue(ctx, global);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

static void test_weakref_kept_oom(void)
{
    test_weakref_oom(0);
    test_weakref_oom(1);
}

static const char * const namespace_oom_sources[] = {
    "export const a = 42; export const b = 42;",
    "export const a = 42; export { a as '0', a as '1' };",
};

static JSModuleDef *namespace_oom_loader(JSContext *ctx, const char *name,
                                       void *opaque)
{
    const char *source = opaque;
    JSValue compiled;
    JSModuleDef *module;

    compiled = JS_Eval(ctx, source, strlen(source), name,
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(compiled))
        return NULL;
    module = JS_VALUE_GET_PTR(compiled);
    JS_FreeValue(ctx, compiled);
    return module;
}

static void test_module_namespace_failure(int mode)
{
    const char *source = namespace_oom_sources[mode / 2];
    int dynamic_import = mode & 1;
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSModuleDef *module = NULL;
        JSValue result = JS_UNDEFINED, promise = JS_UNDEFINED, value;
        void *padding;
        int ret = 0, jobs = 0;

        assert(ctx);
        if (dynamic_import) {
            JS_SetModuleLoaderFunc(rt, NULL, namespace_oom_loader, (void *)source);
            promise = JS_LoadModule(ctx, "", "namespace-oom");
            assert(!JS_IsException(promise));
        } else {
            JSValue compiled = JS_Eval(ctx, source, strlen(source), "namespace-oom",
                JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

            assert(!JS_IsException(compiled));
            module = JS_VALUE_GET_PTR(compiled);
            value = JS_EvalFunction(ctx, compiled);
            assert(!JS_IsException(value));
            JS_FreeValue(ctx, value);
        }
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        if (dynamic_import)
            ret = JS_ExecutePendingJob(rt, NULL);
        else
            result = JS_GetModuleNamespace(ctx, module);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 64);
        } else {
            assert(allocator.failures == 1);
        }
        if (ret < 0 || JS_IsException(result)) {
            assert(step);
            oom_test_check_exception(ctx);
        } else {
            assert(!JS_HasException(ctx));
        }
        if (dynamic_import) {
            /* Namespace failure rejects the import; the rejected reason and
               any queued reactions must still be released during teardown. */
            while (JS_IsJobPending(rt)) {
                assert(++jobs < 16);
                assert(JS_ExecutePendingJob(rt, NULL) > 0);
            }
            if (ret > 0)
                assert(JS_PromiseState(ctx, promise) != JS_PROMISE_PENDING);
        } else {
            JS_FreeValue(ctx, result);
            /* Failed namespaces must not be cached: a later attempt can
               construct the complete namespace after allocation recovers. */
            result = JS_GetModuleNamespace(ctx, module);
            assert(!JS_IsException(result) && !JS_HasException(ctx));
            value = JS_GetPropertyStr(ctx, result, "a");
            assert(JS_StrictEq(ctx, value, JS_NewInt32(ctx, 42)));
            JS_FreeValue(ctx, value);
            {
                JSValue symbol = JS_GetWellKnownSymbol(ctx,
                    JS_WELL_KNOWN_SYMBOL_TO_STRING_TAG);
                JSAtom atom = JS_ValueToAtom(ctx, symbol);
                const char *tag;

                assert(atom != JS_ATOM_NULL);
                value = JS_GetProperty(ctx, result, atom);
                JS_FreeAtom(ctx, atom);
                JS_FreeValue(ctx, symbol);
                tag = JS_ToCString(ctx, value);
                assert(tag && !strcmp(tag, "Module"));
                JS_FreeCString(ctx, tag);
                JS_FreeValue(ctx, value);
            }
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, promise);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_RunGC(rt);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

static void test_module_namespace_oom(void)
{
    int mode;

    for (mode = 0; mode < 4; mode++)
        test_module_namespace_failure(mode);
}

/* Exercise mandatory record allocation and optional table growth separately. */
static void test_collection_growth_oom(void)
{
    static const char * const constructors[] = {
        "Map", "Set", "WeakMap", "WeakSet", "Map", "WeakMap",
    };
    size_t kind;

    for (kind = 0; kind < countof(constructors); kind++) {
        size_t step, calls = 0;

        for (step = 0; step <= calls; step++) {
            OOMTestAllocator allocator;
            JSRuntime *rt = oom_test_runtime(&allocator);
            JSContext *ctx = JS_NewContext(rt);
            JSValue insert, result;
            void *padding;
            char source[512];
            int is_set = kind == 1 || kind == 3;

            assert(ctx);
            snprintf(source, sizeof(source),
                     "(() => { const m = new %s(); const keys = [{},{},{},{}];"
                     "for (let i = 0; i < 3; i++) m.%s(keys[i], i);"
                     "return () => { m.%s(keys[3], 42);"
                     "return m.has(keys[0]) && m.has(keys[3])%s; }; })()",
                     constructors[kind], is_set ? "add" : "set",
                     kind >= 4 ? "getOrInsert" : is_set ? "add" : "set",
                     is_set ? "" : " && m.get(keys[3]) === 42");
            insert = JS_Eval(ctx, source, strlen(source), "collection-oom.js",
                             JS_EVAL_TYPE_GLOBAL);
            assert(!JS_IsException(insert));
            padding = test_pool_padding(rt);
            oom_test_begin(&allocator, step);
            result = JS_Call(ctx, insert, JS_UNDEFINED, 0, NULL);
            oom_test_end(&allocator);
            if (!step) {
                calls = allocator.calls;
                assert(calls && calls < 16);
            } else {
                assert(allocator.failures == 1);
            }
            if (JS_IsException(result)) {
                assert(step);
                oom_test_check_exception(ctx);
            } else {
                assert(!JS_HasException(ctx) && JS_ToBool(ctx, result) == 1);
            }
            JS_FreeValue(ctx, result);
            /* A failed optional resize must leave the old table usable. */
            result = JS_Call(ctx, insert, JS_UNDEFINED, 0, NULL);
            assert(!JS_IsException(result) && !JS_HasException(ctx));
            assert(JS_ToBool(ctx, result) == 1);
            JS_FreeValue(ctx, result);
            JS_FreeValue(ctx, insert);
            test_pool_padding_free(rt, padding);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            assert(!allocator.live);
        }
    }
}

static void test_eval_compile_oom(void)
{
    static const char * const sources[] = {
        "function outer(a) { let x = 1;"
        "return function inner() { eval('x'); }; } outer;",
        "(function named(a = eval('1')) { return () => eval('arguments'); });",
        "class Base {} class Derived extends Base {"
        "constructor(a = 1) { eval(''); super(); } } Derived;",
        "function tag(strings) { return strings; } tag`x`; tag`y`; tag;",
    };
    size_t i;

    for (i = 0; i < countof(sources); i++) {
        size_t step, calls = 0;

        for (step = 0; step <= calls; step++) {
            OOMTestAllocator allocator;
            JSRuntime *rt = oom_test_runtime(&allocator);
            JSContext *ctx = JS_NewContext(rt);
            JSValue compiled, result;
            void *padding;

            assert(ctx);
            padding = test_pool_padding(rt);
            oom_test_begin(&allocator, step);
            compiled = JS_Eval(ctx, sources[i], strlen(sources[i]), "eval-oom.js",
                               JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
            oom_test_end(&allocator);
            if (!step) {
                calls = allocator.calls;
                assert(calls && calls < 256);
            } else {
                assert(allocator.failures == 1);
            }
            if (JS_IsException(compiled)) {
                assert(step);
                oom_test_check_exception(ctx);
            } else {
                /* Allocation failure must not publish incomplete bytecode
                   or leave an exception pending behind a successful result. */
                assert(!JS_HasException(ctx));
                result = JS_EvalFunction(ctx, compiled);
                compiled = JS_UNDEFINED;
                assert(!JS_IsException(result) && JS_IsFunction(ctx, result));
                JS_FreeValue(ctx, result);
            }
            JS_FreeValue(ctx, compiled);
            test_pool_padding_free(rt, padding);
            JS_FreeContext(ctx);
            JS_RunGC(rt);
            JS_FreeRuntime(rt);
            assert(!allocator.live);
        }
    }
}

static int retry_native_init(JSContext *ctx, JSModuleDef *m)
{
    (void)ctx;
    (void)m;
    return 0;
}

static JSModuleDef *retry_cycle_loader(JSContext *ctx, const char *name,
                                       void *opaque)
{
    const char *source = "import 'entry'; export let cycle = 1;";
    JSValue compiled;
    JSModuleDef *m;

    (void)opaque;
    assert(!strcmp(name, "cycle"));
    compiled = JS_Eval(ctx, source, strlen(source), name,
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(compiled))
        return NULL;
    m = JS_VALUE_GET_PTR(compiled);
    JS_FreeValue(ctx, compiled);
    return m;
}

static void test_module_preparation_retry(void)
{
    const char *source = "import 'cycle'; import * as ns from 'native';"
                         "export function f() {} export let result = ns.export0;";
    int mode, fail_at, failures = 0, successes = 0;

    /* Sweep closure, export and linking allocations; the cycle exercises
       rollback of nodes already visited before a sibling fails. */
    for (mode = 0; mode < 2; mode++) {
        for (fail_at = 1; fail_at <= 32; fail_at++) {
            OOMTestAllocator allocator;
            JSRuntime *rt = oom_test_runtime(&allocator);
            JSContext *ctx = JS_NewContext(rt);
            JSModuleDef *m = JS_NewCModule(ctx, "native", retry_native_init);
            JSValue compiled, result;
            char name[32];
            int i;

            assert(ctx && m);
            for (i = 0; i < (mode ? 4000 : 1); i++) {
                snprintf(name, sizeof(name), "export%d", i);
                assert(JS_AddModuleExport(ctx, m, name) == 0);
            }
            JS_SetModuleLoaderFunc(rt, NULL, retry_cycle_loader, NULL);
            compiled = JS_Eval(ctx, source, strlen(source), "entry",
                               JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
            assert(!JS_IsException(compiled));
            oom_test_begin(&allocator, fail_at);
            result = JS_EvalFunction(ctx, JS_DupValue(ctx, compiled));
            oom_test_end(&allocator);
            if (JS_IsException(result)) {
                failures++;
                JS_FreeValue(ctx, JS_GetException(ctx));
            }
            JS_FreeValue(ctx, result);
            result = JS_EvalFunction(ctx, JS_DupValue(ctx, compiled));
            /* Evaluation errors may stay cached as rejected promises, but
               preparation and capability creation must remain retryable. */
            assert(!JS_IsException(result));
            assert(JS_PromiseState(ctx, result) != JS_PROMISE_PENDING);
            if (JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED)
                successes++;
            JS_FreeValue(ctx, result);
            JS_FreeValue(ctx, compiled);
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            assert(allocator.live == 0);
        }
    }
    assert(failures > 0 && successes > 0);
}

static JSModuleDef *async_ancestor_oom_loader(JSContext *ctx,
                                              const char *name, void *opaque)
{
    static const char source[] = "export const value = await Promise.resolve(1);";
    JSValue compiled;
    JSModuleDef *module;

    (void)opaque;
    assert(!strcmp(name, "child"));
    compiled = JS_Eval(ctx, source, sizeof(source) - 1, name,
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(compiled))
        return NULL;
    module = JS_VALUE_GET_PTR(compiled);
    JS_FreeValue(ctx, compiled);
    return module;
}

static void test_async_module_ancestor_oom(void)
{
    static const char source[] =
        "import { value } from 'child'; export const result = value;";
    size_t step, calls = 0;
    int failures = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue compiled, promise;
        void *padding;
        int jobs = 0;

        assert(ctx);
        JS_SetModuleLoaderFunc(rt, NULL, async_ancestor_oom_loader, NULL);
        compiled = JS_Eval(ctx, source, sizeof(source) - 1, "parent",
                           JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        assert(!JS_IsException(compiled));
        promise = JS_EvalFunction(ctx, compiled);
        assert(!JS_IsException(promise));
        /* Fault the fulfilled child's ancestor notification, not its await. */
        assert(JS_IsJobPending(rt));
        assert(JS_ExecutePendingJob(rt, NULL) == 1);
        assert(JS_PromiseState(ctx, promise) == JS_PROMISE_PENDING);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        while (JS_IsJobPending(rt)) {
            int status = JS_ExecutePendingJob(rt, NULL);

            assert(++jobs < 16);
            if (status < 0)
                JS_FreeValue(ctx, JS_GetException(ctx));
        }
        oom_test_end(&allocator);
        if (step == 0) {
            calls = allocator.calls;
            assert(calls && calls < 64);
        } else if (allocator.failures) {
            failures++;
            assert(JS_PromiseState(ctx, promise) != JS_PROMISE_PENDING);
        }
        JS_FreeValue(ctx, promise);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
    assert(failures > 0);
}

static void test_fast_array_growth_oom(void)
{
    int n, failures = 0;

    for (n = 1; n <= 12; n++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue array = JS_NewArray(ctx), length;
        int32_t observed;
        int ret = 0, index;

        assert(!JS_IsException(array));
        oom_test_begin(&allocator, n);
        for (index = 0; index < 100; index++) {
            ret = JS_SetPropertyUint32(ctx, array, index, JS_NewInt32(ctx, index));
            if (ret < 0)
                break;
        }
        oom_test_end(&allocator);
        length = JS_GetPropertyStr(ctx, array, "length");
        assert(JS_ToInt32(ctx, &observed, length) == 0);
        if (allocator.failures) {
            failures++;
            assert(ret < 0 && JS_HasException(ctx));
            assert(observed == index);
            JS_FreeValue(ctx, JS_GetException(ctx));
        } else {
            assert(ret > 0 && observed == 100);
        }
        JS_FreeValue(ctx, length);
        JS_FreeValue(ctx, array);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
    assert(failures > 0);
}

static void test_array_length_atom_failure(void)
{
    int n, failures = 0;

    for (n = 1; n <= 12; n++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        const char *source = "globalThis.a=[]; a.length=2147483651;"
                             "Object.defineProperty(a,'0',{value:5,configurable:true});";
        JSValue value = JS_Eval(ctx, source, strlen(source), "length-oom", 0);
        JSValue global, array;
        void *padding;
        int ret;

        assert(!JS_IsException(value));
        JS_FreeValue(ctx, value);
        global = JS_GetGlobalObject(ctx);
        array = JS_GetPropertyStr(ctx, global, "a");
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, n);
        ret = JS_SetPropertyStr(ctx, array, "length", JS_NewUint32(ctx, 2147483650U));
        oom_test_end(&allocator);
        if (allocator.failures) {
            failures++;
            assert(ret < 0 && JS_HasException(ctx));
            JS_FreeValue(ctx, JS_GetException(ctx));
        } else {
            assert(ret > 0 && !JS_HasException(ctx));
        }
        JS_FreeValue(ctx, array);
        JS_FreeValue(ctx, global);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
    assert(failures > 0);
}

static int resolution_retry_calls;

static JSModuleDef *resolution_retry_loader(JSContext *ctx, const char *name,
                                            void *opaque)
{
    (void)opaque;
    if (!strcmp(name, "cycle")) {
        const char *source = "import 'entry'; export const cycle = 1;";
        JSValue value = JS_Eval(ctx, source, strlen(source), name,
                                JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        JSModuleDef *m;
        if (JS_IsException(value))
            return NULL;
        m = JS_VALUE_GET_PTR(value);
        JS_FreeValue(ctx, value);
        return m;
    }
    if (++resolution_retry_calls < 3) {
        JS_ThrowInternalError(ctx, "temporary loader failure");
        return NULL;
    }
    return JS_NewCModule(ctx, name, retry_native_init);
}

static JSValue nested_resolved_module;

static JSModuleDef *resolution_nested_loader(JSContext *ctx, const char *name,
                                             void *opaque)
{
    JSValue compiled, result;
    JSModuleDef *m;
    const char *source = "export * from 'c';";

    (void)opaque;
    if (!strcmp(name, "c"))
        return JS_NewCModule(ctx, name, retry_native_init);
    if (!strcmp(name, "b")) {
        JS_ThrowInternalError(ctx, "missing b");
        return NULL;
    }
    assert(!strcmp(name, "a"));
    compiled = JS_Eval(ctx, source, strlen(source), name,
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(compiled))
        return NULL;
    m = JS_VALUE_GET_PTR(compiled);
    nested_resolved_module = JS_DupValue(ctx, compiled);
    result = JS_EvalFunction(ctx, compiled);
    if (JS_IsException(result))
        return NULL;
    JS_FreeValue(ctx, result);
    return m;
}

static void test_nested_module_resolution_failure(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    const char *source = "import 'a'; import 'b';";
    JSValue value;

    nested_resolved_module = JS_UNDEFINED;
    JS_SetModuleLoaderFunc(rt, NULL, resolution_nested_loader, NULL);
    value = JS_Eval(ctx, source, strlen(source), "root",
                    JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(JS_IsException(value));
    JS_FreeValue(ctx, JS_GetException(ctx));
    value = JS_GetModuleNamespace(ctx, JS_VALUE_GET_PTR(nested_resolved_module));
    assert(!JS_IsException(value));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, nested_resolved_module);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_module_resolution_retry(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    const char *source = "import 'cycle'; import 'dep'; export const x = 1;";
    JSValue value;
    uint8_t *data, *copy;
    size_t length;

    assert(JS_NewCModule(ctx, "cycle", retry_native_init));
    assert(JS_NewCModule(ctx, "dep", retry_native_init));
    value = JS_Eval(ctx, source, strlen(source), "entry",
                    JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(value));
    data = JS_WriteObject(ctx, &length, value, JS_WRITE_OBJ_BYTECODE);
    assert(data);
    copy = malloc(length);
    assert(copy);
    memcpy(copy, data, length);
    js_free(ctx, data);
    JS_FreeValue(ctx, value);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    rt = JS_NewRuntime();
    ctx = JS_NewContext(rt);
    JS_SetModuleLoaderFunc(rt, NULL, resolution_retry_loader, NULL);
    value = JS_ReadObject(ctx, copy, length, JS_READ_OBJ_BYTECODE);
    free(copy);
    assert(!JS_IsException(value));
    resolution_retry_calls = 0;
    assert(JS_ResolveModule(ctx, value) < 0);
    JS_FreeValue(ctx, JS_GetException(ctx));
    assert(JS_ResolveModule(ctx, value) < 0);
    JS_FreeValue(ctx, JS_GetException(ctx));
    assert(JS_ResolveModule(ctx, value) == 0);
    assert(resolution_retry_calls == 3);
    value = JS_EvalFunction(ctx, value);
    assert(!JS_IsException(value));
    assert(JS_PromiseState(ctx, value) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, value);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_module_link_without_evaluation(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    const char *source = "globalThis.linked_body = 1; export const value = 2;";
    JSValue compiled, global, value;
    int32_t linked_body;

    compiled = JS_Eval(ctx, source, strlen(source), "link-only",
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    assert(JS_LinkModule(ctx, compiled) == 0);
    global = JS_GetGlobalObject(ctx);
    value = JS_GetPropertyStr(ctx, global, "linked_body");
    assert(JS_IsUndefined(value));
    JS_FreeValue(ctx, value);
    value = JS_EvalFunction(ctx, compiled);
    assert(!JS_IsException(value));
    assert(JS_PromiseState(ctx, value) == JS_PROMISE_FULFILLED);
    JS_FreeValue(ctx, value);
    value = JS_GetPropertyStr(ctx, global, "linked_body");
    assert(JS_ToInt32(ctx, &linked_body, value) == 0 && linked_body == 1);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(int argc, char **argv)
{
    static const struct {
        const char *name;
        void (*run)(void);
    } tests[] = {
        { "module-link-without-evaluation", test_module_link_without_evaluation },
        { "nested-module-resolution-failure", test_nested_module_resolution_failure },
        { "module-resolution-retry", test_module_resolution_retry },
        { "fast-array-growth-oom", test_fast_array_growth_oom },
        { "array-length-atom-failure", test_array_length_atom_failure },
        { "module-preparation-retry", test_module_preparation_retry },
        { "async-module-ancestor-oom", test_async_module_ancestor_oom },
        { "eval-compile-oom", test_eval_compile_oom },
        { "module-namespace-oom", test_module_namespace_oom },
        { "collection-growth-oom", test_collection_growth_oom },
        { "weakref-checkpoints", test_weakref_checkpoints },
        { "weakref-kept-oom", test_weakref_kept_oom },
        { "bound-function-oom", test_bound_function_oom },
        { "shared-write-retry", test_shared_write_retry },
        { "promise-constructor-bit", test_promise_constructor_bit },
        { "async-iterator-disposal-raw", test_async_iterator_disposal_raw_context },
        { "async-realm-job-roots", test_async_realm_job_roots },
        { "array-from-async-raw-context", test_array_from_async_raw_context },
        { "module-normalize-attributes", test_module_normalize_attributes },
        { "module-normalize-rejection", test_module_normalize_rejection },
        { "module-normalize-setters", test_module_normalize_setters },
        { "module-normalize-bytecode", test_module_normalize_bytecode },
        { "module-attribute-value-ownership", test_module_attribute_value_ownership },
        { "module-attribute-cycles", test_module_attribute_cycles },
        { "module-attribute-identity", test_module_attribute_identity },
        { "module-attribute-bytecode", test_module_attribute_bytecode },
        { "native-constructor-public-api", test_native_constructor_public_api },
        { "constructor-link-failure", test_constructor_link_failure },
        { "function-property-oom", test_function_property_oom },
        { "property-list-definitions", test_property_list_definitions },
        { "callback-flags-public-api", test_callback_flags_public_api },
        { "iterator-public-api", test_iterator_public_api },
        { "date-value-public-api", test_date_value_public_api },
        { "object-construction-public-api", test_object_construction_public_api },
        { "string-conversion-public-api", test_string_conversion_public_api },
        { "conversion-public-api", test_conversion_public_api },
        { "utf16-public-api", test_utf16_public_api },
        { "proxy-property-oom", test_proxy_property_oom },
        { "utf16-public-api-oom", test_utf16_public_api_oom },
        { "well-known-symbol-api", test_well_known_symbol_api },
        { "typed-array-public-api", test_typed_array_public_api },
        { "int64-property-api", test_int64_property_api },
        { "regexp-interrupt", test_regexp_interrupt },
        { "allocator-overflow", test_allocator_size_overflow },
        { "invalid-job-argc", test_invalid_job_argc },
        { "native-name", test_native_function_initial_name },
        { "typed-array-overlap", test_typed_array_external_overlap },
        { "typed-buffer-resized-length", test_typed_buffer_resized_length },
        { "allocator-api", test_allocator_api_entry_point },
        { "allocator-capacity", test_allocator_capacity_and_reuse },
        { "buffer-allocation", test_empty_buffer_allocation },
        { "shared-buffer-allocation", test_shared_buffer_allocation },
        { "shared-buffer-allocator-failure", test_shared_buffer_allocator_failure },
        { "shared-buffer-growth", test_default_shared_buffer_growth },
        { "shared-buffer-clone", test_shared_buffer_clone_views },
        { "shared-buffer-slice-clone", test_shared_slice_clone_species },
        { "shared-buffer-queued-clone", test_shared_buffer_queued_clone },
        { "shared-length-alignment", test_shared_buffer_length_alignment },
        { "shared-buffer-alignment-rejection", test_shared_buffer_alignment_rejection },
        { "shared-buffer-serialization-lifetime", test_shared_serialization_lifetime },
        { "shared-buffer-write-cleanup", test_shared_write_cleanup },
        { "shared-buffer-write-failure-cleanup", test_shared_write_failure_cleanup },
        { "shared-buffer-external-owner", test_external_shared_buffer_owner },
        { "shared-buffer-clone-release", test_shared_clone_release_callback },
        { "typed-array-arguments", test_typed_array_arguments },
        { "atom", test_empty_atom },
        { "array-native-interrupt", test_array_native_interrupt },
        { "buffer-callback-lifetime", test_external_buffer_callback_lifetime },
        { "buffer-reentrant-free", test_reentrant_buffer_free },
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
