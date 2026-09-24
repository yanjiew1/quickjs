/*
 * QuickJS C Library Thread State
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
#ifndef QUICKJS_LIBC_INTERNAL_H
#define QUICKJS_LIBC_INTERNAL_H

#include <stdint.h>
#include "quickjs-libc.h"
#include "list.h"

typedef struct JSWorkerMessagePipe JSWorkerMessagePipe;
struct pollfd;

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

extern uint64_t os_pending_signals;
extern int (*os_poll_func)(JSContext *ctx);

int get_bool_option(JSContext *ctx, BOOL *pbool, JSValueConst obj,
                    const char *option);
ssize_t js_get_errno(ssize_t ret);
JSValue js_os_now(JSContext *ctx, JSValueConst this_val,
                  int argc, JSValueConst *argv);

#endif /* QUICKJS_LIBC_INTERNAL_H */
