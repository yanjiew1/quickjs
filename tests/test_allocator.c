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

#include "../src/quickjs/internal/allocator.h"
#include "../src/quickjs/internal/error.h"
#include "../src/quickjs/internal/allocator-inlines.h"
#include "../src/quickjs/internal/runtime.h"
#include "../src/quickjs/compiler/compiler-internal.h"
#include "../src/quickjs/builtins/promise.h"

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
    /* Both reactions already own their handlers and both capability functions. */
    for (i = 0; i < failure->function_count; i++)
        assert(js_rc(JS_VALUE_GET_PTR(failure->functions[i]))->ref_count ==
               (i < 2 ? 2 : 3));
    exhaust_pooled_free_blocks(rt, failure);
    failure->arm_failure = FALSE;
    failure->fail_next = TRUE;
}

static void test_settled_promise_enqueue_failure(BOOL with_capability)
{
    static const JSClassDef owner_class = {
        .class_name = "PromiseReactionOwner",
        .finalizer = reaction_owner_finalizer,
    };
    PromiseAllocationFailure failure = { 0 };
    JSMallocFunctions mf = def_malloc_funcs;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue promise, resolving_funcs[2], owner, result, error, message;
    JSValueConst reason;
    JSValueConst empty[2] = { JS_UNDEFINED, JS_UNDEFINED };
    const char *text;
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
    assert(ret == -1);
    assert(failure.failures == 1 && !failure.fail_next);
    assert(JS_HasException(ctx));
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    message = JS_GetPropertyStr(ctx, error, "message");
    assert(!JS_IsException(message));
    text = JS_ToCString(ctx, message);
    assert(text && strcmp(text, "out of memory") == 0);
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, message);
    JS_FreeValue(ctx, error);
    assert(!JS_HasException(ctx));
    assert(!JS_IsJobPending(rt));
    assert(JS_PromiseState(ctx, promise) == JS_PROMISE_REJECTED);
    result = JS_PromiseResult(ctx, promise);
    assert(JS_VALUE_GET_INT(result) == 42);
    JS_FreeValue(ctx, result);
    assert(failure.unhandled == 1 && failure.handled == 1);
    for (i = 0; i < failure.function_count; i++) {
        assert(failure.owners[i].calls == 0);
        assert(failure.owners[i].finalized == 0);
        assert(js_rc(JS_VALUE_GET_PTR(failure.functions[i]))->ref_count == 1);
        JS_FreeValue(ctx, failure.functions[i]);
        assert(failure.owners[i].finalized == 1);
    }
    while ((ptr = failure.padding) != NULL) {
        memcpy(&failure.padding, ptr, sizeof(failure.padding));
        js_free_rt(rt, ptr);
    }
    /* The handled transition remains committed on enqueue OOM, matching the
       notification already delivered to the host; retry must not notify twice. */
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

static void test_int64_atom_table_allocation_failure(void)
{
    AllocationFailure failure = { 0 };
    JSRuntime *rt = new_allocation_failure_runtime(&failure);
    JSContext *ctx = JS_NewContext(rt);
    JSAtom *atoms, atom;
    JSValue object, value, result, exception;
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
    for (operation = 0; operation < 5; operation++) {
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
        default:
            ret = JS_DeletePropertyInt64(ctx, object, INT64_MAX,
                                         JS_PROP_THROW);
            assert(ret == -1);
            break;
        }
        assert(!failure.fail_table_realloc);
        assert(failure.failures == operation + 1);
        assert(JS_HasException(ctx));
        exception = JS_GetException(ctx);
        JS_FreeValue(ctx, exception);
        assert(js_rc(JS_VALUE_GET_OBJ(value))->ref_count == 1);
        assert(rt->atom_free_index == 0);
    }
    while (count)
        JS_FreeAtom(ctx, atoms[--count]);
    free(atoms);
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, object);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(failure.live_allocations == 0);
}

int main(void)
{
    test_int64_atom_table_allocation_failure();
    test_backtrace_allocation_failure();
    test_raw_context_allocation_failure();
    test_malloc_limit_overflow();
    test_realloc_limit_overflow();
    test_gc_accounting_overflow();
    test_lvalue_label_oom();
    test_settled_promise_enqueue_failure(FALSE);
    test_settled_promise_enqueue_failure(TRUE);
    return 0;
}
