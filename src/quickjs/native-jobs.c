/*
 * Owner-thread native jobs and Atomics wait integration.
 *
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
#include "internal/native-jobs.h"
#include "internal/runtime.h"
#include "internal/allocator.h"
#include "internal/gc.h"
#include <limits.h>
#include <errno.h>
#include <time.h>
#ifdef CONFIG_ATOMICS
#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#endif

static pthread_once_t js_native_queue_once = PTHREAD_ONCE_INIT;
static JSNativeWaitQueue js_native_queue;
static int js_native_queue_error;

static void js_native_jobs_init_queue(void)
{
    js_native_queue_error = js_native_wait_queue_init(&js_native_queue);
}

JSNativeWaitQueue *js_native_jobs_wait_queue(void)
{
    int ret = pthread_once(&js_native_queue_once, js_native_jobs_init_queue);
    if (ret || js_native_queue_error) {
        errno = ret ? ret : js_native_queue_error;
        return NULL;
    }
    return &js_native_queue;
}

long double js_native_jobs_now(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency, ticks;
    if (!QueryPerformanceFrequency(&frequency) ||
        !QueryPerformanceCounter(&ticks) || frequency.QuadPart <= 0) {
        SetLastError(ERROR_GEN_FAILURE);
        return NAN;
    }
    return (long double)ticks.QuadPart * 1000000000.0L /
           (long double)frequency.QuadPart;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        return NAN;
    return (long double)ts.tv_sec * 1000000000.0L + ts.tv_nsec;
#endif
}

JSNativeJobOwner *js_native_jobs_get_owner(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    JSNativeJobOwner *owner;
    JSNativeWaitQueue *queue;
    if (rt->native_jobs)
        return rt->native_jobs;
    queue = js_native_jobs_wait_queue();
    if (!queue) {
        JS_ThrowInternalError(ctx, "cannot initialize native wait queue");
        return NULL;
    }
    owner = js_mallocz(ctx, sizeof(*owner));
    if (!owner)
        return NULL;
    if (js_native_wait_owner_init(&owner->native, queue)) {
        js_free(ctx, owner);
        JS_ThrowInternalError(ctx, "cannot initialize native job wake source");
        return NULL;
    }
    if (js_native_jobs_attach_existing(ctx, owner)) {
        js_native_wait_owner_close(&owner->native);
        js_free(ctx, owner);
        return NULL;
    }
    /* No foreign notifier can reach this owner before a waiter is linked. */
    rt->native_jobs = owner;
    return owner;
}

static void js_native_jobs_retire(JSNativeJobOwner *owner,
                                  JSNativeWaiter *waiter)
{
    if (js_native_wait_retire(&owner->native, waiter)) {
        JSNativeAsyncRecord *record = waiter->owner_data;
        record->release_next = owner->retired;
        owner->retired = record;
    }
}

void js_native_jobs_flush_retired(JSRuntime *rt)
{
    JSNativeJobOwner *owner = rt->native_jobs;
    JSNativeAsyncRecord *record;
    /* Shared backing release and context/class finalizers happen outside the
       waiter critical section, including same-agent NotifyWaiter completion. */
    while ((record = owner->retired) != NULL) {
        owner->retired = record->release_next;
        record->release(record);
    }
}

int js_native_jobs_complete(JSRuntime *rt, JSNativeWaiter *waiter,
                            JSNativeWaitResult result, JSContext **pctx)
{
    JSNativeJobOwner *owner = rt->native_jobs;
    JSNativeAsyncRecord *record = waiter->owner_data;
    JSValue value;
    int ret;
    value = record->resolve(record, result);
    ret = JS_IsException(value) ? -1 : 1;
    JS_FreeValue(record->ctx, value);
    js_native_wait_complete(&owner->native, waiter);
    js_native_jobs_retire(owner, waiter);
    if (pctx)
        *pctx = NULL; /* internal callers use the explicit owner lifetime */
    return ret;
}

int js_native_jobs_next(JSRuntime *rt, void **job, JSContext **pctx)
{
    JSNativeJobOwner *owner = rt->native_jobs;
    JSNativeWaiter *waiter;
    JSNativeWaitEvent *event;
    JSNativeWaitResult result;
    long double now = js_native_jobs_now();
    *job = NULL;
    if (pctx)
        *pctx = NULL;
    if (isnan(now)) {
        if (owner->native.first) {
            JSNativeAsyncRecord *record = owner->native.first->owner_data;
            JS_ThrowInternalError(record->ctx, "native wait clock failed");
            if (pctx)
                *pctx = record->ctx;
            return -1;
        }
        /* Ordinary jobs can still make progress without a timer clock. */
        now = 0;
    }
    js_native_wait_schedule_timeouts(&owner->native, now);
    result = js_native_wait_execute_next2(&owner->native, now, &waiter, &event);
    if (result == JS_NATIVE_WAIT_NO_EVENT)
        return 0;
    if (result == JS_NATIVE_WAIT_GENERIC_JOB) {
        *job = event->generic_job;
        if (event->initial_job && --owner->initial_remaining == 0) {
            js_free_rt(rt, owner->initial_tickets);
            owner->initial_tickets = NULL;
        }
        return 2;
    }
    if (result == JS_NATIVE_WAIT_EVENT_NOOP) {
        js_native_jobs_retire(owner, waiter);
        js_native_jobs_flush_retired(rt);
        return 1;
    }
    {
        JSNativeAsyncRecord *record = waiter->owner_data;
        JSContext *ctx = JS_DupContext(record->ctx);
        int ret = js_native_jobs_complete(rt, waiter, result, NULL);
        js_native_jobs_flush_retired(rt);
        if (pctx)
            *pctx = js_rc(ctx)->ref_count > 1 ? ctx : NULL;
        JS_FreeContext(ctx);
        return ret;
    }
}

void js_native_jobs_free(JSRuntime *rt)
{
    JSNativeJobOwner *owner = rt->native_jobs;
    JSNativeWaiter *waiter, *next;
    if (!owner)
        return;
    /* Removal and wake closure serialize with every foreign notification.
       Foreign threads retain no owner pointer after leaving that lock. */
    waiter = js_native_wait_owner_close(&owner->native);
    rt->native_jobs = NULL;
    for (; waiter; waiter = next) {
        JSNativeAsyncRecord *record = waiter->owner_data;
        next = waiter->owner_next;
        record->release(record);
    }
    while (owner->retired) {
        JSNativeAsyncRecord *record = owner->retired;
        owner->retired = record->release_next;
        record->release(record);
    }
    js_free_rt(rt, owner->initial_tickets);
    js_free_rt(rt, owner);
}

JS_BOOL JS_IsNativeJobPending(JSRuntime *rt)
{
    return rt->native_jobs &&
           js_native_wait_has_waiters(&rt->native_jobs->native);
}

intptr_t JS_GetNativeJobWakeHandle(JSRuntime *rt)
{
    return rt->native_jobs ?
           js_native_wait_owner_wake_source(&rt->native_jobs->native) : -1;
}

int JS_GetNativeJobTimeout(JSRuntime *rt)
{
    JSNativeWaitOwner *owner;
    long double now, milliseconds;
    if (!rt->native_jobs)
        return -1;
    owner = &rt->native_jobs->native;
    if (js_native_wait_has_events(owner))
        return 0;
    now = js_native_jobs_now();
    if (isnan(now))
        return 0;
    milliseconds = js_native_wait_next_delay(owner, now);
    if (milliseconds == JS_NATIVE_WAIT_FOREVER)
        return -1;
    if (milliseconds >= INT_MAX)
        return INT_MAX;
    return (int)ceill(milliseconds);
}

int JS_PollNativeJobs(JSRuntime *rt, int timeout_ms)
{
    JSNativeWaitOwner *owner;
    long double now;
    int deadline, ret, error;
    if (!rt->native_jobs)
        return 0;
    owner = &rt->native_jobs->native;
    now = js_native_jobs_now();
    if (isnan(now))
        return -1;
    js_native_wait_schedule_timeouts(owner, now);
    js_native_wait_queue_lock(owner->queue);
    error = owner->wake_error;
    js_native_wait_queue_unlock(owner->queue);
    if (error) {
#ifdef _WIN32
        SetLastError(error);
#else
        errno = error;
#endif
        return -1;
    }
    if (js_native_wait_has_events(owner))
        return 1;
    deadline = JS_GetNativeJobTimeout(rt);
    if (timeout_ms < 0)
        timeout_ms = -1;
    if (deadline >= 0 && (timeout_ms < 0 || deadline < timeout_ms))
        timeout_ms = deadline;
#ifdef _WIN32
    {
        DWORD status = WaitForSingleObject((HANDLE)owner->wake_read,
                                         timeout_ms < 0 ? INFINITE :
                                         (DWORD)timeout_ms);
        if (status == WAIT_FAILED)
            return -1;
        ret = status == WAIT_OBJECT_0;
    }
#else
    {
        struct pollfd fd = { (int)owner->wake_read, POLLIN, 0 };
        ret = poll(&fd, 1, timeout_ms);
        if (ret < 0)
            return errno == EINTR ? 0 : -1;
        if (fd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            errno = EIO;
            return -1;
        }
    }
#endif
    (void)ret;
    now = js_native_jobs_now();
    if (isnan(now))
        return -1;
    js_native_wait_schedule_timeouts(owner, now);
    return js_native_wait_has_events(owner);
}
#else
JS_BOOL JS_IsNativeJobPending(JSRuntime *rt) { return FALSE; }
intptr_t JS_GetNativeJobWakeHandle(JSRuntime *rt) { return -1; }
int JS_GetNativeJobTimeout(JSRuntime *rt) { return -1; }
int JS_PollNativeJobs(JSRuntime *rt, int timeout_ms) { return 0; }
#endif
