/*
 * QuickJS host helpers and event loop
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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
#include <stdlib.h>
#include "event-loop.h"
#include "worker.h"
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#if defined(_WIN32)
#include <windows.h>
#include <conio.h>
#include <utime.h>
#else
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <poll.h>

#endif

#include "cutils.h"
#include "list.h"
#include "quickjs-libc.h"
#include "thread.h"
#include "os.h"
#include "std.h"
#include "module-loader.h"

/* Runtime opaque may belong to a bare embedder. Never probe it as host state.
   Registration is protected across Worker creation and owner-thread transfers. */
static struct list_head handler_registry = LIST_HEAD_INIT(handler_registry);
static pthread_mutex_t handler_registry_mutex = PTHREAD_MUTEX_INITIALIZER;

static BOOL js_std_has_handlers(JSRuntime *rt)
{
    struct list_head *el;
    BOOL found = FALSE;

    pthread_mutex_lock(&handler_registry_mutex);
    list_for_each(el, &handler_registry) {
        JSThreadState *ts = list_entry(el, JSThreadState, registry_link);
        if (ts->runtime == rt) {
            found = TRUE;
            break;
        }
    }
    pthread_mutex_unlock(&handler_registry_mutex);
    return found;
}

typedef struct {
    struct list_head link;
    JSValue promise;
    JSValue reason;
} JSRejectedPromiseEntry;

/**********************************************************/

static JSValue js_print(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv)
{
    int i;
    JSValueConst v;

    for(i = 0; i < argc; i++) {
        if (i != 0)
            putchar(' ');
        v = argv[i];
        if (JS_IsString(v)) {
            const char *str;
            size_t len;
            str = JS_ToCStringLen(ctx, &len, v);
            if (!str)
                return JS_EXCEPTION;
            fwrite(str, 1, len, stdout);
            JS_FreeCString(ctx, str);
        } else {
            JS_PrintValue(ctx, js_print_value_write, stdout, v, NULL);
        }
    }
    putchar('\n');
    return JS_UNDEFINED;
}

static JSValue js_console_log(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    JSValue ret;
    ret = js_print(ctx, this_val, argc, argv);
    fflush(stdout);
    return ret;
}

void js_std_add_helpers(JSContext *ctx, int argc, char **argv)
{
    JSValue global_obj, obj = JS_UNDEFINED, value;
    int i;

    /* XXX: should these global definitions be enumerable? */
    global_obj = JS_GetGlobalObject(ctx);

    /* Publish each helper only after its members have been initialized. */
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        goto done;
    value = JS_NewCFunction(ctx, js_console_log, "log", 1);
    if (JS_IsException(value) || JS_SetPropertyStr(ctx, obj, "log", value) < 0)
        goto done;
    if (JS_SetPropertyStr(ctx, global_obj, "console", JS_DupValue(ctx, obj)) < 0)
        goto done;
    JS_FreeValue(ctx, obj);

    obj = JS_NewObject(ctx);
    if (JS_IsException(obj))
        goto done;
    value = JS_NewCFunction(ctx, js_os_now, "now", 0);
    if (JS_IsException(value) || JS_SetPropertyStr(ctx, obj, "now", value) < 0)
        goto done;
    if (JS_SetPropertyStr(ctx, global_obj, "performance", JS_DupValue(ctx, obj)) < 0)
        goto done;
    JS_FreeValue(ctx, obj);
    obj = JS_UNDEFINED;

    /* same methods as the mozilla JS shell */
    if (argc >= 0) {
        obj = JS_NewArray(ctx);
        if (JS_IsException(obj))
            goto done;
        for(i = 0; i < argc; i++) {
            value = JS_NewString(ctx, argv[i]);
            if (JS_IsException(value) || JS_SetPropertyUint32(ctx, obj, i, value) < 0)
                goto done;
        }
        if (JS_SetPropertyStr(ctx, global_obj, "scriptArgs", JS_DupValue(ctx, obj)) < 0)
            goto done;
    }

    value = JS_NewCFunction(ctx, js_print, "print", 1);
    if (JS_IsException(value) || JS_SetPropertyStr(ctx, global_obj, "print", value) < 0)
        goto done;
    value = JS_NewCFunction(ctx, js_loadScript, "__loadScript", 1);
    if (!JS_IsException(value))
        JS_SetPropertyStr(ctx, global_obj, "__loadScript", value);

 done:
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, global_obj);
}

int js_std_init_handlers_checked(JSRuntime *rt)
{
    JSThreadState *ts;

    ts = malloc(sizeof(*ts));
    if (!ts)
        return -1;
    memset(ts, 0, sizeof(*ts));
    init_list_head(&ts->os_rw_handlers);
    init_list_head(&ts->os_signal_handlers);
    init_list_head(&ts->os_timers);
    init_list_head(&ts->port_list);
    init_list_head(&ts->rejected_promise_list);
    ts->next_timer_id = 1;

    ts->runtime = rt;
    pthread_mutex_lock(&handler_registry_mutex);
    list_add_tail(&ts->registry_link, &handler_registry);
    pthread_mutex_unlock(&handler_registry_mutex);
    JS_SetRuntimeOpaque(rt, ts);

#ifdef USE_WORKER
    /* set the SharedArrayBuffer memory handlers */
    {
        JSSharedArrayBufferFunctions sf;
        memset(&sf, 0, sizeof(sf));
        sf.sab_alloc = js_sab_alloc;
        sf.sab_free = js_sab_free;
        sf.sab_dup = js_sab_dup;
        JS_SetSharedArrayBufferFunctions(rt, &sf);
    }
#endif
    return 0;
}

void js_std_init_handlers(JSRuntime *rt)
{
    if (js_std_init_handlers_checked(rt) < 0) {
        fprintf(stderr, "Could not allocate memory for the worker");
        exit(1);
    }
}

void js_std_free_handlers(JSRuntime *rt)
{
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    struct list_head *el, *el1;

    list_for_each_safe(el, el1, &ts->os_rw_handlers) {
        JSOSRWHandler *rh = list_entry(el, JSOSRWHandler, link);
        free_rw_handler(rt, rh);
    }

    list_for_each_safe(el, el1, &ts->os_signal_handlers) {
        JSOSSignalHandler *sh = list_entry(el, JSOSSignalHandler, link);
        free_sh(rt, sh, TRUE);
    }

    list_for_each_safe(el, el1, &ts->os_timers) {
        JSOSTimer *th = list_entry(el, JSOSTimer, link);
        free_timer(rt, th);
    }

    list_for_each_safe(el, el1, &ts->rejected_promise_list) {
        JSRejectedPromiseEntry *rp = list_entry(el, JSRejectedPromiseEntry, link);
        JS_FreeValueRT(rt, rp->promise);
        JS_FreeValueRT(rt, rp->reason);
        free(rp);
    }

#ifdef USE_WORKER
    js_free_message_pipe(ts->recv_pipe);
    js_free_message_pipe(ts->send_pipe);

    list_for_each_safe(el, el1, &ts->port_list) {
        JSWorkerMessageHandler *port = list_entry(el, JSWorkerMessageHandler, link);
        /* unlink the message ports. They are freed by the Worker object */
        port->link.prev = NULL;
        port->link.next = NULL;
    }
#endif

#if !defined(_WIN32)
    free(ts->poll_fds);
#endif

    pthread_mutex_lock(&handler_registry_mutex);
    list_del(&ts->registry_link);
    pthread_mutex_unlock(&handler_registry_mutex);
    free(ts);
    JS_SetRuntimeOpaque(rt, NULL); /* fail safe */
}

static void js_std_dump_error1(JSContext *ctx, JSValueConst exception_val)
{
    JS_PrintValue(ctx, js_print_value_write, stderr, exception_val, NULL);
    fputc('\n', stderr);
}

void js_std_dump_error(JSContext *ctx)
{
    JSValue exception_val;

    exception_val = JS_GetException(ctx);
    js_std_dump_error1(ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
}

static JSRejectedPromiseEntry *find_rejected_promise(JSContext *ctx, JSThreadState *ts,
                                                     JSValueConst promise)
{
    struct list_head *el;

    list_for_each(el, &ts->rejected_promise_list) {
        JSRejectedPromiseEntry *rp = list_entry(el, JSRejectedPromiseEntry, link);
        if (JS_SameValue(ctx, rp->promise, promise))
            return rp;
    }
    return NULL;
}

void js_std_promise_rejection_tracker(JSContext *ctx, JSValueConst promise,
                                      JSValueConst reason,
                                      BOOL is_handled, void *opaque)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    JSRejectedPromiseEntry *rp;

    if (!is_handled) {
        /* add a new entry if needed */
        rp = find_rejected_promise(ctx, ts, promise);
        if (!rp) {
            rp = malloc(sizeof(*rp));
            if (rp) {
                rp->promise = JS_DupValue(ctx, promise);
                rp->reason = JS_DupValue(ctx, reason);
                list_add_tail(&rp->link, &ts->rejected_promise_list);
            }
        }
    } else {
        /* the rejection is handled, so the entry can be removed if present */
        rp = find_rejected_promise(ctx, ts, promise);
        if (rp) {
            JS_FreeValue(ctx, rp->promise);
            JS_FreeValue(ctx, rp->reason);
            list_del(&rp->link);
            free(rp);
        }
    }
}

/* check if there are pending promise rejections. It must be done
   asynchrously in case a rejected promise is handled later. Currently
   we do it once the application is about to sleep. It could be done
   more often if needed. */
static void js_std_promise_rejection_check(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    struct list_head *el;

    if (unlikely(!list_empty(&ts->rejected_promise_list))) {
        list_for_each(el, &ts->rejected_promise_list) {
            JSRejectedPromiseEntry *rp = list_entry(el, JSRejectedPromiseEntry, link);
            fprintf(stderr, "Possibly unhandled promise rejection: ");
            js_std_dump_error1(ctx, rp->reason);
        }
        exit(1);
    }
}

/* Keep the exception pending for embedding callers. */
static int js_std_poll(JSContext *ctx, int timeout_ms)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSOSPollFunc *poll_func;
    int ret;

    poll_func = atomic_load_explicit(&os_poll_func, memory_order_relaxed);
    if (poll_func && js_std_has_handlers(rt))
        return poll_func(ctx, timeout_ms);
    if (!JS_IsNativeJobPending(rt))
        return -1;
    if (JS_IsJobPending(rt))
        timeout_ms = 0;
    ret = JS_PollNativeJobs(rt, timeout_ms);
    if (ret < 0) {
        JS_ThrowInternalError(ctx, "native job polling failed");
        return -2;
    }
    return ret > 0 ? 1 : 0;
}

int js_std_poll_io(JSContext *ctx, int timeout_ms)
{
    int ret = js_std_poll(ctx, timeout_ms);

    return ret < -1 ? -1 : ret > 0;
}

int js_std_loop_once(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    int ret, progress = 0;

    while ((ret = JS_ExecutePendingJob(rt, NULL)) > 0)
        progress = 1;
    if (ret < 0)
        return -1;
    JS_ClearKeptObjects(rt);
    ret = js_std_poll(ctx, 0);
    if (ret < -1)
        return -1;
    return progress || ret != -1;
}

/* main loop which calls the user JS callbacks */
void js_std_loop(JSContext *ctx)
{
    int err;

    for(;;) {
        /* execute the pending jobs */
        for(;;) {
            err = JS_ExecutePendingJob(JS_GetRuntime(ctx), NULL);
            if (err <= 0) {
                if (err < 0)
                    js_std_dump_error(ctx);
                break;
            }
        }

        js_std_promise_rejection_check(ctx);
        /* A job error can leave microtasks queued for a later host step. */
        if (!JS_IsJobPending(JS_GetRuntime(ctx)))
            JS_ClearKeptObjects(JS_GetRuntime(ctx));

        err = js_std_poll(ctx, -1);
        if (err == -3) {
            js_std_dump_error(ctx);
            continue;
        }
        if (err < 0) {
            if (err == -2)
                js_std_dump_error(ctx);
            break;
        }
    }
}

/* Wait for a promise and execute pending jobs while waiting for
   it. Return the promise result or JS_EXCEPTION in case of promise
   rejection. */
JSValue js_std_await(JSContext *ctx, JSValue obj)
{
    JSValue ret;
    int state;

    for(;;) {
        state = JS_PromiseState(ctx, obj);
        if (state == JS_PROMISE_FULFILLED) {
            ret = JS_PromiseResult(ctx, obj);
            JS_FreeValue(ctx, obj);
            break;
        } else if (state == JS_PROMISE_REJECTED) {
            ret = JS_Throw(ctx, JS_PromiseResult(ctx, obj));
            JS_FreeValue(ctx, obj);
            break;
        } else if (state == JS_PROMISE_PENDING) {
            int err;
            err = JS_ExecutePendingJob(JS_GetRuntime(ctx), NULL);
            if (err < 0) {
                js_std_dump_error(ctx);
            }
            if (err == 0) {
                JS_ClearKeptObjects(JS_GetRuntime(ctx));
                js_std_promise_rejection_check(ctx);
                err = js_std_poll(ctx, -1);
                if (err == -3)
                    js_std_dump_error(ctx);
                if (err == -2) {
                    ret = JS_EXCEPTION;
                    JS_FreeValue(ctx, obj);
                    break;
                }
                if (err == -1 && JS_PromiseState(ctx, obj) == JS_PROMISE_PENDING) {
                    ret = JS_ThrowInternalError(ctx,
                                                "promise is pending with no event sources");
                    JS_FreeValue(ctx, obj);
                    break;
                }
            }
        } else {
            /* not a promise */
            ret = obj;
            break;
        }
    }
    return ret;
}

void js_std_eval_binary(JSContext *ctx, const uint8_t *buf, size_t buf_len,
                        int load_only)
{
    JSValue obj, val;
    obj = JS_ReadObject(ctx, buf, buf_len, JS_READ_OBJ_BYTECODE);
    if (JS_IsException(obj))
        goto exception;
    if (load_only) {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_MODULE) {
            if (js_module_set_import_meta(ctx, obj, FALSE, FALSE) < 0) {
                JS_FreeValue(ctx, obj);
                goto exception;
            }
        }
        JS_FreeValue(ctx, obj);
    } else {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_MODULE) {
            if (JS_ResolveModule(ctx, obj) < 0) {
                JS_FreeValue(ctx, obj);
                goto exception;
            }
            if (js_module_set_import_meta(ctx, obj, FALSE, TRUE) < 0) {
                JS_FreeValue(ctx, obj);
                goto exception;
            }
            val = JS_EvalFunction(ctx, obj);
            val = js_std_await(ctx, val);
        } else {
            val = JS_EvalFunction(ctx, obj);
        }
        if (JS_IsException(val))
            goto exception;
        JS_FreeValue(ctx, val);
    }
    return;

 exception:
    js_std_dump_error(ctx);
    exit(1);
}

void js_std_eval_binary_json_module(JSContext *ctx,
                                    const uint8_t *buf, size_t buf_len,
                                    const char *module_name)
{
    JSValue obj;
    JSModuleDef *m;

    obj = JS_ReadObject(ctx, buf, buf_len, 0);
    if (JS_IsException(obj))
        goto exception;
    m = create_json_module(ctx, module_name, obj);
    if (!m) {
    exception:
        js_std_dump_error(ctx);
        exit(1);
    }
}

void js_std_eval_binary_json_module2(JSContext *ctx,
                                     const uint8_t *buf, size_t buf_len,
                                     const char *module_name,
                                     const uint8_t *attributes_buf,
                                     size_t attributes_buf_len)
{
    JSValue obj, attributes;
    JSModuleDef *m;

    obj = JS_ReadObject(ctx, buf, buf_len, 0);
    if (JS_IsException(obj))
        goto exception;
    attributes = JS_ReadObject(ctx, attributes_buf, attributes_buf_len, 0);
    if (JS_IsException(attributes)) {
        JS_FreeValue(ctx, obj);
        goto exception;
    }
    m = create_json_module2(ctx, module_name, obj, attributes);
    JS_FreeValue(ctx, attributes);
    if (!m)
        goto exception;
    return;
 exception:
    js_std_dump_error(ctx);
    exit(1);
}
