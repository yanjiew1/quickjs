/* A poll-array allocation failure must remain an embedding exception.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdlib.h>
#include "quickjs-libc.h"
#ifndef _WIN32
#include <unistd.h>
#include "../src/quickjs-libc/event-loop.h"

static int fail_allocation;
static void *poll_realloc(void *ptr, size_t size)
{
    return fail_allocation ? NULL : realloc(ptr, size);
}

/* Inject failure only at the system poll-array allocation. */
#define realloc poll_realloc
#include "../src/quickjs-libc/event-loop.c"
#undef realloc

static JSValue handler(JSContext *ctx, JSValueConst self,
                       int argc, JSValueConst *argv)
{
    return JS_UNDEFINED;
}
#endif

int main(void)
{
#ifndef _WIN32
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue args[2], value;
    int fds[2];

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx && pipe(fds) == 0);
    js_std_init_handlers(rt);
    atomic_store(&os_poll_func, js_os_poll);
    args[0] = JS_NewInt32(ctx, fds[0]);
    args[1] = JS_NewCFunction(ctx, handler, "handler", 0);
    assert(!JS_IsException(args[1]));
    value = js_os_setReadHandler(ctx, JS_UNDEFINED, 2, args, 0);
    assert(!JS_IsException(value));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, args[1]);
    fail_allocation = 1;
    assert(js_std_poll_io(ctx, 0) == -1 && JS_HasException(ctx));
    value = JS_GetException(ctx);
    JS_FreeValue(ctx, value);
    fail_allocation = 0;
    assert(js_std_poll_io(ctx, 0) == 0 && !JS_HasException(ctx));
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(close(fds[0]) == 0 && close(fds[1]) == 0);
#endif
    return 0;
}
