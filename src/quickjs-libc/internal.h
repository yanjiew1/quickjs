/*
 * QuickJS C library
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#if defined(_WIN32)
#include <windows.h>
#include <conio.h>
#include <utime.h>
#else
#include <dlfcn.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <poll.h>

#if defined(__FreeBSD__)
extern char **environ;
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
typedef sig_t sighandler_t;
#endif

#if defined(__APPLE__)
#if !defined(environ)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#endif
#endif /* __APPLE__ */

#endif

/* enable the os.Worker API. It relies on POSIX threads */
#define USE_WORKER

#ifdef USE_WORKER
#include <pthread.h>
#include <stdatomic.h>
#endif

#include "cutils.h"
#include "list.h"
#include "quickjs-libc.h"

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

/* TODO:
   - add socket calls
*/

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

typedef struct {
    struct list_head link;
    uint8_t *data;
    size_t data_len;
    /* list of SharedArrayBuffers, necessary to free the message */
    uint8_t **sab_tab;
    size_t sab_tab_len;
} JSWorkerMessage;

typedef struct JSWaker {
#ifdef _WIN32
    HANDLE handle;
#else
    int read_fd;
    int write_fd;
#endif
} JSWaker;

typedef struct {
    int ref_count;
#ifdef USE_WORKER
    pthread_mutex_t mutex;
#endif
    struct list_head msg_queue; /* list of JSWorkerMessage.link */
    JSWaker waker;
} JSWorkerMessagePipe;

typedef struct {
    struct list_head link;
    JSWorkerMessagePipe *recv_pipe;
    JSValue on_message_func;
    int poll_fd_index; /* temporary use in js_os_poll() */
} JSWorkerMessageHandler;

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
JSValue js_os_now(JSContext *ctx, JSValue this_val,
                  int argc, JSValueConst *argv);

#endif /* QUICKJS_LIBC_INTERNAL_H */
