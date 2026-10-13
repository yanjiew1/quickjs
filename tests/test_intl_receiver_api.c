/* Receiver-preserving native constructor call/construct contract. */
#include "quickjs.h"
#include <stdio.h>
#include <string.h>

static JSValue receiver_callback(JSContext *ctx, JSValueConst receiver,
                                  JSValueConst new_target, int argc,
                                  JSValueConst *argv)
{
    JSValue result = JS_NewObject(ctx);
    if (JS_IsException(result))
        return result;
    if (JS_DefinePropertyValueStr(ctx, result, "receiver", JS_DupValue(ctx, receiver),
                                  JS_PROP_C_W_E) < 0 ||
        JS_DefinePropertyValueStr(ctx, result, "newTarget", JS_DupValue(ctx, new_target),
                                  JS_PROP_C_W_E) < 0 ||
        JS_DefinePropertyValueStr(ctx, result, "argument",
                                  argc ? JS_DupValue(ctx, argv[0]) : JS_UNDEFINED,
                                  JS_PROP_C_W_E) < 0) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    return result;
}

int main(void)
{
    static const char script[] =
        "let marker={}; let r=F.call(marker,23);"
        "if(r.receiver!==marker||r.newTarget!==undefined||r.argument!==23)"
        "throw Error('call receiver');"
        "r=F.call(null);if(r.receiver!==null||r.newTarget!==undefined)"
        "throw Error('null receiver');"
        "r=new F(41);if(r.receiver!==undefined||r.newTarget!==F||r.argument!==41)"
        "throw Error('new target');"
        "function Other(){};r=Reflect.construct(F,[59],Other);"
        "if(r.receiver!==undefined||r.newTarget!==Other||r.argument!==59)"
        "throw Error('Reflect.construct target');";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = NULL;
    JSValue function, global, result;
    JSCFunctionType pointer;
    int rc = 1;
    if (!rt)
        return 1;
    ctx = JS_NewContext(rt);
    if (!ctx)
        goto done;
    pointer.constructor_or_func_receiver = receiver_callback;
    function = JS_NewCFunction2(ctx, pointer.generic, "F", 0,
                                JS_CFUNC_constructor_or_func_receiver, 0);
    if (JS_IsException(function))
        goto exception;
    global = JS_GetGlobalObject(ctx);
    if (JS_DefinePropertyValueStr(ctx, global, "F", function, JS_PROP_C_W_E) < 0) {
        JS_FreeValue(ctx, global);
        goto exception;
    }
    JS_FreeValue(ctx, global);
    result = JS_Eval(ctx, script, strlen(script), "receiver-api", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result))
        goto exception;
    JS_FreeValue(ctx, result);
    rc = 0;
    goto done;
 exception:
    result = JS_GetException(ctx);
    {
        const char *message = JS_ToCString(ctx, result);
        if (message) {
            fprintf(stderr, "%s\n", message);
            JS_FreeCString(ctx, message);
        }
    }
    JS_FreeValue(ctx, result);
 done:
    if (ctx)
        JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return rc;
}
