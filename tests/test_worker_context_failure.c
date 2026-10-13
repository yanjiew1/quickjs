/*
 * Native Worker NULL-context factory ownership regression.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
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
static int test_factory_calls, test_handlers_created, test_handlers_freed;
static int test_runtimes_freed, test_contexts_freed;
static int test_helpers_called, test_modules_called, test_loops_called;
static uintptr_t test_args_address, test_filename_address;
static uintptr_t test_basename_address, test_recv_address, test_send_address;
static JSWorkerMessagePipe *test_recv_pipe, *test_send_pipe;
static int test_args_freed, test_filename_freed, test_basename_freed;
static int test_recv_freed, test_send_freed;
static pthread_t test_thread;
static int test_thread_created, test_completed;
static pthread_mutex_t test_completion_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_completion_cond = PTHREAD_COND_INITIALIZER;
static void *(*test_worker_entry)(void *);

static JSRuntime *test_new_runtime(void);
static void test_init_handlers(JSRuntime *rt);
static void test_free_handlers(JSRuntime *rt);
static void test_free_runtime(JSRuntime *rt);
static void test_free_context(JSContext *ctx);
static void test_add_helpers(JSContext *ctx, int argc, char **argv);
static JSValue test_load_module(JSContext *ctx, const char *basename,
                               const char *filename);
static void test_loop(JSContext *ctx);
static void test_free(void *ptr);
static int test_create_thread(pthread_t *thread, const pthread_attr_t *attr,
                              void *(*entry)(void *), void *argument);

/* Headers precede redirection. Compile the actual Worker implementation;
   its exported definitions also replace the archive's normal worker object. */
#define JS_NewRuntime test_new_runtime
#define js_std_init_handlers test_init_handlers
#define js_std_free_handlers test_free_handlers
#define JS_FreeRuntime test_free_runtime
#define JS_FreeContext test_free_context
#define js_std_add_helpers test_add_helpers
#define JS_LoadModule test_load_module
#define js_std_loop test_loop
#define free test_free
#define pthread_create test_create_thread
#include "../src/quickjs-libc/worker.c"
#undef JS_NewRuntime
#undef js_std_init_handlers
#undef js_std_free_handlers
#undef JS_FreeRuntime
#undef JS_FreeContext
#undef js_std_add_helpers
#undef JS_LoadModule
#undef js_std_loop
#undef free
#undef pthread_create

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
    test_worker_rt = JS_NewRuntime2(&functions, NULL);
    assert(test_worker_rt);
    return test_worker_rt;
}

static void test_init_handlers(JSRuntime *rt)
{
    assert(rt == test_worker_rt && !test_handlers_created);
    test_handlers_created++;
    js_std_init_handlers(rt);
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
    assert(rt == test_worker_rt && test_handlers_freed == 1);
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
        if (address == test_args_address)
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

int main(void)
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
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    js_std_init_handlers(rt);
    module = js_init_module_os(ctx, "os");
    assert(module);
    js_std_set_worker_new_context_func(test_null_factory);
    value = JS_Eval(ctx, source, sizeof(source) - 1,
                    "tests/worker-null-factory.js", JS_EVAL_TYPE_MODULE);
    value = js_std_await(ctx, value);
    assert(!JS_IsException(value) && !JS_HasException(ctx));
    JS_FreeValue(ctx, value);
    test_join_worker();
    assert(test_factory_calls == 1 && !test_live);
    assert(test_handlers_created == 1 && test_handlers_freed == 1);
    assert(test_runtimes_freed == 1 && !test_contexts_freed);
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
    js_std_set_worker_new_context_func(JS_NewContext);
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
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
