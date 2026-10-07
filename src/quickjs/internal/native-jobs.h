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
#ifndef JS_NATIVE_JOBS_H
#define JS_NATIVE_JOBS_H
#include "base.h"
#ifdef CONFIG_ATOMICS
#include "../../libwait/wait-queue.h"

struct JSNativeAsyncRecord;

typedef struct JSNativeJobOwner {
    JSNativeWaitOwner native;
    JSNativeWaitEvent *initial_tickets;
    size_t initial_remaining;
    struct JSNativeAsyncRecord *retired;
} JSNativeJobOwner;

/* These fields and callbacks are accessed only by the runtime owner. The
   plain C queue treats native.owner_data as an opaque cookie. */
typedef struct JSNativeAsyncRecord {
    JSNativeWaiter native;
    struct JSNativeAsyncRecord *release_next;
    JSContext *ctx;
    JSValue (*resolve)(struct JSNativeAsyncRecord *record,
                       JSNativeWaitResult result);
    void (*release)(struct JSNativeAsyncRecord *record);
} JSNativeAsyncRecord;

JSNativeWaitQueue *js_native_jobs_wait_queue(void);
JSNativeJobOwner *js_native_jobs_get_owner(JSContext *ctx);
/* Implemented in runtime.c, where JSJobEntry is private. */
int js_native_jobs_attach_existing(JSContext *ctx, JSNativeJobOwner *owner);
/* Returns 2 with an ordinary job cookie, 1 for a native/no-op job, 0 when
   empty, and -1 for an owner-side exception. */
int js_native_jobs_next(JSRuntime *rt, void **job, JSContext **pctx);
int js_native_jobs_complete(JSRuntime *rt, JSNativeWaiter *waiter,
                            JSNativeWaitResult result, JSContext **pctx);
void js_native_jobs_flush_retired(JSRuntime *rt);
void js_native_jobs_free(JSRuntime *rt);
long double js_native_jobs_now(void);
#endif
#endif
