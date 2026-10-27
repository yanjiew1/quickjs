/* Host helper and native module allocation-failure tests.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT
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
#include "oom-allocator.h"
#include "pool-padding.h"
#include "../src/quickjs/internal/module.h"
#include "../src/quickjs-libc/module-loader.h"
#include "../src/quickjs-libc/worker.h"

static FILE *test_pipe;
static int test_pipe_closed;

static FILE *test_popen(const char *command, const char *mode)
{
    FILE *f = test_pipe;

    assert(command && *command && !strcmp(mode, "r"));
    assert(f);
    test_pipe = NULL;
    return f;
}

static int test_pclose(FILE *f)
{
    test_pipe_closed++;
    return fclose(f);
}

#undef popen
#undef pclose
#define popen test_popen
#define pclose test_pclose
#include "../src/quickjs-libc/std.c"
#undef popen
#undef pclose
#include "../src/quickjs-libc/event-loop.h"

/* Exercise the example's real private initializer and module entry point. */
#define js_init_module point_example_module
#include "../examples/point.c"
#undef js_init_module

static void check_body(JSContext *ctx, JSValueConst value)
{
    size_t len, i;
    const char *text = JS_ToCStringLen(ctx, &len, value);

    assert(text && len == 8192);
    for (i = 0; i < len; i++)
        assert(text[i] == 'd');
    JS_FreeCString(ctx, text);
}

static FILE *test_stream(int mode)
{
    FILE *f = tmpfile();
    int i;

    assert(f);
    if (mode) {
        fputs("HTTP/1.1 200 OK\r\nX-Test: ", f);
        for (i = 0; i < 4096; i++)
            fputc('h', f);
        if (mode != 3)
            fputs("\r\n\r\n", f);
    }
    if (mode != 3) {
        for (i = 0; i < 8192; i++)
            fputc('d', f);
    }
    assert(!ferror(f));
    rewind(f);
    return f;
}

static void test_failure(int mode)
{
    size_t fail_at, allocations = 0;

    for (fail_at = 0; fail_at <= allocations; fail_at++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue receiver = JS_UNDEFINED, args[2], result, body, status;
        JSValueConst call_args[2];
        char url[2048];
        FILE *f = test_stream(mode);

        assert(ctx);
        test_pipe_closed = 0;
        if (!mode) {
            JS_NewClassID(&js_std_file_class_id);
            assert(JS_NewClass(rt, js_std_file_class_id, &js_std_file_class) == 0);
            JS_SetClassProto(ctx, js_std_file_class_id, JS_NewObject(ctx));
            receiver = js_new_std_file(ctx, f, TRUE, FALSE);
            assert(!JS_IsException(receiver));
            args[0] = args[1] = JS_UNDEFINED;
        } else {
            memset(url, 'u', sizeof(url));
            args[0] = JS_NewStringLen(ctx, url, sizeof(url));
            args[1] = JS_NewObject(ctx);
            assert(!JS_IsException(args[0]) && !JS_IsException(args[1]));
            if (mode >= 2)
                assert(JS_SetPropertyStr(ctx, args[1], "full", JS_TRUE) == 1);
            test_pipe = f;
        }
        call_args[0] = args[0];
        call_args[1] = args[1];
        oom_test_begin(&allocator, fail_at);
        result = !mode ? js_std_file_readAsString(ctx, receiver, 0, NULL) :
                        js_std_urlGet(ctx, JS_UNDEFINED, 2, call_args);
        oom_test_end(&allocator);
        if (!fail_at) {
            allocations = allocator.calls;
            assert(allocations > 0 && allocations < 128);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(result)) {
            assert(fail_at);
            oom_test_check_exception(ctx);
        } else {
            assert(!JS_HasException(ctx));
            if (mode >= 2) {
                body = JS_GetPropertyStr(ctx, result, "response");
                if (mode == 3) {
                    assert(JS_IsNull(body));
                } else {
                    check_body(ctx, body);
                    status = JS_GetPropertyStr(ctx, result, "status");
                    assert(JS_StrictEq(ctx, status, JS_NewInt32(ctx, 200)));
                    JS_FreeValue(ctx, status);
                }
                JS_FreeValue(ctx, body);
            } else {
                check_body(ctx, result);
            }
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, receiver);
        if (mode) {
            if (test_pipe) {
                assert(test_pipe_closed == 0);
                fclose(test_pipe);
                test_pipe = NULL;
            } else {
                assert(test_pipe_closed == 1);
            }
        }
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
}

static void test_class_registration_failure(int worker)
{
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSAtom atom;
    int ret;

    assert(ctx);
    js_std_init_handlers(rt);
    atom = JS_NewAtom(ctx, worker ? "Worker" : "FILE");
    assert(atom != JS_ATOM_NULL);
    oom_test_begin(&allocator, 1);
#ifdef USE_WORKER
    if (worker)
        ret = js_init_worker(ctx, NULL);
    else
#endif
        ret = js_std_init(ctx, NULL);
    oom_test_end(&allocator);
    assert(allocator.failures == 1 && ret == -1);
    oom_test_check_exception(ctx);
    JS_FreeAtom(ctx, atom);
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(allocator.live == 0);
}

static void test_sleep_timer_deadline(void)
{
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSThreadState *ts;
    JSOSTimer *timer;
    JSValue delay, promise;

    assert(ctx);
    js_std_init_handlers(rt);
    ts = JS_GetRuntimeOpaque(rt);
    delay = JS_NewFloat64(ctx, 9223372036854774784.0);
    promise = js_os_sleepAsync(ctx, JS_UNDEFINED, 1, (JSValueConst *)&delay);
    assert(!JS_IsException(promise));
    assert(!list_empty(&ts->os_timers));
    timer = list_entry(ts->os_timers.next, JSOSTimer, link);
    assert(timer->timeout >= INT64_MAX - 1023);
    JS_FreeValue(ctx, promise);
    /* Teardown cancels the enormous deadline; never poll or await it. */
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(allocator.live == 0);
}

static JSClassID json_value_class_id;
static int json_value_finalized;

static void json_value_finalizer(JSRuntime *rt, JSValue value)
{
    (void)rt;
    (void)value;
    json_value_finalized++;
}

static void test_json_export_failure(int attributed)
{
    static const JSClassDef definition = {
        "JSONModuleValue", .finalizer = json_value_finalizer,
    };
    static const char name[] = "json-allocation-failure";
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSModuleDef *module;
    JSAtom atom;
    JSValue value, attributes;
    void *padding;

    assert(ctx);
    JS_NewClassID(&json_value_class_id);
    assert(JS_NewClass(rt, json_value_class_id, &definition) == 0);
    value = JS_NewObjectClass(ctx, json_value_class_id);
    attributes = attributed ? JS_NewObject(ctx) : JS_UNDEFINED;
    assert(!JS_IsException(value) && !JS_IsException(attributes));
    /* Retain the name so module creation allocates only its record; the next
       host allocation must grow its export table, even with pooled storage. */
    atom = JS_NewAtom(ctx, name);
    assert(atom != JS_ATOM_NULL && list_empty(&ctx->loaded_modules));
    padding = test_pool_padding(rt);
    json_value_finalized = 0;
    oom_test_begin(&allocator, 2);
    module = attributed ? create_json_module2(ctx, name, value, attributes) :
                          create_json_module(ctx, name, value);
    oom_test_end(&allocator);
    assert(allocator.failures == 1 && module == NULL);
    assert(json_value_finalized == 1);
    oom_test_check_exception(ctx);

    /* A created module belongs to the context even when export setup fails.
       It must not acquire the consumed value or be manually freed twice. */
    assert(!list_empty(&ctx->loaded_modules));
    module = list_entry(ctx->loaded_modules.next, JSModuleDef, link);
    assert(module->export_entries_count == 0);
    assert(JS_IsUndefined(module->private_value));
    assert(JS_StrictEq(ctx, module->attributes, attributes));
    test_pool_padding_free(rt, padding);
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, attributes);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(allocator.live == 0);
}

static void test_point_export_failure(void)
{
    static const char name[] = "point-export-failure";
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSModuleDef *module;
    JSAtom atom;
    void *padding;

    assert(ctx);
    atom = JS_NewAtom(ctx, name);
    assert(atom != JS_ATOM_NULL);
    padding = test_pool_padding(rt);
    oom_test_begin(&allocator, 2);
    module = point_example_module(ctx, name);
    oom_test_end(&allocator);
    assert(allocator.failures == 1 && module == NULL);
    oom_test_check_exception(ctx);
    test_pool_padding_free(rt, padding);
    JS_FreeAtom(ctx, atom);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(allocator.live == 0);
}

static OOMTestAllocator *point_allocator;
static size_t point_fail_at;
static int point_init_result;

static int point_init_with_failure(JSContext *ctx, JSModuleDef *module)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    void *padding = test_pool_padding(rt);

    oom_test_begin(point_allocator, point_fail_at);
    point_init_result = js_point_init(ctx, module);
    oom_test_end(point_allocator);
    assert((point_init_result < 0) == !!JS_HasException(ctx));
    test_pool_padding_free(rt, padding);
    return point_init_result;
}

static void test_point_initialization_failures(void)
{
    static const char source[] =
        "import { Point } from 'point-initialization';"
        "if (new Point(3, 4).norm() !== 5) throw Error('Point initialization');";
    size_t step, calls = 0, failures = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSModuleDef *module;
        JSValue result, error;

        assert(ctx);
        module = point_example_module(ctx, "point-initialization");
        assert(module);
        /* Keep parsing/linking outside the injection window. */
        module->init_func = point_init_with_failure;
        point_allocator = &allocator;
        point_fail_at = step;
        result = JS_Eval(ctx, source, sizeof(source) - 1, "point-oom.js",
                         JS_EVAL_TYPE_MODULE);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 128 && point_init_result == 0);
            /* Class registration is per-runtime, prototypes are per-realm. */
            {
                JSContext *other = JS_NewContext(rt);
                JSValue imported;

                assert(other && point_example_module(other, "point-initialization"));
                imported = JS_Eval(other, source, sizeof(source) - 1,
                                   "point-other.js", JS_EVAL_TYPE_MODULE);
                assert(!JS_IsException(imported) && !JS_HasException(other));
                assert(JS_PromiseState(other, imported) == JS_PROMISE_FULFILLED);
                JS_FreeValue(other, imported);
                JS_FreeContext(other);
            }
        } else {
            assert(allocator.failures == 1);
        }
        if (point_init_result < 0) {
            failures++;
            if (JS_IsException(result)) {
                oom_test_check_exception(ctx);
            } else {
                assert(JS_PromiseState(ctx, result) == JS_PROMISE_REJECTED);
                error = JS_PromiseResult(ctx, result);
                assert(JS_IsError(ctx, error));
                JS_FreeValue(ctx, error);
            }
        } else {
            assert(!JS_IsException(result) && !JS_HasException(ctx));
            assert(JS_PromiseState(ctx, result) == JS_PROMISE_FULFILLED);
        }
        JS_FreeValue(ctx, result);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(allocator.live == 0);
    }
    assert(failures);
}

static void test_import_meta_failures(void)
{
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue module, meta;
        void *padding;
        int status;

        assert(ctx);
        module = JS_Eval(ctx, "", 0, "meta-oom.js",
                         JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        assert(!JS_IsException(module));
        meta = JS_GetImportMeta(ctx, JS_VALUE_GET_PTR(module));
        assert(!JS_IsException(meta));
        JS_FreeValue(ctx, meta);
        /* Reach the allocator even when PATH_MAX keeps the URL pool-sized. */
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        status = js_module_set_import_meta(ctx, module, FALSE, TRUE);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 32 && status == 0 && !JS_HasException(ctx));
        } else {
            assert(allocator.failures == 1 && status < 0);
            oom_test_check_exception(ctx);
        }
        test_pool_padding_free(rt, padding);
        JS_FreeValue(ctx, module);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

int main(void)
{
    test_import_meta_failures();
    test_point_export_failure();
    test_point_initialization_failures();
    test_json_export_failure(0);
    test_json_export_failure(1);
    test_sleep_timer_deadline();
    test_class_registration_failure(0);
#ifdef USE_WORKER
    test_class_registration_failure(1);
#endif
    test_failure(0);
    test_failure(1);
    test_failure(2);
    test_failure(3);
    return 0;
}
