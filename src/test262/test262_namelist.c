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
#include "test262_namelist.h"
#include <ctype.h>
#include <errno.h>

void warning(const char *fmt, ...)
{
    va_list ap;

    fflush(stdout);
    fprintf(stderr, "%s: ", CMD_NAME);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

void fatal(int errcode, const char *fmt, ...)
{
    va_list ap;

    fflush(stdout);
    fprintf(stderr, "%s: ", CMD_NAME);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);

    exit(errcode);
}

void perror_exit(int errcode, const char *s)
{
    fflush(stdout);
    fprintf(stderr, "%s: ", CMD_NAME);
    perror(s);
    exit(errcode);
}

char *strdup_len(const char *str, int len)
{
    char *p = malloc(len + 1);
    memcpy(p, str, len);
    p[len] = '\0';
    return p;
}

char *str_append(char **pp, const char *sep, const char *str) {
    char *res, *p;
    size_t len = 0;
    p = *pp;
    if (p) {
        len = strlen(p) + strlen(sep);
    }
    res = malloc(len + strlen(str) + 1);
    if (p) {
        strcpy(res, p);
        strcat(res, sep);
    }
    strcpy(res + len, str);
    free(p);
    return *pp = res;
}

char *str_strip(char *p)
{
    size_t len = strlen(p);
    while (len > 0 && isspace((unsigned char)p[len - 1]))
        p[--len] = '\0';
    while (isspace((unsigned char)*p))
        p++;
    return p;
}

int has_prefix(const char *str, const char *prefix)
{
    return !strncmp(str, prefix, strlen(prefix));
}

char *skip_prefix(const char *str, const char *prefix)
{
    int i;
    for (i = 0;; i++) {
        if (prefix[i] == '\0') {  /* skip the prefix */
            str += i;
            break;
        }
        if (str[i] != prefix[i])
            break;
    }
    return (char *)str;
}

char *get_basename(const char *filename)
{
    char *p;

    p = strrchr(filename, '/');
    if (!p)
        return NULL;
    return strdup_len(filename, p - filename);
}

char *compose_path(const char *path, const char *name)
{
    int path_len, name_len;
    char *d, *q;

    if (!path || path[0] == '\0' || *name == '/') {
        d = strdup(name);
    } else {
        path_len = strlen(path);
        name_len = strlen(name);
        d = malloc(path_len + 1 + name_len + 1);
        if (d) {
            q = d;
            memcpy(q, path, path_len);
            q += path_len;
            if (path[path_len - 1] != '/')
                *q++ = '/';
            memcpy(q, name, name_len + 1);
        }
    }
    return d;
}

int namelist_cmp(const char *a, const char *b)
{
    /* compare strings in modified lexicographical order */
    for (;;) {
        int ca = (unsigned char)*a++;
        int cb = (unsigned char)*b++;
        if (isdigit(ca) && isdigit(cb)) {
            int na = ca - '0';
            int nb = cb - '0';
            while (isdigit(ca = (unsigned char)*a++))
                na = na * 10 + ca - '0';
            while (isdigit(cb = (unsigned char)*b++))
                nb = nb * 10 + cb - '0';
            if (na < nb)
                return -1;
            if (na > nb)
                return +1;
        }
        if (ca < cb)
            return -1;
        if (ca > cb)
            return +1;
        if (ca == '\0')
            return 0;
    }
}

int namelist_cmp_indirect(const void *a, const void *b)
{
    return namelist_cmp(*(const char **)a, *(const char **)b);
}

void namelist_sort(namelist_t *lp, BOOL remove_duplicates)
{
    int i, count;
    if (lp->count > 1) {
        qsort(lp->array, lp->count, sizeof(*lp->array), namelist_cmp_indirect);
        /* remove duplicates */
        if (remove_duplicates) {
            for (count = i = 1; i < lp->count; i++) {
                if (namelist_cmp(lp->array[count - 1], lp->array[i]) == 0) {
                    free(lp->array[i]);
                } else {
                    lp->array[count++] = lp->array[i];
                }
            }
            lp->count = count;
        }
    }
}

/* the list must be sorted */
int namelist_find(const namelist_t *lp, const char *name)
{
    int a, b, m, cmp;

    for (a = 0, b = lp->count; a < b;) {
        m = a + (b - a) / 2;
        cmp = namelist_cmp(lp->array[m], name);
        if (cmp < 0)
            a = m + 1;
        else if (cmp > 0)
            b = m;
        else
            return m;
    }
    return -1;
}

void namelist_add(namelist_t *lp, const char *base, const char *name)
{
    char *s;

    s = compose_path(base, name);
    if (!s)
        goto fail;
    if (lp->count == lp->size) {
        size_t newsize = lp->size + (lp->size >> 1) + 4;
        char **a = realloc(lp->array, sizeof(lp->array[0]) * newsize);
        if (!a)
            goto fail;
        lp->array = a;
        lp->size = newsize;
    }
    lp->array[lp->count] = s;
    lp->count++;
    return;
fail:
    fatal(1, "allocation failure\n");
}

void namelist_load(namelist_t *lp, const char *filename)
{
    char buf[1024];
    char *base_name;
    FILE *f;

    f = fopen(filename, "rb");
    if (!f) {
        perror_exit(1, filename);
    }
    base_name = get_basename(filename);

    while (fgets(buf, sizeof(buf), f) != NULL) {
        char *p = str_strip(buf);
        if (*p == '#' || *p == ';' || *p == '\0')
            continue;  /* line comment */

        namelist_add(lp, base_name, p);
    }
    free(base_name);
    fclose(f);
}

void namelist_add_from_error_file(namelist_t *lp, const char *file)
{
    const char *p, *p0;
    char *pp;

    for (p = file; (p = strstr(p, ".js:")) != NULL; p++) {
        for (p0 = p; p0 > file && p0[-1] != '\n'; p0--)
            continue;
        pp = strdup_len(p0, p + 3 - p0);
        namelist_add(lp, NULL, pp);
        free(pp);
    }
}

void namelist_free(namelist_t *lp)
{
    while (lp->count > 0) {
        free(lp->array[--lp->count]);
    }
    free(lp->array);
    lp->array = NULL;
    lp->size = 0;
}
