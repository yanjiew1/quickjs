/*
 * QuickJS C library: Worker Subsystem Header
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 */
#ifndef QUICKJS_LIBC_WORKER_H
#define QUICKJS_LIBC_WORKER_H

#include "libc_internal.h"

int handle_posted_message(JSRuntime *rt, JSContext *ctx,
                          JSWorkerMessageHandler *port);
void js_std_set_worker_new_context_func(JSContext *(*func)(JSRuntime *rt));
JSValue js_worker_ctor_internal(JSContext *ctx, JSValueConst new_target,
                                JSWorkerMessagePipe *recv_pipe,
                                JSWorkerMessagePipe *send_pipe);
int js_init_worker(JSContext *ctx, JSModuleDef *m);
void *js_sab_alloc(void *opaque, size_t size);
void js_sab_free(void *opaque, void *ptr);
void js_sab_dup(void *opaque, void *ptr);
void js_free_message_pipe(JSWorkerMessagePipe *ps);

#endif /* QUICKJS_LIBC_WORKER_H */
