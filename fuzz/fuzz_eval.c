/*
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

/* Copyright 2020 Google Inc.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

#include "quickjs.h"
#include "quickjs-libc.h"
#include "fuzz/fuzz_common.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0 || size == SIZE_MAX)
        return 0;

    JSRuntime *rt = JS_NewRuntime();
    if (!rt)
        return 0;
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        JS_FreeRuntime(rt);
        return 0;
    }
    uint8_t *null_terminated_data = malloc(size + 1);
    if (!null_terminated_data) {
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return 0;
    }
    test_one_input_init(rt, ctx);
    memcpy(null_terminated_data, data, size);
    null_terminated_data[size] = 0;

    reset_nbinterrupts();
    //the final 0 does not count (as in strlen)
    JSValue val = JS_Eval(ctx, (const char *)null_terminated_data, size, "<none>", JS_EVAL_TYPE_GLOBAL);
    free(null_terminated_data);
    //TODO targets with JS_ParseJSON, JS_ReadObject
    if (!JS_IsException(val)) {
        test_one_input_jobs(ctx);
        JS_FreeValue(ctx, val);
    }
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
