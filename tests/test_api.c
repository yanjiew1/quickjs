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

int main(int argc, char **argv)
{
    static const struct {
        const char *name;
        void (*run)(void);
    } tests[] = {
        { "utf16-public-api", test_utf16_public_api },
        { "utf16-public-api-oom", test_utf16_public_api_oom },
        { "well-known-symbol-api", test_well_known_symbol_api },
        { "typed-array-public-api", test_typed_array_public_api },
        { "int64-property-api", test_int64_property_api },
        { "regexp-interrupt", test_regexp_interrupt },
        { "allocator-overflow", test_allocator_size_overflow },
        { "native-name", test_native_function_initial_name },
        { "stripped-function", test_stripped_function_to_string },
        { "typed-array-overlap", test_typed_array_external_overlap },
        { "typed-buffer-resized-length", test_typed_buffer_resized_length },
        { "allocator-api", test_allocator_api_entry_point },
        { "allocator-capacity", test_allocator_capacity_and_reuse },
        { "buffer-allocation", test_empty_buffer_allocation },
        { "shared-buffer-allocation", test_shared_buffer_allocation },
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
