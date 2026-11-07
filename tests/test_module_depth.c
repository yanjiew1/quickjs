/*
 * QuickJS module depth tests
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#include "../src/quickjs/internal/module.h"

#define MODULE_DEPTH 512

typedef struct DepthLoaderState {
    JSModuleDef *modules[MODULE_DEPTH + 1];
    int indirect;
} DepthLoaderState;

static JSModuleDef *depth_loader(JSContext *ctx, const char *name, void *opaque)
{
    DepthLoaderState *state = opaque;
    char source[80];
    char *end;
    long index = strtol(name + 1, &end, 10);
    JSValue compiled;
    JSModuleDef *module;

    assert(name[0] == 'm' && *end == '\0' && index >= 0 && index <= MODULE_DEPTH);
    if (index == MODULE_DEPTH)
        snprintf(source, sizeof(source), "export const value = 1;");
    else if (state->indirect)
        snprintf(source, sizeof(source), "export { value } from 'm%ld';", index + 1);
    else
        snprintf(source, sizeof(source), "export * from 'm%ld';", index + 1);
    compiled = JS_Eval(ctx, source, strlen(source), name,
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(compiled))
        return NULL;
    module = JS_VALUE_GET_PTR(compiled);
    state->modules[index] = module;
    JS_FreeValue(ctx, compiled);
    return module;
}

static void check_overflow(JSContext *ctx)
{
    JSValue error;

    assert(JS_HasException(ctx));
    error = JS_GetException(ctx);
    assert(JS_IsError(ctx, error));
    JS_FreeValue(ctx, error);
}

static void test_chain(int indirect)
{
    const char *source = indirect ? "export { value } from 'm0';" :
                                    "export * from 'm0';";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JSValue compiled, ns;
    JSModuleDef *module;
    DepthLoaderState state = { .indirect = indirect };
    int i;

    assert(rt && ctx);
    JS_UpdateStackTop(rt);
    JS_SetModuleLoaderFunc(rt, NULL, depth_loader, &state);
    compiled = JS_Eval(ctx, source, strlen(source), "deep-root",
                       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    module = JS_VALUE_GET_PTR(compiled);
    /* Compilation resolves imports. Reset only traversal links to exercise
       the standalone resolver with the already compiled module graph. */
    module->resolved = FALSE;
    module->req_module_entries[0].module = NULL;
    for (i = 0; i <= MODULE_DEPTH; i++) {
        assert(state.modules[i]);
        state.modules[i]->resolved = FALSE;
        if (i < MODULE_DEPTH)
            state.modules[i]->req_module_entries[0].module = NULL;
    }

    JS_SetMaxStackSize(rt, 16 * 1024);
    assert(js_resolve_module(ctx, module) < 0);
    check_overflow(ctx);
    JS_SetMaxStackSize(rt, 4 * 1024 * 1024);
    assert(js_resolve_module(ctx, module) == 0);

    JS_SetMaxStackSize(rt, 16 * 1024);
    assert(js_create_module_function(ctx, module) < 0);
    check_overflow(ctx);
    JS_SetMaxStackSize(rt, 4 * 1024 * 1024);
    assert(js_create_module_function(ctx, module) == 0);
    assert(JS_LinkModule(ctx, compiled) == 0);

    JS_SetMaxStackSize(rt, 4 * 1024);
    ns = JS_GetModuleNamespace(ctx, module);
    /* A compiler can use less native stack for this walk. Both completing
       within the limit and reporting overflow are valid outcomes. */
    if (JS_IsException(ns)) {
        check_overflow(ctx);
        JS_SetMaxStackSize(rt, 4 * 1024 * 1024);
        ns = JS_GetModuleNamespace(ctx, module);
    }
    assert(!JS_IsException(ns));
    JS_FreeValue(ctx, ns);
    JS_FreeValue(ctx, compiled);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
    test_chain(0);
    test_chain(1);
    return 0;
}
