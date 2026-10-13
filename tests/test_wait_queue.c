/* Native waiter queue tests. Copyright (c) 2026 Yan-Jie Wang.
   Distributed under the MIT license in the accompanying LICENSE file. */
#if !defined(__EMSCRIPTEN__)
#include "../src/libwait/wait-queue.h"
#include <float.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#endif

typedef struct TestNotify {
    JSNativeWaitQueue *queue;
    const void *location;
    size_t count;
    size_t notified;
    _Atomic(int) started;
    _Atomic(int) stop;
} TestNotify;

static void add_wait(JSNativeWaitQueue *queue, JSNativeWaitOwner *owner,
                     JSNativeWaiter *waiter, const void *location,
                     long double deadline)
{
    js_native_waiter_init(waiter, (void *)(uintptr_t)1);
    js_native_wait_queue_lock(queue);
    js_native_wait_add_locked(queue, waiter, location, owner, NULL, deadline);
    js_native_wait_queue_unlock(queue);
}

static void finish(JSNativeWaitOwner *owner, JSNativeWaiter *waiter)
{
    js_native_wait_complete(owner, waiter);
    assert(js_native_wait_retire(owner, waiter));
}

static void *notify_thread(void *opaque)
{
    TestNotify *test = opaque;
    JSNativeWaiter *inline_waiters;
    test->notified = js_native_wait_notify(test->queue, test->location,
                                          test->count, NULL, &inline_waiters);
    assert(!inline_waiters);
    return NULL;
}

static void *notify_race_thread(void *opaque)
{
    TestNotify *test = opaque;
    JSNativeWaiter *inline_waiters;
    atomic_store(&test->started, 1);
    while (!atomic_load(&test->stop)) {
        js_native_wait_notify(test->queue, test->location, SIZE_MAX,
                               NULL, &inline_waiters);
        assert(!inline_waiters);
    }
    return NULL;
}

static void assert_wake(JSNativeWaitOwner *owner, int ready)
{
#ifdef _WIN32
    DWORD result = WaitForSingleObject((HANDLE)
        js_native_wait_owner_wake_source(owner), ready ? 1000 : 0);
    assert(result == (ready ? WAIT_OBJECT_0 : WAIT_TIMEOUT));
#else
    struct pollfd fd = { (int)js_native_wait_owner_wake_source(owner),
                         POLLIN, 0 };
    assert(poll(&fd, 1, ready ? 1000 : 0) == ready);
    if (ready)
        assert(fd.revents & POLLIN);
#endif
}

static void test_mixed_fifo(void)
{
    JSNativeWaitQueue queue;
    JSNativeWaitOwner owner;
    JSNativeWaiter first, sync, last, *inline_waiters, *completed;
    pthread_cond_t condition;
    int location;
    assert(!js_native_wait_queue_init(&queue));
    assert(!js_native_wait_owner_init(&owner, &queue));
    assert(!pthread_cond_init(&condition, NULL));
    add_wait(&queue, &owner, &first, &location, JS_NATIVE_WAIT_FOREVER);
    js_native_waiter_init(&sync, NULL);
    js_native_wait_queue_lock(&queue);
    js_native_wait_add_locked(&queue, &sync, &location, NULL,
                               &condition, JS_NATIVE_WAIT_FOREVER);
    assert(js_native_wait_finish_sync_locked(&queue, &sync, 0) ==
           JS_NATIVE_WAIT_NO_EVENT);
    js_native_wait_queue_unlock(&queue);
    add_wait(&queue, &owner, &last, &location, JS_NATIVE_WAIT_FOREVER);
    assert(js_native_wait_notify(&queue, &location, 2, NULL,
                                  &inline_waiters) == 2);
    assert(!inline_waiters);
    assert(first.state == JS_NATIVE_WAIT_NOTIFIED);
    assert(sync.state == JS_NATIVE_WAIT_NOTIFIED);
    assert(last.state == JS_NATIVE_WAIT_LINKED);
    js_native_wait_queue_lock(&queue);
    assert(js_native_wait_finish_sync_locked(&queue, &sync, 1) ==
           JS_NATIVE_WAIT_OK);
    js_native_wait_queue_unlock(&queue);
    assert(js_native_wait_execute_next(&owner, 0, &completed) ==
           JS_NATIVE_WAIT_OK && completed == &first);
    finish(&owner, completed);
    assert(js_native_wait_notify(&queue, &location, 1, &owner,
                                  &inline_waiters) == 1);
    assert(inline_waiters == &last && !inline_waiters->inline_next);
    finish(&owner, &last);
    assert(!js_native_wait_owner_close(&owner));
    pthread_cond_destroy(&condition);
    js_native_wait_queue_destroy(&queue);
}

static void test_expiry_is_not_selection(void)
{
    JSNativeWaitQueue queue;
    JSNativeWaitOwner owner;
    JSNativeWaiter waiter, *completed, *inline_waiters;
    int location;
    assert(!js_native_wait_queue_init(&queue));
    assert(!js_native_wait_owner_init(&owner, &queue));
    add_wait(&queue, &owner, &waiter, &location, 100);
    assert(js_native_wait_next_deadline(&owner) == 100);
    assert(!js_native_wait_schedule_timeouts(&owner, 99));
    assert(js_native_wait_schedule_timeouts(&owner, 100) == 1);
    assert(!js_native_wait_schedule_timeouts(&owner, 101));
    assert(waiter.state == JS_NATIVE_WAIT_LINKED);
    assert(js_native_wait_notify(&queue, &location, 1, NULL,
                                  &inline_waiters) == 1);
    /* The timeout job precedes the foreign resolution job and is a no-op. */
    assert(js_native_wait_execute_next(&owner, 101, &completed) ==
           JS_NATIVE_WAIT_EVENT_NOOP && completed == &waiter);
    assert(js_native_wait_execute_next(&owner, 101, &completed) ==
           JS_NATIVE_WAIT_OK && completed == &waiter);
    finish(&owner, &waiter);
    assert(!js_native_wait_owner_close(&owner));
    js_native_wait_queue_destroy(&queue);
}

static void test_inline_notify_keeps_timeout_token_alive(void)
{
    JSNativeWaitQueue queue;
    JSNativeWaitOwner owner;
    JSNativeWaiter waiter, *completed, *inline_waiters;
    int location;
    assert(!js_native_wait_queue_init(&queue));
    assert(!js_native_wait_owner_init(&owner, &queue));
    add_wait(&queue, &owner, &waiter, &location, 100);
    assert(js_native_wait_schedule_timeouts(&owner, 100) == 1);
    assert(js_native_wait_notify(&queue, &location, 1, &owner,
                                  &inline_waiters) == 1);
    assert(inline_waiters == &waiter);
    js_native_wait_complete(&owner, &waiter);
    assert(!js_native_wait_retire(&owner, &waiter));
    assert(js_native_wait_execute_next(&owner, 100, &completed) ==
           JS_NATIVE_WAIT_EVENT_NOOP && completed == &waiter);
    assert(js_native_wait_retire(&owner, &waiter));
    assert(!js_native_wait_owner_close(&owner));
    js_native_wait_queue_destroy(&queue);
}

static void test_timeout_selection_wins(void)
{
    JSNativeWaitQueue queue;
    JSNativeWaitOwner owner;
    JSNativeWaiter waiter, *completed, *inline_waiters;
    int location;
    assert(!js_native_wait_queue_init(&queue));
    assert(!js_native_wait_owner_init(&owner, &queue));
    add_wait(&queue, &owner, &waiter, &location, 100);
    assert(js_native_wait_schedule_timeouts(&owner, 100) == 1);
    assert(js_native_wait_execute_next(&owner, 100, &completed) ==
           JS_NATIVE_WAIT_TIMEOUT && completed == &waiter);
    assert(!js_native_wait_notify(&queue, &location, SIZE_MAX, NULL,
                                   &inline_waiters));
    finish(&owner, &waiter);
    assert(!js_native_wait_owner_close(&owner));
    js_native_wait_queue_destroy(&queue);
}

static void test_owner_wake_and_coalescing(void)
{
    JSNativeWaitQueue queue;
    JSNativeWaitOwner owner;
    JSNativeWaiter first, second, *completed;
    TestNotify test = { 0 };
    pthread_t thread;
    int location;
    assert(!js_native_wait_queue_init(&queue));
    assert(!js_native_wait_owner_init(&owner, &queue));
    add_wait(&queue, &owner, &first, &location, JS_NATIVE_WAIT_FOREVER);
    add_wait(&queue, &owner, &second, &location, JS_NATIVE_WAIT_FOREVER);
    assert_wake(&owner, 0);
    test.queue = &queue;
    test.location = &location;
    test.count = SIZE_MAX;
    assert(!pthread_create(&thread, NULL, notify_thread, &test));
    assert_wake(&owner, 1);
    assert(!pthread_join(thread, NULL));
    assert(test.notified == 2);
    assert(js_native_wait_execute_next(&owner, 0, &completed) ==
           JS_NATIVE_WAIT_OK && completed == &first);
    finish(&owner, &first);
    assert_wake(&owner, 1);
    assert(js_native_wait_execute_next(&owner, 0, &completed) ==
           JS_NATIVE_WAIT_OK && completed == &second);
    finish(&owner, &second);
    assert_wake(&owner, 0);
    assert(!owner.wake_error);
    assert(!js_native_wait_owner_close(&owner));
    js_native_wait_queue_destroy(&queue);
}

static void test_close_detaches_both_event_slots(void)
{
    JSNativeWaitQueue queue;
    JSNativeWaitOwner owner;
    JSNativeWaiter first, second, *inline_waiters, *closed;
    int location;
    assert(!js_native_wait_queue_init(&queue));
    assert(!js_native_wait_owner_init(&owner, &queue));
    add_wait(&queue, &owner, &first, &location, 100);
    add_wait(&queue, &owner, &second, &location, 100);
    assert(js_native_wait_schedule_timeouts(&owner, 100) == 2);
    assert(js_native_wait_notify(&queue, &location, 1, NULL,
                                  &inline_waiters) == 1);
    closed = js_native_wait_owner_close(&owner);
    assert(closed == &first && closed->owner_next == &second);
    assert(first.state == JS_NATIVE_WAIT_CANCELLED);
    assert(second.state == JS_NATIVE_WAIT_CANCELLED);
    assert(!first.notify_event.queued && !first.timeout_event.queued);
    assert(!second.notify_event.queued && !second.timeout_event.queued);
    assert(!js_native_wait_notify(&queue, &location, SIZE_MAX, NULL,
                                   &inline_waiters));
    js_native_wait_queue_destroy(&queue);
}

static void test_teardown_races_native_notifier(void)
{
    JSNativeWaitQueue queue;
    JSNativeWaitOwner *owner;
    JSNativeWaiter *waiter;
    TestNotify test = { 0 };
    pthread_t thread;
    int location;
    assert(!js_native_wait_queue_init(&queue));
    owner = malloc(sizeof(*owner));
    waiter = malloc(sizeof(*waiter));
    assert(owner && waiter);
    assert(!js_native_wait_owner_init(owner, &queue));
    add_wait(&queue, owner, waiter, &location, JS_NATIVE_WAIT_FOREVER);
    test.queue = &queue;
    test.location = &location;
    assert(!pthread_create(&thread, NULL, notify_race_thread, &test));
    while (!atomic_load(&test.started)) {
        /* Test synchronization only. The implementation has no spin jobs. */
    }
    assert(js_native_wait_owner_close(owner) == waiter);
    free(waiter);
    free(owner);
    atomic_store(&test.stop, 1);
    assert(!pthread_join(thread, NULL));
    js_native_wait_queue_destroy(&queue);
}

static void test_distinct_locations_and_zero_count(void)
{
    JSNativeWaitQueue queue;
    JSNativeWaitOwner owner;
    JSNativeWaiter first, second, *completed, *inline_waiters;
    int location[2];
    assert(!js_native_wait_queue_init(&queue));
    assert(!js_native_wait_owner_init(&owner, &queue));
    add_wait(&queue, &owner, &first, &location[0], 500);
    add_wait(&queue, &owner, &second, &location[1], 100);
    assert(js_native_wait_next_deadline(&owner) == 100);
    assert(!js_native_wait_notify(&queue, &location[0], 0, NULL,
                                   &inline_waiters));
    assert(js_native_wait_notify(&queue, &location[1], SIZE_MAX, NULL,
                                  &inline_waiters) == 1);
    assert(first.state == JS_NATIVE_WAIT_LINKED);
    assert(js_native_wait_execute_next(&owner, 0, &completed) ==
           JS_NATIVE_WAIT_OK && completed == &second);
    finish(&owner, &second);
    assert(js_native_wait_next_deadline(&owner) == 500);
    assert(js_native_wait_owner_close(&owner) == &first);
    js_native_wait_queue_destroy(&queue);
}

static void test_generic_jobs_share_exact_fifo(void)
{
    JSNativeWaitQueue queue;
    JSNativeWaitOwner owner;
    JSNativeWaiter waiter, *completed, *inline_waiters;
    JSNativeWaitEvent first, middle, last, *generic;
    int location, jobs[3];
    assert(!js_native_wait_queue_init(&queue));
    assert(!js_native_wait_owner_init(&owner, &queue));
    add_wait(&queue, &owner, &waiter, &location, 100);
    js_native_wait_enqueue_generic(&owner, &first, &jobs[0], 1);
    assert(js_native_wait_schedule_timeouts(&owner, 100) == 1);
    js_native_wait_enqueue_generic(&owner, &middle, &jobs[1], 0);
    assert(js_native_wait_notify(&queue, &location, 1, NULL,
                                  &inline_waiters) == 1);
    js_native_wait_enqueue_generic(&owner, &last, &jobs[2], 0);
    assert(js_native_wait_execute_next2(&owner, 100, &completed, &generic) ==
           JS_NATIVE_WAIT_GENERIC_JOB && generic == &first);
    assert(generic->generic_job == &jobs[0] && generic->initial_job);
    assert(js_native_wait_execute_next2(&owner, 100, &completed, &generic) ==
           JS_NATIVE_WAIT_EVENT_NOOP && completed == &waiter);
    assert(js_native_wait_execute_next2(&owner, 100, &completed, &generic) ==
           JS_NATIVE_WAIT_GENERIC_JOB && generic == &middle);
    assert(js_native_wait_execute_next2(&owner, 100, &completed, &generic) ==
           JS_NATIVE_WAIT_OK && completed == &waiter);
    finish(&owner, completed);
    assert(js_native_wait_execute_next2(&owner, 100, &completed, &generic) ==
           JS_NATIVE_WAIT_GENERIC_JOB && generic == &last);
    assert(!js_native_wait_has_events(&owner));
    assert(!js_native_wait_owner_close(&owner));
    js_native_wait_queue_destroy(&queue);
}

static void test_recursive_owner_append_and_fractional_deadline(void)
{
    JSNativeWaitQueue queue;
    JSNativeWaitOwner owner;
    JSNativeWaiter first, second, *completed;
    JSNativeWaitEvent reaction, *generic;
    int location, job;
    assert(!js_native_wait_queue_init(&queue));
    assert(!js_native_wait_owner_init(&owner, &queue));
    add_wait(&queue, &owner, &first, &location, 100.5L);
    add_wait(&queue, &owner, &second, &location, JS_NATIVE_WAIT_FOREVER);
    assert(!js_native_wait_schedule_timeouts(&owner, 100));
    js_native_wait_queue_lock(&queue);
    assert(js_native_wait_notify_one_locked(&queue, &location, &owner,
                                             &completed));
    assert(completed == &first);
    /* Emulate one intrinsic resolution publishing its reaction under the
       same recursive lock before selecting another waiter. */
    js_native_wait_enqueue_generic(&owner, &reaction, &job, 0);
    finish(&owner, completed);
    assert(js_native_wait_notify_one_locked(&queue, &location, NULL,
                                             &completed));
    assert(!completed);
    js_native_wait_queue_unlock(&queue);
    assert(js_native_wait_execute_next2(&owner, 101, &completed, &generic) ==
           JS_NATIVE_WAIT_GENERIC_JOB && generic == &reaction);
    assert(js_native_wait_execute_next2(&owner, 101, &completed, &generic) ==
           JS_NATIVE_WAIT_OK && completed == &second);
    finish(&owner, completed);
    js_native_wait_enqueue_generic(&owner, &reaction, &job, 0);
    assert(!js_native_wait_owner_close(&owner));
    assert(!reaction.queued && !reaction.next);
    js_native_wait_queue_destroy(&queue);
}

static void test_finite_huge_and_fractional_relative_timeouts(void)
{
    JSNativeWaitQueue queue;
    JSNativeWaitOwner owner;
    JSNativeWaiter huge, fraction;
    int locations[2];
    assert(!js_native_wait_queue_init(&queue));
    assert(!js_native_wait_owner_init(&owner, &queue));
    js_native_waiter_init(&huge, NULL);
    js_native_waiter_init(&fraction, NULL);
    js_native_wait_queue_lock(&queue);
    js_native_wait_add_relative_locked(&queue, &huge, &locations[0],
                                       &owner, 0, DBL_MAX);
    js_native_wait_queue_unlock(&queue);
    assert(js_native_wait_next_delay(&owner, 0) == (long double)DBL_MAX);
    assert(isfinite(js_native_wait_next_deadline(&owner)));
    js_native_wait_queue_lock(&queue);
    js_native_wait_add_relative_locked(&queue, &fraction, &locations[1],
                                       &owner, 0, 0.5);
    js_native_wait_queue_unlock(&queue);
    assert(!js_native_wait_schedule_timeouts(&owner, 499999));
    assert(js_native_wait_schedule_timeouts(&owner, 500001) == 1);
    assert(js_native_wait_owner_close(&owner) == &huge);
    js_native_wait_queue_destroy(&queue);
}
#endif

int main(void)
{
#if !defined(__EMSCRIPTEN__)
    test_finite_huge_and_fractional_relative_timeouts();
    test_generic_jobs_share_exact_fifo();
    test_recursive_owner_append_and_fractional_deadline();
    test_mixed_fifo();
    test_expiry_is_not_selection();
    test_inline_notify_keeps_timeout_token_alive();
    test_timeout_selection_wins();
    test_owner_wake_and_coalescing();
    test_close_detaches_both_event_slots();
    test_teardown_races_native_notifier();
    test_distinct_locations_and_zero_count();
#endif
    return 0;
}
