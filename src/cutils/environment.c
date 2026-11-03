/*
 * QuickJS host environment access
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
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#elif !defined(_WIN32)
extern char **environ;
#endif

#include "environment.h"

static pthread_mutex_t environment_mutex = PTHREAD_MUTEX_INITIALIZER;

void js_host_environment_lock(void)
{
    pthread_mutex_lock(&environment_mutex);
}

void js_host_environment_unlock(void)
{
    pthread_mutex_unlock(&environment_mutex);
}

char *js_host_getenv_copy(const char *name)
{
    const char *value;
    char *copy;

    js_host_environment_lock();
    errno = 0;
    value = getenv(name);
    copy = value ? strdup(value) : NULL;
    if (!value)
        errno = 0;
    js_host_environment_unlock();
    return copy;
}

void js_host_environ_free(char **envp)
{
    char **p;

    if (!envp)
        return;
    for (p = envp; *p; p++)
        free(*p);
    free(envp);
}

char **js_host_environ_copy(void)
{
    char **copy, **entries;
    size_t count, i;

    js_host_environment_lock();
    entries = environ;
    for (count = 0; entries && entries[count]; count++) {
        if (count >= SIZE_MAX / sizeof(char *) - 1) {
            errno = ENOMEM;
            goto fail_locked;
        }
    }
    copy = calloc(count + 1, sizeof(*copy));
    if (!copy) {
        errno = ENOMEM;
        goto fail_locked;
    }
    for (i = 0; i < count; i++) {
        copy[i] = strdup(entries[i]);
        if (!copy[i]) {
            js_host_environ_free(copy);
            errno = ENOMEM;
            goto fail_locked;
        }
    }
    js_host_environment_unlock();
    return copy;

fail_locked:
    js_host_environment_unlock();
    return NULL;
}
