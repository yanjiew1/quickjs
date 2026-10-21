/* Allocation failures at Segmenter ownership and iterator commit boundaries. */
#include "oom-allocator.h"
#ifdef CONFIG_INTL_SEGMENTER
#include "../src/quickjs/internal/runtime.h"
#include "pool-padding.h"
#include "../src/quickjs/builtins/intl.h"

typedef struct Operations {
    JSContext *ctx;
    JSValue constructor, locale, instance, text, segments;
    JSValue segment, iterator, containing, next, resolved, supported;
} Operations;

static JSValue receiver(Operations *ops, int operation)
{
    JSValue iterator;
    int i;
    if (operation != 5 && operation != 6)
        return JS_UNDEFINED;
    iterator = JS_Call(ops->ctx, ops->iterator, ops->segments, 0, NULL);
    assert(!JS_IsException(iterator));
    if (operation == 6) {
        for (i = 0; i < 2; i++) {
            JSValue result = JS_Call(ops->ctx, ops->next, iterator, 0, NULL);
            assert(!JS_IsException(result));
            JS_FreeValue(ops->ctx, result);
        }
    }
    return iterator;
}

static JSValue invoke(Operations *ops, int operation, JSValueConst iterator)
{
    JSValue zero = JS_NewInt32(ops->ctx, 0);
    switch (operation) {
    case 0: case 8:
        return JS_CallConstructor(ops->ctx, ops->constructor, 1, &ops->locale);
    case 1: case 2:
        return JS_Call(ops->ctx, ops->segment, ops->instance, 1, &ops->text);
    case 3:
        return JS_Call(ops->ctx, ops->iterator, ops->segments, 0, NULL);
    case 4:
        return JS_Call(ops->ctx, ops->containing, ops->segments, 1, &zero);
    case 5: case 6:
        return JS_Call(ops->ctx, ops->next, iterator, 0, NULL);
    case 7:
        return JS_Call(ops->ctx, ops->resolved, ops->instance, 0, NULL);
    default:
        return JS_Call(ops->ctx, ops->supported, ops->constructor, 1, &ops->locale);
    }
}

static void test_operation(int operation)
{
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContextRaw(rt);
    Operations ops = {.ctx = ctx};
    JSValue global, intl, warm, result, expected, it, symbol;
    JSAtom iterator_atom;
    size_t step, calls = 0, failures = 0;
    const uint16_t wide[] = {0x100, 0};
    assert(ctx && !JS_AddIntrinsicBaseObjects(ctx) && !JS_AddIntrinsicIntl(ctx));
    global = JS_GetGlobalObject(ctx);
    intl = JS_GetPropertyStr(ctx, global, "Intl");
    ops.constructor = JS_GetPropertyStr(ctx, intl, "Segmenter");
    ops.locale = JS_NewString(ctx, "en");
    ops.instance = JS_CallConstructor(ctx, ops.constructor, 1, &ops.locale);
    assert(!JS_IsException(ops.instance));
    ops.text = operation == 2 ? JS_NewStringUTF16(ctx, wide, 2) : JS_NewString(ctx, "ab");
    ops.segment = JS_GetPropertyStr(ctx, ops.instance, "segment");
    ops.segments = JS_Call(ctx, ops.segment, ops.instance, 1, &ops.text);
    assert(!JS_IsException(ops.segments));
    symbol = JS_GetWellKnownSymbol(ctx, JS_WELL_KNOWN_SYMBOL_ITERATOR);
    iterator_atom = JS_ValueToAtom(ctx, symbol);
    assert(iterator_atom != JS_ATOM_NULL);
    JS_FreeValue(ctx, symbol);
    ops.iterator = JS_GetProperty(ctx, ops.segments, iterator_atom);
    JS_FreeAtom(ctx, iterator_atom);
    ops.containing = JS_GetPropertyStr(ctx, ops.segments, "containing");
    it = JS_Call(ctx, ops.iterator, ops.segments, 0, NULL);
    assert(!JS_IsException(it));
    ops.next = JS_GetPropertyStr(ctx, it, "next");
    JS_FreeValue(ctx, it);
    ops.resolved = JS_GetPropertyStr(ctx, ops.instance, "resolvedOptions");
    ops.supported = JS_GetPropertyStr(ctx, ops.constructor, "supportedLocalesOf");
    it = receiver(&ops, operation);
    expected = invoke(&ops, operation, it);
    assert(!JS_IsException(expected));
    JS_FreeValue(ctx, it);
    /* Retain the warm result's shapes. Pool padding forces real allocations
       without repeatedly rebuilding the whole runtime/native locale reader. */
    for (step = 0; step <= calls; step++) {
        void *padding;
        int error;
        if (operation == 8) {
            qjs_intl_segment_close(rt->intl_segment);
            rt->intl_segment = NULL;
        }
        it = receiver(&ops, operation);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = invoke(&ops, operation, it);
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        error = JS_IsException(result);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 256 && !error);
        } else {
            assert(allocator.failures == 1);
        }
        if (error) {
            failures++;
            oom_test_check_exception(ctx);
            result = invoke(&ops, operation, it);
        }
        assert(!JS_IsException(result) && !JS_HasException(ctx));
        if (operation == 5) {
            JSValue value = JS_GetPropertyStr(ctx, result, "value");
            JSValue index = JS_GetPropertyStr(ctx, value, "index");
            int32_t number;
            assert(!JS_ToInt32(ctx, &number, index));
            /* Cursor commit precedes every result allocation. Retrying a
               failed next() must therefore resume at the second grapheme. */
            assert(number == (error ? 1 : 0));
            JS_FreeValue(ctx, index);
            JS_FreeValue(ctx, value);
        } else if (operation == 6) {
            JSValue done = JS_GetPropertyStr(ctx, result, "done");
            assert(JS_ToBool(ctx, done) == 1);
            JS_FreeValue(ctx, done);
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, it);
    }
    assert(failures);
    JS_FreeValue(ctx, expected);
    /* Exercise recovery after the entire failure sweep, not just teardown. */
    warm = JS_Call(ctx, ops.segment, ops.instance, 1, &ops.text);
    assert(!JS_IsException(warm) && !JS_HasException(ctx));
    JS_FreeValue(ctx, warm);
    JS_FreeValue(ctx, ops.supported);
    JS_FreeValue(ctx, ops.resolved);
    JS_FreeValue(ctx, ops.next);
    JS_FreeValue(ctx, ops.containing);
    JS_FreeValue(ctx, ops.iterator);
    JS_FreeValue(ctx, ops.segments);
    JS_FreeValue(ctx, ops.segment);
    JS_FreeValue(ctx, ops.text);
    JS_FreeValue(ctx, ops.instance);
    JS_FreeValue(ctx, ops.locale);
    JS_FreeValue(ctx, ops.constructor);
    JS_FreeValue(ctx, intl);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(!allocator.live);
}
#endif

int main(void)
{
#ifdef CONFIG_INTL_SEGMENTER
    int operation;
    for (operation = 0; operation < 10; operation++)
        test_operation(operation);
#endif
    return 0;
}
