/*
 * ECMA Test 262 Runner: Harness, Agent, and Metadata Subsystems
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
#ifndef TEST262_HARNESS_H
#define TEST262_HARNESS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include "cutils.h"
#include "list.h"
#include "quickjs-libc.h"
#include "test262_namelist.h"

/* per execution thread context */
typedef struct {
    pthread_mutex_t agent_mutex;
    pthread_cond_t agent_cond;
    /* list of Test262Agent.link */
    struct list_head agent_list;

    pthread_mutex_t report_mutex;
    /* list of AgentReport.link */
    struct list_head report_list;

    int async_done;
} ThreadLocalStorage;

typedef struct {
    struct list_head link;
    ThreadLocalStorage *tls;
    pthread_t tid;
    char *script;
    JSValue broadcast_func;
    BOOL broadcast_pending;
    JSValue broadcast_sab; /* in the main context */
    uint8_t *broadcast_sab_buf;
    size_t broadcast_sab_size;
    int32_t broadcast_val;
} Test262Agent;

typedef struct {
    struct list_head link;
    char *str;
} AgentReport;

extern FILE *outfile;

void init_thread_local_storage(ThreadLocalStorage *tls);
void js_agent_free(JSContext *ctx);
BOOL is_broadcast_pending(ThreadLocalStorage *tls);
JSValue js_new_agent(JSContext *ctx);

JSValue js_print(JSContext *ctx, JSValueConst this_val,
                 int argc, JSValueConst *argv);
int64_t get_clock_ms(void);

JSValue add_helpers1(JSContext *ctx);
void add_helpers(JSContext *ctx);

char *load_file(const char *filename, size_t *lenp);
JSModuleDef *js_module_loader_test(JSContext *ctx,
                                   const char *module_name, void *opaque,
                                   JSValueConst attributes);

char *find_line(const char *str, const char *line);
int is_word_sep(char c);
char *find_word(const char *str, const char *word);

char *extract_desc(const char *buf, char style);
char *find_tag(char *desc, const char *tag, int *state);
char *get_option(char **pp, int *state);

#endif /* TEST262_HARNESS_H */
