/*
 * QuickJS host event loop interfaces
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
#ifndef QUICKJS_LIBC_EVENT_LOOP_H
#define QUICKJS_LIBC_EVENT_LOOP_H

#include "thread.h"

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

extern uint64_t os_pending_signals;
extern int (*os_poll_func)(JSContext *ctx);

JSValue js_os_now(JSContext *ctx, JSValue this_val,
                  int argc, JSValue *argv);
void free_rw_handler(JSRuntime *rt, JSOSRWHandler *rh);
void free_sh(JSRuntime *rt, JSOSSignalHandler *sh);
void free_timer(JSRuntime *rt, JSOSTimer *th);

JSValue js_os_setReadHandler(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic);
JSValue js_os_signal(JSContext *ctx, JSValueConst this_val,
                     int argc, JSValueConst *argv);
JSValue js_os_setTimeout(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv);
JSValue js_os_clearTimeout(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv);
JSValue js_os_sleepAsync(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv);
int js_os_poll(JSContext *ctx);

#endif
