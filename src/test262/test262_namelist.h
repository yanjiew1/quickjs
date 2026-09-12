/*
 * ECMA Test 262 Runner: Name list and string utilities
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
#ifndef TEST262_NAMELIST_H
#define TEST262_NAMELIST_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdatomic.h>
#include "cutils.h"

#define CMD_NAME "run-test262"

typedef struct namelist_t {
    char **array;
    int count;
    int size;
} namelist_t;

void warning(const char *fmt, ...) __attribute__((__format__(__printf__, 1, 2)));
void fatal(int errcode, const char *fmt, ...) __attribute__((__format__(__printf__, 2, 3)));
void perror_exit(int errcode, const char *s);

static inline void atomic_inc(volatile _Atomic int *p)
{
    atomic_fetch_add(p, 1);
}

char *strdup_len(const char *str, int len);
static inline int str_equal(const char *a, const char *b) {
    return !strcmp(a, b);
}
char *str_append(char **pp, const char *sep, const char *str);
char *str_strip(char *p);
int has_prefix(const char *str, const char *prefix);
char *skip_prefix(const char *str, const char *prefix);
char *get_basename(const char *filename);
char *compose_path(const char *path, const char *name);

int namelist_cmp(const char *a, const char *b);
int namelist_cmp_indirect(const void *a, const void *b);
void namelist_sort(namelist_t *lp, BOOL remove_duplicates);
int namelist_find(const namelist_t *lp, const char *name);
void namelist_add(namelist_t *lp, const char *base, const char *name);
void namelist_load(namelist_t *lp, const char *filename);
void namelist_add_from_error_file(namelist_t *lp, const char *file);
void namelist_free(namelist_t *lp);

#endif /* TEST262_NAMELIST_H */
