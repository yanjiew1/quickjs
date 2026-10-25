/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

/* External embedding contract: use public headers and callable inline helpers. */
#include <assert.h>
#include "quickjs.h"
#include "quickjs-libc.h"

static int interrupt_noop(JSRuntime *rt, void *opaque)
{
    return 0;
}

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
    void *opaque = rt;

    assert(rt);
    assert(JS_GetInterruptHandler(rt, &opaque) == NULL && opaque == NULL);
    JS_SetInterruptHandler(rt, interrupt_noop, rt);
    assert(JS_GetInterruptHandler(rt, NULL) == interrupt_noop);
    assert(JS_GetInterruptHandler(rt, &opaque) == interrupt_noop && opaque == rt);
    JS_SetInterruptHandler(rt, NULL, rt);
    assert(JS_GetInterruptHandler(rt, &opaque) == NULL && opaque == rt);
    JS_SetInterruptHandler(rt, NULL, NULL);
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
