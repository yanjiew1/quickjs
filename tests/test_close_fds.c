/* Check os.exec's fallback after lowering both descriptor limits.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT
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
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

/* Exercise os.exec's fallback even when closefrom or close_range exists. */
#include <sys/syscall.h>
#undef HAVE_CLOSEFROM
#ifdef SYS_close_range
#define syscall(number, ...) ((number) == SYS_close_range ? \
                              (errno = ENOSYS, -1L) : \
                              syscall(number, __VA_ARGS__))
#endif
#define TEST_CLOSE_FDS
#include "../src/quickjs-libc/os.c"

int main(int argc, char **argv)
{
    struct rlimit limit;
    int source, high, status;
    pid_t pid;

    if (argc == 3 && strcmp(argv[1], "--check") == 0) {
        int fd = atoi(argv[2]);
        errno = 0;
        return fcntl(fd, F_GETFD) == -1 && errno == EBADF ? 0 : 1;
    }

    assert(getrlimit(RLIMIT_NOFILE, &limit) == 0);
    if (limit.rlim_cur <= 64)
        return 77;
    source = open("/dev/null", O_RDONLY);
    assert(source >= 0);
    high = fcntl(source, F_DUPFD, 64);
    assert(high >= 64);
    assert((fcntl(high, F_GETFD) & FD_CLOEXEC) == 0);
    pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx = JS_NewContext(rt);
        JSValue args = JS_NewArray(ctx), result;
        char fd_text[32];
        int status_code;

        snprintf(fd_text, sizeof(fd_text), "%d", high);
        JS_SetPropertyUint32(ctx, args, 0, JS_NewString(ctx, argv[0]));
        JS_SetPropertyUint32(ctx, args, 1, JS_NewString(ctx, "--check"));
        JS_SetPropertyUint32(ctx, args, 2, JS_NewString(ctx, fd_text));
        limit.rlim_cur = 32;
        limit.rlim_max = 32;
        if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
            _exit(1);
        result = js_os_exec(ctx, JS_UNDEFINED, 1, &args);
        if (JS_IsException(result) || JS_ToInt32(ctx, &status_code, result))
            _exit(1);
        _exit(status_code == 0 ? 0 : 1);
    }
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    close(high);
    close(source);
    return 0;
}
