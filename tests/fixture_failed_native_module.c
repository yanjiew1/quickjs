/*
 * Failed native module initializer regression fixture
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
#include "quickjs.h"

static JSClassID class_id;
static int finalized;

static void finalizer(JSRuntime *rt, JSValue value)
{
    finalized++;
}

static JSValue get_finalized(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    return JS_NewInt32(ctx, finalized);
}

static int initialize(JSContext *ctx, JSModuleDef *module)
{
    return 0;
}

JSModuleDef *js_init_module(JSContext *ctx, const char *module_name)
{
    JSClassDef def = { "FailedModuleState", .finalizer = finalizer };
    JSModuleDef *module;
    JSValue global, state, callback;

    JS_NewClassID(&class_id);
    if (JS_NewClass(JS_GetRuntime(ctx), class_id, &def) < 0)
        return NULL;
    module = JS_NewCModule(ctx, module_name, initialize);
    if (!module)
        return NULL;
    state = JS_NewObjectClass(ctx, class_id);
    if (JS_IsException(state))
        return NULL;
    JS_SetModulePrivateValue(ctx, module, state);

    global = JS_GetGlobalObject(ctx);
    if (JS_IsException(global))
        return NULL;
    callback = JS_NewCFunction(ctx, get_finalized, "failedModuleFinalized", 0);
    if (JS_IsException(callback)) {
        JS_FreeValue(ctx, global);
        return NULL;
    }
    if (JS_SetPropertyStr(ctx, global, "failedModuleFinalized", callback) < 0) {
        JS_FreeValue(ctx, global);
        return NULL;
    }
    JS_FreeValue(ctx, global);
    JS_ThrowTypeError(ctx, "initializer failed");
    return NULL;
}
