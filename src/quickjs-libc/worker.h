/*
 * QuickJS host Worker state and interfaces
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
#ifndef QUICKJS_LIBC_WORKER_H
#define QUICKJS_LIBC_WORKER_H

#include "thread.h"
#include "cutils.h"
#if defined(_WIN32)
#include <windows.h>
#endif
#ifdef USE_WORKER
#include <pthread.h>
#endif

typedef struct {
    struct list_head link;
    JSWorkerMessagePipe *recv_pipe;
    JSValue on_message_func;
    int poll_fd_index; /* temporary use in js_os_poll() */
} JSWorkerMessageHandler;

typedef struct JSWaker {
#ifdef _WIN32
    HANDLE handle;
#else
    int read_fd;
    int write_fd;
#endif
} JSWaker;

struct JSWorkerMessagePipe {
    int ref_count;
#ifdef USE_WORKER
    pthread_mutex_t mutex;
#endif
    struct list_head msg_queue; /* list of JSWorkerMessage.link */
    JSWaker waker;
};

BOOL is_main_thread(JSRuntime *rt);
int handle_posted_message(JSRuntime *rt, JSContext *ctx,
                          JSWorkerMessageHandler *port);
#ifdef USE_WORKER
void *js_sab_alloc(void *opaque, size_t size);
void js_sab_free(void *opaque, void *ptr);
void js_sab_dup(void *opaque, void *ptr);
void js_free_message_pipe(JSWorkerMessagePipe *ps);
void js_init_worker(JSContext *ctx, JSModuleDef *m);
#endif

#endif
