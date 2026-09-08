/*
 * QuickJS Javascript Engine
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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

#ifndef QUICKJS_JS_RUNTIME_H
#define QUICKJS_JS_RUNTIME_H

#include <stdio.h>
#include "quickjs.h"
#include "quickjs-internal.h"

int init_class_range(JSRuntime *rt, JSClassShortDef const *tab,
                     int start, int count);
int JS_NewClass1(JSRuntime *rt, JSClassID class_id,
                 const JSClassDef *class_def, JSAtom name);
int JS_EnqueueJob2(JSContext *ctx, JSJobFunc *job_func,
                   int argc, JSValueConst *argv, BOOL no_exception);
JSContext *JS_NewContextRaw(JSRuntime *rt);
no_inline __exception int __js_poll_interrupts(JSContext *ctx);

typedef enum JSFreeModuleEnum {
    JS_FREE_MODULE_ALL,
    JS_FREE_MODULE_NOT_RESOLVED,
} JSFreeModuleEnum;

void js_free_modules(JSContext *ctx, JSFreeModuleEnum flag);

#endif /* QUICKJS_JS_RUNTIME_H */
