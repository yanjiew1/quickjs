/* External embedding contract: use public headers and callable inline helpers. */
#include <assert.h>
#include "quickjs.h"
#include "quickjs-libc.h"

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue object, value, retained;
    JSAtom atom;
    int32_t result;
    void *(*allocate)(JSContext *, size_t) = (js_malloc);
    void (*release)(JSContext *, void *) = (js_free);
    void *buffer;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    buffer = allocate(ctx, 32);
    assert(buffer);
    release(ctx, buffer);

    object = JS_NewObject(ctx);
    assert(!JS_IsException(object));
    atom = JS_NewAtom(ctx, "answer");
    assert(atom != JS_ATOM_NULL);
    assert(JS_SetProperty(ctx, object, atom, JS_NewInt32(ctx, 42)) >= 0);
    value = JS_GetProperty(ctx, object, atom);
    assert(JS_ToInt32(ctx, &result, value) == 0 && result == 42);
    JS_FreeValue(ctx, value);
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, object);

    value = JS_Eval(ctx, "6 * 7", 5, "<shared-api>", JS_EVAL_TYPE_GLOBAL);
    assert(JS_ToInt32(ctx, &result, value) == 0 && result == 42);
    JS_FreeValue(ctx, value);
    retained = JS_NewString(ctx, "runtime-owned");
    assert(!JS_IsException(retained));
    js_std_init_handlers(rt);
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeValueRT(rt, retained);
    JS_FreeRuntime(rt);
    return 0;
}
