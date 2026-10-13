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
#ifndef JS_NATIVE_WAIT_QUEUE_H
#define JS_NATIVE_WAIT_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <math.h>

#define JS_NATIVE_WAIT_FOREVER INFINITY

typedef struct JSNativeWaiter JSNativeWaiter;
typedef struct JSNativeWaitOwner JSNativeWaitOwner;
typedef struct JSNativeWaitEvent JSNativeWaitEvent;

typedef enum JSNativeWaitState {
    JS_NATIVE_WAIT_NEW,
    JS_NATIVE_WAIT_LINKED,
    JS_NATIVE_WAIT_NOTIFIED,
    JS_NATIVE_WAIT_TIMED_OUT,
    JS_NATIVE_WAIT_COMPLETE,
    JS_NATIVE_WAIT_CANCELLED,
} JSNativeWaitState;

typedef enum JSNativeWaitEventKind {
    JS_NATIVE_WAIT_NOTIFY_EVENT,
    JS_NATIVE_WAIT_TIMEOUT_EVENT,
    JS_NATIVE_WAIT_GENERIC_EVENT,
} JSNativeWaitEventKind;

typedef enum JSNativeWaitResult {
    JS_NATIVE_WAIT_NO_EVENT,
    JS_NATIVE_WAIT_EVENT_NOOP,
    JS_NATIVE_WAIT_OK,
    JS_NATIVE_WAIT_TIMEOUT,
    JS_NATIVE_WAIT_GENERIC_JOB,
} JSNativeWaitResult;

typedef struct JSNativeWaitQueue {
    pthread_mutex_t mutex;
    JSNativeWaiter *first;
    JSNativeWaiter *last;
} JSNativeWaitQueue;

/* Both event slots are embedded before publication. A timeout token must not
   double as a later notification token: their job ordering is observable. */
struct JSNativeWaitEvent {
    JSNativeWaitEvent *next;
    JSNativeWaiter *waiter;
    JSNativeWaitEventKind kind;
    int queued;
    void *generic_job;
    int initial_job;
};

struct JSNativeWaitOwner {
    JSNativeWaitQueue *queue;
    JSNativeWaiter *first;
    JSNativeWaiter *last;
    JSNativeWaitEvent *ready_first;
    JSNativeWaitEvent *ready_last;
    intptr_t wake_read;
    intptr_t wake_write;
    int wake_error;
    int closing;
};

struct JSNativeWaiter {
    JSNativeWaiter *wait_prev;
    JSNativeWaiter *wait_next;
    JSNativeWaiter *owner_prev;
    JSNativeWaiter *owner_next;
    JSNativeWaiter *inline_next;
    JSNativeWaitOwner *owner;
    pthread_cond_t *sync_condition;
    const void *location;
    /* Opaque owner data is never read by a notifier. It may identify the
       separately rooted JS record after owner-side dispatch. */
    void *owner_data;
    long double deadline_ns; /* absolute-deadline callers */
    long double started_ns;  /* relative asynchronous host timeout */
    double duration_ms;      /* retain finite values, including DBL_MAX */
    int relative_deadline;
    JSNativeWaitState state;
    JSNativeWaitEvent notify_event;
    JSNativeWaitEvent timeout_event;
};

/* Owner and waiter storage belongs to the caller. Queue operations do not
   allocate, invoke callbacks or access QuickJS objects. The queue must outlive
   every owner. An owner is destroyed only by its owning thread. */
int js_native_wait_queue_init(JSNativeWaitQueue *queue);
void js_native_wait_queue_destroy(JSNativeWaitQueue *queue);
int js_native_wait_owner_init(JSNativeWaitOwner *owner,
                              JSNativeWaitQueue *queue);
/* Detach every record and event while holding the queue lock, then close the
   wake source. Return the owner list for owner-only JS cleanup/freeing. */
JSNativeWaiter *js_native_wait_owner_close(JSNativeWaitOwner *owner);
intptr_t js_native_wait_owner_wake_source(const JSNativeWaitOwner *owner);

void js_native_waiter_init(JSNativeWaiter *waiter, void *owner_data);
void js_native_wait_queue_lock(JSNativeWaitQueue *queue);
void js_native_wait_queue_unlock(JSNativeWaitQueue *queue);
/* Witness load and insertion must share this lock. The witness is the caller's
   seq-cst atomic load; this backend never dereferences location. */
void js_native_wait_add_locked(JSNativeWaitQueue *queue,
                               JSNativeWaiter *waiter,
                               const void *location,
                               JSNativeWaitOwner *owner,
                               pthread_cond_t *sync_condition,
                               long double deadline_ns);
/* Relative host timeout preserves the original finite milliseconds instead
   of multiplying them into an overflowing nanosecond absolute deadline. */
void js_native_wait_add_relative_locked(JSNativeWaitQueue *queue,
                                        JSNativeWaiter *waiter,
                                        const void *location,
                                        JSNativeWaitOwner *owner,
                                        long double now_ns,
                                        double timeout_ms);
long double js_native_wait_next_delay(JSNativeWaitOwner *owner,
                                      long double now_ns);
/* Synchronous callers keep the lock across their witness check and cond wait.
   A spurious successful wake is not completion. At a real deadline, notify
   wins if it already removed the waiter under the lock. */
JSNativeWaitResult js_native_wait_finish_sync_locked(JSNativeWaitQueue *queue,
                                                    JSNativeWaiter *waiter,
                                                    int deadline_reached);
/* Select synchronous and asynchronous waits in one FIFO. Same-agent async
   completions return in inline_first and must be resolved before notify
   returns. Foreign completions queue only embedded native tokens. */
size_t js_native_wait_notify(JSNativeWaitQueue *queue, const void *location,
                              size_t count, JSNativeWaitOwner *calling_owner,
                              JSNativeWaiter **inline_first);
/* Select exactly one waiter. The owner builtin resolves an inline result
   before selecting the next waiter, still in this critical section. */
int js_native_wait_notify_one_locked(JSNativeWaitQueue *queue,
                                     const void *location,
                                     JSNativeWaitOwner *calling_owner,
                                     JSNativeWaiter **inline_waiter);
/* A host timer schedules tokens without unlinking a waiter. A notification
   after timer expiry but before the owner executes its timeout job can win. */
size_t js_native_wait_schedule_timeouts(JSNativeWaitOwner *owner,
                                       long double now_ns);
long double js_native_wait_next_deadline(JSNativeWaitOwner *owner);
/* Called at owner-job execution, not at generic host ingestion. The caller
   may now use waiter->owner_data. An obsolete timeout returns EVENT_NOOP. */
JSNativeWaitResult js_native_wait_execute_next(JSNativeWaitOwner *owner,
                                              long double now_ns,
                                              JSNativeWaiter **waiter);
JSNativeWaitResult js_native_wait_execute_next2(JSNativeWaitOwner *owner,
                                               long double now_ns,
                                               JSNativeWaiter **waiter,
                                               JSNativeWaitEvent **generic);
void js_native_wait_enqueue_generic(JSNativeWaitOwner *owner,
                                    JSNativeWaitEvent *event,
                                    void *job, int initial_job);
int js_native_wait_has_events(JSNativeWaitOwner *owner);
int js_native_wait_has_waiters(JSNativeWaitOwner *owner);
/* Claiming an event does not free its record. Mark complete only after the
   owner resolver finishes. Retirement waits for both token slots to drain. */
void js_native_wait_complete(JSNativeWaitOwner *owner,
                             JSNativeWaiter *waiter);
int js_native_wait_retire(JSNativeWaitOwner *owner, JSNativeWaiter *waiter);

#endif
