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

int main(int argc, char **argv)
{
    static const struct {
        const char *name;
        void (*run)(void);
    } tests[] = {
        { "iterator-realm", test_iterator_constructor_realm },
        { "typed-array-overlap", test_typed_array_external_overlap },
        { "regexp-interrupt", test_regexp_interrupt },
        { "allocator-api", test_allocator_api_entry_point },
        { "allocator-capacity", test_allocator_capacity_and_reuse },
        { "buffer-allocation", test_empty_buffer_allocation },
        { "shared-buffer-allocation", test_shared_buffer_allocation },
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
