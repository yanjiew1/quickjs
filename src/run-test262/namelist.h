/*
 * ECMA Test 262 Runner name/path list private interfaces
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
#ifndef RUN_TEST262_NAMELIST_H
#define RUN_TEST262_NAMELIST_H

#include <string.h>

#include "../../cutils.h"

#if defined(__GNUC__) || defined(__clang__)
#define RUN_TEST262_INTERNAL __attribute__((visibility("hidden")))
#else
#define RUN_TEST262_INTERNAL
#endif

typedef struct namelist_t {
    char **array;
    int count;
    int size;
} namelist_t;

static inline int str_equal(const char *a, const char *b)
{
    return !strcmp(a, b);
}

RUN_TEST262_INTERNAL char *strdup_len(const char *str, int len);
RUN_TEST262_INTERNAL char *str_append(char **pp, const char *sep,
                                     const char *str);
RUN_TEST262_INTERNAL char *str_strip(char *p);
RUN_TEST262_INTERNAL int has_prefix(const char *str, const char *prefix);
RUN_TEST262_INTERNAL char *skip_prefix(const char *str, const char *prefix);
RUN_TEST262_INTERNAL char *get_basename(const char *filename);
RUN_TEST262_INTERNAL char *compose_path(const char *path, const char *name);

RUN_TEST262_INTERNAL void namelist_sort(namelist_t *lp,
                                        BOOL remove_duplicates);
RUN_TEST262_INTERNAL void namelist_sort_from(namelist_t *lp, int start);
RUN_TEST262_INTERNAL int namelist_find(const namelist_t *lp,
                                       const char *name);
RUN_TEST262_INTERNAL void namelist_add(namelist_t *lp, const char *base,
                                       const char *name);
RUN_TEST262_INTERNAL void namelist_load(namelist_t *lp,
                                        const char *filename);
RUN_TEST262_INTERNAL void namelist_free(namelist_t *lp);

#endif /* RUN_TEST262_NAMELIST_H */
