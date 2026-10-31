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

#ifndef _WIN32
static int os_pipe_fds[2], os_pipe_closes;

static int test_os_pipe(int fds[2])
{
    int ret = pipe(fds);

    assert(ret == 0);
    memcpy(os_pipe_fds, fds, sizeof(os_pipe_fds));
    return ret;
}

static int test_os_close(int fd)
{
    if (fd == os_pipe_fds[0] || fd == os_pipe_fds[1])
        os_pipe_closes++;
    return close(fd);
}

#define pipe test_os_pipe
#define close test_os_close
#endif
#include "../src/quickjs-libc/os.c"
#ifndef _WIN32
#undef pipe
#undef close
#endif

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

static void test_file_wrapper_failure(int owned, int is_popen)
{
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue result;
        FILE *f = tmpfile();
        void *padding;
        int fd;

        assert(ctx && f);
        fd = fileno(f);
        JS_NewClassID(&js_std_file_class_id);
        assert(JS_NewClass(rt, js_std_file_class_id, &js_std_file_class) == 0);
        JS_SetClassProto(ctx, js_std_file_class_id, JS_NewObject(ctx));
        padding = test_pool_padding(rt);
        test_pipe_closed = 0;
        oom_test_begin(&allocator, step);
        result = js_new_std_file(ctx, f, owned, is_popen, JS_UNDEFINED);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 16);
        } else {
            assert(allocator.failures == 1 && JS_IsException(result));
            oom_test_check_exception(ctx);
        }
        JS_FreeValue(ctx, result);
        if (owned) {
            /* Both allocation failure and the successful wrapper consume f. */
            assert(dup(fd) < 0 && errno == EBADF);
            assert(test_pipe_closed == is_popen);
        } else {
            assert(fputc('x', f) == 'x');
            assert(fclose(f) == 0);
        }
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

static void test_environ_failure(void)
{
    static const char prefix[] = "QJS_ENV_TEST=";
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue result, value;
        char entry[1024];
        char malformed[] = "NO_EQUALS";
        char *entries[] = { malformed, entry, NULL };
        char **saved;
        void *padding;

        assert(ctx);
        memcpy(entry, prefix, sizeof(prefix) - 1);
        memset(entry + sizeof(prefix) - 1, 'v', sizeof(entry) - sizeof(prefix));
        entry[sizeof(entry) - 1] = '\0';
        padding = test_pool_padding(rt);
        /* Replace only the list read by this synchronous call, then restore
           it before inspecting results or running any other host operation. */
        saved = environ;
        environ = entries;
        oom_test_begin(&allocator, step);
        result = js_std_getenviron(ctx, JS_UNDEFINED, 0, NULL);
        oom_test_end(&allocator);
        environ = saved;
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 32);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(result)) {
            assert(step);
            oom_test_check_exception(ctx);
        } else {
            const char *text;

            assert(!JS_HasException(ctx));
            value = JS_GetPropertyStr(ctx, result, "QJS_ENV_TEST");
            text = JS_ToCString(ctx, value);
            assert(text && !strcmp(text, entry + sizeof(prefix) - 1));
            JS_FreeCString(ctx, text);
            JS_FreeValue(ctx, value);
            value = JS_GetPropertyStr(ctx, result, "NO_EQUALS");
            assert(JS_IsUndefined(value));
            JS_FreeValue(ctx, value);
        }
        JS_FreeValue(ctx, result);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

static void test_module_loader_file_oom(void)
{
    static const char filename[] = "module-loader-oom.js";
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSModuleDef *module;
    JSValue error, name;
    const char *text;
    void *padding;
    FILE *f = fopen(filename, "wb");

    assert(ctx && f);
    assert(fputs("export const value = 1;\n", f) >= 0);
    assert(fclose(f) == 0);
    padding = test_pool_padding(rt);
    oom_test_begin(&allocator, 1);
    module = js_module_loader(ctx, filename, NULL, JS_UNDEFINED);
    oom_test_end(&allocator);
    assert(!module && allocator.failures == 1 && JS_HasException(ctx));
    error = JS_GetException(ctx);
    name = JS_GetPropertyStr(ctx, error, "name");
    text = JS_ToCString(ctx, name);
    assert(text && !strcmp(text, "InternalError"));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, name);
    JS_FreeValue(ctx, error);
    test_pool_padding_free(rt, padding);

    module = js_module_loader(ctx, "missing-module-loader-oom.js", NULL,
                              JS_UNDEFINED);
    assert(!module && JS_HasException(ctx));
    error = JS_GetException(ctx);
    name = JS_GetPropertyStr(ctx, error, "name");
    text = JS_ToCString(ctx, name);
    assert(text && !strcmp(text, "ReferenceError"));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, name);
    JS_FreeValue(ctx, error);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(!allocator.live);
    assert(remove(filename) == 0);
}

static void test_load_file_failure(void)
{
    static const char filename[] = "std-load-oom.tmp";
    size_t step, calls = 0;
    FILE *f = fopen(filename, "wb");
    int i;

    assert(f);
    for (i = 0; i < 8192; i++)
        assert(fputc('d', f) == 'd');
    assert(fclose(f) == 0);

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue input, result;
        void *padding;

        assert(ctx);
        input = JS_NewString(ctx, filename);
        assert(!JS_IsException(input));
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        result = js_std_loadFile(ctx, JS_UNDEFINED, 1, (JSValueConst *)&input);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 16);
        } else {
            assert(allocator.failures == 1 && JS_IsException(result));
            oom_test_check_exception(ctx);
        }
        if (!JS_IsException(result)) {
            assert(!JS_HasException(ctx));
            check_body(ctx, result);
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, input);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
    assert(remove(filename) == 0);
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
            receiver = js_new_std_file(ctx, f, TRUE, FALSE, JS_UNDEFINED);
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

static void test_host_module_export_failures(int is_os)
{
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSModuleDef *module;
        void *padding;

        assert(ctx);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        module = is_os ? js_init_module_os(ctx, "os-exports-oom") :
                         js_init_module_std(ctx, "std-exports-oom");
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(module && calls > 2 && calls < 128);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_HasException(ctx)) {
            assert(step && module == NULL);
            oom_test_check_exception(ctx);
        } else {
            assert(module);
        }
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
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

static JSValue format_later_to_string(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    int *calls = JS_GetContextOpaque(ctx);

    (*calls)++;
    return JS_NewString(ctx, "later");
}

static void test_format_stops_after_oom(int mode)
{
    static const char *const formats[] = { "prefix %s", "%%%s", "%c%s" };
    OOMTestAllocator allocator;
    JSRuntime *rt = oom_test_runtime(&allocator);
    JSContext *ctx = JS_NewContext(rt);
    JSValue args[3], later, result;
    void *padding;
    int calls = 0, argc;

    assert(ctx);
    JS_SetContextOpaque(ctx, &calls);
    later = JS_NewObject(ctx);
    assert(!JS_IsException(later));
    assert(JS_SetPropertyStr(ctx, later, "toString",
           JS_NewCFunction(ctx, format_later_to_string, "toString", 0)) >= 0);
    args[0] = JS_NewString(ctx, formats[mode]);
    argc = mode == 2 ? 3 : 2;
    if (mode == 2) {
        args[1] = JS_NewInt32(ctx, 'A');
        args[2] = later;
    } else {
        args[1] = later;
    }
    assert(!JS_IsException(args[0]));

    padding = test_pool_padding(rt);
    oom_test_begin(&allocator, 1);
    result = js_printf_internal(ctx, argc, (JSValueConst *)args, NULL);
    oom_test_end(&allocator);
    assert(allocator.failures == 1 && JS_IsException(result));
    assert(calls == 0);
    oom_test_check_exception(ctx);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, later);
    test_pool_padding_free(rt, padding);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(!allocator.live);
}

#ifndef _WIN32
static void test_negative_utimes_conversion(void)
{
    struct timeval value;

    ms_to_timeval(&value, -1000);
    assert(value.tv_sec == -1 && value.tv_usec == 0);
    ms_to_timeval(&value, -1001);
    assert(value.tv_sec == -2 && value.tv_usec == 999000);
}
#endif

/* Results own their payloads; pipe descriptors transfer only on success. */
static void test_os_result_oom(int mode)
{
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue input = JS_UNDEFINED, result, value;
        void *padding;
        char payload[1024];

        assert(ctx);
        if (!mode) {
            memset(payload, 'x', sizeof(payload));
            input = JS_NewStringLen(ctx, payload, sizeof(payload));
        } else if (mode >= 2) {
            input = JS_NewString(ctx, ".");
        }
        assert(!JS_IsException(input));
        padding = test_pool_padding(rt);
#ifndef _WIN32
        os_pipe_fds[0] = os_pipe_fds[1] = -1;
        os_pipe_closes = 0;
#endif
        oom_test_begin(&allocator, step);
        if (!mode) {
            result = js_os_pair(ctx, input, 0);
            input = JS_UNDEFINED;
        } else if (mode == 2) {
            result = js_os_readdir(ctx, JS_UNDEFINED, 1, (JSValueConst *)&input);
        } else if (mode == 4) {
            result = js_os_stat(ctx, JS_UNDEFINED, 1, (JSValueConst *)&input, 0);
        } else if (mode == 3) {
#ifndef _WIN32
            JSValueConst args[2] = { JS_NewInt32(ctx, INT32_MAX),
                                    JS_NewInt32(ctx, WNOHANG) };

            result = js_os_waitpid(ctx, JS_UNDEFINED, 2, args);
#else
            abort();
#endif
        } else {
#ifndef _WIN32
            result = js_os_pipe(ctx, JS_UNDEFINED, 0, NULL);
#else
            abort();
#endif
        }
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 256);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_IsException(result)) {
            assert(step);
            oom_test_check_exception(ctx);
#ifndef _WIN32
            if (mode == 1) {
                assert(os_pipe_closes == 2);
                assert(fcntl(os_pipe_fds[0], F_GETFD) < 0 && errno == EBADF);
                assert(fcntl(os_pipe_fds[1], F_GETFD) < 0 && errno == EBADF);
            }
#endif
        } else {
            assert(!JS_HasException(ctx));
            value = JS_GetPropertyUint32(ctx, result, 0);
            assert(!JS_IsException(value));
            if (!mode)
                assert(JS_IsString(value));
            else if (mode == 2)
                assert(JS_IsArray(ctx, value));
            else if (mode == 4) {
                JSValue field = JS_GetPropertyStr(ctx, value, "mode");

                assert(JS_IsObject(value) && JS_IsNumber(field));
                JS_FreeValue(ctx, field);
            } else if (mode == 3) {
                int32_t pid;

                assert(JS_ToInt32(ctx, &pid, value) == 0 && pid < 0);
            }
            JS_FreeValue(ctx, value);
#ifndef _WIN32
            if (mode == 1) {
                assert(os_pipe_closes == 0);
                assert(close(os_pipe_fds[0]) == 0);
                assert(close(os_pipe_fds[1]) == 0);
            }
#endif
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, input);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

static void test_helper_initialization_failures(void)
{
    char arg0[] = "helper-test", arg1[] = "argument";
    char *argv[] = { arg0, arg1 };
    size_t step, calls = 0;

    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContext(rt);
        JSValue global, value, member;
        void *padding;
        int i;
        const char *objects[] = { "console", "performance", "scriptArgs" };
        const char *members[] = { "log", "now" };
        const char *functions[] = { "print", "__loadScript" };

        assert(ctx);
        padding = test_pool_padding(rt);
        oom_test_begin(&allocator, step);
        js_std_add_helpers(ctx, 2, argv);
        oom_test_end(&allocator);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 256);
        } else {
            assert(allocator.failures == 1);
        }
        if (JS_HasException(ctx))
            oom_test_check_exception(ctx);
        global = JS_GetGlobalObject(ctx);
        /* Earlier helpers may remain installed after failure, but each
           published helper must contain only completed JavaScript values. */
        for (i = 0; i < 3; i++) {
            value = JS_GetPropertyStr(ctx, global, objects[i]);
            if (!JS_IsUndefined(value)) {
                assert(JS_IsObject(value));
                if (i < 2) {
                    member = JS_GetPropertyStr(ctx, value, members[i]);
                    assert(JS_IsFunction(ctx, member));
                    JS_FreeValue(ctx, member);
                } else {
                    int j;

                    for (j = 0; j < 2; j++) {
                        member = JS_GetPropertyUint32(ctx, value, j);
                        assert(JS_IsString(member));
                        JS_FreeValue(ctx, member);
                    }
                }
            }
            JS_FreeValue(ctx, value);
        }
        for (i = 0; i < 2; i++) {
            value = JS_GetPropertyStr(ctx, global, functions[i]);
            assert(JS_IsUndefined(value) || JS_IsFunction(ctx, value));
            JS_FreeValue(ctx, value);
        }
        assert(!JS_HasException(ctx));
        JS_FreeValue(ctx, global);
        test_pool_padding_free(rt, padding);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
}

int main(void)
{
    test_helper_initialization_failures();
    test_file_wrapper_failure(TRUE, FALSE);
    test_file_wrapper_failure(TRUE, TRUE);
    test_file_wrapper_failure(FALSE, FALSE);
    test_environ_failure();
    test_module_loader_file_oom();
    test_load_file_failure();
    test_os_result_oom(0);
#ifndef _WIN32
    test_os_result_oom(1);
    test_os_result_oom(3);
#endif
    test_os_result_oom(2);
    test_os_result_oom(4);
    test_import_meta_failures();
    test_format_stops_after_oom(0);
    test_format_stops_after_oom(1);
    test_format_stops_after_oom(2);
#ifndef _WIN32
    test_negative_utimes_conversion();
#endif
    test_point_export_failure();
    test_host_module_export_failures(0);
    test_host_module_export_failures(1);
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
