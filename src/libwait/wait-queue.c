/*
 * Native wait selection and owner event queue.
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
#ifndef __EMSCRIPTEN__
#include "wait-queue.h"
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static int js_native_wait_wake_init(JSNativeWaitOwner *owner)
{
#ifdef _WIN32
    HANDLE handle = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!handle)
        return -1;
    owner->wake_read = owner->wake_write = (intptr_t)handle;
#else
    int fds[2], i, flags;
    if (pipe(fds))
        return -1;
    for (i = 0; i < 2; i++) {
        flags = fcntl(fds[i], F_GETFL);
        if (flags < 0 || fcntl(fds[i], F_SETFL, flags | O_NONBLOCK) < 0 ||
            fcntl(fds[i], F_SETFD, FD_CLOEXEC) < 0) {
            int saved = errno;
            close(fds[0]);
            close(fds[1]);
            errno = saved;
            return -1;
        }
    }
    owner->wake_read = fds[0];
    owner->wake_write = fds[1];
#endif
    return 0;
}

static void js_native_wait_wake_set(JSNativeWaitOwner *owner)
{
#ifdef _WIN32
    if (!SetEvent((HANDLE)owner->wake_write))
        owner->wake_error = GetLastError();
#else
    unsigned char byte = 0;
    ssize_t n;
    do {
        n = write((int)owner->wake_write, &byte, 1);
    } while (n < 0 && errno == EINTR);
    if (n != 1 && !(n < 0 && errno == EAGAIN))
        owner->wake_error = errno ? errno : EIO;
#endif
}

static void js_native_wait_wake_clear(JSNativeWaitOwner *owner)
{
#ifdef _WIN32
    if (!ResetEvent((HANDLE)owner->wake_read))
        owner->wake_error = GetLastError();
#else
    unsigned char bytes[32];
    ssize_t n;
    do {
        n = read((int)owner->wake_read, bytes, sizeof(bytes));
    } while (n > 0 || (n < 0 && errno == EINTR));
    if (n == 0 || (n < 0 && errno != EAGAIN))
        owner->wake_error = errno ? errno : EIO;
#endif
}

static void js_native_wait_wake_close(JSNativeWaitOwner *owner)
{
#ifdef _WIN32
    CloseHandle((HANDLE)owner->wake_read);
#else
    close((int)owner->wake_read);
    close((int)owner->wake_write);
#endif
    owner->wake_read = owner->wake_write = -1;
}

int js_native_wait_queue_init(JSNativeWaitQueue *queue)
{
    pthread_mutexattr_t attr;
    int ret;
    memset(queue, 0, sizeof(*queue));
    ret = pthread_mutexattr_init(&attr);
    if (ret)
        return ret;
    /* Same-agent intrinsic promise resolution occurs inside the waiter
       critical section. Its owner-side job append may enter this lock again;
       a foreign notifier still waits until that resolution is published. */
    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (!ret)
        ret = pthread_mutex_init(&queue->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    return ret;
}

void js_native_wait_queue_destroy(JSNativeWaitQueue *queue)
{
    assert(!queue->first && !queue->last);
    pthread_mutex_destroy(&queue->mutex);
}

int js_native_wait_owner_init(JSNativeWaitOwner *owner,
                              JSNativeWaitQueue *queue)
{
    memset(owner, 0, sizeof(*owner));
    owner->queue = queue;
    owner->wake_read = owner->wake_write = -1;
    return js_native_wait_wake_init(owner);
}

intptr_t js_native_wait_owner_wake_source(const JSNativeWaitOwner *owner)
{
    return owner->wake_read;
}

void js_native_waiter_init(JSNativeWaiter *waiter, void *owner_data)
{
    memset(waiter, 0, sizeof(*waiter));
    waiter->owner_data = owner_data;
    waiter->state = JS_NATIVE_WAIT_NEW;
    waiter->notify_event.waiter = waiter;
    waiter->notify_event.kind = JS_NATIVE_WAIT_NOTIFY_EVENT;
    waiter->timeout_event.waiter = waiter;
    waiter->timeout_event.kind = JS_NATIVE_WAIT_TIMEOUT_EVENT;
}

void js_native_wait_queue_lock(JSNativeWaitQueue *queue)
{
    pthread_mutex_lock(&queue->mutex);
}

void js_native_wait_queue_unlock(JSNativeWaitQueue *queue)
{
    pthread_mutex_unlock(&queue->mutex);
}

void js_native_wait_add_locked(JSNativeWaitQueue *queue,
                               JSNativeWaiter *waiter,
                               const void *location,
                               JSNativeWaitOwner *owner,
                               pthread_cond_t *sync_condition,
                               long double deadline_ns)
{
    assert(waiter->state == JS_NATIVE_WAIT_NEW);
    assert(owner ? owner->queue == queue && !owner->closing :
           sync_condition != NULL);
    waiter->location = location;
    waiter->owner = owner;
    waiter->sync_condition = sync_condition;
    waiter->deadline_ns = deadline_ns;
    waiter->state = JS_NATIVE_WAIT_LINKED;
    waiter->wait_prev = queue->last;
    if (queue->last)
        queue->last->wait_next = waiter;
    else
        queue->first = waiter;
    queue->last = waiter;
    if (owner) {
        waiter->owner_prev = owner->last;
        if (owner->last)
            owner->last->owner_next = waiter;
        else
            owner->first = waiter;
        owner->last = waiter;
    }
}

void js_native_wait_add_relative_locked(JSNativeWaitQueue *queue,
                                        JSNativeWaiter *waiter,
                                        const void *location,
                                        JSNativeWaitOwner *owner,
                                        long double now_ns,
                                        double timeout_ms)
{
    assert(owner && timeout_ms > 0 && !isnan(now_ns));
    js_native_wait_add_locked(queue, waiter, location, owner, NULL,
                              JS_NATIVE_WAIT_FOREVER);
    waiter->relative_deadline = 1;
    /* Clock conversion and elapsed arithmetic are biased conservatively:
       positive timeouts can run late, but must not become an early zero. */
    waiter->started_ns = nextafterl(now_ns, INFINITY);
    waiter->duration_ms = timeout_ms;
}

static long double js_native_wait_remaining(JSNativeWaiter *waiter,
                                            long double now_ns)
{
    long double elapsed, remaining;
    if (!waiter->relative_deadline) {
        if (waiter->deadline_ns == JS_NATIVE_WAIT_FOREVER)
            return JS_NATIVE_WAIT_FOREVER;
        if (waiter->deadline_ns <= now_ns)
            return 0;
        return nextafterl((waiter->deadline_ns - now_ns) / 1000000.0L,
                           INFINITY);
    }
    if (isinf(waiter->duration_ms))
        return JS_NATIVE_WAIT_FOREVER;
    elapsed = nextafterl(nextafterl(now_ns, -INFINITY) - waiter->started_ns,
                         -INFINITY);
    if (elapsed <= 0)
        return waiter->duration_ms;
    elapsed = nextafterl(elapsed / 1000000.0L, -INFINITY);
    if (elapsed >= (long double)waiter->duration_ms)
        return 0;
    remaining = (long double)waiter->duration_ms - elapsed;
    /* The original duration is already an exact upper bound. Do not round
       DBL_MAX upward to Infinity on platforms with double-sized long double. */
    if (remaining < (long double)waiter->duration_ms)
        remaining = nextafterl(remaining, INFINITY);
    return remaining;
}

long double js_native_wait_next_delay(JSNativeWaitOwner *owner,
                                      long double now_ns)
{
    JSNativeWaiter *waiter;
    long double delay = JS_NATIVE_WAIT_FOREVER, candidate;
    js_native_wait_queue_lock(owner->queue);
    if (!owner->closing) {
        for (waiter = owner->first; waiter; waiter = waiter->owner_next) {
            if (waiter->state == JS_NATIVE_WAIT_LINKED &&
                !waiter->timeout_event.queued) {
                candidate = js_native_wait_remaining(waiter, now_ns);
                if (candidate < delay)
                    delay = candidate;
            }
        }
    }
    js_native_wait_queue_unlock(owner->queue);
    return delay;
}

static void js_native_wait_unlink(JSNativeWaitQueue *queue,
                                   JSNativeWaiter *waiter)
{
    assert(waiter->state == JS_NATIVE_WAIT_LINKED);
    if (waiter->wait_prev)
        waiter->wait_prev->wait_next = waiter->wait_next;
    else
        queue->first = waiter->wait_next;
    if (waiter->wait_next)
        waiter->wait_next->wait_prev = waiter->wait_prev;
    else
        queue->last = waiter->wait_prev;
    waiter->wait_prev = waiter->wait_next = NULL;
}

JSNativeWaitResult js_native_wait_finish_sync_locked(JSNativeWaitQueue *queue,
                                                    JSNativeWaiter *waiter,
                                                    int deadline_reached)
{
    assert(!waiter->owner && waiter->sync_condition);
    if (waiter->state == JS_NATIVE_WAIT_LINKED) {
        if (!deadline_reached)
            return JS_NATIVE_WAIT_NO_EVENT;
        js_native_wait_unlink(queue, waiter);
        waiter->state = JS_NATIVE_WAIT_TIMED_OUT;
        return JS_NATIVE_WAIT_TIMEOUT;
    }
    assert(waiter->state == JS_NATIVE_WAIT_NOTIFIED);
    waiter->state = JS_NATIVE_WAIT_COMPLETE;
    return JS_NATIVE_WAIT_OK;
}

static void js_native_wait_enqueue(JSNativeWaitOwner *owner,
                                    JSNativeWaitEvent *event)
{
    assert(!event->queued && !owner->closing);
    event->next = NULL;
    event->queued = 1;
    if (owner->ready_last)
        owner->ready_last->next = event;
    else {
        owner->ready_first = event;
        js_native_wait_wake_set(owner);
    }
    owner->ready_last = event;
}

size_t js_native_wait_notify(JSNativeWaitQueue *queue, const void *location,
                              size_t count, JSNativeWaitOwner *calling_owner,
                              JSNativeWaiter **inline_first)
{
    JSNativeWaiter *waiter, *inline_last = NULL;
    size_t n = 0;
    *inline_first = NULL;
    js_native_wait_queue_lock(queue);
    while (n < count && js_native_wait_notify_one_locked(queue, location,
                                                         calling_owner,
                                                         &waiter)) {
        n++;
        if (waiter) {
            waiter->inline_next = NULL;
            if (inline_last)
                inline_last->inline_next = waiter;
            else
                *inline_first = waiter;
            inline_last = waiter;
        }
    }
    js_native_wait_queue_unlock(queue);
    return n;
}

int js_native_wait_notify_one_locked(JSNativeWaitQueue *queue,
                                     const void *location,
                                     JSNativeWaitOwner *calling_owner,
                                     JSNativeWaiter **inline_waiter)
{
    JSNativeWaiter *waiter;
    *inline_waiter = NULL;
    for (waiter = queue->first; waiter; waiter = waiter->wait_next) {
        if (waiter->location == location) {
            js_native_wait_unlink(queue, waiter);
            waiter->state = JS_NATIVE_WAIT_NOTIFIED;
            if (!waiter->owner) {
                pthread_cond_signal(waiter->sync_condition);
            } else if (waiter->owner == calling_owner) {
                *inline_waiter = waiter;
            } else {
                js_native_wait_enqueue(waiter->owner, &waiter->notify_event);
            }
            return 1;
        }
    }
    return 0;
}

size_t js_native_wait_schedule_timeouts(JSNativeWaitOwner *owner,
                                       long double now_ns)
{
    JSNativeWaiter *waiter;
    size_t n = 0;
    js_native_wait_queue_lock(owner->queue);
    if (!owner->closing) {
        for (waiter = owner->first; waiter; waiter = waiter->owner_next) {
            if (waiter->state == JS_NATIVE_WAIT_LINKED &&
                js_native_wait_remaining(waiter, now_ns) == 0 &&
                !waiter->timeout_event.queued) {
                js_native_wait_enqueue(owner, &waiter->timeout_event);
                n++;
            }
        }
    }
    js_native_wait_queue_unlock(owner->queue);
    return n;
}

long double js_native_wait_next_deadline(JSNativeWaitOwner *owner)
{
    JSNativeWaiter *waiter;
    long double deadline = JS_NATIVE_WAIT_FOREVER, candidate;
    js_native_wait_queue_lock(owner->queue);
    if (!owner->closing) {
        for (waiter = owner->first; waiter; waiter = waiter->owner_next) {
            if (waiter->state != JS_NATIVE_WAIT_LINKED ||
                waiter->timeout_event.queued)
                continue;
            candidate = waiter->deadline_ns;
            if (waiter->relative_deadline && !isinf(waiter->duration_ms)) {
                candidate = waiter->started_ns +
                            (long double)waiter->duration_ms * 1000000.0L;
                if (isinf(candidate))
                    candidate = LDBL_MAX;
            }
            if (candidate < deadline)
                deadline = candidate;
        }
    }
    js_native_wait_queue_unlock(owner->queue);
    return deadline;
}

JSNativeWaitResult js_native_wait_execute_next2(JSNativeWaitOwner *owner,
                                               long double now_ns,
                                               JSNativeWaiter **pwaiter,
                                               JSNativeWaitEvent **pgeneric)
{
    JSNativeWaitEvent *event;
    JSNativeWaiter *waiter;
    JSNativeWaitResult result = JS_NATIVE_WAIT_NO_EVENT;
    *pwaiter = NULL;
    *pgeneric = NULL;
    js_native_wait_queue_lock(owner->queue);
    event = owner->ready_first;
    if (event) {
        owner->ready_first = event->next;
        if (!owner->ready_first) {
            owner->ready_last = NULL;
            js_native_wait_wake_clear(owner);
        }
        event->queued = 0;
        event->next = NULL;
        waiter = event->waiter;
        *pwaiter = waiter;
        if (event->kind == JS_NATIVE_WAIT_GENERIC_EVENT) {
            *pgeneric = event;
            result = JS_NATIVE_WAIT_GENERIC_JOB;
        } else if (event->kind == JS_NATIVE_WAIT_NOTIFY_EVENT) {
            assert(waiter->state == JS_NATIVE_WAIT_NOTIFIED);
            result = JS_NATIVE_WAIT_OK;
        } else if (waiter->state == JS_NATIVE_WAIT_LINKED) {
            assert(js_native_wait_remaining(waiter, now_ns) == 0);
            js_native_wait_unlink(owner->queue, waiter);
            waiter->state = JS_NATIVE_WAIT_TIMED_OUT;
            result = JS_NATIVE_WAIT_TIMEOUT;
        } else {
            result = JS_NATIVE_WAIT_EVENT_NOOP;
        }
    }
    js_native_wait_queue_unlock(owner->queue);
    return result;
}

JSNativeWaitResult js_native_wait_execute_next(JSNativeWaitOwner *owner,
                                              long double now_ns,
                                              JSNativeWaiter **pwaiter)
{
    JSNativeWaitEvent *generic;
    JSNativeWaitResult result;
    result = js_native_wait_execute_next2(owner, now_ns, pwaiter, &generic);
    assert(result != JS_NATIVE_WAIT_GENERIC_JOB);
    return result;
}

void js_native_wait_enqueue_generic(JSNativeWaitOwner *owner,
                                    JSNativeWaitEvent *event,
                                    void *job, int initial_job)
{
    memset(event, 0, sizeof(*event));
    event->kind = JS_NATIVE_WAIT_GENERIC_EVENT;
    event->generic_job = job;
    event->initial_job = initial_job;
    js_native_wait_queue_lock(owner->queue);
    js_native_wait_enqueue(owner, event);
    js_native_wait_queue_unlock(owner->queue);
}

int js_native_wait_has_events(JSNativeWaitOwner *owner)
{
    int result;
    js_native_wait_queue_lock(owner->queue);
    result = owner->ready_first != NULL;
    js_native_wait_queue_unlock(owner->queue);
    return result;
}

int js_native_wait_has_waiters(JSNativeWaitOwner *owner)
{
    int result;
    js_native_wait_queue_lock(owner->queue);
    result = owner->first != NULL;
    js_native_wait_queue_unlock(owner->queue);
    return result;
}

void js_native_wait_complete(JSNativeWaitOwner *owner,
                             JSNativeWaiter *waiter)
{
    js_native_wait_queue_lock(owner->queue);
    assert(waiter->owner == owner);
    assert(waiter->state == JS_NATIVE_WAIT_NOTIFIED ||
           waiter->state == JS_NATIVE_WAIT_TIMED_OUT);
    waiter->state = JS_NATIVE_WAIT_COMPLETE;
    js_native_wait_queue_unlock(owner->queue);
}

int js_native_wait_retire(JSNativeWaitOwner *owner, JSNativeWaiter *waiter)
{
    int retire = 0;
    js_native_wait_queue_lock(owner->queue);
    assert(waiter->owner == owner);
    if (waiter->state == JS_NATIVE_WAIT_COMPLETE &&
        !waiter->notify_event.queued && !waiter->timeout_event.queued) {
        if (waiter->owner_prev)
            waiter->owner_prev->owner_next = waiter->owner_next;
        else
            owner->first = waiter->owner_next;
        if (waiter->owner_next)
            waiter->owner_next->owner_prev = waiter->owner_prev;
        else
            owner->last = waiter->owner_prev;
        waiter->owner = NULL;
        waiter->owner_prev = waiter->owner_next = NULL;
        retire = 1;
    }
    js_native_wait_queue_unlock(owner->queue);
    return retire;
}

JSNativeWaiter *js_native_wait_owner_close(JSNativeWaitOwner *owner)
{
    JSNativeWaiter *first, *waiter;
    JSNativeWaitEvent *event, *next;
    js_native_wait_queue_lock(owner->queue);
    assert(!owner->closing);
    owner->closing = 1;
    for (event = owner->ready_first; event; event = next) {
        next = event->next;
        event->queued = 0;
        event->next = NULL;
    }
    first = owner->first;
    for (waiter = first; waiter; waiter = waiter->owner_next) {
        if (waiter->state == JS_NATIVE_WAIT_LINKED)
            js_native_wait_unlink(owner->queue, waiter);
        waiter->state = JS_NATIVE_WAIT_CANCELLED;
        waiter->notify_event.queued = waiter->timeout_event.queued = 0;
        waiter->notify_event.next = waiter->timeout_event.next = NULL;
        waiter->owner = NULL;
    }
    owner->first = owner->last = NULL;
    owner->ready_first = owner->ready_last = NULL;
    /* Notifiers can access the wake source only while holding this same
       lock. No notifier retains an owner pointer after unlocking. */
    js_native_wait_wake_close(owner);
    js_native_wait_queue_unlock(owner->queue);
    return first;
}

#endif /* !__EMSCRIPTEN__ */
