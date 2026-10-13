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
#include "../src/quickjs/internal/runtime.h"
#include "../src/quickjs/compiler/compiler-internal.h"

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

int main(void)
{
    test_malloc_limit_overflow();
    test_realloc_limit_overflow();
    test_gc_accounting_overflow();
    test_lvalue_label_oom();
    return 0;
}
