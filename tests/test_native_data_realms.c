/* QuickJS public C API realm and native capture ownership probes.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <string.h>
#include "quickjs.h"

typedef struct RealmCapture {
    JSContext *expected;
    void *opaque;
    int calls;
    int finalized;
} RealmCapture;

static JSClassID realm_capture_class_id;

static void realm_capture_finalizer(JSRuntime *rt, JSValue value)
{
    RealmCapture *capture = JS_GetOpaque(value, realm_capture_class_id);

    assert(capture);
    capture->finalized++;
}

static void register_realm_capture(JSRuntime *rt)
{
    static const JSClassDef class_def = {
        .class_name = "NativeRealmCapture",
        .finalizer = realm_capture_finalizer,
    };

    JS_NewClassID(&realm_capture_class_id);
    assert(JS_NewClass(rt, realm_capture_class_id, &class_def) == 0);
}

static JSValue new_realm_capture(JSContext *ctx, RealmCapture *capture)
{
    JSValue value = JS_NewObjectProtoClass(ctx, JS_NULL,
                                           realm_capture_class_id);

    assert(!JS_IsException(value));
    JS_SetOpaque(value, capture);
    return value;
}

static JSValue realm_eval(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source),
                            "native-data-realm-probe.js",
                            JS_EVAL_TYPE_GLOBAL);

    assert(!JS_IsException(value));
    return value;
}

static void assert_realm_proto(JSContext *ctx, JSValueConst value,
                               JSValueConst expected)
{
    JSValue prototype = JS_GetPrototype(ctx, value);

    assert(!JS_IsException(prototype));
    assert(JS_StrictEq(ctx, prototype, expected));
    JS_FreeValue(ctx, prototype);
}

static JSValue public_realm_callback(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv, int magic,
                                     JSValue *data)
{
    RealmCapture *capture = JS_GetOpaque(data[0], realm_capture_class_id);

    assert(capture && ctx == capture->expected);
    assert(JS_GetContextOpaque(ctx) == capture->opaque);
    assert(argc == 1 && JS_IsUndefined(argv[1]));
    capture->calls++;
    JS_RunGC(JS_GetRuntime(ctx));
    if (magic == 1)
        return JS_ThrowTypeError(ctx, "public data callback");
    if (magic == 2)
        return JS_Throw(ctx, JS_DupValue(ctx, data[1]));
    return JS_NewObject(ctx);
}

static void test_public_c_function_data_realms(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *owner, *caller;
    RealmCapture capture = { 0 }, owner_global = { 0 };
    int caller_opaque, i;
    JSValue functions[3], data[2], global, marker, result, error;
    JSValue object_ctor, object_proto, type_error_proto;
    JSValueConst argument = JS_UNDEFINED;

    assert(rt);
    owner = JS_NewContext(rt);
    caller = JS_NewContext(rt);
    assert(owner && caller);
    register_realm_capture(rt);
    capture.expected = caller;
    capture.opaque = &caller_opaque;
    JS_SetContextOpaque(caller, &caller_opaque);
    data[0] = new_realm_capture(caller, &capture);
    data[1] = JS_NewObject(caller);
    assert(!JS_IsException(data[1]));
    for (i = 0; i < 3; i++) {
        functions[i] = JS_NewCFunctionData(owner, public_realm_callback,
                                            2, i, 2,
                                            (JSValueConst *)data);
        assert(!JS_IsException(functions[i]));
        assert(JS_IsFunction(caller, functions[i]));
        assert(!JS_IsConstructor(caller, functions[i]));
        /* Remove the indirect owner reference through Function.prototype. */
        assert(JS_SetPrototype(caller, functions[i], JS_NULL) == 1);
    }
    object_ctor = realm_eval(caller, "Object");
    object_proto = realm_eval(caller, "Object.prototype");
    type_error_proto = realm_eval(caller, "TypeError.prototype");

    /* Public data functions have no [[Realm]]: preserve current-realm fallback. */
    assert(JS_SetConstructorBit(caller, functions[0], 1));
    result = JS_CallConstructor2(caller, object_ctor, functions[0], 0, NULL);
    assert(!JS_IsException(result));
    assert_realm_proto(caller, result, object_proto);
    JS_FreeValue(caller, result);
    assert(JS_SetConstructorBit(caller, functions[0], 0));
    JS_FreeValue(caller, object_ctor);

    /* The captured object owns a cycle; its finalizer proves release by GC. */
    assert(JS_SetPropertyStr(caller, data[0], "cycle",
                              JS_DupValue(caller, functions[0])) == 1);
    marker = new_realm_capture(owner, &owner_global);
    global = JS_GetGlobalObject(owner);
    assert(JS_SetPropertyStr(owner, global, "creationRealmMarker", marker) == 1);
    JS_FreeValue(owner, global);
    JS_FreeValue(caller, data[0]);
    JS_FreeContext(owner);
    JS_RunGC(rt);
    assert(owner_global.finalized == 1);
    assert(capture.finalized == 0);

    result = JS_Call(caller, functions[0], JS_UNDEFINED, 1, &argument);
    assert(!JS_IsException(result));
    assert_realm_proto(caller, result, object_proto);
    JS_FreeValue(caller, result);
    result = JS_Call(caller, functions[1], JS_UNDEFINED, 1, &argument);
    assert(JS_IsException(result) && JS_HasException(caller));
    error = JS_GetException(caller);
    assert_realm_proto(caller, error, type_error_proto);
    JS_FreeValue(caller, error);
    result = JS_Call(caller, functions[2], JS_UNDEFINED, 1, &argument);
    assert(JS_IsException(result));
    error = JS_GetException(caller);
    assert(JS_StrictEq(caller, error, data[1]));
    JS_FreeValue(caller, error);
    assert(capture.calls == 3 && !JS_HasException(caller));
    for (i = 0; i < 3; i++)
        JS_FreeValue(caller, functions[i]);
    JS_FreeValue(caller, data[1]);
    JS_RunGC(rt);
    assert(capture.finalized == 1);
    JS_FreeValue(caller, object_proto);
    JS_FreeValue(caller, type_error_proto);
    JS_FreeContext(caller);
    JS_RunGC(rt);
    JS_FreeRuntime(rt);
}

#ifdef CONFIG_INTL
static JSValue intl_coercion_callback(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int magic,
                                      JSValue *data)
{
    RealmCapture *capture = JS_GetOpaque(data[0], realm_capture_class_id);

    assert(capture && ctx == capture->expected);
    assert(JS_GetContextOpaque(ctx) == capture->opaque);
    assert(argc == 0 && JS_IsObject(this_val));
    capture->calls++;
    JS_RunGC(JS_GetRuntime(ctx));
    if (magic == 1)
        return JS_ThrowTypeError(ctx, "Intl bound callback coercion");
    if (magic == 2)
        return JS_Throw(ctx, JS_DupValue(ctx, data[1]));
    return JS_NewString(ctx, "a");
}

static void test_intl_bound_function_realms(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *owner, *caller;
    RealmCapture coercion = { 0 }, owner_global = { 0 };
    int owner_opaque, i;
    JSValue collator, getter, function, global, result, marker;
    JSValue object_ctor, object_proto, error_proto, target, value;
    JSValue data[2], input, callback, args[2], error;
    JSPropertyEnum *properties;
    uint32_t property_count;
    JSMemoryUsage before, after;
    const char *name;
    static const char *const targets[] = {
        "intlBoundRealmProbe",
        "Function.prototype.bind.call(intlBoundRealmProbe)",
        "new Proxy(intlBoundRealmProbe, {})",
    };

    assert(rt);
    owner = JS_NewContext(rt);
    caller = JS_NewContext(rt);
    assert(owner && caller);
    assert(JS_AddIntrinsicIntl(owner) == 0);
    assert(JS_AddIntrinsicIntl(caller) == 0);
    register_realm_capture(rt);
    coercion.expected = owner;
    coercion.opaque = &owner_opaque;
    JS_SetContextOpaque(owner, &owner_opaque);
    collator = realm_eval(caller, "new Intl.Collator('en')");
    getter = realm_eval(owner,
        "Object.getOwnPropertyDescriptor(Intl.Collator.prototype,'compare').get");
    JS_ComputeMemoryUsage(rt, &before);
    function = JS_Call(caller, getter, collator, 0, NULL);
    assert(!JS_IsException(function));
    JS_ComputeMemoryUsage(rt, &after);
    assert(after.c_func_count == before.c_func_count + 1);
    assert(JS_IsFunction(caller, function));
    assert(!JS_IsConstructor(caller, function));
    value = realm_eval(owner, "Function.prototype");
    assert_realm_proto(caller, function, value);
    JS_FreeValue(caller, value);
    assert(JS_GetOwnPropertyNames(caller, &properties, &property_count,
                                   function, JS_GPN_STRING_MASK) == 0);
    assert(property_count == 2);
    for (i = 0; i < 2; i++) {
        name = JS_AtomToCString(caller, properties[i].atom);
        assert(name && strcmp(name, i ? "name" : "length") == 0);
        JS_FreeCString(caller, name);
    }
    JS_FreePropertyEnum(caller, properties, property_count);
    value = JS_GetPropertyStr(caller, function, "length");
    assert(JS_VALUE_GET_TAG(value) == JS_TAG_INT && JS_VALUE_GET_INT(value) == 2);
    JS_FreeValue(caller, value);
    value = JS_GetPropertyStr(caller, function, "name");
    name = JS_ToCString(caller, value);
    assert(name && *name == '\0');
    JS_FreeCString(caller, name);
    JS_FreeValue(caller, value);

    /* Probe GetFunctionRealm through public constructor customization. */
    object_ctor = realm_eval(caller, "Object");
    object_proto = realm_eval(owner, "Object.prototype");
    assert(JS_SetConstructorBit(caller, function, 1));
    global = JS_GetGlobalObject(caller);
    assert(JS_SetPropertyStr(caller, global, "intlBoundRealmProbe",
                              JS_DupValue(caller, function)) == 1);
    for (i = 0; i < 3; i++) {
        target = realm_eval(caller, targets[i]);
        result = JS_CallConstructor2(caller, object_ctor, target, 0, NULL);
        assert(!JS_IsException(result));
        assert_realm_proto(caller, result, object_proto);
        JS_FreeValue(caller, result);
        JS_FreeValue(caller, target);
    }
    value = realm_eval(caller, "delete intlBoundRealmProbe");
    JS_FreeValue(caller, value);
    JS_FreeValue(caller, global);
    assert(JS_SetConstructorBit(caller, function, 0));
    JS_FreeValue(caller, object_ctor);
    JS_FreeValue(caller, object_proto);

    /* Leave only the retained realm edge: prototype and getter no longer own it. */
    assert(JS_SetPrototype(caller, function, JS_NULL) == 1);
    JS_FreeValue(caller, getter);
    JS_FreeValue(caller, collator);
    marker = new_realm_capture(owner, &owner_global);
    global = JS_GetGlobalObject(owner);
    assert(JS_SetPropertyStr(owner, global, "creationRealmMarker", marker) == 1);
    assert(JS_SetPropertyStr(owner, global, "boundRealmCycle",
                              JS_DupValue(owner, function)) == 1);
    JS_FreeValue(owner, global);
    JS_FreeContext(owner);
    JS_RunGC(rt);
    assert(owner_global.finalized == 0);

    /* A public data callback is invoked by the private Intl callback in its realm. */
    data[0] = new_realm_capture(caller, &coercion);
    data[1] = JS_NewObject(caller);
    assert(!JS_IsException(data[1]));
    for (i = 0; i < 3; i++) {
        input = JS_NewObject(caller);
        assert(!JS_IsException(input));
        callback = JS_NewCFunctionData(caller, intl_coercion_callback,
                                        0, i, 2, (JSValueConst *)data);
        assert(!JS_IsException(callback));
        assert(JS_SetPropertyStr(caller, input, "toString", callback) == 1);
        args[0] = input;
        args[1] = JS_NewString(caller, "b");
        assert(!JS_IsException(args[1]));
        result = JS_Call(caller, function, JS_UNDEFINED, 2,
                          (JSValueConst *)args);
        if (i == 0) {
            assert(!JS_IsException(result));
            JS_FreeValue(caller, result);
        } else {
            assert(JS_IsException(result) && JS_HasException(caller));
            error = JS_GetException(caller);
            if (i == 1) {
                /* Safe after releasing the external context ref: function retains it. */
                assert(owner_global.finalized == 0);
                error_proto = realm_eval(owner, "TypeError.prototype");
                assert_realm_proto(caller, error, error_proto);
                JS_FreeValue(caller, error_proto);
            } else {
                assert(JS_StrictEq(caller, error, data[1]));
            }
            JS_FreeValue(caller, error);
        }
        assert(!JS_HasException(caller));
        JS_FreeValue(caller, args[1]);
        JS_FreeValue(caller, input);
    }
    assert(coercion.calls == 3);
    JS_FreeValue(caller, data[0]);
    JS_FreeValue(caller, data[1]);
    assert(coercion.finalized == 1);
    JS_FreeValue(caller, function);
    JS_RunGC(rt);
    assert(owner_global.finalized == 1);
    JS_FreeContext(caller);
    JS_RunGC(rt);
    JS_FreeRuntime(rt);
}
#endif

int main(void)
{
    test_public_c_function_data_realms();
#ifdef CONFIG_INTL
    test_intl_bound_function_realms();
#endif
    return 0;
}
