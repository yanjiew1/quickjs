/*
 * QuickJS host polling, signals and timers
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
#include <inttypes.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#else
#include <poll.h>
#include <fcntl.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
typedef sig_t sighandler_t;
#endif
#endif
#include "cutils.h"
#include "quickjs-libc.h"
#include "event-loop.h"
#include "worker.h"

/* A signal handler may write sig_atomic_t, but must not update a shared mask. */
volatile sig_atomic_t os_pending_signals[64];
static JSThreadState *signal_owner[64];
static pthread_mutex_t signal_owner_mutex = PTHREAD_MUTEX_INITIALIZER;
#ifndef _WIN32
/* Keep a wake pipe for each installed signal for the process lifetime. A
   handler can still be running while its owner unregisters the signal. */
static int signal_wake_read_fd[64];
static volatile sig_atomic_t signal_wake_write_plus_one[64];

static int init_signal_wake(unsigned sig_num)
{
    int fds[2], i, flags;

    if (signal_wake_write_plus_one[sig_num])
        return 0;
    if (pipe(fds) < 0)
        return -1;
    for (i = 0; i < 2; i++) {
        flags = fcntl(fds[i], F_GETFL);
        if (flags < 0 || fcntl(fds[i], F_SETFL, flags | O_NONBLOCK) < 0)
            goto fail;
        flags = fcntl(fds[i], F_GETFD);
        if (flags < 0 || fcntl(fds[i], F_SETFD, flags | FD_CLOEXEC) < 0)
            goto fail;
    }
    signal_wake_read_fd[sig_num] = fds[0];
    signal_wake_write_plus_one[sig_num] = fds[1] + 1;
    return 0;

fail:
    close(fds[0]);
    close(fds[1]);
    return -1;
}

static void drain_signal_wake(unsigned sig_num)
{
    char buf[64];

    while (read(signal_wake_read_fd[sig_num], buf, sizeof(buf)) > 0)
        ;
}
#endif
_Atomic(JSOSPollFunc *) os_poll_func;

int js_os_eval_sigint_pending(JSThreadState *ts)
{
    int pending;

    pthread_mutex_lock(&signal_owner_mutex);
    pending = os_pending_signals[SIGINT] &&
        (!signal_owner[SIGINT] || signal_owner[SIGINT] == ts);
    pthread_mutex_unlock(&signal_owner_mutex);
    return pending;
}

void js_os_clear_eval_sigint(JSThreadState *ts)
{
    pthread_mutex_lock(&signal_owner_mutex);
    if (!signal_owner[SIGINT] || signal_owner[SIGINT] == ts)
        os_pending_signals[SIGINT] = 0;
    pthread_mutex_unlock(&signal_owner_mutex);
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

void free_rw_handler(JSRuntime *rt, JSOSRWHandler *rh)
{
    int i;
    list_del(&rh->link);
    for(i = 0; i < 2; i++) {
        JS_FreeValueRT(rt, rh->rw_func[i]);
    }
    js_free_rt(rt, rh);
}

JSValue js_os_setReadHandler(JSContext *ctx, JSValueConst this_val,
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

void free_sh(JSRuntime *rt, JSOSSignalHandler *sh, JS_BOOL restore_handler)
{
    if (restore_handler) {
        pthread_mutex_lock(&signal_owner_mutex);
        if (signal_owner[sh->sig_num] == JS_GetRuntimeOpaque(rt)) {
#ifndef _WIN32
            sigaction(sh->sig_num, &sh->previous_action, NULL);
#else
            signal(sh->sig_num, sh->previous_handler);
#endif
            signal_owner[sh->sig_num] = NULL;
            os_pending_signals[sh->sig_num] = 0;
        }
        pthread_mutex_unlock(&signal_owner_mutex);
    }
    list_del(&sh->link);
    JS_FreeValueRT(rt, sh->func);
    js_free_rt(rt, sh);
}

static void os_signal_handler(int sig_num)
{
    if ((unsigned)sig_num < 64) {
        os_pending_signals[sig_num] = 1;
#ifndef _WIN32
        if (signal_wake_write_plus_one[sig_num]) {
            int saved_errno = errno;
            char byte = 0;
            ssize_t written = write(signal_wake_write_plus_one[sig_num] - 1,
                                    &byte, 1);
            /* A full nonblocking pipe is already readable by the owner. */
            (void)written;
            errno = saved_errno;
        }
#endif
    }
}

#if defined(_WIN32)
typedef void (*sighandler_t)(int sig_num);
#endif

JSValue js_os_signal(JSContext *ctx, JSValueConst this_val,
                     int argc, JSValueConst *argv)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    JSOSSignalHandler *sh;
    uint32_t sig_num;
    JSValueConst func;
    sighandler_t handler;
    BOOL new_handler;
#ifndef _WIN32
    struct sigaction action, previous_action;
#endif

    if (!is_main_thread(rt))
        return JS_ThrowTypeError(ctx, "signal handler can only be set in the main thread");

    if (JS_ToUint32(ctx, &sig_num, argv[0]))
        return JS_EXCEPTION;
    if (sig_num >= 64)
        return JS_ThrowRangeError(ctx, "invalid signal number");
    func = argv[1];
    /* func = null: SIG_DFL, func = undefined, SIG_IGN */
    if (JS_IsNull(func) || JS_IsUndefined(func)) {
        if (JS_IsNull(func))
            handler = SIG_DFL;
        else
            handler = SIG_IGN;
        pthread_mutex_lock(&signal_owner_mutex);
        if (signal_owner[sig_num] && signal_owner[sig_num] != ts) {
            pthread_mutex_unlock(&signal_owner_mutex);
            return JS_ThrowTypeError(ctx, "signal is owned by another runtime");
        }
#ifndef _WIN32
        memset(&action, 0, sizeof(action));
        action.sa_handler = handler;
        sigemptyset(&action.sa_mask);
        if (sigaction(sig_num, &action, NULL) < 0) {
#else
        if (signal(sig_num, handler) == SIG_ERR) {
#endif
            pthread_mutex_unlock(&signal_owner_mutex);
            return JS_ThrowTypeError(ctx, "could not set signal handler");
        }
        sh = find_sh(ts, sig_num);
        if (sh) {
            signal_owner[sig_num] = NULL;
            os_pending_signals[sig_num] = 0;
        }
        pthread_mutex_unlock(&signal_owner_mutex);
        if (sh)
            free_sh(rt, sh, FALSE);
    } else {
        if (!JS_IsFunction(ctx, func))
            return JS_ThrowTypeError(ctx, "not a function");
        sh = find_sh(ts, sig_num);
        new_handler = !sh;
        if (!sh) {
            sh = js_mallocz(ctx, sizeof(*sh));
            if (!sh)
                return JS_EXCEPTION;
        }
        pthread_mutex_lock(&signal_owner_mutex);
        if (signal_owner[sig_num] && signal_owner[sig_num] != ts) {
            pthread_mutex_unlock(&signal_owner_mutex);
            if (new_handler)
                js_free(ctx, sh);
            return JS_ThrowTypeError(ctx, "signal is owned by another runtime");
        }
#ifndef _WIN32
        if (init_signal_wake(sig_num) < 0) {
            pthread_mutex_unlock(&signal_owner_mutex);
            if (new_handler)
                js_free(ctx, sh);
            return JS_ThrowInternalError(ctx, "could not create signal wake pipe");
        }
        memset(&action, 0, sizeof(action));
        action.sa_handler = os_signal_handler;
        sigemptyset(&action.sa_mask);
#ifdef SA_RESTART
        action.sa_flags = SA_RESTART;
#endif
        if (sigaction(sig_num, &action, &previous_action) < 0) {
#else
        handler = signal(sig_num, os_signal_handler);
        if (handler == SIG_ERR) {
#endif
            pthread_mutex_unlock(&signal_owner_mutex);
            if (new_handler)
                js_free(ctx, sh);
            return JS_ThrowTypeError(ctx, "could not set signal handler");
        }
        if (new_handler) {
            sh->sig_num = sig_num;
#ifndef _WIN32
            sh->previous_action = previous_action;
#else
            sh->previous_handler = handler;
#endif
            signal_owner[sig_num] = ts;
        }
        pthread_mutex_unlock(&signal_owner_mutex);
        if (new_handler)
            list_add_tail(&sh->link, &ts->os_signal_handlers);
        JS_FreeValue(ctx, sh->func);
        sh->func = JS_DupValue(ctx, func);
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

JSValue js_os_now(JSContext *ctx, JSValueConst this_val,
                  int argc, JSValueConst *argv)
{
    return JS_NewFloat64(ctx, (double)get_time_ns() / 1e6);
}

void free_timer(JSRuntime *rt, JSOSTimer *th)
{
    list_del(&th->link);
    JS_FreeValueRT(rt, th->func);
    js_free_rt(rt, th);
}

/* Negative delays are already due. A very distant future timer saturates
   instead of overflowing the signed clock representation. */
static int64_t timer_deadline(int64_t delay)
{
    int64_t now = get_time_ms();
    if (delay <= 0)
        return now;
    if (now > INT64_MAX - delay)
        return INT64_MAX;
    return now + delay;
}

JSValue js_os_setTimeout(JSContext *ctx, JSValueConst this_val,
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
    th->timeout = timer_deadline(delay);
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

JSValue js_os_clearTimeout(JSContext *ctx, JSValueConst this_val,
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
JSValue js_os_sleepAsync(JSContext *ctx, JSValueConst this_val,
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
    th->timeout = timer_deadline(delay);
    th->func = JS_DupValue(ctx, resolving_funcs[0]);
    list_add_tail(&th->link, &ts->os_timers);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return promise;
}

static int call_handler(JSContext *ctx, JSValueConst func)
{
    JSValue ret, func1;
    /* 'func' might be destroyed when calling itself (if it frees the
       handler), so must take extra care */
    func1 = JS_DupValue(ctx, func);
    ret = JS_Call(ctx, func1, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, func1);
    if (JS_IsException(ret))
        return -3;
    JS_FreeValue(ctx, ret);
    return 1;
}



#if defined(_WIN32)

int js_os_poll(JSContext *ctx, int timeout_ms)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    int min_delay, count, worker_base, native_delay, native_index = -1;
    size_t worker_count = 0;
    intptr_t native_wake;
    int64_t cur_time;
    uint64_t delay;
    JSOSRWHandler *rh;
    struct list_head *el;
    HANDLE handles[MAXIMUM_WAIT_OBJECTS]; // 64

    /* XXX: handle signals if useful */

    native_delay = JS_PollNativeJobs(rt, 0);
    if (native_delay < 0) {
        JS_ThrowInternalError(ctx, "native job polling failed");
        return -2;
    }
    if (native_delay > 0)
        return 1;
    if (list_empty(&ts->os_rw_handlers) && list_empty(&ts->os_timers) &&
        list_empty(&ts->port_list) && !JS_IsNativeJobPending(rt)) {
        return -1; /* no more events */
    }

    if (!list_empty(&ts->os_timers)) {
        cur_time = get_time_ms();
        min_delay = 10000;
        list_for_each(el, &ts->os_timers) {
            JSOSTimer *th = list_entry(el, JSOSTimer, link);
            if (th->timeout <= cur_time) {
                JSValue func;
                /* the timer expired */
                func = th->func;
                th->func = JS_UNDEFINED;
                free_timer(rt, th);
                int ret = call_handler(ctx, func);
                JS_FreeValue(ctx, func);
                return ret;
            } else {
                delay = (uint64_t)th->timeout - (uint64_t)cur_time;
                if (delay < (unsigned)min_delay)
                    min_delay = delay;
            }
        }
    } else {
        min_delay = -1;
    }

    native_delay = JS_GetNativeJobTimeout(rt);
    if (native_delay >= 0 && (min_delay < 0 || native_delay < min_delay))
        min_delay = native_delay;
    if (JS_IsJobPending(rt))
        timeout_ms = 0;
    if (timeout_ms >= 0 && (min_delay < 0 || timeout_ms < min_delay))
        min_delay = timeout_ms;
    native_wake = JS_IsNativeJobPending(rt) ?
                  JS_GetNativeJobWakeHandle(rt) : -1;
    count = 0;
    if (native_wake != -1) {
        native_index = count;
        handles[count++] = (HANDLE)native_wake;
    }
    list_for_each(el, &ts->os_rw_handlers) {
        rh = list_entry(el, JSOSRWHandler, link);
        if (rh->fd == 0 && !JS_IsNull(rh->rw_func[0])) {
            handles[count++] = (HANDLE)_get_osfhandle(rh->fd); // stdin
            if (count == (int)countof(handles))
                break;
        }
    }

    worker_base = count;
    list_for_each(el, &ts->port_list) {
        JSWorkerMessageHandler *port = list_entry(el, JSWorkerMessageHandler, link);
        if (JS_IsNull(port->on_message_func))
            continue;
        worker_count++;
        if (count < (int)countof(handles))
            handles[count++] = port->recv_pipe->waker.handle;
    }

    if (worker_count > countof(handles) - worker_base) {
        size_t first = ts->poll_worker_offset % worker_count;
        size_t index = 0, slots = countof(handles) - worker_base;

        /* Keep native wake and stdin in every wait. Rotate the Worker window
           so every port can be selected, even while earlier ports stay ready.
           Bound an idle window's wait because an omitted port cannot wake it. */
        list_for_each(el, &ts->port_list) {
            JSWorkerMessageHandler *port = list_entry(el, JSWorkerMessageHandler, link);
            size_t distance;
            if (JS_IsNull(port->on_message_func))
                continue;
            distance = index >= first ? index - first :
                       worker_count - (first - index);
            if (distance < slots)
                handles[worker_base + distance] = port->recv_pipe->waker.handle;
            index++;
        }
        ts->poll_worker_offset = first + 1 == worker_count ? 0 : first + 1;
        if (min_delay < 0 || min_delay > 10)
            min_delay = 10;
    }

    if (count > 0) {
        DWORD ret, timeout = INFINITE;
        if (min_delay != -1)
            timeout = min_delay;
        ret = WaitForMultipleObjects(count, handles, FALSE, timeout);
        if (ret == WAIT_FAILED) {
            JS_ThrowInternalError(ctx, "host polling failed: %lu",
                                  (unsigned long)GetLastError());
            return -2;
        }
        native_delay = JS_PollNativeJobs(rt, 0);
        if (native_delay < 0) {
            JS_ThrowInternalError(ctx, "native job polling failed");
            return -2;
        }
        if (native_delay > 0)
            return 1;
        if (native_index >= 0 && ret == (DWORD)native_index)
            return 0;

        if (ret < count) {
            list_for_each(el, &ts->os_rw_handlers) {
                rh = list_entry(el, JSOSRWHandler, link);
                if (rh->fd == 0 && !JS_IsNull(rh->rw_func[0]) &&
                    (HANDLE)_get_osfhandle(rh->fd) == handles[ret]) {
                    /* The callback can modify both handler lists. */
                    return call_handler(ctx, rh->rw_func[0]);
                }
            }

            list_for_each(el, &ts->port_list) {
                JSWorkerMessageHandler *port = list_entry(el, JSWorkerMessageHandler, link);
                if (!JS_IsNull(port->on_message_func)) {
                    JSWorkerMessagePipe *ps = port->recv_pipe;
                    if (ps->waker.handle == handles[ret]) {
                        int status = handle_posted_message(rt, ctx, port);
                        if (status)
                            return status < 0 ? -3 : 1;
                    }
                }
            }
        }
    } else {
        Sleep(min_delay);
    }
    return 0;
}

#else

static no_inline int js_poll_expand(JSThreadState *ts)
{
    struct pollfd *new_fds;
    size_t max_size = min_int64(INT_MAX, SIZE_MAX / sizeof(*new_fds));
    size_t new_size = max_int64((int64_t)ts->poll_fds_size +
                               ts->poll_fds_size / 2, 16);

    if (ts->poll_fds_size >= max_size)
        return -1;
    if (new_size > max_size)
        new_size = max_size;
    new_fds = realloc(ts->poll_fds, new_size * sizeof(*new_fds));
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

int js_os_poll(JSContext *ctx, int timeout_ms)
{
    JSRuntime *rt = JS_GetRuntime(ctx);
    JSThreadState *ts = JS_GetRuntimeOpaque(rt);
    int min_delay, nfds, native_delay, native_index = -1;
    intptr_t native_wake;
    int64_t cur_time;
    uint64_t delay;
    JSOSRWHandler *rh;
    struct list_head *el;

    /* only check signals in the main thread */
    if (!ts->recv_pipe) {
        JSOSSignalHandler *sh;

        list_for_each(el, &ts->os_signal_handlers) {
            sh = list_entry(el, JSOSSignalHandler, link);
            if (os_pending_signals[sh->sig_num]) {
                os_pending_signals[sh->sig_num] = 0;
                return call_handler(ctx, sh->func);
            }
        }
    }

    native_delay = JS_PollNativeJobs(rt, 0);
    if (native_delay < 0) {
        JS_ThrowInternalError(ctx, "native job polling failed");
        return -2;
    }
    if (native_delay > 0)
        return 1;
    if (list_empty(&ts->os_rw_handlers) && list_empty(&ts->os_timers) &&
        list_empty(&ts->port_list) && !JS_IsNativeJobPending(rt))
        return -1; /* no more events */

    if (!list_empty(&ts->os_timers)) {
        cur_time = get_time_ms();
        min_delay = 10000;
        list_for_each(el, &ts->os_timers) {
            JSOSTimer *th = list_entry(el, JSOSTimer, link);
            if (th->timeout <= cur_time) {
                JSValue func;
                /* the timer expired */
                func = th->func;
                th->func = JS_UNDEFINED;
                free_timer(rt, th);
                int ret = call_handler(ctx, func);
                JS_FreeValue(ctx, func);
                return ret;
            } else {
                delay = (uint64_t)th->timeout - (uint64_t)cur_time;
                if (delay < (unsigned)min_delay)
                    min_delay = delay;
            }
        }
    } else {
        min_delay = -1; /* infinite */
    }

    native_delay = JS_GetNativeJobTimeout(rt);
    if (native_delay >= 0 && (min_delay < 0 || native_delay < min_delay))
        min_delay = native_delay;
    if (JS_IsJobPending(rt))
        timeout_ms = 0;
    if (timeout_ms >= 0 && (min_delay < 0 || timeout_ms < min_delay))
        min_delay = timeout_ms;
    native_wake = JS_IsNativeJobPending(rt) ?
                  JS_GetNativeJobWakeHandle(rt) : -1;
    nfds = 0;
    if (native_wake != -1) {
        native_index = nfds;
        if (js_poll_add_poll_fd(ts, &nfds, (int)native_wake, POLLIN))
            goto oom;
    }
    if (!ts->recv_pipe) {
        JSOSSignalHandler *sh;

        list_for_each(el, &ts->os_signal_handlers) {
            sh = list_entry(el, JSOSSignalHandler, link);
            sh->poll_fd_index = nfds;
            if (js_poll_add_poll_fd(ts, &nfds,
                                    signal_wake_read_fd[sh->sig_num], POLLIN))
                goto oom;
        }
    }
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
                goto oom;
        }
    }

    list_for_each(el, &ts->port_list) {
        JSWorkerMessageHandler *port = list_entry(el, JSWorkerMessageHandler, link);
        if (!JS_IsNull(port->on_message_func)) {
            JSWorkerMessagePipe *ps = port->recv_pipe;
            port->poll_fd_index = nfds;
            if (js_poll_add_poll_fd(ts, &nfds, ps->waker.read_fd, POLLIN))
                goto oom;
        }
    }

    nfds = poll(ts->poll_fds, nfds, min_delay);
    if (nfds < 0) {
        if (errno == EINTR)
            return 0;
        JS_ThrowInternalError(ctx, "host polling failed: %s", strerror(errno));
        return -2;
    }
    native_delay = JS_PollNativeJobs(rt, 0);
    if (native_delay < 0) {
        JS_ThrowInternalError(ctx, "native job polling failed");
        return -2;
    }
    if (native_delay > 0)
        return 1;
    if (native_index >= 0 && ts->poll_fds[native_index].revents != 0)
        return 0;
    if (nfds > 0) {
        if (!ts->recv_pipe) {
            JSOSSignalHandler *sh;

            list_for_each(el, &ts->os_signal_handlers) {
                sh = list_entry(el, JSOSSignalHandler, link);
                if (ts->poll_fds[sh->poll_fd_index].revents) {
                    drain_signal_wake(sh->sig_num);
                    if (os_pending_signals[sh->sig_num]) {
                        os_pending_signals[sh->sig_num] = 0;
                        return call_handler(ctx, sh->func);
                    }
                }
            }
        }
        list_for_each(el, &ts->os_rw_handlers) {
            rh = list_entry(el, JSOSRWHandler, link);
            if (ts->poll_fds[rh->poll_fd_index].revents & POLLNVAL) {
                JS_ThrowInternalError(ctx, "invalid host file descriptor");
                return -2;
            }
            if (!JS_IsNull(rh->rw_func[0]) &&
                (ts->poll_fds[rh->poll_fd_index].revents & (POLLERR | POLLHUP | POLLNVAL | POLLIN))) {
                /* The callback can modify both handler lists. */
                return call_handler(ctx, rh->rw_func[0]);
            }
            if (!JS_IsNull(rh->rw_func[1]) &&
                (ts->poll_fds[rh->poll_fd_index].revents & (POLLERR | POLLHUP | POLLNVAL | POLLOUT))) {
                return call_handler(ctx, rh->rw_func[1]);
            }
        }

        list_for_each(el, &ts->port_list) {
            JSWorkerMessageHandler *port = list_entry(el, JSWorkerMessageHandler, link);
            if (!JS_IsNull(port->on_message_func)) {
                if (ts->poll_fds[port->poll_fd_index].revents != 0) {
                    int status = handle_posted_message(rt, ctx, port);
                    if (status)
                        return status < 0 ? -3 : 1;
                }
            }
        }
    }
    return 0;
 oom:
    JS_ThrowOutOfMemory(ctx);
    return -2;
}
#endif /* !_WIN32 */

