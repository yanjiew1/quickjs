/*
 * QuickJS default allocator accounting tests
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "oom-allocator.h"
#include "pool-padding.h"

#include "../src/quickjs/internal/allocator.h"
#include "../src/quickjs/internal/atom.h"
#include "../src/quickjs/internal/bytecode.h"
#include "../src/quickjs/internal/error.h"
#include "../src/quickjs/internal/global-environment.h"
#include "../src/quickjs/internal/bigint.h"
#include "../src/quickjs/internal/disposal.h"
#include "../src/quickjs/builtins/array-from-async.h"
#include "../src/quickjs/builtins/resource-management.h"
/* Inspect the actual pool owner without adding an engine test API. */
#include "../src/quickjs/allocator.c"
#include "../src/quickjs/internal/runtime.h"
#include "../src/quickjs/internal/object.h"
#include "../src/quickjs/internal/string-buffer.h"
#include "../src/quickjs/internal/string.h"
#include "../src/quickjs/compiler/compiler-internal.h"
#include "../src/quickjs/builtins/promise.h"

typedef struct ArraySizeAllocator {
    BOOL enabled;
    size_t requested;
} ArraySizeAllocator;

static void *array_size_malloc(JSMallocState *ms, size_t size)
{
    ArraySizeAllocator *s = ms->opaque;
    if (s->enabled && size >= 1024 * 1024) {
        s->requested = size;
        return NULL;
    }
    return def_malloc_funcs.js_malloc(ms, size);
}

static void *array_size_realloc(JSMallocState *ms, void *ptr, size_t size)
{
    ArraySizeAllocator *s = ms->opaque;
    if (s->enabled && size >= 1024 * 1024) {
        s->requested = size;
        return NULL;
    }
    return def_malloc_funcs.js_realloc(ms, ptr, size);
}

static void test_array_allocation_size(void)
{
    ArraySizeAllocator allocator = { 0 };
    JSMallocFunctions mf = def_malloc_funcs;
    JSRuntime *rt;
    JSContext *ctx;
    void *array = NULL, *saved;
    int capacity = 0;
    int count = INT_MAX / sizeof(JSValue) + 1;

    mf.js_malloc = array_size_malloc;
    mf.js_realloc = array_size_realloc;
    rt = JS_NewRuntime2(&mf, &allocator);
    ctx = JS_NewContext(rt);
    assert(rt && ctx);
    allocator.enabled = TRUE;
    assert(js_realloc_array(ctx, &array, sizeof(JSValue), &capacity, count) < 0);
    allocator.enabled = FALSE;
    assert(!array && capacity == 0);
    assert(allocator.requested >= (size_t)count * sizeof(JSValue));
    assert(allocator.requested - (size_t)count * sizeof(JSValue) < 1024);
    oom_test_check_exception(ctx);

    assert(js_resize_array(ctx, &array, 1, &capacity, 64) == 0);
    memset(array, 0x5a, 64);
    saved = array;
    count = capacity;
    allocator.enabled = TRUE;
    assert(js_realloc_array(ctx, &array, 1, &capacity, INT_MAX) < 0);
    allocator.enabled = FALSE;
    assert(array == saved && capacity == count);
    for (count = 0; count < 64; count++)
        assert(((unsigned char *)array)[count] == 0x5a);
    oom_test_check_exception(ctx);
    js_free(ctx, array);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_cstring_allocation_failure(void)
{
    int wide;
    for (wide = 0; wide < 2; wide++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue input;
        const char *str;
        size_t length = 1;
        char text[4096];

        assert(ctx);
        memset(text, 'a', sizeof(text));
        text[0] = wide ? '\xc4' : '\xc2';
        text[1] = wide ? '\x80' : '\xa9';
        input = JS_NewStringLen(ctx, text, sizeof(text));
        assert(JS_IsString(input));
        oom_test_begin(&allocator, 1);
        str = JS_ToCStringLen(ctx, &length, input);
        oom_test_end(&allocator);
        assert(!str && length == 0 && allocator.failures == 1);
        oom_test_check_exception(ctx);
        assert(js_rc(JS_VALUE_GET_PTR(input))->ref_count == 1);
        JS_FreeValue(ctx, input);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_string_buffer_growth(void)
{
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    StringBuffer buffer;
    JSValue value;
    const uint16_t *text;
    size_t length;
    int i;

    assert(ctx && string_buffer_init(ctx, &buffer, 1) == 0);
    for (i = 0; i < 1024; i++) {
        assert(string_buffer_putc(&buffer, i < 64 ? 'a' : 0x100) == 0);
        assert(buffer.size <= JS_STRING_LEN_MAX);
    }
    value = string_buffer_end(&buffer);
    text = JS_ToCStringLenUTF16(ctx, &length, value);
    assert(text && length == 1024);
    for (i = 0; i < 1024; i++)
        assert(text[i] == (i < 64 ? 'a' : 0x100));
    JS_FreeCStringUTF16(ctx, text);
    JS_FreeValue(ctx, value);

    assert(string_buffer_init(ctx, &buffer, -1) < 0);
    assert(!buffer.str && buffer.error_status && buffer.size == 0);
    oom_test_check_exception(ctx);
    string_buffer_free(&buffer);

    assert(string_buffer_init(ctx, &buffer, 2048) == 0);
    assert(string_buffer_fill(&buffer, 'a', 2048) == 0);
    oom_test_begin(&allocator, 1);
    assert(string_buffer_putc16(&buffer, 0x100) < 0);
    oom_test_end(&allocator);
    assert(allocator.failures == 1 && buffer.error_status);
    assert(!buffer.str && buffer.len == 0 && buffer.size == 0);
    oom_test_check_exception(ctx);
    string_buffer_free(&buffer);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(allocator.live == 0);
}

#ifdef JS_MALLOC_USE_ITER
typedef struct AllocationIteration {
    void *ptrs[97];
    int seen[97];
} AllocationIteration;

static void allocation_iteration_visit(void *opaque, void *ptr)
{
    AllocationIteration *test = opaque;
    int i;
    for (i = 0; i < 97; i++) {
        if (ptr == test->ptrs[i])
            test->seen[i]++;
    }
}

static void test_allocation_iteration(void)
{
    AllocationIteration test = { 0 };
    JSRuntime *rt = JS_NewRuntime();
    int i;

    assert(rt);
    for (i = 0; i < 97; i++) {
        test.ptrs[i] = js_malloc_rt(rt, i == 96 ? 4096 : 24);
        assert(test.ptrs[i]);
    }
    js_malloc_iter(&rt->malloc_ctx, allocation_iteration_visit, &test);
    for (i = 0; i < 97; i++) {
        assert(test.seen[i] == 1);
        if (!(i & 1)) {
            js_free_rt(rt, test.ptrs[i]);
            test.ptrs[i] = NULL;
        }
    }
    memset(test.seen, 0, sizeof(test.seen));
    js_malloc_iter(&rt->malloc_ctx, allocation_iteration_visit, &test);
    for (i = 0; i < 97; i++) {
        assert(test.seen[i] == (i & 1));
        js_free_rt(rt, test.ptrs[i]);
    }
    JS_FreeRuntime(rt);
}
#endif

typedef struct ReactionOwner {
    int calls;
    int finalized;
} ReactionOwner;

typedef struct PromiseAllocationFailure {
    BOOL fail_next;
    BOOL arm_failure;
    int failures;
    int unhandled;
    int handled;
    size_t live_allocations;
    void *padding;
    JSValue functions[4];
    int function_count;
    ReactionOwner owners[4];
} PromiseAllocationFailure;

static JSClassID reaction_owner_class_id;

static void *promise_failure_malloc(JSMallocState *s, size_t size)
{
    PromiseAllocationFailure *failure = s->opaque;
    void *ptr;

    if (failure->fail_next) {
        failure->fail_next = FALSE;
        failure->failures++;
        return NULL;
    }
    ptr = def_malloc_funcs.js_malloc(s, size);
    if (ptr)
        failure->live_allocations++;
    return ptr;
}

static void promise_failure_free(JSMallocState *s, void *ptr)
{
    PromiseAllocationFailure *failure = s->opaque;

    if (ptr) {
        assert(failure->live_allocations > 0);
        failure->live_allocations--;
    }
    def_malloc_funcs.js_free(s, ptr);
}

static void *promise_failure_realloc(JSMallocState *s, void *ptr, size_t size)
{
    PromiseAllocationFailure *failure = s->opaque;
    BOOL had_ptr = ptr != NULL;
    void *result;

    if (size != 0 && failure->fail_next) {
        failure->fail_next = FALSE;
        failure->failures++;
        return NULL;
    }
    result = def_malloc_funcs.js_realloc(s, ptr, size);
    if (!had_ptr && result) {
        failure->live_allocations++;
    } else if (had_ptr && size == 0) {
        assert(failure->live_allocations > 0);
        failure->live_allocations--;
    }
    return result;
}

/* Check the public refcount offset as well as the payload address. */
static void check_block_alignment(void *ptr)
{
    assert(ptr && (uintptr_t)ptr % _Alignof(max_align_t) == 0);
    assert(&js_rc(ptr)->ref_count == &__js_rc(ptr)->ref_count);
}

static void check_block_contents(uint8_t *ptr, size_t size, uint8_t value)
{
    size_t i;

    check_block_alignment(ptr);
    assert(js_rc(ptr)->ref_count == 17);
    assert(js_rc(ptr)->gc_obj_type == 3 && js_rc(ptr)->mark == 1);
    for (i = 0; i < size; i++)
        assert(ptr[i] == value);
}

static void test_payload_alignment(void)
{
    enum { BLOCK_COUNT = 4 };
    PromiseAllocationFailure failure = { 0 };
    JSMallocFunctions mf = def_malloc_funcs;
    JSRuntime *rt;
    JSMallocState *state;
    uint8_t *blocks[BLOCK_COUNT], *ptr;
    size_t size, count_before, bytes_before, live_before;
    int i;

    mf.js_malloc = promise_failure_malloc;
    mf.js_free = promise_failure_free;
    mf.js_realloc = promise_failure_realloc;
    rt = JS_NewRuntime2(&mf, &failure);
    assert(rt);
    state = &rt->malloc_ctx.malloc_state;
    count_before = state->malloc_count;
    bytes_before = state->malloc_size;
    live_before = failure.live_allocations;
    ptr = js_malloc_rt(rt, 0);
    check_block_alignment(ptr);
    ptr = js_realloc_rt(rt, ptr, 1);
    check_block_alignment(ptr);
    assert(js_realloc_rt(rt, ptr, 0) == NULL);
    ptr = js_realloc_rt(rt, NULL, 0);
    check_block_alignment(ptr);
    js_free_rt(rt, ptr);
    /* Consecutive live blocks expose arena stride errors. Every size covers
       pool class edges, the pool/host boundary, and direct host allocations. */
    for (size = 1; size <= 1025; size++) {
        for (i = 0; i < BLOCK_COUNT; i++) {
            blocks[i] = (i & 1) ? js_realloc_rt(rt, NULL, size) :
                                   js_malloc_rt(rt, size);
            check_block_alignment(blocks[i]);
            js_rc(blocks[i])->ref_count = 17;
            js_rc(blocks[i])->gc_obj_type = 3;
            js_rc(blocks[i])->mark = 1;
            memset(blocks[i], 0xa0 + i, size);
        }
        for (i = 0; i < BLOCK_COUNT; i++) {
            blocks[i] = js_realloc_rt(rt, blocks[i], size);
            check_block_contents(blocks[i], size, 0xa0 + i);
            blocks[i] = js_realloc_rt(rt, blocks[i], size + 1025);
            check_block_contents(blocks[i], size, 0xa0 + i);
            blocks[i] = js_realloc_rt(rt, blocks[i], size);
            check_block_contents(blocks[i], size, 0xa0 + i);
        }
        for (i = BLOCK_COUNT - 1; i >= 0; i--)
            js_free_rt(rt, blocks[i]);
        assert(state->malloc_count == count_before);
        assert(state->malloc_size == bytes_before);
        assert(failure.live_allocations == live_before);
    }
    /* Force host failures without relying on whether a pool has spare room. */
    ptr = js_malloc_rt(rt, 1024);
    check_block_alignment(ptr);
    js_rc(ptr)->ref_count = 17;
    js_rc(ptr)->gc_obj_type = 3;
    js_rc(ptr)->mark = 1;
    memset(ptr, 0xa5, 1024);
    count_before = state->malloc_count;
    bytes_before = state->malloc_size;
    live_before = failure.live_allocations;
    failure.fail_next = TRUE;
    assert(js_realloc_rt(rt, ptr, 4096) == NULL);
    assert(!failure.fail_next && failure.failures == 1);
    check_block_contents(ptr, 1024, 0xa5);
    failure.fail_next = TRUE;
    assert(js_malloc_rt(rt, 4096) == NULL);
    assert(!failure.fail_next && failure.failures == 2);
    assert(state->malloc_count == count_before);
    assert(state->malloc_size == bytes_before);
    assert(failure.live_allocations == live_before);
    assert(js_realloc_rt(rt, ptr, 0) == NULL);
    JS_FreeRuntime(rt);
    assert(failure.live_allocations == 0);
}

static void reaction_owner_finalizer(JSRuntime *rt, JSValue value)
{
    ReactionOwner *owner = JS_GetOpaque(value, reaction_owner_class_id);

    assert(owner);
    owner->finalized++;
}

static JSValue reaction_callback(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int magic,
                                 JSValue *data)
{
    ReactionOwner *owner = JS_GetOpaque(data[0], reaction_owner_class_id);

    assert(owner);
    owner->calls++;
    return JS_UNDEFINED;
}

/* Consume only existing spare pool blocks; the next engine allocation must
   reach the host allocator even when its size is otherwise served by a pool.
   No knowledge of the runtime.c-local JSJobEntry layout is needed. */
static void exhaust_pooled_free_blocks(JSRuntime *rt,
                                       PromiseAllocationFailure *failure)
{
    size_t size, total_size;
    int index, previous_index = -1;
    void *ptr;

    for (size = sizeof(void *);
         size <= JS_MALLOC_MAX_SMALL_SIZE - sizeof(JSMallocBlockHeader);
         size++) {
        total_size = ((size + JS_MALLOC_ALIGN - 1) & ~(JS_MALLOC_ALIGN - 1)) +
            sizeof(JSMallocBlockHeader);
        index = get_block_size_index(total_size);
        if (index == previous_index)
            continue;
        previous_index = index;
        while (!list_empty(&rt->malloc_ctx.free_arena_list[index])) {
            ptr = js_malloc_rt(rt, size);
            assert(ptr);
            memcpy(ptr, &failure->padding, sizeof(failure->padding));
            failure->padding = ptr;
        }
    }
    for (index = 0; index < JS_MALLOC_BLOCK_SIZE_COUNT; index++)
        assert(list_empty(&rt->malloc_ctx.free_arena_list[index]));
}

static void promise_failure_tracker(JSContext *ctx, JSValueConst promise,
                                    JSValueConst reason, BOOL is_handled,
                                    void *opaque)
{
    PromiseAllocationFailure *failure = opaque;
    JSRuntime *rt = JS_GetRuntime(ctx);
    int i;

    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED);
    assert(JS_VALUE_GET_INT(reason) == 42);
    /* The handled notification precedes enqueueing the reaction job. */
    assert(!JS_IsJobPending(rt));
    if (!is_handled) {
        failure->unhandled++;
        return;
    }
    failure->handled++;
    if (!failure->arm_failure)
        return;
    /* One prepared reaction owns both handlers and both capability functions. */
    for (i = 0; i < failure->function_count; i++)
        assert(js_rc(JS_VALUE_GET_PTR(failure->functions[i]))->ref_count == 2);
    exhaust_pooled_free_blocks(rt, failure);
    failure->arm_failure = FALSE;
    failure->fail_next = TRUE;
}

static void test_settled_promise_prepared_reaction(BOOL with_capability)
{
    static const JSClassDef owner_class = {
        .class_name = "PromiseReactionOwner",
        .finalizer = reaction_owner_finalizer,
    };
    PromiseAllocationFailure failure = { 0 };
    JSMallocFunctions mf = def_malloc_funcs;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue promise, resolving_funcs[2], owner, result;
    JSValueConst reason;
    JSValueConst empty[2] = { JS_UNDEFINED, JS_UNDEFINED };
    void *ptr;
    int i, ret;

    mf.js_malloc = promise_failure_malloc;
    mf.js_free = promise_failure_free;
    mf.js_realloc = promise_failure_realloc;
    rt = JS_NewRuntime2(&mf, &failure);
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    reason = JS_NewInt32(ctx, 42);
    JS_NewClassID(&reaction_owner_class_id);
    assert(JS_NewClass(rt, reaction_owner_class_id, &owner_class) == 0);
    JS_SetHostPromiseRejectionTracker(rt, promise_failure_tracker, &failure);
    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    assert(!JS_IsException(promise));
    result = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &reason);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    assert(failure.unhandled == 1 && failure.handled == 0);
    failure.function_count = with_capability ? 4 : 2;
    for (i = 0; i < 4; i++) {
        failure.functions[i] = JS_UNDEFINED;
        if (i >= failure.function_count)
            continue;
        owner = JS_NewObjectClass(ctx, reaction_owner_class_id);
        assert(!JS_IsException(owner));
        JS_SetOpaque(owner, &failure.owners[i]);
        failure.functions[i] = JS_NewCFunctionData(ctx, reaction_callback,
                                                   1, 0, 1,
                                                   (JSValueConst *)&owner);
        JS_FreeValue(ctx, owner);
        assert(!JS_IsException(failure.functions[i]));
        assert(js_rc(JS_VALUE_GET_PTR(failure.functions[i]))->ref_count == 1);
    }
    failure.arm_failure = TRUE;
    ret = perform_promise_then(ctx, promise,
                                (JSValueConst *)failure.functions,
                                (JSValueConst *)(failure.functions + 2));
    assert(ret == 0);
    assert(failure.failures == 0 && failure.fail_next);
    failure.fail_next = FALSE;
    assert(!JS_HasException(ctx));
    assert(JS_IsJobPending(rt));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED);
    result = JS_PromiseResult(ctx, promise);
    assert(JS_VALUE_GET_INT(result) == 42);
    JS_FreeValue(ctx, result);
    assert(failure.unhandled == 1 && failure.handled == 1);
    for (i = 0; i < failure.function_count; i++) {
        assert(failure.owners[i].calls == 0);
        assert(failure.owners[i].finalized == 0);
        assert(js_rc(JS_VALUE_GET_PTR(failure.functions[i]))->ref_count ==
               (i == 0 ? 1 : 2));
    }
    assert(JS_ExecutePendingJob(rt, NULL) == 1);
    assert(JS_ExecutePendingJob(rt, NULL) == 0);
    assert(failure.owners[1].calls == 1);
    if (with_capability)
        assert(failure.owners[2].calls == 1);
    for (i = 0; i < failure.function_count; i++) {
        assert(js_rc(JS_VALUE_GET_PTR(failure.functions[i]))->ref_count == 1);
        JS_FreeValue(ctx, failure.functions[i]);
        assert(failure.owners[i].finalized == 1);
    }
    while ((ptr = failure.padding) != NULL) {
        memcpy(&failure.padding, ptr, sizeof(failure.padding));
        js_free_rt(rt, ptr);
    }
    /* Attaching another reaction must not repeat the handled notification. */
    assert(perform_promise_then(ctx, promise, empty, empty) == 0);
    assert(failure.unhandled == 1 && failure.handled == 1);
    assert(JS_IsJobPending(rt));
    assert(JS_ExecutePendingJob(rt, NULL) == 1);
    assert(JS_ExecutePendingJob(rt, NULL) == 0);
    assert(!JS_HasException(ctx));
    JS_FreeValue(ctx, promise);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(failure.live_allocations == 0);
}

static void test_malloc_limit_overflow(void)
{
    JSMallocState state = { 0 };
    size_t used = SIZE_MAX - 128;

    state.malloc_size = used;
    state.malloc_limit = SIZE_MAX;
    assert(def_malloc_funcs.js_malloc(&state, 256) == NULL);
    assert(state.malloc_size == used && state.malloc_count == 0);
    state.malloc_limit = 0;
    assert(def_malloc_funcs.js_malloc(&state, 1) == NULL);
    assert(state.malloc_size == used && state.malloc_count == 0);
}

static void test_realloc_limit_overflow(void)
{
    JSMallocState state = { 0 };
    uint8_t *ptr;
    size_t allocated, i;

    state.malloc_limit = SIZE_MAX;
    ptr = def_malloc_funcs.js_malloc(&state, 32);
    assert(ptr);
    memset(ptr, 0xa5, 32);
    allocated = state.malloc_size;
    state.malloc_size = SIZE_MAX - 128;
    assert(def_malloc_funcs.js_realloc(&state, ptr, 512) == NULL);
    assert(state.malloc_size == SIZE_MAX - 128 && state.malloc_count == 1);
    for (i = 0; i < 32; i++)
        assert(ptr[i] == 0xa5);
    state.malloc_size = allocated;
    state.malloc_limit = 0;
    assert(def_malloc_funcs.js_realloc(&state, ptr, 16) == NULL);
    assert(state.malloc_size == allocated && state.malloc_count == 1);
    for (i = 0; i < 32; i++)
        assert(ptr[i] == 0xa5);
    assert(def_malloc_funcs.js_realloc(&state, ptr, 0) == NULL);
    assert(state.malloc_size == 0 && state.malloc_count == 0);
}

#ifndef FORCE_GC_AT_MALLOC
static int gc_estimate_finalized;

static void gc_estimate_finalizer(JSRuntime *rt, JSValue value)
{
    gc_estimate_finalized++;
}
#endif

static void test_gc_upcoming_properties(void)
{
#ifndef FORCE_GC_AT_MALLOC
    static JSClassID class_id;
    static const JSClassDef class_def = {
        "GCEstimate", .finalizer = gc_estimate_finalizer,
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue template, garbage, object;
    JSProperty props[16];
    JSShape *shape;
    int i;

    assert(rt);
    JS_SetGCThreshold(rt, SIZE_MAX);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_NewClassID(&class_id);
    assert(JS_NewClass(rt, class_id, &class_def) == 0);
    template = JS_NewObject(ctx);
    assert(!JS_IsException(template));
    for (i = 0; i < countof(props); i++) {
        props[i].u.value = JS_NewInt32(ctx, i);
        assert(JS_DefinePropertyValueUint32(ctx, template, i,
            JS_NewInt32(ctx, i), JS_PROP_C_W_E) >= 0);
    }
    shape = js_dup_shape(JS_VALUE_GET_OBJ(template)->shape);
    assert(shape->prop_count == countof(props));
    assert(shape->prop_size >= countof(props));
    garbage = JS_NewObjectClass(ctx, class_id);
    assert(!JS_IsException(garbage));
    assert(JS_SetPropertyStr(ctx, garbage, "self", JS_DupValue(ctx, garbage)) >= 0);
    JS_FreeValue(ctx, garbage);
    assert(gc_estimate_finalized == 0);
    /* The header alone fits the budget; its imminent properties do not. */
    JS_SetGCThreshold(rt, rt->malloc_ctx.malloc_state.malloc_size + sizeof(JSObject));
    object = JS_NewObjectFromShape(ctx, shape, JS_CLASS_OBJECT, props);
    assert(!JS_IsException(object));
    assert(gc_estimate_finalized == 1);
    JS_FreeValue(ctx, object);
    JS_FreeValue(ctx, template);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
#endif
}

static void test_gc_accounting_overflow(void)
{
    JSRuntime *rt = JS_NewRuntime();
    size_t allocated;

    assert(rt);
    allocated = rt->malloc_ctx.malloc_state.malloc_size;
#ifndef FORCE_GC_AT_MALLOC
    rt->malloc_gc_threshold = SIZE_MAX;
    js_trigger_gc(rt, 1);
    assert(rt->malloc_gc_threshold == SIZE_MAX);
    rt->malloc_gc_threshold = allocated;
    js_trigger_gc(rt, 0);
    assert(rt->malloc_gc_threshold == allocated);
#endif
    rt->malloc_ctx.malloc_state.malloc_size = SIZE_MAX - 128;
    rt->malloc_gc_threshold = SIZE_MAX - 64;
    js_trigger_gc(rt, 256);
    assert(rt->malloc_gc_threshold == SIZE_MAX);
    rt->malloc_ctx.malloc_state.malloc_size = allocated;
    rt->malloc_gc_threshold = 0;
    js_trigger_gc(rt, 1);
    assert(rt->malloc_gc_threshold == allocated + (allocated >> 1));
    JS_FreeRuntime(rt);
}

typedef union LabelOOMHeader {
    size_t size;
    max_align_t alignment;
} LabelOOMHeader;

typedef struct LabelOOMState {
    JSRuntime *rt;
    JSAtom atom;
    void *last_ptr;
    size_t engine_header_size, old_size, new_size;
    int label_count, armed, failures;
} LabelOOMState;

static void *label_oom_malloc(JSMallocState *s, size_t size)
{
    LabelOOMState *state = s->opaque;
    LabelOOMHeader *header;

    assert(size <= SIZE_MAX - sizeof(*header));
    header = malloc(sizeof(*header) + size);
    if (!header)
        return NULL;
    header->size = size;
    s->malloc_count++;
    s->malloc_size += size;
    state->last_ptr = header + 1;
    return state->last_ptr;
}

static void label_oom_free(JSMallocState *s, void *ptr)
{
    LabelOOMHeader *header;

    if (!ptr)
        return;
    header = (LabelOOMHeader *)ptr - 1;
    s->malloc_count--;
    s->malloc_size -= header->size;
    free(header);
}

static void *label_oom_realloc(JSMallocState *s, void *ptr, size_t size)
{
    LabelOOMState *state = s->opaque;
    LabelOOMHeader *header;
    size_t old_size;

    if (!ptr)
        return label_oom_malloc(s, size);
    if (!size) {
        label_oom_free(s, ptr);
        return NULL;
    }
    header = (LabelOOMHeader *)ptr - 1;
    old_size = header->size;
    if (state->armed && old_size == state->old_size &&
        size == state->new_size &&
        js_rc(state->rt->atom_array[state->atom])->ref_count == 2) {
        LabelSlot *slots = (LabelSlot *)((uint8_t *)ptr + state->engine_header_size);
        int i;

        /* The prefix has one resolved conditional label per statement.
           Verify this is its label table before rejecting the real realloc. */
        for (i = 0; i < state->label_count; i++) {
            if (slots[i].ref_count != 1 || slots[i].pos <= 0 ||
                slots[i].pos2 != -1 || slots[i].addr != -1 ||
                slots[i].first_reloc != NULL)
                break;
        }
        if (i == state->label_count) {
            state->armed = 0;
            state->failures++;
            return NULL;
        }
    }
    assert(size <= SIZE_MAX - sizeof(*header));
    header = realloc(header, sizeof(*header) + size);
    if (!header)
        return NULL;
    header->size = size;
    s->malloc_size += size;
    s->malloc_size -= old_size;
    state->last_ptr = header + 1;
    return state->last_ptr;
}

static size_t label_oom_usable_size(const void *ptr)
{
    return ((const LabelOOMHeader *)ptr - 1)->size;
}

static void check_lvalue_label_oom(const char *name, const char *assignment)
{
    static const JSMallocFunctions mf = {
        label_oom_malloc, label_oom_free, label_oom_realloc,
        label_oom_usable_size,
    };
    static const char prefix[] = "if (0);";
    static const char scope[] = "with ({}) {";
    LabelOOMState state = { 0 };
    JSFunctionDef fd = { 0 };
    JSRuntime *rt = JS_NewRuntime2(&mf, &state);
    JSContext *ctx;
    JSValue result, exception;
    void *probe;
    char *source;
    size_t source_size, pos;
    int i;

    assert(rt);
    state.rt = rt;
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetGCThreshold(rt, SIZE_MAX);

    /* Measure the existing allocator's large-block header without copying
       its private layout. The test allocator reports exact usable sizes. */
    probe = js_malloc_rt(rt, 1024);
    assert(probe);
    state.engine_header_size = (uint8_t *)probe - (uint8_t *)state.last_ptr;
    js_free_rt(rt, probe);

    /* Calibrate the real LabelSlot capacity growth for this build, then
       fill a large table so the next label requires a host realloc. */
    fd.ctx = ctx;
    do {
        assert(new_label_fd(&fd) >= 0);
    } while ((size_t)fd.label_size * sizeof(LabelSlot) <= 1024 ||
             fd.label_count < fd.label_size);
    state.label_count = fd.label_count;
    state.old_size = state.engine_header_size + fd.label_size * sizeof(LabelSlot);
    state.new_size = state.engine_header_size +
        max_int(fd.label_count + 1, fd.label_size * 3 / 2) * sizeof(LabelSlot);
    js_free(ctx, fd.label_slots);

    source_size = state.label_count * (sizeof(prefix) - 1) +
        sizeof(scope) - 1 + strlen(name) + strlen(assignment) + 2;
    source = malloc(source_size);
    assert(source);
    pos = 0;
    for (i = 0; i < state.label_count; i++) {
        memcpy(source + pos, prefix, sizeof(prefix) - 1);
        pos += sizeof(prefix) - 1;
    }
    strcpy(source + pos, scope);
    strcat(source + pos, name);
    strcat(source + pos, assignment);
    strcat(source + pos, "}");

    /* Hold one reference so the precise leaked bytecode reference can be
       checked independently of error metadata and other runtime atoms. */
    state.atom = JS_NewAtom(ctx, name);
    assert(state.atom != JS_ATOM_NULL);
    assert(js_rc(rt->atom_array[state.atom])->ref_count == 1);
    state.armed = 1;
    result = JS_Eval(ctx, source, strlen(source), "lvalue-label-oom",
                     JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(state.failures == 1 && !state.armed);
    assert(JS_IsException(result));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, result);
    assert(js_rc(rt->atom_array[state.atom])->ref_count == 1);
    JS_FreeAtom(ctx, state.atom);
    free(source);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

static void test_lvalue_label_oom(void)
{
    check_lvalue_label_oom("label_oom_nonkeep_fresh", " = 0;");
    check_lvalue_label_oom("label_oom_keep_fresh", " += 0;");
}

typedef struct AllocationFailure {
    size_t live_allocations;
    unsigned allocations_until_failure;
    unsigned failures;
    JSRuntime *rt;
    BOOL context_registered_at_failure;
    BOOL fail_table_realloc;
} AllocationFailure;

static BOOL allocation_should_fail(AllocationFailure *failure)
{
    if (!failure->allocations_until_failure ||
        --failure->allocations_until_failure != 0)
        return FALSE;
    failure->failures++;
    if (failure->rt)
        failure->context_registered_at_failure =
            !list_empty(&failure->rt->gc_obj_list);
    return TRUE;
}

static void *allocation_failure_malloc(JSMallocState *s, size_t size)
{
    AllocationFailure *failure = s->opaque;
    void *ptr;

    if (allocation_should_fail(failure))
        return NULL;
    ptr = def_malloc_funcs.js_malloc(s, size);
    if (ptr)
        failure->live_allocations++;
    return ptr;
}

static void allocation_failure_free(JSMallocState *s, void *ptr)
{
    AllocationFailure *failure = s->opaque;

    if (ptr) {
        assert(failure->live_allocations > 0);
        failure->live_allocations--;
    }
    def_malloc_funcs.js_free(s, ptr);
}

static void *allocation_failure_realloc(JSMallocState *s, void *ptr,
                                       size_t size)
{
    AllocationFailure *failure = s->opaque;
    BOOL had_ptr = ptr != NULL;
    void *result;

    if (size != 0 && ptr && failure->fail_table_realloc) {
        failure->fail_table_realloc = FALSE;
        failure->failures++;
        return NULL;
    }
    if (size != 0 && allocation_should_fail(failure))
        return NULL;
    result = def_malloc_funcs.js_realloc(s, ptr, size);
    if (!had_ptr && result) {
        failure->live_allocations++;
    } else if (had_ptr && size == 0) {
        assert(failure->live_allocations > 0);
        failure->live_allocations--;
    }
    return result;
}

static JSRuntime *new_allocation_failure_runtime(AllocationFailure *failure)
{
    JSMallocFunctions mf = def_malloc_funcs;
    JSRuntime *rt;

    mf.js_malloc = allocation_failure_malloc;
    mf.js_free = allocation_failure_free;
    mf.js_realloc = allocation_failure_realloc;
    rt = JS_NewRuntime2(&mf, failure);
    assert(rt);
    failure->rt = rt;
    return rt;
}

static void test_raw_context_allocation_failure(void)
{
    AllocationFailure failure = { 0 };
    JSRuntime *rt = new_allocation_failure_runtime(&failure);
    JSContext *ctx;

    assert(list_empty(&rt->gc_obj_list));
    failure.allocations_until_failure = 2;
    ctx = JS_NewContextRaw(rt);
    assert(!ctx);
    assert(failure.failures == 1);
    assert(failure.context_registered_at_failure);
    assert(list_empty(&rt->gc_obj_list));
    ctx = JS_NewContextRaw(rt);
    assert(ctx);
    JS_RunGC(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(failure.live_allocations == 0);
}

static void test_proxy_registration_allocation_failure(void)
{
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSContext *other;
        void *padding;
        int status;

        assert(ctx);
        other = JS_NewContextRaw(rt);
        assert(other && JS_AddIntrinsicBaseObjects(other) == 0);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        status = JS_AddIntrinsicProxy(other);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(status == 0 && calls > 0 && calls < 64);
        } else {
            assert(allocator.failures == 1);
        }
        assert((status < 0) == !!JS_HasException(ctx));
        if (status < 0)
            JS_FreeValue(ctx, JS_GetException(ctx));
        test_pool_padding_free(rt, padding);
        JS_FreeContext(other);
        JS_FreeContext(ctx);
        JS_RunGC(rt);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

static void check_backtrace_allocation_failure(unsigned fail_at)
{
    AllocationFailure failure = { 0 };
    PromiseAllocationFailure padding = { 0 };
    JSRuntime *rt = new_allocation_failure_runtime(&failure);
    JSContext *ctx = JS_NewContext(rt);
    JSValue error, exception;
    char filename[2048];
    void *ptr;

    assert(ctx);
    error = JS_NewError(ctx);
    assert(!JS_IsException(error));
    memset(filename, 'x', sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
    exhaust_pooled_free_blocks(rt, &padding);
    failure.allocations_until_failure = fail_at;
    build_backtrace(ctx, error, filename, -1, 1, 0);
    assert(failure.failures == 1);
    assert(failure.allocations_until_failure == 0);
    assert(JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, error);
    while (padding.padding) {
        ptr = padding.padding;
        memcpy(&padding.padding, ptr, sizeof(padding.padding));
        js_free_rt(rt, ptr);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(failure.live_allocations == 0);
}

static void test_backtrace_allocation_failure(void)
{
    /* The large filename follows the DynBuf allocation. Exhausting spare
       pool blocks exposes the subsequent metadata allocation as well. */
    check_backtrace_allocation_failure(2);
    check_backtrace_allocation_failure(3);
}

/* The VM and parser borrow an exception owned only by the pending slot.
   Pre-existing metadata removes property allocations from the schedule:
   DynBuf, filename string, DynBuf growth, then the final stack string. */
static void check_pending_backtrace_allocation_failure(unsigned fail_at,
                                                       BOOL retain_error)
{
    AllocationFailure failure = { 0 };
    JSRuntime *rt = new_allocation_failure_runtime(&failure);
    JSContext *ctx = JS_NewContext(rt);
    JSValue error, retained, exception, stack, message;
    const char *text;
    uintptr_t original;
    char filename[2048];
    const int flags = JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE;

    assert(ctx);
    assert(rt->current_stack_frame == NULL);
    error = JS_NewError(ctx);
    assert(!JS_IsException(error));
    assert(JS_DefinePropertyValueStr(ctx, error, "fileName", JS_UNDEFINED,
                                    flags) >= 0);
    assert(JS_DefinePropertyValueStr(ctx, error, "lineNumber", JS_UNDEFINED,
                                    flags) >= 0);
    assert(JS_DefinePropertyValueStr(ctx, error, "columnNumber", JS_UNDEFINED,
                                    flags) >= 0);
    assert(JS_DefinePropertyValueStr(ctx, error, "stack", JS_UNDEFINED,
                                    flags) >= 0);
    retained = retain_error ? JS_DupValue(ctx, error) : JS_UNDEFINED;
    original = (uintptr_t)JS_VALUE_GET_PTR(error);
    assert(JS_IsException(JS_Throw(ctx, error))); /* transfers ownership */
    error = JS_UNDEFINED;
    JS_SetUncatchableException(ctx, TRUE);
    memset(filename, 'x', sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
    failure.allocations_until_failure = fail_at;
    build_backtrace(ctx, rt->current_exception, filename, -1, 1, 0);
    assert(failure.failures == (fail_at != 0));
    assert(failure.allocations_until_failure == 0);
    assert(JS_HasException(ctx));
    assert(rt->current_exception_is_uncatchable == (fail_at == 0));
    exception = JS_GetException(ctx);
    assert(JS_IsError(ctx, exception));
    if (fail_at) {
        /* Backtrace OOM keeps its existing replacement/propagation policy. */
        assert((uintptr_t)JS_VALUE_GET_PTR(exception) != original);
        message = JS_GetPropertyStr(ctx, exception, "message");
        assert(!JS_IsException(message));
        text = JS_ToCString(ctx, message);
        assert(text && !strcmp(text, "out of memory"));
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, message);
        if (retain_error) {
            /* A failed string must not become a JS_EXCEPTION stack value. */
            stack = JS_GetPropertyStr(ctx, retained, "stack");
            assert(JS_IsUndefined(stack));
            JS_FreeValue(ctx, stack);
        }
    } else {
        assert((uintptr_t)JS_VALUE_GET_PTR(exception) == original);
        stack = JS_GetPropertyStr(ctx, exception, "stack");
        assert(JS_IsString(stack));
        text = JS_ToCString(ctx, stack);
        assert(text && strlen(text) == sizeof(filename) - 1 + 8);
        assert(!memcmp(text, "    at ", 7));
        assert(!memcmp(text + 7, filename, sizeof(filename) - 1));
        assert(text[sizeof(filename) - 1 + 7] == '\n');
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, stack);
    }
    assert(!JS_HasException(ctx));
    JS_FreeValue(ctx, retained);
    JS_FreeValue(ctx, exception);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(failure.live_allocations == 0);
}

static void test_pending_backtrace_allocation_failure(void)
{
    check_pending_backtrace_allocation_failure(0, FALSE);
    check_pending_backtrace_allocation_failure(2, FALSE);
    check_pending_backtrace_allocation_failure(4, FALSE);
    check_pending_backtrace_allocation_failure(4, TRUE);
}

static JSValue autoinit_oom_callback(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    return JS_NewInt32(ctx, 42);
}

/* An embedding allocation failure must preserve the lazy recipe for retry.
   Pool exhaustion forces the actual initializer (or global binding reference)
   to reach the host allocator under both pooled and direct allocation builds. */
static void test_autoinit_allocation_failure(int kind, BOOL retry)
{
    static const JSCFunctionListEntry function_entry[] = {
        JS_CFUNC_DEF("autoinitValue", 0, autoinit_oom_callback),
    };
    static const JSCFunctionListEntry string_entry[] = {
        JS_PROP_STRING_DEF("autoinitValue", "autoinit-global-owned-value",
                           JS_PROP_C_W_E),
    };
    static const char source[] = "(function autoinitPrototype() {})";
    PromiseAllocationFailure failure = { 0 };
    JSMallocFunctions mf = def_malloc_funcs;
    JSRuntime *rt;
    JSContext *ctx;
    JSObject *object;
    JSProperty *pr;
    JSShapeProperty *prs;
    JSValue target, result, again, exception, retained = JS_UNDEFINED;
    JSAtom atom;
    uintptr_t realm_and_id;
    void *opaque, *ptr;
    int flags, realm_refs, string_refs = 0, i;

    mf.js_malloc = promise_failure_malloc;
    mf.js_free = promise_failure_free;
    mf.js_realloc = promise_failure_realloc;
    rt = JS_NewRuntime2(&mf, &failure);
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetGCThreshold(rt, SIZE_MAX);
    if (kind == 1) {
        target = JS_Eval(ctx, source, sizeof(source) - 1,
                         "autoinit-oom", JS_EVAL_TYPE_GLOBAL);
    } else {
        target = kind == 2 ? JS_GetGlobalObject(ctx) : JS_NewObject(ctx);
    }
    assert(!JS_IsException(target));
    if (kind != 1)
        assert(JS_SetPropertyFunctionList(ctx, target,
                   kind == 2 ? string_entry : function_entry, 1) == 0);
    atom = JS_NewAtom(ctx, kind == 1 ? "prototype" : "autoinitValue");
    assert(atom != JS_ATOM_NULL);
    /* Separate the shape before arming injection, so shape cloning cannot
       consume the failure intended for property materialization. */
    assert(JS_SetPropertyStr(ctx, target, "__autoinit_oom_shape__", JS_NULL) >= 0);
    object = JS_VALUE_GET_OBJ(target);
    assert(!object->shape->is_hashed || js_rc(object->shape)->ref_count == 1);
    prs = find_own_property(&pr, object, atom);
    assert(prs && (prs->flags & JS_PROP_TMASK) == JS_PROP_AUTOINIT);
    flags = prs->flags;
    realm_and_id = pr->u.init.realm_and_id;
    opaque = pr->u.init.opaque;
    if (kind == 2) {
        /* Intern the initializer value ahead of time. Its next instantiation
           only duplicates this string, making global var-ref allocation the
           first allocator call; the saved refcount also detects a lost value. */
        retained = JS_NewAtomString(ctx, string_entry[0].u.str);
        assert(!JS_IsException(retained));
        string_refs = js_rc(JS_VALUE_GET_PTR(retained))->ref_count;
    }
    realm_refs = js_rc(ctx)->ref_count;
    for (i = 0; i < 2; i++) {
        exhaust_pooled_free_blocks(rt, &failure);
        failure.fail_next = TRUE;
        result = JS_GetProperty(ctx, target, atom);
        assert(JS_IsException(result));
        assert(!failure.fail_next && failure.failures == i + 1);
        assert(JS_HasException(ctx));
        exception = JS_GetException(ctx);
        JS_FreeValue(ctx, exception);
        assert(!JS_HasException(ctx));
        /* Re-find the property after every attempt: shape preparation can
           replace a shape even though the initializer preserves its recipe. */
        prs = find_own_property(&pr, object, atom);
        assert(prs && prs->flags == flags);
        assert(pr->u.init.realm_and_id == realm_and_id);
        assert(pr->u.init.opaque == opaque);
        assert(js_rc(ctx)->ref_count == realm_refs);
        if (kind == 2)
            assert(js_rc(JS_VALUE_GET_PTR(retained))->ref_count == string_refs);
    }
    while ((ptr = failure.padding) != NULL) {
        memcpy(&failure.padding, ptr, sizeof(failure.padding));
        js_free_rt(rt, ptr);
    }
    if (retry) {
        result = JS_GetProperty(ctx, target, atom);
        assert(!JS_IsException(result) && !JS_HasException(ctx));
        prs = find_own_property(&pr, object, atom);
        assert(prs && prs->flags == ((flags & ~JS_PROP_TMASK) |
                                    (kind == 2 ? JS_PROP_VARREF : 0)));
        again = JS_GetProperty(ctx, target, atom);
        assert(!JS_IsException(again));
        assert(JS_VALUE_GET_PTR(again) == JS_VALUE_GET_PTR(result));
        JS_FreeValue(ctx, again);
        if (kind == 0) {
            assert(JS_IsFunction(ctx, result));
            again = JS_Call(ctx, result, target, 0, NULL);
            assert(JS_IsNumber(again) && JS_VALUE_GET_INT(again) == 42);
        } else if (kind == 1) {
            assert(JS_IsObject(result));
            again = JS_GetPropertyStr(ctx, result, "constructor");
            assert(JS_IsObject(again));
            assert(JS_VALUE_GET_OBJ(again) == object);
        } else {
            assert(JS_IsString(result));
            assert(JS_VALUE_GET_PTR(result) == JS_VALUE_GET_PTR(retained));
            again = JS_UNDEFINED;
        }
        JS_FreeValue(ctx, again);
        JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, retained);
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, target);
    JS_RunGC(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(failure.live_allocations == 0);
}

static void test_inherited_autoinit_set_failure(void)
{
    static const char source[] = "(function inheritedPrototype() {})";
    PromiseAllocationFailure failure = { 0 };
    JSMallocFunctions mf = def_malloc_funcs;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue function, receiver, value, exception;
    JSAtom atom;
    int refs;
    void *ptr;

    mf.js_malloc = promise_failure_malloc;
    mf.js_free = promise_failure_free;
    mf.js_realloc = promise_failure_realloc;
    rt = JS_NewRuntime2(&mf, &failure);
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_SetGCThreshold(rt, SIZE_MAX);
    function = JS_Eval(ctx, source, sizeof(source) - 1,
                       "autoinit-set-oom", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(function));
    receiver = JS_NewObjectProto(ctx, function);
    value = JS_NewObject(ctx);
    atom = JS_NewAtom(ctx, "prototype");
    assert(!JS_IsException(receiver) && !JS_IsException(value));
    assert(atom != JS_ATOM_NULL);
    refs = js_rc(JS_VALUE_GET_PTR(value))->ref_count;

    /* The receiver has no own property; the inherited lazy recipe must fail. */
    exhaust_pooled_free_blocks(rt, &failure);
    failure.fail_next = TRUE;
    assert(JS_SetPropertyInternal(ctx, receiver, atom,
                                  JS_DupValue(ctx, value), receiver,
                                  JS_PROP_THROW) < 0);
    assert(!failure.fail_next && failure.failures == 1);
    exception = JS_GetException(ctx);
    assert(JS_IsError(ctx, exception));
    JS_FreeValue(ctx, exception);
    assert(js_rc(JS_VALUE_GET_PTR(value))->ref_count == refs);

    while ((ptr = failure.padding) != NULL) {
        memcpy(&failure.padding, ptr, sizeof(failure.padding));
        js_free_rt(rt, ptr);
    }
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, receiver);
    JS_FreeValue(ctx, function);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(failure.live_allocations == 0);
}

static void test_string_wrapper_length_failure(void)
{
    size_t fail_at;
    int saw_late_failure = FALSE;

    for (fail_at = 1; fail_at <= 16; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue proto, peer, str, wrapper;

        assert(ctx);
        JS_SetGCThreshold(rt, SIZE_MAX);
        proto = JS_GetClassProto(ctx, JS_CLASS_STRING);
        peer = JS_NewObjectProto(ctx, proto);
        str = JS_NewString(ctx, "wrapper");
        assert(!JS_IsException(proto) && !JS_IsException(peer) &&
               !JS_IsException(str));

        /* The peer makes the new wrapper share a hashed empty shape. */
        oom_test_begin(&allocator, fail_at);
        wrapper = JS_ToObject(ctx, str);
        oom_test_end(&allocator);
        if (allocator.failures) {
            if (fail_at > 1)
                saw_late_failure = TRUE;
            assert(JS_IsException(wrapper));
            oom_test_check_exception(ctx);
        } else {
            assert(JS_IsObject(wrapper) && !JS_HasException(ctx));
        }
        JS_FreeValue(ctx, wrapper);
        JS_FreeValue(ctx, str);
        JS_FreeValue(ctx, peer);
        JS_FreeValue(ctx, proto);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
    assert(saw_late_failure);
}

static void test_atom_table_allocation_failure(void)
{
    AllocationFailure failure = { 0 };
    JSRuntime *rt = new_allocation_failure_runtime(&failure);
    JSContext *ctx = JS_NewContext(rt);
    JSAtom *atoms, atom;
    JSValue object, value, property, result, exception;
    size_t count = 0, capacity;
    int operation, ret;

    assert(ctx);
    object = JS_NewObject(ctx);
    value = JS_NewObject(ctx);
    assert(!JS_IsException(object) && !JS_IsException(value));
    capacity = rt->atom_size;
    atoms = malloc(capacity * sizeof(*atoms));
    assert(atoms);
    while (rt->atom_free_index != 0) {
        assert(count < capacity);
        atoms[count] = JS_NewAtomInt64(ctx, INT64_MIN + (int64_t)count);
        assert(atoms[count] != JS_ATOM_NULL && !JS_HasException(ctx));
        count++;
    }
    property = JS_NewString(ctx, "missing_atom_oom");
    assert(!JS_IsException(property));
    for (operation = 0; operation < 9; operation++) {
        failure.fail_table_realloc = TRUE;
        switch (operation) {
        case 0:
            atom = JS_NewAtomInt64(ctx, INT64_MAX);
            assert(atom == JS_ATOM_NULL);
            break;
        case 1:
            result = JS_GetPropertyInt64(ctx, object, INT64_MAX);
            assert(JS_IsException(result));
            break;
        case 2:
            ret = JS_SetPropertyInt64(ctx, object, INT64_MAX,
                                      JS_DupValue(ctx, value));
            assert(ret == -1);
            break;
        case 3:
            ret = JS_DefinePropertyValueInt64(ctx, object, INT64_MAX,
                                              JS_DupValue(ctx, value),
                                              JS_PROP_C_W_E);
            assert(ret == -1);
            break;
        case 4:
            ret = JS_DeletePropertyInt64(ctx, object, INT64_MAX,
                                         JS_PROP_THROW);
            assert(ret == -1);
            break;
        case 5:
            atom = JS_NewAtom(ctx, "missing_atom_oom");
            assert(atom == JS_ATOM_NULL);
            break;
        case 6:
            atom = JS_NewAtomUInt32(ctx, UINT32_MAX);
            assert(atom == JS_ATOM_NULL);
            break;
        case 7:
            result = JS_GetPropertyStr(ctx, object, "missing_atom_oom");
            assert(JS_IsException(result));
            break;
        default:
            atom = JS_ValueToAtom(ctx, property);
            assert(atom == JS_ATOM_NULL);
            break;
        }
        assert(!failure.fail_table_realloc);
        assert(failure.failures == operation + 1);
        assert(JS_HasException(ctx));
        exception = JS_GetException(ctx);
        assert(JS_IsError(ctx, exception));
        JS_FreeValue(ctx, exception);
        assert(js_rc(JS_VALUE_GET_OBJ(value))->ref_count == 1);
        assert(rt->atom_free_index == 0);
    }
    atom = JS_NewAtom(ctx, "atom_after_failed_growth");
    assert(atom != JS_ATOM_NULL && !JS_HasException(ctx));
    assert(rt->atom_size > capacity);
    for (size_t i = 0; i < count; i++) {
        JSAtom retained = JS_NewAtomInt64(ctx, INT64_MIN + (int64_t)i);
        assert(retained == atoms[i]);
        JS_FreeAtom(ctx, retained);
    }
    JS_FreeAtom(ctx, atom);
    while (count)
        JS_FreeAtom(ctx, atoms[--count]);
    free(atoms);
    JS_FreeValue(ctx, property);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(failure.live_allocations == 0);
}

static int factory_proto_finalized;

static void factory_proto_finalizer(JSRuntime *rt, JSValue value)
{
    factory_proto_finalized++;
}

static JSValue constructor_setup_callback(JSContext *ctx, JSValueConst new_target,
                                          int argc, JSValueConst *argv)
{
    return JS_NewObject(ctx);
}

static JSValue constructor_setup_create(JSContext *ctx, int class_id,
                                        const char *name, int flags)
{
    return JS_NewCConstructor(ctx, class_id, name, constructor_setup_callback,
                              0, JS_CFUNC_constructor, 0, JS_UNDEFINED,
                              NULL, 0, NULL, 0, flags);
}

static void test_native_constructor_setup(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSClassID class_id = 0;
    JSClassDef def = { .class_name = "FactoryPrototype",
                       .finalizer = factory_proto_finalizer };
    JSValue ctor, proto, old_proto, global, value, error;
    int i;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    JS_NewClassID(&class_id);
    assert(JS_NewClass(rt, class_id, &def) == 0);
    factory_proto_finalized = 0;
    proto = JS_NewObjectClass(ctx, class_id);
    assert(!JS_IsException(proto));
    JS_SetClassProto(ctx, class_id, proto);
    for (i = 0; i < 2; i++) {
        ctor = constructor_setup_create(ctx, class_id, "UnpublishedFactory",
                  JS_NEW_CTOR_NO_GLOBAL | JS_NEW_CTOR_PROTO_CLASS);
        assert(JS_IsConstructor(ctx, ctor));
        JS_FreeValue(ctx, ctor);
        JS_RunGC(rt);
    }
    assert(factory_proto_finalized == 2);

    global = JS_GetGlobalObject(ctx);
    assert(JS_DefinePropertyValueStr(ctx, global, "BlockedFactory",
                                    JS_NewInt32(ctx, 47), 0) == 1);
    old_proto = JS_GetClassProto(ctx, class_id);
    ctor = constructor_setup_create(ctx, class_id, "BlockedFactory", 0);
    assert(JS_IsException(ctor));
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
    proto = JS_GetClassProto(ctx, class_id);
    assert(JS_StrictEq(ctx, proto, old_proto));
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, old_proto);
    value = JS_GetPropertyStr(ctx, global, "BlockedFactory");
    assert(JS_VALUE_GET_TAG(value) == JS_TAG_INT && JS_VALUE_GET_INT(value) == 47);
    JS_FreeValue(ctx, value);

    for (i = 0; i < 2; i++) {
        const char *source = i == 0 ? "Object.freeze({})" :
            "new Proxy({}, { defineProperty() { throw 53; } })";
        proto = JS_Eval(ctx, source, strlen(source), "constructor-setup",
                        JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(proto));
        JS_SetClassProto(ctx, class_id, JS_DupValue(ctx, proto));
        ctor = constructor_setup_create(ctx, class_id, "UnpublishedFactory",
                                         JS_NEW_CTOR_PROTO_EXIST);
        assert(JS_IsException(ctor));
        error = JS_GetException(ctx);
        assert(i == 0 ? JS_IsError(ctx, error) :
               JS_StrictEq(ctx, error, JS_NewInt32(ctx, 53)));
        JS_FreeValue(ctx, error);
        value = JS_GetPropertyStr(ctx, global, "UnpublishedFactory");
        assert(JS_IsUndefined(value));
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, proto);
    }
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(factory_proto_finalized == 3);
}

static void test_normalization_allocation_failure(void)
{
    size_t fail_at, allocations = 0;

    for (fail_at = 0; fail_at <= allocations; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue input, method, result;
        char text[2048];
        const char *normalized;
        size_t len;

        assert(ctx);
        memset(text, 'x', sizeof(text));
        memcpy(text, "e\xcc\x81", 3);
        input = JS_NewStringLen(ctx, text, sizeof(text));
        method = JS_GetPropertyStr(ctx, input, "normalize");
        assert(!JS_IsException(input) && JS_IsFunction(ctx, method));
        oom_test_begin(&allocator, fail_at);
        result = JS_Call(ctx, method, input, 0, NULL);
        oom_test_end(&allocator);
        if (!fail_at) {
            allocations = allocator.calls;
            assert(allocations > 0 && allocations < 32);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(result)) {
            assert(fail_at);
            oom_test_check_exception(ctx);
        } else {
            assert(!JS_HasException(ctx));
            normalized = JS_ToCStringLen(ctx, &len, result);
            assert(normalized && len == sizeof(text) - 1);
            assert(!memcmp(normalized, "\xc3\xa9", 2));
            assert(!memcmp(normalized + 2, text + 3, sizeof(text) - 3));
            JS_FreeCString(ctx, normalized);
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, method);
        JS_FreeValue(ctx, input);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_intrinsic_class_registration_failure(void)
{
    int kind;

    for (kind = 0; kind < 4; kind++) {
        AllocationFailure failure = { 0 };
        JSRuntime *rt = new_allocation_failure_runtime(&failure);
        JSContext *ctx = JS_NewContextRaw(rt);
        JSAtom *atoms;
        size_t count = 0, capacity = rt->atom_size;
        char name[48];
        JSValue value;
        int ret;

        assert(ctx);
        atoms = malloc(capacity * sizeof(*atoms));
        assert(atoms);
        while (rt->atom_free_index) {
            assert(count < capacity);
            snprintf(name, sizeof(name), "class_failure_%zu", count);
            atoms[count] = JS_NewAtom(ctx, name);
            assert(atoms[count] != JS_ATOM_NULL);
            count++;
        }
        failure.fail_table_realloc = TRUE;
        if (kind == 0) {
            value = js_new_disposable_resource_list(ctx);
            assert(JS_IsException(value));
        } else {
            ret = kind == 1 ? js_init_array_from_async(ctx) :
                  kind == 2 ? js_init_disposable_stack(ctx) :
                              js_init_async_disposable_stack(ctx);
            assert(ret == -1);
        }
        assert(failure.failures == 1 && !failure.fail_table_realloc);
        assert(JS_HasException(ctx));
        value = JS_GetException(ctx);
        assert(JS_IsError(ctx, value));
        JS_FreeValue(ctx, value);
        while (count)
            JS_FreeAtom(ctx, atoms[--count]);
        free(atoms);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(failure.live_allocations == 0);
    }
}

static char export_oom_text[2048];

static int allocation_failure_module_init(JSContext *ctx, JSModuleDef *m)
{
    static const JSCFunctionListEntry exports[] = {
        JS_PROP_STRING_DEF("value", export_oom_text, 0),
    };
    OOMTestAllocator *allocator = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    int ret;

    oom_test_begin(allocator, 1);
    ret = JS_SetModuleExportList(ctx, m, exports, countof(exports));
    oom_test_end(allocator);
    assert(allocator->failures == 1);
    assert(ret == -1 && JS_HasException(ctx));
    return ret;
}

static void test_module_export_allocation_failure(void)
{
    static const char source[] = "import { value } from 'allocation-failure';";
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSModuleDef *m;
    JSValue result, error;

    assert(ctx);
    memset(export_oom_text, 'x', sizeof(export_oom_text) - 1);
    JS_SetRuntimeOpaque(rt, &allocator);
    m = JS_NewCModule(ctx, "allocation-failure", allocation_failure_module_init);
    assert(m && JS_AddModuleExport(ctx, m, "value") == 0);
    result = JS_Eval(ctx, source, sizeof(source) - 1, "export-oom.js",
                     JS_EVAL_TYPE_MODULE);
    assert(allocator.failures == 1);
    if (JS_IsException(result)) {
        oom_test_check_exception(ctx);
    } else {
        assert(JS_PromiseState(ctx, result) == JS_PROMISE_REJECTED);
        error = JS_PromiseResult(ctx, result);
        assert(JS_IsError(ctx, error));
        JS_FreeValue(ctx, error);
    }
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(allocator.live == 0);
}

static void test_optional_bigint_shrink(void)
{
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSBigInt *value, *result;

    assert(ctx);
    value = js_bigint_new(ctx, 256);
    assert(value);
    memset(value->tab, 0, 256 * sizeof(value->tab[0]));
    value->tab[0] = 17;
    oom_test_begin(&allocator, 1);
    result = js_bigint_normalize(rt, value);
    oom_test_end(&allocator);
    assert(allocator.failures == 1);
    assert(result == value && result->len == 1 && result->tab[0] == 17);
    assert(!JS_HasException(ctx));
    JS_FreeValue(ctx, JS_MKPTR(JS_TAG_BIG_INT, result));
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(allocator.live == 0);
}

static void test_optional_property_compaction(void)
{
    int fail_at, i;

    for (fail_at = 1; fail_at <= 2; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue obj, value;
        JSAtom atom;
        int number, ret;

        assert(ctx);
        obj = JS_NewObject(ctx);
        assert(!JS_IsException(obj));
        for (i = 0; i < 256; i++)
            assert(JS_DefinePropertyValueUint32(ctx, obj, i,
                       JS_NewInt32(ctx, i), JS_PROP_C_W_E) == 1);
        for (i = 0; i < 127; i++) {
            atom = JS_NewAtomUInt32(ctx, i);
            assert(JS_DeleteProperty(ctx, obj, atom, 0) == 1);
            JS_FreeAtom(ctx, atom);
        }
        atom = JS_NewAtomUInt32(ctx, 127);
        oom_test_begin(&allocator, fail_at);
        ret = JS_DeleteProperty(ctx, obj, atom, 0);
        oom_test_end(&allocator);
        assert(allocator.failures == 1 && ret == 1);
        assert(!JS_HasException(ctx));
        JS_FreeAtom(ctx, atom);
        for (i = 128; i < 256; i++) {
            value = JS_GetPropertyUint32(ctx, obj, i);
            assert(JS_ToInt32(ctx, &number, value) == 0 && number == i);
            JS_FreeValue(ctx, value);
        }
        assert(JS_IsUndefined(JS_GetPropertyUint32(ctx, obj, 127)));
        JS_FreeValue(ctx, obj);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_promise_capability_allocation_failure(void)
{
    size_t fail_at, allocations = 0;

    for (fail_at = 0; fail_at <= allocations; fail_at++) {
        OOMTestAllocator allocator;
        PromiseAllocationFailure padding = { 0 };
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue promise, funcs[2];
        void *ptr;

        assert(ctx);
        exhaust_pooled_free_blocks(rt, &padding);
        oom_test_begin(&allocator, fail_at);
        promise = JS_NewPromiseCapability(ctx, funcs);
        oom_test_end(&allocator);
        if (!fail_at) {
            allocations = allocator.calls;
            assert(allocations > 0 && allocations < 32);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(promise)) {
            assert(fail_at);
            oom_test_check_exception(ctx);
        } else {
            assert(!JS_HasException(ctx));
            assert(JS_IsFunction(ctx, funcs[0]) && JS_IsFunction(ctx, funcs[1]));
            assert(JS_PromiseState(ctx, promise) == JS_PROMISE_PENDING);
            JS_FreeValue(ctx, funcs[0]);
            JS_FreeValue(ctx, funcs[1]);
            JS_FreeValue(ctx, promise);
        }
        while ((ptr = padding.padding) != NULL) {
            memcpy(&padding.padding, ptr, sizeof(padding.padding));
            js_free_rt(rt, ptr);
        }
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_compiler_var_lookup_allocation_failure(void)
{
    size_t fail_at, allocations = 0;

    for (fail_at = 0; fail_at <= allocations; fail_at++) {
        OOMTestAllocator allocator;
        PromiseAllocationFailure padding = { 0 };
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSFunctionDef fd = { 0 };
        int i, count;
        void *ptr;

        assert(ctx);
        exhaust_pooled_free_blocks(rt, &padding);
        oom_test_begin(&allocator, fail_at);
        for (i = 0; i < 100; i++) {
            JSAtom atom = __JS_AtomFromUInt32(i);
            if (add_var(ctx, &fd, atom) < 0)
                break;
        }
        oom_test_end(&allocator);
        count = i;
        if (!fail_at) {
            allocations = allocator.calls;
            assert(count == 100 && allocations > 0 && allocations < 64);
        } else {
            assert(allocator.failures == 1 && count < 100);
            oom_test_check_exception(ctx);
        }
        /* A failed growth must leave all previously inserted rows searchable. */
        for (i = 0; i < count; i++) {
            assert(find_var(&fd, __JS_AtomFromUInt32(i)) == i);
            JS_FreeAtom(ctx, fd.vars[i].var_name);
        }
        js_free(ctx, fd.vars);
        js_free(ctx, fd.var_hash);
        while ((ptr = padding.padding) != NULL) {
            memcpy(&padding.padding, ptr, sizeof(padding.padding));
            js_free_rt(rt, ptr);
        }
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_compiler_debug_allocation_failure(int nested)
{
    char source[8192];
    uint8_t expected_pc2line[8192];
    size_t length, fail_at, allocations = 0, pc2line_len = 0;
    int i;

    strcpy(source, nested ? "function f(a) {\n" : "var a = 0;\n");
    for (i = 0; i < 512; i++)
        strcat(source, "a += 1;\n");
    strcat(source, nested ? "return a;\n}\nf(0);\n" : "a;\n");
    length = strlen(source);
    for (fail_at = 0; fail_at <= allocations; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue result;
        int number;

        assert(ctx);
        JS_SetStripInfo(rt, 0);
        oom_test_begin(&allocator, fail_at);
        result = JS_Eval(ctx, source, length, "compiler-oom.js",
                         JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
        oom_test_end(&allocator);
        if (!fail_at) {
            allocations = allocator.calls;
            assert(allocations > 0 && allocations < 256);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(result)) {
            oom_test_check_exception(ctx);
        } else {
            JSFunctionBytecode *b = JS_VALUE_GET_PTR(result);

            assert(!JS_HasException(ctx));
            if (nested) {
                assert(b->cpool_count > 0);
                assert(JS_VALUE_GET_TAG(b->cpool[0]) == JS_TAG_FUNCTION_BYTECODE);
                b = JS_VALUE_GET_PTR(b->cpool[0]);
            }
            assert(b->has_debug);
            if (!fail_at) {
                pc2line_len = b->debug.pc2line_len;
                assert(pc2line_len > 512 && pc2line_len <= sizeof(expected_pc2line));
                memcpy(expected_pc2line, b->debug.pc2line_buf, pc2line_len);
            } else {
                assert(b->debug.pc2line_len == pc2line_len);
                assert(!memcmp(expected_pc2line, b->debug.pc2line_buf, pc2line_len));
            }
            result = JS_EvalFunction(ctx, result);
            assert(JS_ToInt32(ctx, &number, result) == 0 && number == 512);
            JS_FreeValue(ctx, result);
        }
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_function_list_allocation_failure(void)
{
    int kind;

    for (kind = 0; kind < 2; kind++) {
        OOMTestAllocator allocator;
        PromiseAllocationFailure padding = { 0 };
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSCFunctionListEntry entry = JS_PROP_ATOM_DEF("value", 0, JS_PROP_C_W_E);
        JSValue obj;
        JSAtom name;
        char alias[2048];
        void *ptr;
        int ret;

        assert(ctx);
        obj = JS_NewObject(ctx);
        name = JS_NewAtom(ctx, "value");
        assert(!JS_IsException(obj) && name != JS_ATOM_NULL);
        assert(JS_DefinePropertyValue(ctx, obj, name, JS_UNDEFINED,
                                      JS_PROP_C_W_E) == 1);
        if (kind) {
            memset(alias, 'a', sizeof(alias) - 1);
            alias[sizeof(alias) - 1] = '\0';
            entry.def_type = JS_DEF_ALIAS;
            entry.u.alias.name = alias;
            entry.u.alias.base = -1;
        } else {
            entry.u.i32 = JS_NewAtomUInt32(ctx, 12345);
            exhaust_pooled_free_blocks(rt, &padding);
        }
        oom_test_begin(&allocator, 1);
        ret = JS_SetPropertyFunctionList(ctx, obj, &entry, 1);
        oom_test_end(&allocator);
        assert(allocator.failures == 1 && ret == -1);
        oom_test_check_exception(ctx);
        assert(JS_IsUndefined(JS_GetProperty(ctx, obj, name)));
        if (!kind)
            JS_FreeAtom(ctx, entry.u.i32);
        while ((ptr = padding.padding) != NULL) {
            memcpy(&padding.padding, ptr, sizeof(padding.padding));
            js_free_rt(rt, ptr);
        }
        JS_FreeAtom(ctx, name);
        JS_FreeValue(ctx, obj);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_compiler_value_allocation_failure(void)
{
    static const char * const classes[] = {
        "class C { x = 7; static y = 11; } new C().x + C.y;",
        "class C { a() {} b() {} c() {} x = 7; static y = 11; } new C().x + C.y;",
        "class A {} class B {} class C {} 18;",
    };
    char literal[2059];
    const char *source;
    int kind;

    literal[0] = '"';
    memset(literal + 1, 'q', 2048);
    strcpy(literal + 2049, "\".length;");
    for (kind = 0; kind < 4; kind++) {
        size_t fail_at, allocations = 0;

        source = kind < 3 ? classes[kind] : literal;
        for (fail_at = 0; fail_at <= allocations; fail_at++) {
            OOMTestAllocator allocator;
            PromiseAllocationFailure padding = { 0 };
            JSRuntime *rt = oom_test_runtime(&allocator);
            JSContext *ctx = JS_NewContext(rt);
            JSValue result;
            int number;
            void *ptr;

            assert(ctx);
            exhaust_pooled_free_blocks(rt, &padding);
            oom_test_begin(&allocator, fail_at);
            result = JS_Eval(ctx, source, strlen(source), "value-oom.js",
                             JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
            oom_test_end(&allocator);
            if (!fail_at) {
                allocations = allocator.calls;
                assert(allocations > 0 && allocations < 256);
            } else {
                assert(allocator.failures == 1);
            }
            if (JS_IsException(result)) {
                oom_test_check_exception(ctx);
            } else {
                assert(!JS_HasException(ctx));
                result = JS_EvalFunction(ctx, result);
                assert(JS_ToInt32(ctx, &number, result) == 0);
                assert(number == (kind < 3 ? 18 : 2048));
                JS_FreeValue(ctx, result);
            }
            while ((ptr = padding.padding) != NULL) {
                memcpy(&padding.padding, ptr, sizeof(padding.padding));
                js_free_rt(rt, ptr);
            }
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            assert(allocator.live == 0);
        }
    }
}

static void test_rope_rebalance_allocation_failure(BOOL prepend)
{
    OOMTestAllocator allocator;
    PromiseAllocationFailure padding = { 0 };
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSValue leaf, rope, result;
    char buffer[513];
    const char *text;
    size_t len, i;
    int rope_refs, leaf_refs;
    void *reserve, *ptr;

    assert(ctx);
    memset(buffer, 'x', sizeof(buffer));
    leaf = JS_NewStringLen(ctx, buffer, sizeof(buffer));
    assert(!JS_IsException(leaf));
    rope = JS_DupValue(ctx, leaf);
    /* Stop immediately before the depth limit forces recursive rebalancing. */
    for (i = 0; i < 60; i++) {
        if (prepend)
            rope = JS_ConcatString(ctx, JS_DupValue(ctx, leaf), rope);
        else
            rope = JS_ConcatString(ctx, rope, JS_DupValue(ctx, leaf));
        assert(!JS_IsException(rope));
    }
    assert(JS_IsStringRope(rope));
    assert(JS_VALUE_GET_STRING_ROPE(rope)->depth == 60);
    rope_refs = js_rc(JS_VALUE_GET_PTR(rope))->ref_count;
    leaf_refs = js_rc(JS_VALUE_GET_PTR(leaf))->ref_count;
    reserve = js_malloc_rt(rt, sizeof(JSStringRope));
    assert(reserve);
    exhaust_pooled_free_blocks(rt, &padding);
    /* Leave room for the outer node, then fail the first rebalance join. */
    js_free_rt(rt, reserve);
    oom_test_begin(&allocator, JS_MALLOC_LARGE_BLOCKS_ONLY ? 2 : 1);
    if (prepend)
        result = JS_ConcatString(ctx, JS_DupValue(ctx, leaf),
                                 JS_DupValue(ctx, rope));
    else
        result = JS_ConcatString(ctx, JS_DupValue(ctx, rope),
                                 JS_DupValue(ctx, leaf));
    oom_test_end(&allocator);
    assert(allocator.failures == 1);
    if (!JS_IsException(result))
        fprintf(stderr, "rebalance OOM returned a string of length %u (prepend=%d)\n",
                JS_IsStringRope(result) ? JS_VALUE_GET_STRING_ROPE(result)->len :
                    JS_VALUE_GET_STRING(result)->len, prepend);
    assert(JS_IsException(result));
    oom_test_check_exception(ctx);
    assert(js_rc(JS_VALUE_GET_PTR(rope))->ref_count == rope_refs);
    assert(js_rc(JS_VALUE_GET_PTR(leaf))->ref_count == leaf_refs);

    if (prepend)
        result = JS_ConcatString(ctx, JS_DupValue(ctx, leaf),
                                 JS_DupValue(ctx, rope));
    else
        result = JS_ConcatString(ctx, JS_DupValue(ctx, rope),
                                 JS_DupValue(ctx, leaf));
    assert(!JS_IsException(result) && !JS_HasException(ctx));
    text = JS_ToCStringLen(ctx, &len, result);
    assert(text && len == 62 * sizeof(buffer));
    for (i = 0; i < len; i++)
        assert(text[i] == 'x');
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, rope);
    JS_FreeValue(ctx, leaf);
    while ((ptr = padding.padding) != NULL) {
        memcpy(&padding.padding, ptr, sizeof(padding.padding));
        js_free_rt(rt, ptr);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(allocator.live == 0);
}

static void test_vm_result_allocation_failures(void)
{
    static const char * const operations[] = {
        "({ value: 1 });",
        "let f = function() { return 1; }; f();",
        "void arguments;",
        "(function() { 'use strict'; return arguments; })(1);",
        "((...args) => args)(1, 2, 3);",
        "/abc/g;",
        "for (let key in 'abcd') {}",
        "for (let key in { 123456789: 1 }) {}",
        "'a'.repeat(10000) + 'b'.repeat(10000);",
        "let value = { toString() { return 'x'; } }; value + 'y';",
        "class C { #x; get(obj) { return obj.#x; } } new C().get({});",
        "(function() { with ({}) { eval('var x = 1'); } })();",
    };
    size_t kind, fail_at, allocations;

    for (kind = 0; kind < countof(operations); kind++) {
        allocations = 0;
        for (fail_at = 0; fail_at <= allocations; fail_at++) {
            OOMTestAllocator allocator;
            PromiseAllocationFailure padding = { 0 };
            JSRuntime *rt = oom_test_runtime(&allocator);
            JSContext *ctx = JS_NewContext(rt);
            JSValue func, result;
            char source[1024];
            void *ptr;
            int32_t number;

            assert(ctx);
            snprintf(source, sizeof(source),
                     "(function() { try { for (const value of [1]) { %s }"
                     " return 42; } catch (error) { return 17; } })",
                     operations[kind]);
            func = JS_Eval(ctx, source, strlen(source), "vm-result-oom.js",
                           JS_EVAL_TYPE_GLOBAL);
            assert(!JS_IsException(func));
            exhaust_pooled_free_blocks(rt, &padding);
            oom_test_begin(&allocator, fail_at);
            result = JS_Call(ctx, func, JS_UNDEFINED, 0, NULL);
            oom_test_end(&allocator);
            if (!fail_at) {
                allocations = allocator.calls;
                assert(allocations < 256);
            } else {
                assert(allocator.failures == 1);
            }
            if (JS_IsException(result)) {
                oom_test_check_exception(ctx);
            } else {
                assert(!JS_HasException(ctx));
                assert(JS_ToInt32(ctx, &number, result) == 0);
                assert(number == 17 || number == 42);
            }
            JS_FreeValue(ctx, result);
            JS_FreeValue(ctx, func);
            while ((ptr = padding.padding) != NULL) {
                memcpy(&padding.padding, ptr, sizeof(padding.padding));
                js_free_rt(rt, ptr);
            }
            JS_FreeContext(ctx);
            JS_FreeRuntime(rt);
            assert(allocator.live == 0);
        }
    }
}

static void test_numeric_global_reference_allocation_failure(void)
{
    static const char source[] = "(function() { reference_target += 1; })";
    OOMTestAllocator allocator;
    PromiseAllocationFailure padding = { 0 };
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSFunctionBytecode *b;
    JSValue func, result, base;
    JSAtom atom;
    int pos, count = 0, ref_count;
    void *ptr;

    assert(ctx);
    atom = JS_NewAtomUInt32(ctx, 123456789);
    assert(atom != JS_ATOM_NULL);
    base = JS_GetGlobalVarRef(ctx, atom);
    assert(JS_IsUndefined(base));
    assert(JS_DefinePropertyValue(ctx, ctx->global_obj, atom,
                                 JS_NewInt32(ctx, 1), JS_PROP_C_W_E) >= 0);
    ref_count = js_rc(JS_VALUE_GET_PTR(ctx->global_var_obj))->ref_count;
    base = JS_GetGlobalVarRef(ctx, atom);
    assert(JS_IsObject(base));
    assert(JS_VALUE_GET_OBJ(base) == JS_VALUE_GET_OBJ(ctx->global_var_obj));
    assert(js_rc(JS_VALUE_GET_PTR(base))->ref_count == ref_count + 1);
    JS_FreeValue(ctx, base);
    assert(js_rc(JS_VALUE_GET_PTR(ctx->global_var_obj))->ref_count == ref_count);

    func = JS_Eval(ctx, source, strlen(source), "numeric-global-reference.js",
                   JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(func));
    b = JS_GetFunctionBytecode(func);
    assert(b);
    /* Numeric identifiers require an internal fixture. Keep the compiler's
       stack layout and replace only the reference's owned atom operand. */
    for (pos = 0; pos < b->byte_code_len;) {
        int op = b->byte_code_buf[pos];
        if (op == OP_make_var_ref) {
            JS_FreeAtom(ctx, get_u32(b->byte_code_buf + pos + 1));
            put_u32(b->byte_code_buf + pos + 1, JS_DupAtom(ctx, atom));
            count++;
        }
        pos += short_opcode_info(op).size;
    }
    assert(count == 1);
    exhaust_pooled_free_blocks(rt, &padding);
    ref_count = js_rc(JS_VALUE_GET_PTR(ctx->global_var_obj))->ref_count;
    oom_test_begin(&allocator, 1);
    result = JS_Call(ctx, func, JS_UNDEFINED, 0, NULL);
    oom_test_end(&allocator);
    assert(JS_IsException(result) && allocator.failures == 1);
    assert(js_rc(JS_VALUE_GET_PTR(ctx->global_var_obj))->ref_count == ref_count);
    oom_test_check_exception(ctx);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, func);
    while ((ptr = padding.padding) != NULL) {
        memcpy(&padding.padding, ptr, sizeof(padding.padding));
        js_free_rt(rt, ptr);
    }
    JS_FreeAtom(ctx, atom);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(allocator.live == 0);
}

static void test_basic_object_allocation_failures(void)
{
    size_t fail_at, allocations = 0;

    for (fail_at = 0; fail_at <= allocations; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx;

        oom_test_begin(&allocator, fail_at);
        ctx = JS_NewContextRaw(rt);
        oom_test_end(&allocator);
        if (!fail_at) {
            allocations = allocator.calls;
            assert(ctx && allocations > 0 && allocations < 1024);
        } else {
            assert(allocator.failures == 1);
        }
        if (ctx) {
            JSObject *global = JS_VALUE_GET_OBJ(ctx->global_obj);

            assert(!JS_HasException(ctx));
            assert(JS_IsObject(global->u.global_object.uninitialized_vars));
            JS_FreeContext(ctx);
        }
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_shape_capacity_limits(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue value, shared;
    int saved_count;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    value = JS_NewObjectProtoClassAlloc(ctx, JS_NULL, JS_CLASS_OBJECT, INT_MAX);
    assert(JS_IsException(value));
    oom_test_check_exception(ctx);
    value = JS_NewObject(ctx);
    shared = JS_NewObject(ctx);
    assert(!JS_IsException(value) && !JS_IsException(shared));
    saved_count = rt->shape_hash_count;
    rt->shape_hash_count = INT_MAX;
    assert(js_new_shape2(ctx, NULL, 2, 0) == NULL);
    rt->shape_hash_count = saved_count;
    /* The artificial limit also prevents allocating an OOM Error object. */
    assert(JS_HasException(ctx));
    JS_FreeValue(ctx, JS_GetException(ctx));
    rt->shape_hash_count = INT_MAX;
    assert(JS_DefinePropertyValueStr(ctx, value, "capacity_probe", JS_UNDEFINED,
                                     JS_PROP_C_W_E) < 0);
    rt->shape_hash_count = saved_count;
    assert(JS_HasException(ctx));
    JS_FreeValue(ctx, JS_GetException(ctx));
    assert(JS_IsUndefined(JS_GetPropertyStr(ctx, value, "capacity_probe")));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, shared);
    assert(get_shape_size(SIZE_MAX, 0) == SIZE_MAX);
    assert(get_shape_size(2, SIZE_MAX) == SIZE_MAX);
    value = JS_NewObject(ctx);
    assert(!JS_IsException(value));
    JS_FreeValue(ctx, value);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
    test_rope_rebalance_allocation_failure(FALSE);
    test_rope_rebalance_allocation_failure(TRUE);
    test_vm_result_allocation_failures();
    test_numeric_global_reference_allocation_failure();
    test_shape_capacity_limits();
#ifdef JS_MALLOC_USE_ITER
    test_allocation_iteration();
#endif
    test_cstring_allocation_failure();
    test_string_buffer_growth();
    test_array_allocation_size();
    test_basic_object_allocation_failures();
    test_promise_capability_allocation_failure();
    test_compiler_var_lookup_allocation_failure();
    test_compiler_value_allocation_failure();
    test_function_list_allocation_failure();
    test_compiler_debug_allocation_failure(0);
    test_compiler_debug_allocation_failure(1);
    test_optional_bigint_shrink();
    test_optional_property_compaction();
    test_module_export_allocation_failure();
    test_intrinsic_class_registration_failure();
    test_normalization_allocation_failure();
    test_gc_upcoming_properties();
    test_atom_table_allocation_failure();
    test_native_constructor_setup();
    int kind;

    test_payload_alignment();
    for (kind = 0; kind < 3; kind++) {
        test_autoinit_allocation_failure(kind, FALSE);
        test_autoinit_allocation_failure(kind, TRUE);
    }
    test_inherited_autoinit_set_failure();
    test_string_wrapper_length_failure();
    test_backtrace_allocation_failure();
    test_pending_backtrace_allocation_failure();
    test_raw_context_allocation_failure();
    test_proxy_registration_allocation_failure();
    test_malloc_limit_overflow();
    test_realloc_limit_overflow();
    test_gc_accounting_overflow();
    test_settled_promise_prepared_reaction(FALSE);
    test_settled_promise_prepared_reaction(TRUE);
    test_lvalue_label_oom();
    return 0;
}
