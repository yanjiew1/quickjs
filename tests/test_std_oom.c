/* Host FILE and URL allocation-failure tests.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include "oom-allocator.h"
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

int main(void)
{
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
