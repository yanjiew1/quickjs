/* qjsc's actual loader must preserve an attribute getter's exception.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
#include <assert.h>
#include "quickjs-libc.h"

JSModuleDef *jsc_module_loader(JSContext *ctx, const char *name, void *opaque,
                              JSValueConst attributes);
static int getter_calls;

static JSValue throwing_type(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic,
                              JSValue *data)
{
    (void)this_val;
    (void)argc;
    (void)argv;
    (void)magic;
    getter_calls++;
    return JS_Throw(ctx, JS_DupValue(ctx, data[0]));
}

int main(void)
{
    static const char *const names[] = {
        "qjsc-json-probe-must-not-read.js",
        "qjsc-json-probe-must-not-load.so",
        "std",
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    unsigned int i;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        JSValue marker = JS_NewObject(ctx);
        JSValue attributes = JS_NewObject(ctx);
        JSValue getter, exception;
        JSAtom type = JS_NewAtom(ctx, "type");

        assert(JS_IsObject(marker) && JS_IsObject(attributes) && type);
        getter = JS_NewCFunctionData(ctx, throwing_type, 0, 0, 1, &marker);
        assert(JS_IsFunction(ctx, getter));
        assert(JS_DefinePropertyGetSet(ctx, attributes, type, getter,
                    JS_UNDEFINED, JS_PROP_ENUMERABLE) == 1);
        JS_FreeAtom(ctx, type);
        getter_calls = 0;
        assert(jsc_module_loader(ctx, names[i], NULL, attributes) == NULL);
        assert(getter_calls == 1 && JS_HasException(ctx));
        exception = JS_GetException(ctx);
        assert(JS_IsObject(exception));
        assert(JS_VALUE_GET_PTR(exception) == JS_VALUE_GET_PTR(marker));
        JS_FreeValue(ctx, exception);
        assert(!JS_HasException(ctx));
        JS_FreeValue(ctx, attributes);
        JS_FreeValue(ctx, marker);
        JS_RunGC(rt);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
