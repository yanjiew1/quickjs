/*
 * Runtime finalizer embedding contracts
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
#include "quickjs.h"
#include "oom-allocator.h"
#include "pool-padding.h"

typedef struct CleanupOrder {
    int count;
    int values[3];
} CleanupOrder;

typedef struct Cleanup {
    CleanupOrder *order;
    void *resource;
    int id, calls;
} Cleanup;

static void cleanup(JSRuntime *rt, void *opaque)
{
    Cleanup *entry = opaque;

    assert(JS_GetRuntimeOpaque(rt) == entry->order);
    assert(entry->calls++ == 0);
    assert(entry->order->count < 3);
    entry->order->values[entry->order->count++] = entry->id;
    /* Teardown is closed to new registrations, even from another finalizer. */
    assert(JS_AddRuntimeFinalizer(rt, cleanup, opaque) < 0);
    js_free_rt(rt, entry->resource);
}

static void count_cleanup(JSRuntime *rt, void *opaque)
{
    int *calls = opaque;

    (*calls)++;
}

static void test_order_and_native_storage(void)
{
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    CleanupOrder order = { 0 };
    Cleanup entries[3];
    int i;

    assert(ctx);
    JS_SetRuntimeOpaque(rt, &order);
    assert(JS_AddRuntimeFinalizer(rt, NULL, NULL) < 0);
    for (i = 0; i < 3; i++) {
        entries[i] = (Cleanup){ &order, js_malloc_rt(rt, 4096), i, 0 };
        assert(entries[i].resource);
        assert(JS_AddRuntimeFinalizer(rt, cleanup, &entries[i]) == 0);
    }
    JS_FreeContext(ctx);
    assert(order.count == 0);
    JS_FreeRuntime(rt);
    assert(order.count == 3);
    for (i = 0; i < 3; i++) {
        assert(entries[i].calls == 1);
        assert(order.values[i] == 2 - i);
    }
    assert(allocator.live == 0);
}

static void test_registration_failure(void)
{
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSValue pending, error;
    void *padding;
    int registered = 0, rejected = 0;

    assert(ctx);
    assert(JS_AddRuntimeFinalizer(rt, count_cleanup, &registered) == 0);
    pending = JS_NewString(ctx, "preserved pending exception");
    assert(!JS_IsException(pending));
    JS_Throw(ctx, JS_DupValue(ctx, pending));
    padding = test_pool_padding(rt);
    oom_test_begin(&allocator, 1);
    assert(JS_AddRuntimeFinalizer(rt, count_cleanup, &rejected) < 0);
    oom_test_end(&allocator);
    assert(allocator.failures == 1);
    error = JS_GetException(ctx);
    assert(JS_StrictEq(ctx, pending, error));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, pending);
    test_pool_padding_free(rt, padding);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(registered == 1 && rejected == 0);
    assert(allocator.live == 0);
}

static int object_finalizers, unexpected_cleanup;

static void object_finalizer(JSRuntime *rt, JSValue value)
{
    object_finalizers++;
    assert(JS_AddRuntimeFinalizer(rt, count_cleanup, &unexpected_cleanup) < 0);
}

static void test_registration_closed_before_gc(void)
{
    const JSClassDef def = { "finalizer-probe", .finalizer = object_finalizer };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSClassID class_id = 0;
    JSValue object;

    assert(rt);
    JS_NewClassID(&class_id);
    assert(JS_NewClass(rt, class_id, &def) == 0);
    ctx = JS_NewContext(rt);
    assert(ctx);
    object = JS_NewObjectClass(ctx, class_id);
    assert(!JS_IsException(object));
    assert(JS_SetPropertyStr(ctx, object, "self", JS_DupValue(ctx, object)) >= 0);
    JS_FreeValue(ctx, object);
    JS_FreeContext(ctx);
    assert(object_finalizers == 0);
    /* The unreachable cycle is collected during runtime teardown. */
    JS_FreeRuntime(rt);
    assert(object_finalizers == 1 && unexpected_cleanup == 0);
}

int main(void)
{
    test_order_and_native_storage();
    test_registration_failure();
    test_registration_closed_before_gc();
    return 0;
}
