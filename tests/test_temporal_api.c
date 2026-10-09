/*
 * Public Temporal initialization and intrinsic realm regression units
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
#include <string.h>
#include "../include/quickjs.h"

#ifdef CONFIG_TEMPORAL

static JSContext *new_raw_context(JSRuntime *rt)
{
    JSContext *ctx = JS_NewContextRaw(rt);

    assert(ctx);
    assert(JS_AddIntrinsicBaseObjects(ctx) == 0);
    assert(JS_AddIntrinsicDate(ctx) == 0);
    return ctx;
}

static JSValue get_global_temporal(JSContext *ctx)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue temporal = JS_GetPropertyStr(ctx, global, "Temporal");

    JS_FreeValue(ctx, global);
    assert(!JS_IsException(temporal));
    return temporal;
}

static void test_raw_initialization(JSRuntime *rt)
{
    JSContext *ctx = new_raw_context(rt);
    JSValue before, after, ctor, argument, instant, method, text;
    const char *string;

    before = get_global_temporal(ctx);
    assert(JS_IsUndefined(before));
    JS_FreeValue(ctx, before);
    assert(JS_AddIntrinsicTemporal(ctx) == 0);
    before = get_global_temporal(ctx);
    assert(JS_IsObject(before));
    assert(JS_AddIntrinsicTemporal(ctx) == 0);
    after = get_global_temporal(ctx);
    assert(JS_VALUE_GET_PTR(before) == JS_VALUE_GET_PTR(after));
    ctor = JS_GetPropertyStr(ctx, before, "Instant");
    assert(!JS_IsException(ctor));
    argument = JS_NewBigInt64(ctx, -1);
    instant = JS_CallConstructor(ctx, ctor, 1, &argument);
    assert(!JS_IsException(instant));
    method = JS_GetPropertyStr(ctx, instant, "toString");
    assert(!JS_IsException(method));
    text = JS_Call(ctx, method, instant, 0, NULL);
    assert(!JS_IsException(text));
    string = JS_ToCString(ctx, text);
    assert(string && !strcmp(string, "1969-12-31T23:59:59.999999999Z"));
    JS_FreeCString(ctx, string);
    JS_FreeValue(ctx, text);
    JS_FreeValue(ctx, method);
    JS_FreeValue(ctx, instant);
    JS_FreeValue(ctx, argument);
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, before);
    JS_FreeValue(ctx, after);
    JS_FreeContext(ctx);
}

static void test_publication_failure(JSRuntime *rt)
{
    JSContext *ctx = new_raw_context(rt);
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue value, exception;

    assert(JS_DefinePropertyValueStr(ctx, global, "Temporal", JS_UNDEFINED, 0) >= 0);
    assert(JS_AddIntrinsicTemporal(ctx) == -1);
    assert(JS_HasException(ctx));
    exception = JS_GetException(ctx);
    assert(JS_IsObject(exception));
    JS_FreeValue(ctx, exception);
    value = JS_GetPropertyStr(ctx, global, "Temporal");
    assert(JS_IsUndefined(value));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, global);
    JS_RunGC(rt);
    JS_FreeContext(ctx);
    ctx = JS_NewContext(rt);
    assert(ctx);
    value = get_global_temporal(ctx);
    assert(JS_IsObject(value));
    JS_FreeValue(ctx, value);
    JS_FreeContext(ctx);
}

static void test_intrinsic_realm(JSRuntime *rt)
{
    JSContext *one = JS_NewContext(rt);
    JSContext *two = JS_NewContext(rt);
    JSValue temporal, ctor, intrinsic, argument, instant, method, copy, prototype;

    assert(one && two);
    temporal = get_global_temporal(one);
    ctor = JS_GetPropertyStr(one, temporal, "Instant");
    intrinsic = JS_GetPropertyStr(one, ctor, "prototype");
    assert(!JS_IsException(ctor) && !JS_IsException(intrinsic));
    argument = JS_NewBigInt64(two, 17);
    instant = JS_CallConstructor(two, ctor, 1, &argument);
    assert(!JS_IsException(instant));
    method = JS_GetPropertyStr(one, ctor, "from");
    assert(!JS_IsException(method));
    copy = JS_Call(two, method, JS_UNDEFINED, 1, &instant);
    assert(!JS_IsException(copy));
    prototype = JS_GetPrototype(two, copy);
    assert(JS_IsObject(prototype) && JS_IsObject(intrinsic));
    assert(JS_VALUE_GET_PTR(prototype) == JS_VALUE_GET_PTR(intrinsic));
    assert(JS_VALUE_GET_PTR(copy) != JS_VALUE_GET_PTR(instant));
    JS_FreeValue(two, prototype);
    JS_FreeValue(two, copy);
    JS_FreeValue(one, method);
    JS_FreeValue(two, instant);
    JS_FreeValue(two, argument);
    JS_FreeValue(one, intrinsic);
    JS_FreeValue(one, ctor);
    JS_FreeValue(one, temporal);
    JS_RunGC(rt);
    JS_FreeContext(two);
    JS_FreeContext(one);
}

static int prototype_gets;
static int prototype_getter_throws;

static JSValue foreign_target_body(JSContext *ctx, JSValueConst new_target,
                                   int argc, JSValueConst *argv)
{
    return JS_ThrowInternalError(ctx, "newTarget body must not run");
}

static JSValue foreign_prototype_getter(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    JSValue temporal = get_global_temporal(ctx);

    prototype_gets++;
    assert(JS_IsUndefined(temporal));
    JS_FreeValue(ctx, temporal);
    if (prototype_getter_throws)
        return JS_ThrowTypeError(ctx, "prototype getter marker");
    return JS_NULL;
}

static JSValue foreign_target(JSContext *ctx)
{
    JSValue target = JS_NewCFunction2(ctx, foreign_target_body, "Foreign", 0,
                                     JS_CFUNC_constructor, 0);

    assert(!JS_IsException(target));
    return target;
}

static void set_prototype_getter(JSContext *ctx, JSValueConst target)
{
    JSAtom atom = JS_NewAtom(ctx, "prototype");
    JSValue getter = JS_NewCFunction(ctx, foreign_prototype_getter,
                                    "get prototype", 0);

    assert(atom && !JS_IsException(getter));
    assert(JS_DefinePropertyGetSet(ctx, target, atom, getter, JS_UNDEFINED,
                                  JS_PROP_CONFIGURABLE) >= 0);
    JS_FreeAtom(ctx, atom);
}

static void test_foreign_raw_intrinsics(JSRuntime *rt)
{
    JSContext *source = JS_NewContext(rt);
    JSContext *foreign = new_raw_context(rt);
    JSContext *blocked = new_raw_context(rt);
    JSContext *abrupt = new_raw_context(rt);
    JSValue temporal, ctor, target, argument, result, prototype, expected;
    JSValue custom, value, global, exception;
    JSClassID instant_class;

    assert(source);
    temporal = get_global_temporal(source);
    ctor = JS_GetPropertyStr(source, temporal, "Instant");
    assert(!JS_IsException(ctor));
    argument = JS_NewBigInt64(source, 19);
    target = foreign_target(foreign);
    custom = JS_NewObject(foreign);
    assert(!JS_IsException(custom));
    assert(JS_DefinePropertyValueStr(foreign, target, "prototype",
                JS_DupValue(foreign, custom), JS_PROP_C_W_E) >= 0);
    result = JS_CallConstructor2(source, ctor, target, 1, &argument);
    assert(!JS_IsException(result));
    instant_class = JS_GetClassID(result);
    prototype = JS_GetPrototype(source, result);
    assert(JS_VALUE_GET_PTR(prototype) == JS_VALUE_GET_PTR(custom));
    JS_FreeValue(source, prototype);
    JS_FreeValue(source, result);
    value = JS_GetClassProto(foreign, instant_class);
    assert(JS_IsNull(value)); /* Object prototype bypasses intrinsic fallback. */
    JS_FreeValue(foreign, value);
    JS_FreeValue(foreign, custom);

    prototype_gets = prototype_getter_throws = 0;
    set_prototype_getter(foreign, target);
    result = JS_CallConstructor2(source, ctor, target, 1, &argument);
    assert(!JS_IsException(result) && prototype_gets == 1);
    prototype = JS_GetPrototype(source, result);
    expected = JS_GetClassProto(foreign, instant_class);
    assert(JS_IsObject(prototype) && JS_IsObject(expected));
    assert(JS_VALUE_GET_PTR(prototype) == JS_VALUE_GET_PTR(expected));
    value = get_global_temporal(foreign);
    assert(JS_IsUndefined(value)); /* Intrinsic lookup never publishes globals. */
    JS_FreeValue(foreign, value);
    assert(JS_AddIntrinsicTemporal(foreign) == 0);
    value = JS_GetClassProto(foreign, instant_class);
    assert(JS_VALUE_GET_PTR(value) == JS_VALUE_GET_PTR(expected));
    JS_FreeValue(foreign, value);
    JS_FreeValue(foreign, expected);
    JS_FreeValue(source, prototype);
    JS_FreeValue(source, result);
    JS_FreeValue(foreign, target);

    global = JS_GetGlobalObject(blocked);
    assert(JS_DefinePropertyValueStr(blocked, global, "Temporal",
                                     JS_UNDEFINED, 0) >= 0);
    target = foreign_target(blocked);
    assert(JS_DefinePropertyValueStr(blocked, target, "prototype", JS_NULL,
                                     JS_PROP_C_W_E) >= 0);
    result = JS_CallConstructor2(source, ctor, target, 1, &argument);
    assert(!JS_IsException(result));
    prototype = JS_GetPrototype(source, result);
    expected = JS_GetClassProto(blocked, instant_class);
    assert(JS_IsObject(expected));
    assert(JS_VALUE_GET_PTR(prototype) == JS_VALUE_GET_PTR(expected));
    assert(JS_AddIntrinsicTemporal(blocked) == -1);
    exception = JS_GetException(blocked);
    assert(JS_IsObject(exception));
    JS_FreeValue(blocked, exception);
    value = JS_GetClassProto(blocked, instant_class);
    assert(JS_VALUE_GET_PTR(value) == JS_VALUE_GET_PTR(expected));
    JS_FreeValue(blocked, value);
    value = get_global_temporal(blocked);
    assert(JS_IsUndefined(value));
    JS_FreeValue(blocked, value);
    JS_FreeValue(blocked, expected);
    JS_FreeValue(source, prototype);
    JS_FreeValue(source, result);
    JS_FreeValue(blocked, target);
    JS_FreeValue(blocked, global);

    target = foreign_target(abrupt);
    prototype_gets = 0;
    prototype_getter_throws = 1;
    set_prototype_getter(abrupt, target);
    result = JS_CallConstructor2(source, ctor, target, 1, &argument);
    assert(JS_IsException(result) && prototype_gets == 1);
    exception = JS_GetException(source);
    assert(JS_IsObject(exception));
    JS_FreeValue(source, exception);
    value = JS_GetClassProto(abrupt, instant_class);
    assert(JS_IsNull(value));
    JS_FreeValue(abrupt, value);
    value = get_global_temporal(abrupt);
    assert(JS_IsUndefined(value));
    JS_FreeValue(abrupt, value);
    JS_FreeValue(abrupt, target);

    JS_FreeValue(source, argument);
    JS_FreeValue(source, ctor);
    JS_FreeValue(source, temporal);
    JS_RunGC(rt);
    JS_FreeContext(abrupt);
    JS_FreeContext(blocked);
    JS_FreeContext(foreign);
    JS_FreeContext(source);
}

static void assert_embedding_error(JSContext *ctx, const char *expected)
{
    JSValue error, name;
    const char *text;

    assert(JS_HasException(ctx));
    error = JS_GetException(ctx);
    assert(JS_IsObject(error));
    name = JS_GetPropertyStr(ctx, error, "name");
    assert(!JS_IsException(name));
    text = JS_ToCString(ctx, name);
    assert(text && !strcmp(text, expected));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, name);
    JS_FreeValue(ctx, error);
    assert(!JS_HasException(ctx));
}

static JSValue embedding_poison_getter(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    return JS_ThrowInternalError(ctx, "Instant embedding must not call getters");
}

static void assert_instant_words(JSContext *ctx, JSValueConst value,
                                 uint64_t expected_low, uint64_t expected_high)
{
    uint64_t low = 7, high = 11;

    assert(JS_GetTemporalInstantEpochNanoseconds(ctx, value, &low, &high) == 0);
    assert(low == expected_low && high == expected_high);
    assert(!JS_HasException(ctx));
}

static void test_instant_embedding_raw(JSRuntime *rt)
{
    JSContext *ctx = JS_NewContextRaw(rt);
    JSValue global, instant, namespace_object, constructor, expected, prototype;
    JSValue getter, value;
    JSAtom atom;

    assert(ctx && JS_AddIntrinsicBaseObjects(ctx) == 0);
    global = JS_GetGlobalObject(ctx);
    /* A hostile global cannot prevent private intrinsic initialization. */
    getter = JS_NewCFunction(ctx, embedding_poison_getter, "get Temporal", 0);
    atom = JS_NewAtom(ctx, "Temporal");
    assert(!JS_IsException(getter) && atom);
    assert(JS_DefinePropertyGetSet(ctx, global, atom, getter, JS_UNDEFINED,
                                   JS_PROP_CONFIGURABLE) >= 0);
    instant = JS_NewTemporalInstant(ctx, UINT64_MAX, UINT64_MAX);
    assert(!JS_IsException(instant));
    assert_instant_words(ctx, instant, UINT64_MAX, UINT64_MAX);
    /* Slot extraction ignores an own getter with the standard field name. */
    getter = JS_NewCFunction(ctx, embedding_poison_getter,
                             "get epochNanoseconds", 0);
    {
        JSAtom field = JS_NewAtom(ctx, "epochNanoseconds");

        assert(!JS_IsException(getter) && field);
        assert(JS_DefinePropertyGetSet(ctx, instant, field, getter,
                                       JS_UNDEFINED, JS_PROP_CONFIGURABLE) >= 0);
        JS_FreeAtom(ctx, field);
    }
    assert_instant_words(ctx, instant, UINT64_MAX, UINT64_MAX);
    prototype = JS_GetPrototype(ctx, instant);
    expected = JS_GetClassProto(ctx, JS_GetClassID(instant));
    assert(JS_StrictEq(ctx, prototype, expected));
    /* Private initialization does not replace or invoke the global getter. */
    value = JS_GetProperty(ctx, global, atom);
    assert(JS_IsException(value));
    assert_embedding_error(ctx, "InternalError");
    assert(JS_DeleteProperty(ctx, global, atom, 0) == 1);
    JS_FreeAtom(ctx, atom);
    value = get_global_temporal(ctx);
    assert(JS_IsUndefined(value));
    JS_FreeValue(ctx, value);
    assert(JS_AddIntrinsicTemporal(ctx) == 0);
    namespace_object = get_global_temporal(ctx);
    constructor = JS_GetPropertyStr(ctx, namespace_object, "Instant");
    value = JS_GetPropertyStr(ctx, constructor, "prototype");
    assert(JS_StrictEq(ctx, prototype, value));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, constructor);
    JS_FreeValue(ctx, namespace_object);
    JS_FreeValue(ctx, expected);
    JS_FreeValue(ctx, prototype);
    JS_FreeValue(ctx, instant);
    JS_FreeValue(ctx, global);
    JS_FreeContext(ctx);
}

static void test_instant_embedding_bounds(JSRuntime *rt)
{
    static const uint64_t words[][2] = {
        { 0, 0 },
        { UINT64_MAX, UINT64_MAX }, /* -1 ns, not rounded to milliseconds. */
        { UINT64_C(9007199254740993), 0 }, /* Above binary64's exact range. */
        { UINT64_C(0x60162f516f000000), UINT64_C(0x1d4) },
        { UINT64_C(0x9fe9d0ae91000000), UINT64_C(0xfffffffffffffe2b) },
    };
    static const uint64_t invalid[][2] = {
        { UINT64_C(0x60162f516f000001), UINT64_C(0x1d4) },
        { UINT64_C(0x9fe9d0ae90ffffff), UINT64_C(0xfffffffffffffe2b) },
        { 0, UINT64_C(0x8000000000000000) },
        { UINT64_MAX, UINT64_C(0x7fffffffffffffff) },
    };
    JSContext *ctx = JS_NewContext(rt), *other = JS_NewContext(rt);
    JSValue instant;
    size_t i;

    assert(ctx && other);
    for (i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        instant = JS_NewTemporalInstant(ctx, words[i][0], words[i][1]);
        assert(!JS_IsException(instant));
        assert_instant_words(ctx, instant, words[i][0], words[i][1]);
        assert_instant_words(other, instant, words[i][0], words[i][1]);
        JS_FreeValue(ctx, instant);
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        instant = JS_NewTemporalInstant(ctx, invalid[i][0], invalid[i][1]);
        assert(JS_IsException(instant));
        assert_embedding_error(ctx, "RangeError");
    }
    JS_FreeContext(other);
    JS_FreeContext(ctx);
}

static JSValue embedding_eval(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source),
                            "instant-embedding.js", JS_EVAL_TYPE_GLOBAL);

    assert(!JS_IsException(value));
    return value;
}

static void test_instant_embedding_brand(JSRuntime *rt)
{
    JSContext *ctx = JS_NewContext(rt);
    JSValue subclass, fake, proxy, prototype, zoned, number, string, instant;
    JSValue values[9];
    uint64_t low, high;
    size_t i;

    assert(ctx);
    subclass = embedding_eval(ctx, "new (class extends Temporal.Instant {})(-1n)");
    assert_instant_words(ctx, subclass, UINT64_MAX, UINT64_MAX);
    fake = embedding_eval(ctx,
        "({ get epochNanoseconds() { throw 'must not run'; },"
        " [Symbol.toPrimitive]() { throw 'must not run'; } })");
    proxy = embedding_eval(ctx,
        "new Proxy(new Temporal.Instant(1n), {"
        " get() { throw 'proxy must not run'; } })");
    prototype = embedding_eval(ctx, "Temporal.Instant.prototype");
    zoned = embedding_eval(ctx, "new Temporal.ZonedDateTime(1n, 'UTC')");
    number = JS_NewInt32(ctx, 1);
    string = JS_NewString(ctx, "1970-01-01T00:00:00Z");
    assert(!JS_IsException(string));
    values[0] = fake;
    values[1] = proxy;
    values[2] = prototype;
    values[3] = zoned;
    values[4] = number;
    values[5] = string;
    values[6] = JS_NULL;
    values[7] = JS_UNDEFINED;
    values[8] = JS_NewBigInt64(ctx, 1);
    assert(!JS_IsException(values[8]));
    for (i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        low = 7;
        high = 11;
        assert(JS_GetTemporalInstantEpochNanoseconds(ctx, values[i],
                                                      &low, &high) == -1);
        assert(low == 7 && high == 11);
        assert_embedding_error(ctx, "TypeError");
        JS_FreeValue(ctx, values[i]);
    }
    instant = JS_NewTemporalInstant(ctx, 1, 0);
    assert(!JS_IsException(instant));
    low = 7;
    high = 11;
    assert(JS_GetTemporalInstantEpochNanoseconds(ctx, instant, NULL, &high) == -1);
    assert(low == 7 && high == 11);
    assert_embedding_error(ctx, "TypeError");
    assert(JS_GetTemporalInstantEpochNanoseconds(ctx, instant, &low, NULL) == -1);
    assert(low == 7 && high == 11);
    assert_embedding_error(ctx, "TypeError");
    assert(JS_GetTemporalInstantEpochNanoseconds(ctx, instant, &low, &low) == -1);
    assert(low == 7 && high == 11);
    assert_embedding_error(ctx, "TypeError");
    JS_FreeValue(ctx, instant);
    JS_FreeValue(ctx, subclass);
    JS_FreeContext(ctx);
}

/* A zero host-memory limit still permits spare arena blocks to be reused.
   Retain them so the operation must fail if it requests any allocation. */
static void exhaust_spare_blocks(JSRuntime *rt, void **padding)
{
    size_t size;
    void *ptr;

    JS_SetMemoryLimit(rt, 0);
    for (size = sizeof(void *); size <= 512; size += sizeof(void *)) {
        while ((ptr = js_malloc_rt(rt, size)) != NULL) {
            memcpy(ptr, padding, sizeof(*padding));
            *padding = ptr;
        }
    }
}

static void test_instant_embedding_allocation(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *cold, *warm;
    JSValue value, owned, exception;
    uint64_t low = 7, high = 11;
    void *padding = NULL, *next;

    assert(rt);
    cold = new_raw_context(rt);
    warm = JS_NewContext(rt);
    assert(warm);
    owned = JS_NewTemporalInstant(warm, UINT64_MAX, UINT64_MAX);
    assert(!JS_IsException(owned));
    JS_RunGC(rt);
    JS_SetGCThreshold(rt, (size_t)-1);
    exhaust_spare_blocks(rt, &padding);
    /* Slot reads do not allocate or need JavaScript intrinsic publication. */
    assert(JS_GetTemporalInstantEpochNanoseconds(warm, owned, &low, &high) == 0);
    assert(low == UINT64_MAX && high == UINT64_MAX && !JS_HasException(warm));
    value = JS_NewTemporalInstant(warm, 0, 0);
    assert(JS_IsException(value) && JS_HasException(warm));
    JS_SetMemoryLimit(rt, (size_t)-1);
    exception = JS_GetException(warm);
    JS_FreeValue(warm, exception);
    /* Error cleanup may return blocks to an arena. Retain those as well. */
    exhaust_spare_blocks(rt, &padding);
    value = JS_NewTemporalInstant(cold, 0, 0);
    assert(JS_IsException(value) && JS_HasException(cold));
    JS_SetMemoryLimit(rt, (size_t)-1);
    exception = JS_GetException(cold);
    JS_FreeValue(cold, exception);
    /* Failed lazy initialization must permit a later successful retry. */
    value = JS_NewTemporalInstant(cold, 0, 0);
    assert(!JS_IsException(value) && !JS_HasException(cold));
    assert_instant_words(cold, value, 0, 0);
    JS_FreeValue(cold, value);
    value = get_global_temporal(cold);
    assert(JS_IsUndefined(value));
    JS_FreeValue(cold, value);
    while (padding) {
        memcpy(&next, padding, sizeof(next));
        js_free_rt(rt, padding);
        padding = next;
    }
    JS_FreeValue(warm, owned);
    JS_FreeContext(cold);
    JS_FreeContext(warm);
    JS_FreeRuntime(rt);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();

    assert(rt);
    test_raw_initialization(rt);
    test_publication_failure(rt);
    test_intrinsic_realm(rt);
    test_foreign_raw_intrinsics(rt);
    test_instant_embedding_raw(rt);
    test_instant_embedding_bounds(rt);
    test_instant_embedding_brand(rt);
    test_instant_embedding_allocation();
    JS_RunGC(rt);
    JS_FreeRuntime(rt);
    return 0;
}

#else

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, temporal, exception, name;
    const char *text;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    global = JS_GetGlobalObject(ctx);
    temporal = JS_GetPropertyStr(ctx, global, "Temporal");
    assert(JS_IsUndefined(temporal) && !JS_HasException(ctx));
    JS_FreeValue(ctx, temporal);
    assert(JS_AddIntrinsicTemporal(ctx) == -1 && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    assert(JS_IsObject(exception));
    name = JS_GetPropertyStr(ctx, exception, "name");
    assert(!JS_IsException(name));
    text = JS_ToCString(ctx, name);
    assert(text && !strcmp(text, "TypeError"));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, name);
    JS_FreeValue(ctx, exception);
    {
        uint64_t low = 7, high = 11;
        JSValue value = JS_NewTemporalInstant(ctx, 0, 0);

        assert(JS_IsException(value) && JS_HasException(ctx));
        exception = JS_GetException(ctx);
        name = JS_GetPropertyStr(ctx, exception, "name");
        text = JS_ToCString(ctx, name);
        assert(text && !strcmp(text, "TypeError"));
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, name);
        JS_FreeValue(ctx, exception);
        assert(JS_GetTemporalInstantEpochNanoseconds(ctx, JS_UNDEFINED,
                                                      &low, &high) == -1);
        assert(low == 7 && high == 11 && JS_HasException(ctx));
        exception = JS_GetException(ctx);
        name = JS_GetPropertyStr(ctx, exception, "name");
        text = JS_ToCString(ctx, name);
        assert(text && !strcmp(text, "TypeError"));
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, name);
        JS_FreeValue(ctx, exception);
    }
    temporal = JS_GetPropertyStr(ctx, global, "Temporal");
    assert(JS_IsUndefined(temporal) && !JS_HasException(ctx));
    JS_FreeValue(ctx, temporal);
    JS_FreeValue(ctx, global);
    JS_RunGC(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}

#endif /* CONFIG_TEMPORAL */
