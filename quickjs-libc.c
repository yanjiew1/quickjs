/*
 * QuickJS C library
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <poll.h>

#if defined(__APPLE__) || defined(__FreeBSD__)
typedef sig_t sighandler_t;
#endif

#endif

/* enable the os.Worker API. It relies on POSIX threads */
#define USE_WORKER

#ifdef USE_WORKER
#include <pthread.h>
#include <stdatomic.h>
#endif

#include "cutils.h"
#include "list.h"
#include "quickjs-libc.h"
#include "src/quickjs-libc/internal-base.h"
#include "src/quickjs-libc/internal-event.h"
#include "src/quickjs-libc/internal-host.h"
#include "src/quickjs-libc/internal-loader.h"
#include "src/quickjs-libc/internal-std.h"

/* TODO:
   - add socket calls
*/

typedef struct {
    struct list_head link;
    int fd;
    int poll_fd_index; /* temporary use in js_os_poll() */
    JSValue rw_func[2];
} JSOSRWHandler;

typedef struct {
    struct list_head link;
    int sig_num;
    JSValue func;
} JSOSSignalHandler;

typedef struct {
    struct list_head link;
    int timer_id;
    int64_t timeout;
    JSValue func;
} JSOSTimer;

typedef struct {
    struct list_head link;
    uint8_t *data;
    size_t data_len;
    /* list of SharedArrayBuffers, necessary to free the message */
    uint8_t **sab_tab;
    size_t sab_tab_len;
} JSWorkerMessage;

typedef struct JSWaker {
#ifdef _WIN32
    HANDLE handle;
#else
    int read_fd;
    int write_fd;
#endif
} JSWaker;

typedef struct {
    int ref_count;
#ifdef USE_WORKER
    pthread_mutex_t mutex;
#endif
    struct list_head msg_queue; /* list of JSWorkerMessage.link */
    JSWaker waker;
} JSWorkerMessagePipe;

typedef struct {
    struct list_head link;
    JSWorkerMessagePipe *recv_pipe;
    JSValue on_message_func;
    int poll_fd_index; /* temporary use in js_os_poll() */
} JSWorkerMessageHandler;

typedef struct {
    struct list_head link;
    JSValue promise;
    JSValue reason;
} JSRejectedPromiseEntry;

typedef struct JSThreadState {
    struct list_head os_rw_handlers; /* list of JSOSRWHandler.link */
    struct list_head os_signal_handlers; /* list JSOSSignalHandler.link */
    struct list_head os_timers; /* list of JSOSTimer.link */
    struct list_head port_list; /* list of JSWorkerMessageHandler.link */
    struct list_head rejected_promise_list; /* list of JSRejectedPromiseEntry.link */
    int eval_script_recurse; /* only used in the main thread */
    int next_timer_id; /* for setTimeout() */
    /* not used in the main thread */
    JSWorkerMessagePipe *recv_pipe, *send_pipe;
#if !defined(_WIN32)
    struct pollfd *poll_fds;
    int poll_fds_size;
#endif    
} JSThreadState;

static uint64_t os_pending_signals;
static int (*os_poll_func)(JSContext *ctx);

static int interrupt_handler(JSRuntime *rt, void *opaque)
{
    return (os_pending_signals >> SIGINT) & 1;
}

JSValue js_evalScript(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    const char *str;
    size_t len;
    JSValue ret;
    JSValueConst options_obj;
    BOOL backtrace_barrier = FALSE;
    BOOL is_async = FALSE;
    int flags;

    if (argc >= 2) {
        options_obj = argv[1];
        if (get_bool_option(ctx, &backtrace_barrier, options_obj,
                                    "backtrace_barrier"))
            return JS_EXCEPTION;
        if (get_bool_option(ctx, &is_async, options_obj, "async"))
            return JS_EXCEPTION;
    }

    str = JS_ToCStringLen(ctx, &len, argv[0]);
    if (!str)
        return JS_EXCEPTION;
    if (!ts->recv_pipe && ++ts->eval_script_recurse == 1) {
        /* install the interrupt handler */
        JS_SetInterruptHandler(JS_GetRuntime(ctx), interrupt_handler, NULL);
    }
    flags = JS_EVAL_TYPE_GLOBAL;
    if (backtrace_barrier)
        flags |= JS_EVAL_FLAG_BACKTRACE_BARRIER;
    if (is_async)
        flags |= JS_EVAL_FLAG_ASYNC;
    ret = JS_Eval(ctx, str, len, "<evalScript>", flags);
    JS_FreeCString(ctx, str);
    if (!ts->recv_pipe && --ts->eval_script_recurse == 0) {
        /* remove the interrupt handler */
        JS_SetInterruptHandler(JS_GetRuntime(ctx), NULL, NULL);
        os_pending_signals &= ~((uint64_t)1 << SIGINT);
        /* convert the uncatchable "interrupted" error into a normal error
           so that it can be caught by the REPL */
        if (JS_IsException(ret))
            JS_SetUncatchableException(ctx, FALSE);
    }
    return ret;
}

static BOOL is_main_thread(JSRuntime *rt)
{
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    return !ts->recv_pipe;
}

static JSOSRWHandler *find_rh(JSThreadState *ts, int fd)
{
    JSOSRWHandler *rh;
    struct list_head *el;

    list_for_each(el, &ts->os_rw_handlers) {
        rh = list_entry(el, JSOSRWHandler, link);
        if (rh->fd == fd)
            return rh;
    }
    return NULL;
}

static void free_rw_handler(JSRuntime *rt, JSOSRWHandler *rh)
{
    int i;
    list_del(&rh->link);
    for(i = 0; i < 2; i++) {
        JS_FreeValueRT(rt, rh->rw_func[i]);
    }
    js_free_rt(rt, rh);
}

static JSValue js_os_setReadHandler(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv, int magic)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    JSOSRWHandler *rh;
    int fd;
    JSValueConst func;

    if (JS_ToInt32(ctx, &fd, argv[0]))
        return JS_EXCEPTION;
    func = argv[1];
    if (JS_IsNull(func)) {
        rh = find_rh(ts, fd);
        if (rh) {
            JS_FreeValue(ctx, rh->rw_func[magic]);
            rh->rw_func[magic] = JS_NULL;
            if (JS_IsNull(rh->rw_func[0]) &&
                JS_IsNull(rh->rw_func[1])) {
                /* remove the entry */
                free_rw_handler(JS_GetRuntime(ctx), rh);
            }
        }
    } else {
        if (!JS_IsFunction(ctx, func))
            return JS_ThrowTypeError(ctx, "not a function");
        rh = find_rh(ts, fd);
        if (!rh) {
            rh = js_mallocz(ctx, sizeof(*rh));
            if (!rh)
                return JS_EXCEPTION;
            rh->fd = fd;
            rh->rw_func[0] = JS_NULL;
            rh->rw_func[1] = JS_NULL;
            list_add_tail(&rh->link, &ts->os_rw_handlers);
        }
        JS_FreeValue(ctx, rh->rw_func[magic]);
        rh->rw_func[magic] = JS_DupValue(ctx, func);
    }
    return JS_UNDEFINED;
}

static JSOSSignalHandler *find_sh(JSThreadState *ts, int sig_num)
{
    JSOSSignalHandler *sh;
    struct list_head *el;
    list_for_each(el, &ts->os_signal_handlers) {
        sh = list_entry(el, JSOSSignalHandler, link);
        if (sh->sig_num == sig_num)
            return sh;
    }
    return NULL;
}

static void free_sh(JSRuntime *rt, JSOSSignalHandler *sh)
{
    list_del(&sh->link);
    JS_FreeValueRT(rt, sh->func);
    js_free_rt(rt, sh);
}

static void os_signal_handler(int sig_num)
{
    os_pending_signals |= ((uint64_t)1 << sig_num);
}

#if defined(_WIN32)
typedef void (*sighandler_t)(int sig_num);
#endif

static JSValue js_os_signal(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    JSOSSignalHandler *sh;
    uint32_t sig_num;
    JSValueConst func;
    sighandler_t handler;

    if (!is_main_thread(rt))
        return JS_ThrowTypeError(ctx, "signal handler can only be set in the main thread");

    if (JS_ToUint32(ctx, &sig_num, argv[0]))
        return JS_EXCEPTION;
    if (sig_num >= 64)
        return JS_ThrowRangeError(ctx, "invalid signal number");
    func = argv[1];
    /* func = null: SIG_DFL, func = undefined, SIG_IGN */
    if (JS_IsNull(func) || JS_IsUndefined(func)) {
        sh = find_sh(ts, sig_num);
        if (sh) {
            free_sh(JS_GetRuntime(ctx), sh);
        }
        if (JS_IsNull(func))
            handler = SIG_DFL;
        else
            handler = SIG_IGN;
        signal(sig_num, handler);
    } else {
        if (!JS_IsFunction(ctx, func))
            return JS_ThrowTypeError(ctx, "not a function");
        sh = find_sh(ts, sig_num);
        if (!sh) {
            sh = js_mallocz(ctx, sizeof(*sh));
            if (!sh)
                return JS_EXCEPTION;
            sh->sig_num = sig_num;
            list_add_tail(&sh->link, &ts->os_signal_handlers);
        }
        JS_FreeValue(ctx, sh->func);
        sh->func = JS_DupValue(ctx, func);
        signal(sig_num, os_signal_handler);
    }
    return JS_UNDEFINED;
}

#if defined(__linux__) || defined(__APPLE__)
static int64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
}

static int64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}
#else
/* more portable, but does not work if the date is updated */
static int64_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}

static int64_t get_time_ns(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000000 + (tv.tv_usec * 1000);
}
#endif

static JSValue js_os_now(JSContext *ctx, JSValue this_val,
                         int argc, JSValue *argv)
{
    return JS_NewFloat64(ctx, (double)get_time_ns() / 1e6);
}

static void free_timer(JSRuntime *rt, JSOSTimer *th)
{
    list_del(&th->link);
    JS_FreeValueRT(rt, th->func);
    js_free_rt(rt, th);
}

static JSValue js_os_setTimeout(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    int64_t delay;
    JSValueConst func;
    JSOSTimer *th;

    func = argv[0];
    if (!JS_IsFunction(ctx, func))
        return JS_ThrowTypeError(ctx, "not a function");
    if (JS_ToInt64(ctx, &delay, argv[1]))
        return JS_EXCEPTION;
    th = js_mallocz(ctx, sizeof(*th));
    if (!th)
        return JS_EXCEPTION;
    th->timer_id = ts->next_timer_id;
    if (ts->next_timer_id == INT32_MAX)
        ts->next_timer_id = 1;
    else
        ts->next_timer_id++;
    th->timeout = get_time_ms() + delay;
    th->func = JS_DupValue(ctx, func);
    list_add_tail(&th->link, &ts->os_timers);
    return JS_NewInt32(ctx, th->timer_id);
}

static JSOSTimer *find_timer_by_id(JSThreadState *ts, int timer_id)
{
    struct list_head *el;
    if (timer_id <= 0)
        return NULL;
    list_for_each(el, &ts->os_timers) {
        JSOSTimer *th = list_entry(el, JSOSTimer, link);
        if (th->timer_id == timer_id)
            return th;
    }
    return NULL;
}

static JSValue js_os_clearTimeout(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    JSOSTimer *th;
    int timer_id;

    if (JS_ToInt32(ctx, &timer_id, argv[0]))
        return JS_EXCEPTION;
    th = find_timer_by_id(ts, timer_id);
    if (!th)
        return JS_UNDEFINED;
    free_timer(rt, th);
    return JS_UNDEFINED;
}

/* return a promise */
static JSValue js_os_sleepAsync(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    int64_t delay;
    JSOSTimer *th;
    JSValue promise, resolving_funcs[2];

    if (JS_ToInt64(ctx, &delay, argv[0]))
        return JS_EXCEPTION;
    promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise))
        return JS_EXCEPTION;

    th = js_mallocz(ctx, sizeof(*th));
    if (!th) {
        JS_FreeValue(ctx, promise);
        JS_FreeValue(ctx, resolving_funcs[0]);
        JS_FreeValue(ctx, resolving_funcs[1]);
        return JS_EXCEPTION;
    }
    th->timer_id = -1;
    th->timeout = get_time_ms() + delay;
    th->func = JS_DupValue(ctx, resolving_funcs[0]);
    list_add_tail(&th->link, &ts->os_timers);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return promise;
}

static void call_handler(JSContext *ctx, JSValueConst func)
{
    JSValue ret, func1;
    /* 'func' might be destroyed when calling itself (if it frees the
       handler), so must take extra care */
    func1 = JS_DupValue(ctx, func);
    ret = JS_Call(ctx, func1, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, func1);
    if (JS_IsException(ret))
        js_std_dump_error(ctx);
    JS_FreeValue(ctx, ret);
}

#ifdef USE_WORKER

#ifdef _WIN32

static int js_waker_init(JSWaker *w)
{
    w->handle = CreateEvent(NULL, TRUE, FALSE, NULL);
    return w->handle ? 0 : -1;
}

static void js_waker_signal(JSWaker *w)
{
    SetEvent(w->handle);
}

static void js_waker_clear(JSWaker *w)
{
    ResetEvent(w->handle);
}

static void js_waker_close(JSWaker *w)
{
    CloseHandle(w->handle);
    w->handle = INVALID_HANDLE_VALUE;
}

#else // !_WIN32

static int js_waker_init(JSWaker *w)
{
    int fds[2];

    if (pipe(fds) < 0)
        return -1;
    w->read_fd = fds[0];
    w->write_fd = fds[1];
    return 0;
}

static void js_waker_signal(JSWaker *w)
{
    int ret;

    for(;;) {
        ret = write(w->write_fd, "", 1);
        if (ret == 1)
            break;
        if (ret < 0 && (errno != EAGAIN || errno != EINTR))
            break;
    }
}

static void js_waker_clear(JSWaker *w)
{
    uint8_t buf[16];
    int ret;

    for(;;) {
        ret = read(w->read_fd, buf, sizeof(buf));
        if (ret >= 0)
            break;
        if (errno != EAGAIN && errno != EINTR)
            break;
    }
}

static void js_waker_close(JSWaker *w)
{
    close(w->read_fd);
    close(w->write_fd);
    w->read_fd = -1;
    w->write_fd = -1;
}

#endif // _WIN32

static void js_free_message(JSWorkerMessage *msg);

/* return 1 if a message was handled, 0 if no message */
static int handle_posted_message(JSRuntime *rt, JSContext *ctx,
                                 JSWorkerMessageHandler *port)
{
    JSWorkerMessagePipe *ps = port->recv_pipe;
    int ret;
    struct list_head *el;
    JSWorkerMessage *msg;
    JSValue obj, data_obj, func, retval;

    pthread_mutex_lock(&ps->mutex);
    if (!list_empty(&ps->msg_queue)) {
        el = ps->msg_queue.next;
        msg = list_entry(el, JSWorkerMessage, link);

        /* remove the message from the queue */
        list_del(&msg->link);

        if (list_empty(&ps->msg_queue))
            js_waker_clear(&ps->waker);

        pthread_mutex_unlock(&ps->mutex);

        data_obj = JS_ReadObject(ctx, msg->data, msg->data_len,
                                 JS_READ_OBJ_SAB | JS_READ_OBJ_REFERENCE);

        js_free_message(msg);

        if (JS_IsException(data_obj))
            goto fail;
        obj = JS_NewObject(ctx);
        if (JS_IsException(obj)) {
            JS_FreeValue(ctx, data_obj);
            goto fail;
        }
        JS_DefinePropertyValueStr(ctx, obj, "data", data_obj, JS_PROP_C_W_E);

        /* 'func' might be destroyed when calling itself (if it frees the
           handler), so must take extra care */
        func = JS_DupValue(ctx, port->on_message_func);
        retval = JS_Call(ctx, func, JS_UNDEFINED, 1, (JSValueConst *)&obj);
        JS_FreeValue(ctx, obj);
        JS_FreeValue(ctx, func);
        if (JS_IsException(retval)) {
        fail:
            js_std_dump_error(ctx);
        } else {
            JS_FreeValue(ctx, retval);
        }
        ret = 1;
    } else {
        pthread_mutex_unlock(&ps->mutex);
        ret = 0;
    }
    return ret;
}
#else
static int handle_posted_message(JSRuntime *rt, JSContext *ctx,
                                 JSWorkerMessageHandler *port)
{
    return 0;
}
#endif /* !USE_WORKER */

#if defined(_WIN32)

static int js_os_poll(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    int min_delay, count;
    int64_t cur_time, delay;
    JSOSRWHandler *rh;
    struct list_head *el;
    HANDLE handles[MAXIMUM_WAIT_OBJECTS]; // 64

    /* XXX: handle signals if useful */

    if (list_empty(&ts->os_rw_handlers) && list_empty(&ts->os_timers) &&
        list_empty(&ts->port_list)) {
        return -1; /* no more events */
    }

    if (!list_empty(&ts->os_timers)) {
        cur_time = get_time_ms();
        min_delay = 10000;
        list_for_each(el, &ts->os_timers) {
            JSOSTimer *th = list_entry(el, JSOSTimer, link);
            delay = th->timeout - cur_time;
            if (delay <= 0) {
                JSValue func;
                /* the timer expired */
                func = th->func;
                th->func = JS_UNDEFINED;
                free_timer(rt, th);
                call_handler(ctx, func);
                JS_FreeValue(ctx, func);
                return 0;
            } else if (delay < min_delay) {
                min_delay = delay;
            }
        }
    } else {
        min_delay = -1;
    }

    count = 0;
    list_for_each(el, &ts->os_rw_handlers) {
        rh = list_entry(el, JSOSRWHandler, link);
        if (rh->fd == 0 && !JS_IsNull(rh->rw_func[0])) {
            handles[count++] = (HANDLE)_get_osfhandle(rh->fd); // stdin
            if (count == (int)countof(handles))
                break;
        }
    }

    list_for_each(el, &ts->port_list) {
        JSWorkerMessageHandler *port = list_entry(el, JSWorkerMessageHandler, link);
        if (JS_IsNull(port->on_message_func))
            continue;
        handles[count++] = port->recv_pipe->waker.handle;
        if (count == (int)countof(handles))
            break;
    }

    if (count > 0) {
        DWORD ret, timeout = INFINITE;
        if (min_delay != -1)
            timeout = min_delay;
        ret = WaitForMultipleObjects(count, handles, FALSE, timeout);

        if (ret < count) {
            list_for_each(el, &ts->os_rw_handlers) {
                rh = list_entry(el, JSOSRWHandler, link);
                if (rh->fd == 0 && !JS_IsNull(rh->rw_func[0])) {
                    call_handler(ctx, rh->rw_func[0]);
                    /* must stop because the list may have been modified */
                    goto done;
                }
            }

            list_for_each(el, &ts->port_list) {
                JSWorkerMessageHandler *port = list_entry(el, JSWorkerMessageHandler, link);
                if (!JS_IsNull(port->on_message_func)) {
                    JSWorkerMessagePipe *ps = port->recv_pipe;
                    if (ps->waker.handle == handles[ret]) {
                        if (handle_posted_message(rt, ctx, port))
                            goto done;
                    }
                }
            }
        }
    } else {
        Sleep(min_delay);
    }
 done:
    return 0;
}

#else

static no_inline int js_poll_expand(JSThreadState *ts)
{
    struct pollfd *new_fds;
    int new_size = max_int(ts->poll_fds_size +
                           ts->poll_fds_size / 2, 16);
    new_fds = realloc(ts->poll_fds, new_size * sizeof(struct pollfd));
    if (!new_fds)
        return -1;
    ts->poll_fds = new_fds;
    ts->poll_fds_size = new_size;
    return 0;
}

static int js_poll_add_poll_fd(JSThreadState *ts, int *pnfds, int fd, int events)
{
    struct pollfd *fds;
    int nfds;
    nfds = *pnfds;
    if (unlikely(nfds >= ts->poll_fds_size)) {
        if (js_poll_expand(ts))
            return -1;
    }
    fds = &ts->poll_fds[nfds++];
    fds->fd = fd;
    fds->events = events;
    fds->revents = 0;
    *pnfds = nfds;
    return 0;
}

static int js_os_poll(JSContext *ctx)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    int min_delay, nfds;
    int64_t cur_time, delay;
    JSOSRWHandler *rh;
    struct list_head *el;

    /* only check signals in the main thread */
    if (!ts->recv_pipe &&
        unlikely(os_pending_signals != 0)) {
        JSOSSignalHandler *sh;
        uint64_t mask;

        list_for_each(el, &ts->os_signal_handlers) {
            sh = list_entry(el, JSOSSignalHandler, link);
            mask = (uint64_t)1 << sh->sig_num;
            if (os_pending_signals & mask) {
                os_pending_signals &= ~mask;
                call_handler(ctx, sh->func);
                return 0;
            }
        }
    }

    if (list_empty(&ts->os_rw_handlers) && list_empty(&ts->os_timers) &&
        list_empty(&ts->port_list))
        return -1; /* no more events */

    if (!list_empty(&ts->os_timers)) {
        cur_time = get_time_ms();
        min_delay = 10000;
        list_for_each(el, &ts->os_timers) {
            JSOSTimer *th = list_entry(el, JSOSTimer, link);
            delay = th->timeout - cur_time;
            if (delay <= 0) {
                JSValue func;
                /* the timer expired */
                func = th->func;
                th->func = JS_UNDEFINED;
                free_timer(rt, th);
                call_handler(ctx, func);
                JS_FreeValue(ctx, func);
                return 0;
            } else if (delay < min_delay) {
                min_delay = delay;
            }
        }
    } else {
        min_delay = -1; /* infinite */
    }

    nfds = 0;
    list_for_each(el, &ts->os_rw_handlers) {
        int events;

        rh = list_entry(el, JSOSRWHandler, link);
        events = 0;
        if (!JS_IsNull(rh->rw_func[0]))
            events |= POLLIN;
        if (!JS_IsNull(rh->rw_func[1]))
            events |= POLLOUT;
        if (events) {
            rh->poll_fd_index = nfds;
            if (js_poll_add_poll_fd(ts, &nfds, rh->fd, events))
                return -1;
        }
    }

    list_for_each(el, &ts->port_list) {
        JSWorkerMessageHandler *port = list_entry(el, JSWorkerMessageHandler, link);
        if (!JS_IsNull(port->on_message_func)) {
            JSWorkerMessagePipe *ps = port->recv_pipe;
            port->poll_fd_index = nfds;
            if (js_poll_add_poll_fd(ts, &nfds, ps->waker.read_fd, POLLIN))
                return -1;
        }
    }

    nfds = poll(ts->poll_fds, nfds, min_delay);
    if (nfds > 0) {
        list_for_each(el, &ts->os_rw_handlers) {
            rh = list_entry(el, JSOSRWHandler, link);
            if (!JS_IsNull(rh->rw_func[0]) &&
                (ts->poll_fds[rh->poll_fd_index].revents & (POLLERR | POLLHUP | POLLNVAL | POLLIN))) {
                call_handler(ctx, rh->rw_func[0]);
                /* must stop because the list may have been modified */
                goto done;
            }
            if (!JS_IsNull(rh->rw_func[1]) &&
                (ts->poll_fds[rh->poll_fd_index].revents & (POLLERR | POLLHUP | POLLNVAL | POLLOUT))) {
                call_handler(ctx, rh->rw_func[1]);
                /* must stop because the list may have been modified */
                goto done;
            }
        }

        list_for_each(el, &ts->port_list) {
            JSWorkerMessageHandler *port = list_entry(el, JSWorkerMessageHandler, link);
            if (!JS_IsNull(port->on_message_func)) {
                if (ts->poll_fds[port->poll_fd_index].revents != 0) {
                    if (handle_posted_message(rt, ctx, port))
                        goto done;
                }
            }
        }
    }
 done:
    return 0;
}
#endif /* !_WIN32 */

#ifdef USE_WORKER

/* Worker */

typedef struct {
    JSWorkerMessagePipe *recv_pipe;
    JSWorkerMessagePipe *send_pipe;
    JSWorkerMessageHandler *msg_handler;
} JSWorkerData;

typedef struct {
    char *filename; /* module filename */
    char *basename; /* module base name */
    JSWorkerMessagePipe *recv_pipe, *send_pipe;
    int strip_flags;
} WorkerFuncArgs;

typedef struct {
    int ref_count;
    uint64_t buf[0];
} JSSABHeader;

static JSClassID js_worker_class_id;
static JSContext *(*js_worker_new_context_func)(JSRuntime *rt);

static int atomic_add_int(int *ptr, int v)
{
    return atomic_fetch_add((_Atomic(uint32_t) *)ptr, v) + v;
}

/* shared array buffer allocator */
static void *js_sab_alloc(void *opaque, size_t size)
{
    JSSABHeader *sab;
    sab = malloc(sizeof(JSSABHeader) + size);
    if (!sab)
        return NULL;
    sab->ref_count = 1;
    return sab->buf;
}

static void js_sab_free(void *opaque, void *ptr)
{
    JSSABHeader *sab;
    int ref_count;
    sab = (JSSABHeader *)((uint8_t *)ptr - sizeof(JSSABHeader));
    ref_count = atomic_add_int(&sab->ref_count, -1);
    assert(ref_count >= 0);
    if (ref_count == 0) {
        free(sab);
    }
}

static void js_sab_dup(void *opaque, void *ptr)
{
    JSSABHeader *sab;
    sab = (JSSABHeader *)((uint8_t *)ptr - sizeof(JSSABHeader));
    atomic_add_int(&sab->ref_count, 1);
}

static JSWorkerMessagePipe *js_new_message_pipe(void)
{
    JSWorkerMessagePipe *ps;

    ps = malloc(sizeof(*ps));
    if (!ps)
        return NULL;
    if (js_waker_init(&ps->waker)) {
        free(ps);
        return NULL;
    }
    ps->ref_count = 1;
    init_list_head(&ps->msg_queue);
    pthread_mutex_init(&ps->mutex, NULL);
    return ps;
}

static JSWorkerMessagePipe *js_dup_message_pipe(JSWorkerMessagePipe *ps)
{
    atomic_add_int(&ps->ref_count, 1);
    return ps;
}

static void js_free_message(JSWorkerMessage *msg)
{
    size_t i;
    /* free the SAB */
    for(i = 0; i < msg->sab_tab_len; i++) {
        js_sab_free(NULL, msg->sab_tab[i]);
    }
    free(msg->sab_tab);
    free(msg->data);
    free(msg);
}

static void js_free_message_pipe(JSWorkerMessagePipe *ps)
{
    struct list_head *el, *el1;
    JSWorkerMessage *msg;
    int ref_count;

    if (!ps)
        return;

    ref_count = atomic_add_int(&ps->ref_count, -1);
    assert(ref_count >= 0);
    if (ref_count == 0) {
        list_for_each_safe(el, el1, &ps->msg_queue) {
            msg = list_entry(el, JSWorkerMessage, link);
            js_free_message(msg);
        }
        pthread_mutex_destroy(&ps->mutex);
        js_waker_close(&ps->waker);
        free(ps);
    }
}

static void js_free_port(JSRuntime *rt, JSWorkerMessageHandler *port)
{
    if (port) {
        js_free_message_pipe(port->recv_pipe);
        JS_FreeValueRT(rt, port->on_message_func);
        if (port->link.prev)
            list_del(&port->link);
        js_free_rt(rt, port);
    }
}

static void js_worker_finalizer(JSRuntime *rt, JSValue val)
{
    JSWorkerData *worker = JS_GetOpaque(val, js_worker_class_id);
    if (worker) {
        js_free_message_pipe(worker->recv_pipe);
        js_free_message_pipe(worker->send_pipe);
        js_free_port(rt, worker->msg_handler);
        js_free_rt(rt, worker);
    }
}

static void js_worker_mark(JSRuntime *rt, JSValueConst val,
                           JS_MarkFunc *mark_func)
{
    JSWorkerData *worker = JS_GetOpaque(val, js_worker_class_id);
    if (worker) {
        JSWorkerMessageHandler *port = worker->msg_handler;
        if (port) {
            JS_MarkValue(rt, port->on_message_func, mark_func);
        }
    }
}

static JSClassDef js_worker_class = {
    "Worker",
    .finalizer = js_worker_finalizer,
    .gc_mark = js_worker_mark,
};

static void *worker_func(void *opaque)
{
    WorkerFuncArgs *args = opaque;
    JSRuntime *rt;
    JSThreadState *ts;
    JSContext *ctx;
    JSValue val;

    rt = JS_NewRuntime();
    if (rt == NULL) {
        fprintf(stderr, "JS_NewRuntime failure");
        exit(1);
    }
    JS_SetStripInfo(rt, args->strip_flags);
    js_std_init_handlers(rt);

    JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader, js_module_check_attributes, NULL);

    /* set the pipe to communicate with the parent */
    ts = JS_GetRuntimeOpaque(rt);
    ts->recv_pipe = args->recv_pipe;
    ts->send_pipe = args->send_pipe;

    /* function pointer to avoid linking the whole JS_NewContext() if
       not needed */
    ctx = js_worker_new_context_func(rt);
    if (ctx == NULL) {
        fprintf(stderr, "JS_NewContext failure");
    }

    JS_SetCanBlock(rt, TRUE);

    js_std_add_helpers(ctx, -1, NULL);

    val = JS_LoadModule(ctx, args->basename, args->filename);
    free(args->filename);
    free(args->basename);
    free(args);
    val = js_std_await(ctx, val);
    if (JS_IsException(val))
        js_std_dump_error(ctx);
    JS_FreeValue(ctx, val);

    js_std_loop(ctx);

    JS_FreeContext(ctx);
    js_std_free_handlers(rt);
    JS_FreeRuntime(rt);
    return NULL;
}

static JSValue js_worker_ctor_internal(JSContext *ctx, JSValueConst new_target,
                                       JSWorkerMessagePipe *recv_pipe,
                                       JSWorkerMessagePipe *send_pipe)
{
    JSValue obj = JS_UNDEFINED, proto;
    JSWorkerData *s;

    /* create the object */
    if (JS_IsUndefined(new_target)) {
        proto = JS_GetClassProto(ctx, js_worker_class_id);
    } else {
        proto = JS_GetPropertyStr(ctx, new_target, "prototype");
        if (JS_IsException(proto))
            goto fail;
    }
    obj = JS_NewObjectProtoClass(ctx, proto, js_worker_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        goto fail;
    s->recv_pipe = js_dup_message_pipe(recv_pipe);
    s->send_pipe = js_dup_message_pipe(send_pipe);

    JS_SetOpaque(obj, s);
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_worker_ctor(JSContext *ctx, JSValueConst new_target,
                              int argc, JSValueConst *argv)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    WorkerFuncArgs *args = NULL;
    pthread_t tid;
    pthread_attr_t attr;
    JSValue obj = JS_UNDEFINED;
    int ret;
    const char *filename = NULL, *basename;
    JSAtom basename_atom;

    /* XXX: in order to avoid problems with resource liberation, we
       don't support creating workers inside workers */
    if (!is_main_thread(rt))
        return JS_ThrowTypeError(ctx, "cannot create a worker inside a worker");

    /* base name, assuming the calling function is a normal JS
       function */
    basename_atom = JS_GetScriptOrModuleName(ctx, 1);
    if (basename_atom == JS_ATOM_NULL) {
        return JS_ThrowTypeError(ctx, "could not determine calling script or module name");
    }
    basename = JS_AtomToCString(ctx, basename_atom);
    JS_FreeAtom(ctx, basename_atom);
    if (!basename)
        goto fail;

    /* module name */
    filename = JS_ToCString(ctx, argv[0]);
    if (!filename)
        goto fail;

    args = malloc(sizeof(*args));
    if (!args)
        goto oom_fail;
    memset(args, 0, sizeof(*args));
    args->filename = strdup(filename);
    args->basename = strdup(basename);

    /* ports */
    args->recv_pipe = js_new_message_pipe();
    if (!args->recv_pipe)
        goto oom_fail;
    args->send_pipe = js_new_message_pipe();
    if (!args->send_pipe)
        goto oom_fail;

    args->strip_flags = JS_GetStripInfo(rt);
    
    obj = js_worker_ctor_internal(ctx, new_target,
                                  args->send_pipe, args->recv_pipe);
    if (JS_IsException(obj))
        goto fail;

    pthread_attr_init(&attr);
    /* no join at the end */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&tid, &attr, worker_func, args);
    pthread_attr_destroy(&attr);
    if (ret != 0) {
        JS_ThrowTypeError(ctx, "could not create worker");
        goto fail;
    }
    JS_FreeCString(ctx, basename);
    JS_FreeCString(ctx, filename);
    return obj;
 oom_fail:
    JS_ThrowOutOfMemory(ctx);
 fail:
    JS_FreeCString(ctx, basename);
    JS_FreeCString(ctx, filename);
    if (args) {
        free(args->filename);
        free(args->basename);
        js_free_message_pipe(args->recv_pipe);
        js_free_message_pipe(args->send_pipe);
        free(args);
    }
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_worker_postMessage(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSWorkerData *worker = JS_GetOpaque2(ctx, this_val, js_worker_class_id);
    JSWorkerMessagePipe *ps;
    size_t data_len, sab_tab_len, i;
    uint8_t *data;
    JSWorkerMessage *msg;
    uint8_t **sab_tab;

    if (!worker)
        return JS_EXCEPTION;

    data = JS_WriteObject2(ctx, &data_len, argv[0],
                           JS_WRITE_OBJ_SAB | JS_WRITE_OBJ_REFERENCE,
                           &sab_tab, &sab_tab_len);
    if (!data)
        return JS_EXCEPTION;

    msg = malloc(sizeof(*msg));
    if (!msg)
        goto fail;
    msg->data = NULL;
    msg->sab_tab = NULL;

    /* must reallocate because the allocator may be different */
    msg->data = malloc(data_len);
    if (!msg->data)
        goto fail;
    memcpy(msg->data, data, data_len);
    msg->data_len = data_len;

    if (sab_tab_len > 0) {
        msg->sab_tab = malloc(sizeof(msg->sab_tab[0]) * sab_tab_len);
        if (!msg->sab_tab)
            goto fail;
        memcpy(msg->sab_tab, sab_tab, sizeof(msg->sab_tab[0]) * sab_tab_len);
    }
    msg->sab_tab_len = sab_tab_len;

    js_free(ctx, data);
    js_free(ctx, sab_tab);

    /* increment the SAB reference counts */
    for(i = 0; i < msg->sab_tab_len; i++) {
        js_sab_dup(NULL, msg->sab_tab[i]);
    }

    ps = worker->send_pipe;
    pthread_mutex_lock(&ps->mutex);
    /* indicate that data is present */
    if (list_empty(&ps->msg_queue))
        js_waker_signal(&ps->waker);
    list_add_tail(&msg->link, &ps->msg_queue);
    pthread_mutex_unlock(&ps->mutex);
    return JS_UNDEFINED;
 fail:
    if (msg) {
        free(msg->data);
        free(msg->sab_tab);
        free(msg);
    }
    js_free(ctx, data);
    js_free(ctx, sab_tab);
    return JS_EXCEPTION;

}

static JSValue js_worker_set_onmessage(JSContext *ctx, JSValueConst this_val,
                                   JSValueConst func)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    JSWorkerData *worker = JS_GetOpaque2(ctx, this_val, js_worker_class_id);
    JSWorkerMessageHandler *port;

    if (!worker)
        return JS_EXCEPTION;

    port = worker->msg_handler;
    if (JS_IsNull(func)) {
        if (port) {
            js_free_port(rt, port);
            worker->msg_handler = NULL;
        }
    } else {
        if (!JS_IsFunction(ctx, func))
            return JS_ThrowTypeError(ctx, "not a function");
        if (!port) {
            port = js_mallocz(ctx, sizeof(*port));
            if (!port)
                return JS_EXCEPTION;
            port->recv_pipe = js_dup_message_pipe(worker->recv_pipe);
            port->on_message_func = JS_NULL;
            list_add_tail(&port->link, &ts->port_list);
            worker->msg_handler = port;
        }
        JS_FreeValue(ctx, port->on_message_func);
        port->on_message_func = JS_DupValue(ctx, func);
    }
    return JS_UNDEFINED;
}

static JSValue js_worker_get_onmessage(JSContext *ctx, JSValueConst this_val)
{
    JSWorkerData *worker = JS_GetOpaque2(ctx, this_val, js_worker_class_id);
    JSWorkerMessageHandler *port;
    if (!worker)
        return JS_EXCEPTION;
    port = worker->msg_handler;
    if (port) {
        return JS_DupValue(ctx, port->on_message_func);
    } else {
        return JS_NULL;
    }
}

static const JSCFunctionListEntry js_worker_proto_funcs[] = {
    JS_CFUNC_DEF("postMessage", 1, js_worker_postMessage ),
    JS_CGETSET_DEF("onmessage", js_worker_get_onmessage, js_worker_set_onmessage ),
};

#endif /* USE_WORKER */

static const JSCFunctionListEntry js_os_event_funcs[] = {
    JS_CFUNC_MAGIC_DEF("setReadHandler", 2, js_os_setReadHandler, 0 ),
    JS_CFUNC_MAGIC_DEF("setWriteHandler", 2, js_os_setReadHandler, 1 ),
    JS_CFUNC_DEF("signal", 2, js_os_signal ),
    JS_CFUNC_DEF("now", 0, js_os_now ),
    JS_CFUNC_DEF("setTimeout", 2, js_os_setTimeout ),
    JS_CFUNC_DEF("clearTimeout", 1, js_os_clearTimeout ),
    JS_CFUNC_DEF("sleepAsync", 1, js_os_sleepAsync ),
};

int qjs_libc_init_event_module(JSContext *ctx, JSModuleDef *m)
{
    os_poll_func = js_os_poll;

#ifdef USE_WORKER
    {
        JSRuntime *rt = JS_GetRuntime(ctx);
        JSThreadState *ts = JS_GetRuntimeOpaque(rt);
        JSValue proto, obj;

        /* Worker class */
        JS_NewClassID(&js_worker_class_id);
        JS_NewClass(rt, js_worker_class_id, &js_worker_class);
        proto = JS_NewObject(ctx);
        JS_SetPropertyFunctionList(ctx, proto, js_worker_proto_funcs,
                                   countof(js_worker_proto_funcs));
        obj = JS_NewCFunction2(ctx, js_worker_ctor, "Worker", 1,
                              JS_CFUNC_constructor, 0);
        JS_SetConstructor(ctx, obj, proto);
        JS_SetClassProto(ctx, js_worker_class_id, proto);

        /* set 'Worker.parent' if necessary */
        if (ts->recv_pipe && ts->send_pipe) {
            JS_DefinePropertyValueStr(
                ctx, obj, "parent",
                js_worker_ctor_internal(ctx, JS_UNDEFINED, ts->recv_pipe,
                                        ts->send_pipe),
                JS_PROP_C_W_E);
        }
        JS_SetModuleExport(ctx, m, "Worker", obj);
    }
#endif /* USE_WORKER */

    return JS_SetModuleExportList(ctx, m, js_os_event_funcs,
                                  countof(js_os_event_funcs));
}

void qjs_libc_add_event_module_exports(JSContext *ctx, JSModuleDef *m)
{
    JS_AddModuleExportList(ctx, m, js_os_event_funcs,
                           countof(js_os_event_funcs));
#ifdef USE_WORKER
    JS_AddModuleExport(ctx, m, "Worker");
#endif
}

void js_std_set_worker_new_context_func(JSContext *(*func)(JSRuntime *rt))
{
#ifdef USE_WORKER
    js_worker_new_context_func = func;
#endif
}


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
    JSValue global_obj, console, args, performance;
    int i;

    /* XXX: should these global definitions be enumerable? */
    global_obj = JS_GetGlobalObject(ctx);

    console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log",
                      JS_NewCFunction(ctx, js_console_log, "log", 1));
    JS_SetPropertyStr(ctx, global_obj, "console", console);

    performance = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, performance, "now",
                      JS_NewCFunction(ctx, js_os_now, "now", 0));
    JS_SetPropertyStr(ctx, global_obj, "performance", performance);

    /* same methods as the mozilla JS shell */
    if (argc >= 0) {
        args = JS_NewArray(ctx);
        for(i = 0; i < argc; i++) {
            JS_SetPropertyUint32(ctx, args, i, JS_NewString(ctx, argv[i]));
        }
        JS_SetPropertyStr(ctx, global_obj, "scriptArgs", args);
    }

    JS_SetPropertyStr(ctx, global_obj, "print",
                      JS_NewCFunction(ctx, js_print, "print", 1));
    JS_SetPropertyStr(ctx, global_obj, "__loadScript",
                      JS_NewCFunction(ctx, js_loadScript, "__loadScript", 1));

    JS_FreeValue(ctx, global_obj);
}

void js_std_init_handlers(JSRuntime *rt)
{
    JSThreadState *ts;

    ts = malloc(sizeof(*ts));
    if (!ts) {
        fprintf(stderr, "Could not allocate memory for the worker");
        exit(1);
    }
    memset(ts, 0, sizeof(*ts));
    init_list_head(&ts->os_rw_handlers);
    init_list_head(&ts->os_signal_handlers);
    init_list_head(&ts->os_timers);
    init_list_head(&ts->port_list);
    init_list_head(&ts->rejected_promise_list);
    ts->next_timer_id = 1;

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
        free_sh(rt, sh);
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
        
        if (!os_poll_func || os_poll_func(ctx))
            break;
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
                js_std_promise_rejection_check(ctx);

                if (os_poll_func)
                    os_poll_func(ctx);
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
            js_module_set_import_meta(ctx, obj, FALSE, FALSE);
        }
        JS_FreeValue(ctx, obj);
    } else {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_MODULE) {
            if (JS_ResolveModule(ctx, obj) < 0) {
                JS_FreeValue(ctx, obj);
                goto exception;
            }
            js_module_set_import_meta(ctx, obj, FALSE, TRUE);
            val = JS_EvalFunction(ctx, obj);
            val = js_std_await(ctx, val);
        } else {
            val = JS_EvalFunction(ctx, obj);
        }
        if (JS_IsException(val)) {
        exception:
            js_std_dump_error(ctx);
            exit(1);
        }
        JS_FreeValue(ctx, val);
    }
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
