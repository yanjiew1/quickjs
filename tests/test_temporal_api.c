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

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();

    assert(rt);
    test_raw_initialization(rt);
    test_publication_failure(rt);
    test_intrinsic_realm(rt);
    test_foreign_raw_intrinsics(rt);
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
