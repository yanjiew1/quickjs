/*
 * QuickJS C library: Event Loop & Timers Subsystem Header
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 */
#ifndef QUICKJS_LIBC_EVENT_H
#define QUICKJS_LIBC_EVENT_H

#include "libc_internal.h"

void js_std_loop(JSContext *ctx);
JSValue js_std_await(JSContext *ctx, JSValue obj);
void js_std_init_handlers(JSRuntime *rt);
void js_std_free_handlers(JSRuntime *rt);
void js_std_promise_rejection_tracker(JSContext *ctx, JSValueConst promise,
                                      JSValueConst reason,
                                      JS_BOOL is_handled, void *opaque);

int js_os_poll(JSContext *ctx);
JSValue js_os_setReadHandler(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int magic);
JSValue js_os_signal(JSContext *ctx, JSValueConst this_val,
                     int argc, JSValueConst *argv);
JSValue js_os_now(JSContext *ctx, JSValueConst this_val,
                  int argc, JSValueConst *argv);
JSValue js_os_setTimeout(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv);
JSValue js_os_clearTimeout(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv);
JSValue js_os_sleepAsync(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv);



#endif /* QUICKJS_LIBC_EVENT_H */
