/*
 * Native Worker NULL-context factory ownership regression.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
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
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../src/quickjs/internal/allocator.h"
#include "../src/quickjs-libc/worker.h"

#ifdef USE_WORKER
static JSRuntime *test_worker_rt;
static size_t test_live;
static int test_failure_mode; /* 0: context, 1: runtime, 2: host setup */
static int test_runtime_factory_calls;
static int test_factory_calls, test_handlers_created, test_handlers_freed;
static int test_runtimes_freed, test_contexts_freed;
static int test_helpers_called, test_modules_called, test_loops_called;
static uintptr_t test_args_address, test_filename_address;
static uintptr_t test_basename_address, test_recv_address, test_send_address;
static JSWorkerMessagePipe *test_recv_pipe, *test_send_pipe;
static int test_args_freed, test_filename_freed, test_basename_freed;
static int test_recv_freed, test_send_freed;
static uintptr_t test_queued_message_address;
static int test_queued_message_freed;
static pthread_t test_thread;
static int test_thread_created, test_completed;
static pthread_mutex_t test_completion_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_completion_cond = PTHREAD_COND_INITIALIZER;
static void *(*test_worker_entry)(void *);
static size_t test_message_fail_at, test_message_calls;
static int test_pthread_failure, test_reject_thread_creation;
static int test_unexpected_thread_create;
#ifndef _WIN32
static int test_waker_checks, test_waker_calls;
static ssize_t test_waker_write(int fd, const void *buffer, size_t size);
#endif

static JSRuntime *test_new_runtime(void);
static int test_init_handlers(JSRuntime *rt);
static void test_free_handlers(JSRuntime *rt);
static void test_free_runtime(JSRuntime *rt);
static void test_free_context(JSContext *ctx);
static void test_add_helpers(JSContext *ctx, int argc, char **argv);
static JSValue test_load_module(JSContext *ctx, const char *basename,
                               const char *filename);
static void test_loop(JSContext *ctx);
static void test_free(void *ptr);
static void *test_message_malloc(size_t size);
static int test_create_thread(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*entry)(void *), void *argument);
static int test_mutex_init(pthread_mutex_t *mutex,
                           const pthread_mutexattr_t *attributes);
static int test_attr_init(pthread_attr_t *attributes);
static int test_attr_setdetachstate(pthread_attr_t *attributes, int state);

/* Headers precede redirection. Compile the actual Worker implementation;
   its exported definitions also replace the archive's normal worker object. */
#define JS_NewRuntime test_new_runtime
#define js_std_init_handlers_checked test_init_handlers
#define js_std_free_handlers test_free_handlers
#define JS_FreeRuntime test_free_runtime
#define JS_FreeContext test_free_context
#define js_std_add_helpers test_add_helpers
#define JS_LoadModule test_load_module
#define js_std_loop test_loop
#define free test_free
#define malloc test_message_malloc
#ifndef _WIN32
#define write test_waker_write
#endif
#define pthread_create test_create_thread
#define pthread_mutex_init test_mutex_init
#define pthread_attr_init test_attr_init
#define pthread_attr_setdetachstate test_attr_setdetachstate
#include "../src/quickjs-libc/worker.c"
#undef JS_NewRuntime
#undef js_std_init_handlers_checked
#undef js_std_free_handlers
#undef JS_FreeRuntime
#undef JS_FreeContext
#undef js_std_add_helpers
#undef JS_LoadModule
#undef js_std_loop
#undef free
#undef malloc
#ifndef _WIN32
#undef write
#endif
#undef pthread_create
#undef pthread_mutex_init
#undef pthread_attr_init
#undef pthread_attr_setdetachstate

static int test_mutex_init(pthread_mutex_t *mutex,
                           const pthread_mutexattr_t *attributes)
{
    if (test_pthread_failure == 1) {
        test_pthread_failure = 0;
        return EAGAIN;
    }
    return pthread_mutex_init(mutex, attributes);
}

static int test_attr_init(pthread_attr_t *attributes)
{
    if (test_pthread_failure == 2) {
        test_pthread_failure = 0;
        return EAGAIN;
    }
    return pthread_attr_init(attributes);
}

static int test_attr_setdetachstate(pthread_attr_t *attributes, int state)
{
    if (test_pthread_failure == 3) {
        test_pthread_failure = 0;
        return EINVAL;
    }
    return pthread_attr_setdetachstate(attributes, state);
}

static void test_pthread_setup_failures(void)
{
    static const char source[] =
        "import { Worker } from 'os';"
        "try { new Worker('unused.js'); throw Error('worker started'); }"
        "catch (error) { if (!(error instanceof TypeError)) throw error; }";
    int mode;

    test_pthread_failure = 1;
    assert(!js_new_message_pipe());
    assert(!test_pthread_failure);
    for (mode = 2; mode <= 3; mode++) {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        JSValue result;

        assert(rt && ctx);
        js_std_init_handlers(rt);
        assert(js_init_module_os(ctx, "os"));
        test_pthread_failure = mode;
        test_reject_thread_creation = 1;
        test_unexpected_thread_create = 0;
        result = JS_Eval(ctx, source, sizeof(source) - 1,
                         "tests/worker-pthread-failure.js", JS_EVAL_TYPE_MODULE);
        result = js_std_await(ctx, result);
        assert(!JS_IsException(result) && !JS_HasException(ctx));
        JS_FreeValue(ctx, result);
        assert(!test_pthread_failure && !test_unexpected_thread_create);
        test_reject_thread_creation = 0;
        js_std_free_handlers(rt);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
}

static void *test_message_malloc(size_t size)
{
    if (test_message_fail_at && ++test_message_calls == test_message_fail_at)
        return NULL;
    return malloc(size);
}

#ifndef _WIN32
static ssize_t test_waker_write(int fd, const void *buffer, size_t size)
{
    if (test_waker_checks && ++test_waker_calls == 1) {
        errno = EINTR;
        return -1;
    }
    return write(fd, buffer, size);
}

static void test_interrupted_wakeup(void)
{
    JSWaker waker;

    assert(!js_waker_init(&waker));
    test_waker_checks = 1;
    js_waker_signal(&waker);
    test_waker_checks = 0;
    /* Check before reading so a missed wakeup fails without blocking. */
    assert(test_waker_calls == 2);
    js_waker_clear(&waker);
    js_waker_close(&waker);
}
#endif

static void test_message_allocation_failure(void)
{
    static const char source[] = "new SharedArrayBuffer(4)";
    size_t step;

    for (step = 1; step <= 3; step++) {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        JSWorkerMessagePipe *recv, *send;
        JSValue worker, input, result;

        assert(rt && ctx);
        JS_NewClassID(&js_worker_class_id);
        assert(!JS_NewClass(rt, js_worker_class_id, &js_worker_class));
        JS_SetClassProto(ctx, js_worker_class_id, JS_NewObject(ctx));
        js_std_init_handlers(rt);
        recv = js_new_message_pipe();
        send = js_new_message_pipe();
        assert(recv && send);
        worker = js_worker_ctor_internal(ctx, JS_UNDEFINED, recv, send);
        input = JS_Eval(ctx, source, sizeof(source) - 1, "<worker-message-oom>",
                        JS_EVAL_TYPE_GLOBAL);
        assert(!JS_IsException(worker) && !JS_IsException(input));
        /* Cross-thread buffers use system malloc, not the JS allocator. */
        test_message_calls = 0;
        test_message_fail_at = step;
        result = js_worker_postMessage(ctx, worker, 1, (JSValueConst *)&input);
        test_message_fail_at = 0;
        assert(test_message_calls == step && JS_IsException(result));
        assert(JS_HasException(ctx) && list_empty(&send->msg_queue));
        JS_FreeValue(ctx, JS_GetException(ctx));
        JS_FreeValue(ctx, input);
        JS_FreeValue(ctx, worker);
        js_free_message_pipe(recv);
        js_free_message_pipe(send);
        js_std_free_handlers(rt);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
}

static void test_close_discards_inbound_messages(void)
{
    JSWorkerMessagePipe *pipe = js_new_message_pipe();
    JSWorkerMessage *message = malloc(sizeof(*message));

    assert(pipe && message);
    memset(message, 0, sizeof(*message));
    message->data = malloc(1);
    assert(message->data);
    test_queued_message_address = (uintptr_t)message;
    test_queued_message_freed = 0;
    list_add_tail(&message->link, &pipe->msg_queue);
    js_waker_signal(&pipe->waker);

    js_close_message_pipe(pipe, TRUE);
    assert(list_empty(&pipe->msg_queue) && test_queued_message_freed == 1);
    js_free_message_pipe(pipe);
    test_queued_message_address = 0;
}

static void *test_malloc(JSMallocState *state, size_t size)
{
    void *ptr = def_malloc_funcs.js_malloc(state, size);
    if (ptr)
        test_live++;
    return ptr;
}

static void test_allocator_free(JSMallocState *state, void *ptr)
{
    if (ptr) {
        assert(test_live);
        test_live--;
    }
    def_malloc_funcs.js_free(state, ptr);
}

static void *test_realloc(JSMallocState *state, void *ptr, size_t size)
{
    if (!ptr)
        return size ? test_malloc(state, size) : NULL;
    if (!size) {
        test_allocator_free(state, ptr);
        return NULL;
    }
    return def_malloc_funcs.js_realloc(state, ptr, size);
}

static JSRuntime *test_new_runtime(void)
{
    static const JSMallocFunctions functions = {
        test_malloc, test_allocator_free, test_realloc, NULL,
    };
    assert(!test_worker_rt && !test_live);
    test_runtime_factory_calls++;
    if (test_failure_mode == 1)
        return NULL;
    test_worker_rt = JS_NewRuntime2(&functions, NULL);
    assert(test_worker_rt);
    return test_worker_rt;
}

static int test_init_handlers(JSRuntime *rt)
{
    assert(rt == test_worker_rt && !test_handlers_created);
    if (test_failure_mode == 2)
        return -1;
    test_handlers_created++;
    return js_std_init_handlers_checked(rt);
}

static void test_free_handlers(JSRuntime *rt)
{
    assert(rt == test_worker_rt && test_handlers_created == 1);
    assert(!test_handlers_freed && !test_runtimes_freed);
    test_handlers_freed++;
    js_std_free_handlers(rt);
}

static void test_free_runtime(JSRuntime *rt)
{
    assert(rt == test_worker_rt);
    assert(test_handlers_freed == (test_failure_mode == 2 ? 0 : 1));
    assert(!test_runtimes_freed && !JS_GetRuntimeOpaque(rt));
    test_runtimes_freed++;
    JS_FreeRuntime(rt);
    assert(!test_live);
}

static void test_free_context(JSContext *ctx)
{
    test_contexts_freed++;
    assert(ctx);
    JS_FreeContext(ctx);
}

static void test_add_helpers(JSContext *ctx, int argc, char **argv)
{
    test_helpers_called++;
    assert(ctx);
    js_std_add_helpers(ctx, argc, argv);
}

static JSValue test_load_module(JSContext *ctx, const char *basename,
                               const char *filename)
{
    test_modules_called++;
    assert(ctx);
    return JS_LoadModule(ctx, basename, filename);
}

static void test_loop(JSContext *ctx)
{
    test_loops_called++;
    assert(ctx);
    js_std_loop(ctx);
}

static void test_free(void *ptr)
{
    uintptr_t address = (uintptr_t)ptr;
    /* Retain addresses for comparison after free, rather than freed pointers. */
    if (ptr) {
        if (address == test_queued_message_address)
            test_queued_message_freed++;
        else if (address == test_args_address)
            test_args_freed++;
        else if (address == test_filename_address)
            test_filename_freed++;
        else if (address == test_basename_address)
            test_basename_freed++;
        else if (address == test_recv_address)
            test_recv_freed++;
        else if (address == test_send_address)
            test_send_freed++;
    }
    free(ptr);
}

static void *test_worker_trampoline(void *argument)
{
    void *result = test_worker_entry(argument);
    assert(!result);
    if (pthread_mutex_lock(&test_completion_mutex))
        abort();
    test_completed = 1;
    if (pthread_cond_signal(&test_completion_cond))
        abort();
    if (pthread_mutex_unlock(&test_completion_mutex))
        abort();
    return result;
}

static int test_create_thread(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*entry)(void *), void *argument)
{
    WorkerFuncArgs *args = argument;
    int detach_state, result;
    if (test_reject_thread_creation) {
        test_unexpected_thread_create++;
        return EAGAIN;
    }
    assert(!test_thread_created && attr);
    if (pthread_attr_getdetachstate(attr, &detach_state))
        abort();
    assert(detach_state == PTHREAD_CREATE_DETACHED);
    test_args_address = (uintptr_t)args;
    test_filename_address = (uintptr_t)args->filename;
    test_basename_address = (uintptr_t)args->basename;
    test_recv_pipe = args->recv_pipe;
    test_send_pipe = args->send_pipe;
    test_recv_address = (uintptr_t)test_recv_pipe;
    test_send_address = (uintptr_t)test_send_pipe;
    test_worker_entry = entry;
    /* Changing setters after capture must not affect this Worker. */
    js_std_set_worker_new_runtime_func(NULL);
    js_std_set_worker_new_context_func(NULL);
    /* Keep the test's real worker joinable so cleanup/return is acknowledged.
       Production still requests a detached worker; its caller is unchanged. */
    result = pthread_create(thread, NULL, test_worker_trampoline, argument);
    if (!result) {
        test_thread = *thread;
        test_thread_created++;
    }
    return result;
}

static JSContext *test_null_factory(JSRuntime *rt)
{
    JSContext *ctx;
    JSThreadState *state = JS_GetRuntimeOpaque(rt);
    assert(rt == test_worker_rt && test_handlers_created == 1);
    assert(state && state->recv_pipe == test_recv_pipe);
    assert(state->send_pipe == test_send_pipe);
    test_factory_calls++;
    /* Leave a real pending exception and partial-context graph for runtime
       cleanup, as a failed intrinsic initializer can do. */
    ctx = JS_NewContextRaw(rt);
    assert(ctx);
    JS_ThrowInternalError(ctx, "worker context factory regression");
    assert(JS_HasException(ctx));
    JS_FreeContext(ctx);
    return NULL;
}

static void test_join_worker(void)
{
    struct timespec deadline;
    void *result;
    assert(test_thread_created == 1);
    if (clock_gettime(CLOCK_REALTIME, &deadline))
        abort();
    deadline.tv_sec += 5;
    if (pthread_mutex_lock(&test_completion_mutex))
        abort();
    while (!test_completed) {
        int status = pthread_cond_timedwait(&test_completion_cond,
                                           &test_completion_mutex, &deadline);
        if (status) {
            fprintf(stderr, "worker cleanup did not finish: %d\n", status);
            abort();
        }
    }
    if (pthread_mutex_unlock(&test_completion_mutex))
        abort();
    if (pthread_join(test_thread, &result))
        abort();
    assert(!result);
}

static void test_failure(int mode)
{
    static const char source[] =
        "import { Worker } from 'os';\n"
        "globalThis.worker = new Worker('must-not-load.js');\n";
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSModuleDef *module;
    JSValue value, global, worker;
    JSWorkerData *data;
    int status;
    test_failure_mode = mode;
    test_worker_rt = NULL;
    test_factory_calls = test_runtime_factory_calls = 0;
    test_handlers_created = test_handlers_freed = test_runtimes_freed = 0;
    test_thread_created = test_completed = 0;
    test_args_freed = test_filename_freed = test_basename_freed = 0;
    test_recv_freed = test_send_freed = 0;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    js_std_init_handlers(rt);
    module = js_init_module_os(ctx, "os");
    assert(module);
    js_std_set_worker_new_runtime_func(test_new_runtime);
    js_std_set_worker_new_context_func(test_null_factory);
    value = JS_Eval(ctx, source, sizeof(source) - 1,
                    "tests/worker-null-factory.js", JS_EVAL_TYPE_MODULE);
    value = js_std_await(ctx, value);
    assert(!JS_IsException(value) && !JS_HasException(ctx));
    JS_FreeValue(ctx, value);
    test_join_worker();
    assert(test_runtime_factory_calls == 1 && !test_live);
    assert(test_factory_calls == (mode == 0));
    assert(test_handlers_created == (mode == 0));
    assert(test_handlers_freed == (mode == 0));
    assert(test_runtimes_freed == (mode != 1) && !test_contexts_freed);
    assert(!test_helpers_called && !test_modules_called && !test_loops_called);
    assert(test_args_freed == 1 && test_filename_freed == 1);
    assert(test_basename_freed == 1);
    assert(test_recv_pipe->ref_count == 1 && test_send_pipe->ref_count == 1);
    assert(!test_recv_freed && !test_send_freed);

    global = JS_GetGlobalObject(ctx);
    worker = JS_GetPropertyStr(ctx, global, "worker");
    data = JS_GetOpaque(worker, js_worker_class_id);
    assert(data && data->recv_pipe == test_send_pipe);
    assert(data->send_pipe == test_recv_pipe);
    status = JS_SetPropertyStr(ctx, global, "worker", JS_UNDEFINED);
    assert(status == 1);
    JS_FreeValue(ctx, worker);
    JS_FreeValue(ctx, global);
    assert(test_recv_freed == 1 && test_send_freed == 1);
    js_std_set_worker_new_runtime_func(NULL);
    js_std_set_worker_new_context_func(NULL);
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

int main(void)
{
#ifndef _WIN32
    test_interrupted_wakeup();
#endif
    test_message_allocation_failure();
    test_close_discards_inbound_messages();
    test_pthread_setup_failures();
    test_failure(0);
    test_failure(1);
    test_failure(2);
    if (pthread_cond_destroy(&test_completion_cond) ||
        pthread_mutex_destroy(&test_completion_mutex))
        abort();
    return 0;
}
#else
int main(void)
{
    return 0;
}
#endif
