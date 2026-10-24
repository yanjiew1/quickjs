/* Expose one internal coercion operation to focused JS regression tests.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../include/quickjs-libc.h"
#include "../src/quickjs/builtins/intl.h"
#include "../src/quickjs/builtins/intl/number-value.h"
#include <assert.h>
#include <string.h>

static JSValue read_value(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    QJSIntlNumberDecimal value;
    QJSIntlAllocator allocator = js_intl_allocator(JS_GetRuntime(ctx));
    JSValue result, text;
    size_t i;
    (void)this_val;
    memset(&value, 0xff, sizeof(value));
    if (js_intl_to_mathematical_value(ctx, argc ? argv[0] : JS_UNDEFINED, &value)) {
        const unsigned char *bytes = (const unsigned char *)&value;
        for (i = 0; i < sizeof(value); i++)
            assert(!bytes[i]);
        return JS_EXCEPTION;
    }
    result = JS_NewArray(ctx);
    if (JS_IsException(result))
        goto done;
    text = value.text ? JS_NewStringLen(ctx, value.text, value.value.decimal.length) : JS_UNDEFINED;
    if (JS_IsException(text)) {
        JS_FreeValue(ctx, result);
        result = JS_EXCEPTION;
        goto done;
    }
    if (JS_SetPropertyUint32(ctx, result, 0, text) < 0 ||
        JS_SetPropertyUint32(ctx, result, 1, JS_NewInt32(ctx, value.value.kind)) < 0) {
        JS_FreeValue(ctx, result);
        result = JS_EXCEPTION;
    }
done:
    qjs_intl_number_decimal_clear(&allocator, &value);
    return result;
}

static void check_allocation_failure(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    QJSIntlAllocator allocator = js_intl_allocator(rt);
    QJSIntlNumberDecimal value;
    JSValue input, exception;
    char text[8192];
    size_t i;
    memset(text, '1', sizeof(text));
    text[1] = '.';
    input = JS_NewStringLen(ctx, text, sizeof(text));
    assert(!JS_IsException(input));
    /* The exact-text allocation exceeds the small pools. */
    JS_SetMemoryLimit(rt, 0);
    memset(&value, 0xff, sizeof(value));
    assert(js_intl_to_mathematical_value(ctx, input, &value) < 0);
    for (i = 0; i < sizeof(value); i++)
        assert(!((const unsigned char *)&value)[i]);
    JS_SetMemoryLimit(rt, SIZE_MAX);
    assert(JS_HasException(ctx));
    /* Exhaustion can prevent allocation of an Error object as well. */
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    assert(!js_intl_to_mathematical_value(ctx, input, &value));
    assert(value.value.kind == QJS_INTL_FINITE && value.value.decimal.length == sizeof(text));
    qjs_intl_number_decimal_clear(&allocator, &value);
    JS_FreeValue(ctx, input);
}

int main(int argc, char **argv)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, result;
    size_t length;
    uint8_t *script;
    assert(argc == 2 && rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "readMathematicalValue",
                             JS_NewCFunction(ctx, read_value, "readMathematicalValue", 1)) >= 0);
    JS_FreeValue(ctx, global);
    script = js_load_file(ctx, &length, argv[1]);
    assert(script);
    result = JS_Eval(ctx, (const char *)script, length, argv[1], JS_EVAL_TYPE_GLOBAL);
    js_free(ctx, script);
    if (JS_IsException(result))
        js_std_dump_error(ctx);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    check_allocation_failure(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
