/*
 * QuickJS OS module interface
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
#ifndef QUICKJS_LIBC_OS_H
#define QUICKJS_LIBC_OS_H

#include "thread.h"

ssize_t js_get_errno(ssize_t ret);
JSValue js_os_now(JSContext *ctx, JSValue this_val,
                  int argc, JSValue *argv);
void *js_sab_alloc(void *opaque, size_t size);
void js_sab_free(void *opaque, void *ptr);
void js_sab_dup(void *opaque, void *ptr);
void free_rw_handler(JSRuntime *rt, JSOSRWHandler *rh);
void free_sh(JSRuntime *rt, JSOSSignalHandler *sh);
void free_timer(JSRuntime *rt, JSOSTimer *th);
void js_free_message_pipe(JSWorkerMessagePipe *ps);

#endif
