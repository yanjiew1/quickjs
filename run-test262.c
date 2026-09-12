/*
 * ECMA Test 262 Runner for QuickJS
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <ftw.h>
#include <stdatomic.h>
#include <pthread.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "cutils.h"
#include "list.h"
#include "quickjs-libc.h"

#include "src/test262/test262_harness.h"
namelist_t test_list;
namelist_t exclude_list;
namelist_t exclude_dir_list;
namelist_t error_list;
pthread_mutex_t error_list_mutex;

int nthreads;
pthread_t progress_thread;
BOOL progress_exit_request;
pthread_cond_t progress_cond;
pthread_mutex_t progress_mutex;

FILE *outfile;
enum test_mode_t {
    TEST_DEFAULT_NOSTRICT, /* run tests as nostrict unless test is flagged as strictonly */
    TEST_DEFAULT_STRICT,   /* run tests as strict unless test is flagged as nostrict */
    TEST_NOSTRICT,         /* run tests as nostrict, skip strictonly tests */
    TEST_STRICT,           /* run tests as strict, skip nostrict tests */
    TEST_ALL,              /* run tests in both strict and nostrict, unless restricted by spec */
} test_mode = TEST_DEFAULT_NOSTRICT;
int compact;
int show_timings;
int skip_async;
int skip_module;
int new_style;
int dump_memory;
int stats_count;
JSMemoryUsage stats_all, stats_avg, stats_min, stats_max;
char *stats_min_filename;
char *stats_max_filename;
pthread_mutex_t stats_mutex;
int verbose;
char *harness_dir;
char *harness_exclude;
char *harness_features;
char *harness_skip_features;
int *harness_skip_features_count;
char *error_filename;
char *error_file;
char *report_filename;
int update_errors;
int slow_test_threshold;
int start_index, stop_index;
int test_excluded;
_Atomic int test_count, test_failed, test_skipped;
_Atomic int new_errors, changed_errors, fixed_errors;


#if defined(_WIN32)
static int cpu_count(void)
{
    DWORD_PTR procmask, sysmask;
    long count;
    int i;

    count = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &procmask, &sysmask))
        for (i = 0; i < 8 * sizeof(procmask); i++)
            count += 1 & (procmask >> i);
    return count;
}
#elif defined(__linux__)
/* return the number of available physical cores or -1 if not available */
static int get_cpu_info_physical_cores(void)
{
    FILE *f;
    int nb_cores, physical_id;
    char line[1024], *p;
    char *field, *value;
    int len;
    
    f = fopen("/proc/cpuinfo", "rb");
    if (!f)
        return -1;
    nb_cores = 0;
    physical_id = -1;
    for(;;) {
        if (fgets(line, sizeof(line), f) == NULL)
            break;
        len = strlen(line);
        while (len > 0 && isspace(line[len - 1]))
            len--;
        line[len] = '\0';
        field = line;
        p = line;
        if (*p == '#')
            continue;
        while (*p != ':' && *p != '\0')
            p++;
        if (*p == '\0')
            continue;
        *p = '\0';
        p++;
        while (isspace(*p))
            p++;
        value = p;
        
        len = strlen(field);
        while (len > 0 && isspace(field[len - 1]))
            len--;
        field[len] = '\0';

        //        printf("'%s' '%s'\n", field, value);
        if (!strcmp(field, "cpu cores")) {
            if (nb_cores == 0) {
                nb_cores = strtol(value, NULL, 0);
            }
        } else if (!strcmp(field, "physical id")) {
            physical_id = max_int(physical_id, strtol(value, NULL, 0));
        }
    }
    fclose(f);
    //    printf("nb_cores=%d physical_id=%d\n", nb_cores, physical_id);
    if (nb_cores <= 0 || physical_id < 0)
        return -1;
    return nb_cores * (physical_id + 1);
}

static int cpu_count(void)
{
    int n = get_cpu_info_physical_cores();
    if (n <= 0)
        n = 1;
    return n;
}
#else /* __linux__ */
static int cpu_count(void)
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}
#endif /* !__linux__ */

static int add_test_file(const char *filename, const struct stat *ptr, int flag)
{
    namelist_t *lp = &test_list;
    if (has_suffix(filename, ".js") && !has_suffix(filename, "_FIXTURE.js"))
        namelist_add(lp, NULL, filename);
    return 0;
}

/* find js files from the directory tree and sort the list */
static void enumerate_tests(const char *path)
{
    namelist_t *lp = &test_list;
    int start = lp->count;
    ftw(path, add_test_file, 100);
    qsort(lp->array + start, lp->count - start, sizeof(*lp->array),
          namelist_cmp_indirect);
}


/* handle exclude directories */
void update_exclude_dirs(void)
{
    namelist_t *lp = &test_list;
    namelist_t *ep = &exclude_list;
    namelist_t *dp = &exclude_dir_list;
    char *name;
    int i, j, count;

    /* split directories from exclude_list */
    for (count = i = 0; i < ep->count; i++) {
        name = ep->array[i];
        if (has_suffix(name, "/")) {
            namelist_add(dp, NULL, name);
            free(name);
        } else {
            ep->array[count++] = name;
        }
    }
    ep->count = count;

    namelist_sort(dp, TRUE);

    /* filter out excluded directories */
    for (count = i = 0; i < lp->count; i++) {
        name = lp->array[i];
        for (j = 0; j < dp->count; j++) {
            if (has_prefix(name, dp->array[j])) {
                test_excluded++;
                free(name);
                name = NULL;
                break;
            }
        }
        if (name) {
            lp->array[count++] = name;
        }
    }
    lp->count = count;
}

void load_config(const char *filename, const char *ignore)
{
    char buf[1024];
    FILE *f;
    char *base_name;
    enum {
        SECTION_NONE = 0,
        SECTION_CONFIG,
        SECTION_EXCLUDE,
        SECTION_FEATURES,
        SECTION_TESTS,
    } section = SECTION_NONE;
    int lineno = 0;

    f = fopen(filename, "rb");
    if (!f) {
        perror_exit(1, filename);
    }
    base_name = get_basename(filename);

    while (fgets(buf, sizeof(buf), f) != NULL) {
        char *p, *q;
        lineno++;
        p = str_strip(buf);
        if (*p == '#' || *p == ';' || *p == '\0')
            continue;  /* line comment */

        if (*p == "[]"[0]) {
            /* new section */
            p++;
            p[strcspn(p, "]")] = '\0';
            if (str_equal(p, "config"))
                section = SECTION_CONFIG;
            else if (str_equal(p, "exclude"))
                section = SECTION_EXCLUDE;
            else if (str_equal(p, "features"))
                section = SECTION_FEATURES;
            else if (str_equal(p, "tests"))
                section = SECTION_TESTS;
            else
                section = SECTION_NONE;
            continue;
        }
        q = strchr(p, '=');
        if (q) {
            /* setting: name=value */
            *q++ = '\0';
            q = str_strip(q);
        }
        switch (section) {
        case SECTION_CONFIG:
            if (!q) {
                printf("%s:%d: syntax error\n", filename, lineno);
                continue;
            }
            if (strstr(ignore, p)) {
                printf("%s:%d: ignoring %s=%s\n", filename, lineno, p, q);
                continue;
            }
            if (str_equal(p, "style")) {
                new_style = str_equal(q, "new");
                continue;
            }
            if (str_equal(p, "testdir")) {
                char *testdir = compose_path(base_name, q);
                enumerate_tests(testdir);
                free(testdir);
                continue;
            }
            if (str_equal(p, "harnessdir")) {
                harness_dir = compose_path(base_name, q);
                continue;
            }
            if (str_equal(p, "harnessexclude")) {
                str_append(&harness_exclude, " ", q);
                continue;
            }
            if (str_equal(p, "features")) {
                str_append(&harness_features, " ", q);
                continue;
            }
            if (str_equal(p, "skip-features")) {
                str_append(&harness_skip_features, " ", q);
                continue;
            }
            if (str_equal(p, "mode")) {
                if (str_equal(q, "default") || str_equal(q, "default-nostrict"))
                    test_mode = TEST_DEFAULT_NOSTRICT;
                else if (str_equal(q, "default-strict"))
                    test_mode = TEST_DEFAULT_STRICT;
                else if (str_equal(q, "nostrict"))
                    test_mode = TEST_NOSTRICT;
                else if (str_equal(q, "strict"))
                    test_mode = TEST_STRICT;
                else if (str_equal(q, "all") || str_equal(q, "both"))
                    test_mode = TEST_ALL;
                else
                    fatal(2, "unknown test mode: %s", q);
                continue;
            }
            if (str_equal(p, "strict")) {
                if (str_equal(q, "skip") || str_equal(q, "no"))
                    test_mode = TEST_NOSTRICT;
                continue;
            }
            if (str_equal(p, "nostrict")) {
                if (str_equal(q, "skip") || str_equal(q, "no"))
                    test_mode = TEST_STRICT;
                continue;
            }
            if (str_equal(p, "async")) {
                skip_async = !str_equal(q, "yes");
                continue;
            }
            if (str_equal(p, "module")) {
                skip_module = !str_equal(q, "yes");
                continue;
            }
            if (str_equal(p, "verbose")) {
                verbose = str_equal(q, "yes");
                continue;
            }
            if (str_equal(p, "errorfile")) {
                error_filename = compose_path(base_name, q);
                continue;
            }
            if (str_equal(p, "excludefile")) {
                char *path = compose_path(base_name, q);
                namelist_load(&exclude_list, path);
                free(path);
                continue;
            }
            if (str_equal(p, "reportfile")) {
                report_filename = compose_path(base_name, q);
                continue;
            }
        case SECTION_EXCLUDE:
            namelist_add(&exclude_list, base_name, p);
            break;
        case SECTION_FEATURES:
            if (!q || str_equal(q, "yes"))
                str_append(&harness_features, " ", p);
            else
                str_append(&harness_skip_features, " ", p);
            break;
        case SECTION_TESTS:
            namelist_add(&test_list, base_name, p);
            break;
        default:
            /* ignore settings in other sections */
            break;
        }
    }
    fclose(f);
    free(base_name);
}

char *find_error(const char *filename, int *pline, int is_strict)
{
    if (error_file) {
        size_t len = strlen(filename);
        const char *p, *q, *r;
        int line;

        for (p = error_file; (p = strstr(p, filename)) != NULL; p += len) {
            if ((p == error_file || p[-1] == '\n' || p[-1] == '(') && p[len] == ':') {
                q = p + len;
                line = 1;
                if (*q == ':') {
                    line = strtol(q + 1, (char**)&q, 10);
                    if (*q == ':')
                        q++;
                }
                while (*q == ' ') {
                    q++;
                }
                /* check strict mode indicator */
                if (!strstart(q, "strict mode: ", &q) != !is_strict)
                    continue;
                r = q = skip_prefix(q, "unexpected error: ");
                r += strcspn(r, "\n");
                while (r[0] == '\n' && r[1] && strncmp(r + 1, filename, 8)) {
                    r++;
                    r += strcspn(r, "\n");
                }
                if (pline)
                    *pline = line;
                return strdup_len(q, r - q);
            }
        }
    }
    return NULL;
}

int skip_comments(const char *str, int line, int *pline)
{
    const char *p;
    int c;

    p = str;
    while ((c = (unsigned char)*p++) != '\0') {
        if (isspace(c)) {
            if (c == '\n')
                line++;
            continue;
        }
        if (c == '/' && *p == '/') {
            while (*++p && *p != '\n')
                continue;
            continue;
        }
        if (c == '/' && *p == '*') {
            for (p += 1; *p; p++) {
                if (*p == '\n') {
                    line++;
                    continue;
                }
                if (*p == '*' && p[1] == '/') {
                    p += 2;
                    break;
                }
            }
            continue;
        }
        break;
    }
    if (pline)
        *pline = line;

    return p - str;
}

int longest_match(const char *str, const char *find, int pos, int *ppos, int line, int *pline)
{
    int len, maxlen;

    maxlen = 0;

    if (*find) {
        const char *p;
        for (p = str + pos; *p; p++) {
            if (*p == *find) {
                for (len = 1; p[len] && p[len] == find[len]; len++)
                    continue;
                if (len > maxlen) {
                    maxlen = len;
                    if (ppos)
                        *ppos = p - str;
                    if (pline)
                        *pline = line;
                    if (!find[len])
                        break;
                }
            }
            if (*p == '\n')
                line++;
        }
    }
    return maxlen;
}

static __attribute__((__format__(__printf__, 1, 2))) void print_error(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (update_errors) {
        pthread_mutex_lock(&error_list_mutex);
        namelist_add(&error_list, NULL, buf);
        pthread_mutex_unlock(&error_list_mutex);
    } else {
        fputs(buf, stdout);
    }
}

static int eval_buf(JSContext *ctx, const char *buf, size_t buf_len,
                    const char *filename, int is_test, int is_negative,
                    const char *error_type, FILE *outfile, int eval_flags,
                    int is_async)
{
    ThreadLocalStorage *tls = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    JSValue res_val, exception_val;
    int ret, error_line, pos, pos_line;
    BOOL is_error, has_error_line, ret_promise;
    const char *error_name;

    pos = skip_comments(buf, 1, &pos_line);
    error_line = pos_line;
    has_error_line = FALSE;
    exception_val = JS_UNDEFINED;
    error_name = NULL;

    /* a module evaluation returns a promise */
    ret_promise = ((eval_flags & JS_EVAL_TYPE_MODULE) != 0);
    tls->async_done = 0; /* counter of "Test262:AsyncTestComplete" messages */

    res_val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);

    if ((is_async || ret_promise) && !JS_IsException(res_val)) {
        JSValue promise = JS_UNDEFINED;
        if (ret_promise) {
            promise = res_val;
        } else {
            JS_FreeValue(ctx, res_val);
        }
        for(;;) {
            ret = JS_ExecutePendingJob(JS_GetRuntime(ctx), NULL);
            if (ret < 0) {
                res_val = JS_EXCEPTION;
                break;
            } else if (ret == 0) {
                if (is_async) {
                    /* test if the test called $DONE() once */
                    if (tls->async_done != 1) {
                        res_val = JS_ThrowTypeError(ctx, "$DONE() not called");
                    } else {
                        res_val = JS_UNDEFINED;
                    }
                } else {
                    /* check that the returned promise is fulfilled */
                    JSPromiseStateEnum state = JS_PromiseState(ctx, promise);
                    if (state == JS_PROMISE_FULFILLED)
                        res_val = JS_UNDEFINED;
                    else if (state == JS_PROMISE_REJECTED)
                        res_val = JS_Throw(ctx, JS_PromiseResult(ctx, promise));
                    else
                        res_val = JS_ThrowTypeError(ctx, "promise is pending");
                }
                break;
            }
        }
        JS_FreeValue(ctx, promise);
    }

    if (JS_IsException(res_val)) {
        exception_val = JS_GetException(ctx);
        is_error = JS_IsError(ctx, exception_val);
        /* XXX: should get the filename and line number */
        if (outfile) {
            if (!is_error)
                fprintf(outfile, "%sThrow: ", (eval_flags & JS_EVAL_FLAG_STRICT) ?
                        "strict mode: " : "");
            js_print(ctx, JS_NULL, 1, &exception_val);
        }
        if (is_error) {
            JSValue name, stack;
            const char *stack_str;

            name = JS_GetPropertyStr(ctx, exception_val, "name");
            error_name = JS_ToCString(ctx, name);
            stack = JS_GetPropertyStr(ctx, exception_val, "stack");
            if (!JS_IsUndefined(stack)) {
                stack_str = JS_ToCString(ctx, stack);
                if (stack_str) {
                    const char *p;
                    int len;

                    if (outfile)
                        fprintf(outfile, "%s", stack_str);

                    len = strlen(filename);
                    p = strstr(stack_str, filename);
                    if (p != NULL && p[len] == ':') {
                        error_line = atoi(p + len + 1);
                        has_error_line = TRUE;
                    }
                    JS_FreeCString(ctx, stack_str);
                }
            }
            JS_FreeValue(ctx, stack);
            JS_FreeValue(ctx, name);
        }
        if (is_negative) {
            ret = 0;
            if (error_type) {
                char *error_class;
                const char *msg;

                msg = JS_ToCString(ctx, exception_val);
                error_class = strdup_len(msg, strcspn(msg, ":"));
                if (!str_equal(error_class, error_type))
                    ret = -1;
                free(error_class);
                JS_FreeCString(ctx, msg);
            }
        } else {
            ret = -1;
        }
    } else {
        if (is_negative)
            ret = -1;
        else
            ret = 0;
    }

    if (verbose && is_test) {
        JSValue msg_val = JS_UNDEFINED;
        const char *msg = NULL;
        int s_line;
        char *s = find_error(filename, &s_line, eval_flags & JS_EVAL_FLAG_STRICT);
        const char *strict_mode = (eval_flags & JS_EVAL_FLAG_STRICT) ? "strict mode: " : "";

        if (!JS_IsUndefined(exception_val)) {
            msg_val = JS_ToString(ctx, exception_val);
            msg = JS_ToCString(ctx, msg_val);
        }
        if (is_negative) {  // expect error
            if (ret == 0) {
                if (msg && s &&
                    (str_equal(s, "expected error") ||
                     strstart(s, "unexpected error type:", NULL) ||
                     str_equal(s, msg))) {     // did not have error yet
                    if (!has_error_line) {
                        longest_match(buf, msg, pos, &pos, pos_line, &error_line);
                    }
                    printf("%s:%d: %sOK, now has error %s\n",
                           filename, error_line, strict_mode, msg);
                    fixed_errors++;
                }
            } else {
                if (!s) {   // not yet reported
                    if (msg) {
                        print_error("%s:%d: %sunexpected error type: %s\n",
                                    filename, error_line, strict_mode, msg);
                    } else {
                        print_error("%s:%d: %sexpected error\n",
                                    filename, error_line, strict_mode);
                    }
                    new_errors++;
                }
            }
        } else {            // should not have error
            if (msg) {
                if (!s || !str_equal(s, msg)) {
                    if (!has_error_line) {
                        char *p = skip_prefix(msg, "Test262 Error: ");
                        if (strstr(p, "Test case returned non-true value!")) {
                            longest_match(buf, "runTestCase", pos, &pos, pos_line, &error_line);
                        } else {
                            longest_match(buf, p, pos, &pos, pos_line, &error_line);
                        }
                    }
                    print_error("%s:%d: %s%s%s\n", filename, error_line, strict_mode,
                                error_file ? "unexpected error: " : "", msg);

                    if (s && (!str_equal(s, msg) || error_line != s_line)) {
                        printf("%s:%d: %sprevious error: %s\n", filename, s_line, strict_mode, s);
                        changed_errors++;
                    } else {
                        new_errors++;
                    }
                }
            } else {
                if (s) {
                    printf("%s:%d: %sOK, fixed error: %s\n", filename, s_line, strict_mode, s);
                    fixed_errors++;
                }
            }
        }
        JS_FreeValue(ctx, msg_val);
        JS_FreeCString(ctx, msg);
        free(s);
    }
    JS_FreeCString(ctx, error_name);
    JS_FreeValue(ctx, exception_val);
    JS_FreeValue(ctx, res_val);
    return ret;
}

static int eval_file(JSContext *ctx, const char *base, const char *p,
                     int eval_flags)
{
    char *buf;
    size_t buf_len;
    char *filename = compose_path(base, p);

    buf = load_file(filename, &buf_len);
    if (!buf) {
        warning("cannot load %s", filename);
        goto fail;
    }
    if (eval_buf(ctx, buf, buf_len, filename, FALSE, FALSE, NULL, stderr,
                 eval_flags, FALSE)) {
        warning("error evaluating %s", filename);
        goto fail;
    }
    free(buf);
    free(filename);
    return 0;

fail:
    free(buf);
    free(filename);
    return 1;
}

void update_stats(JSRuntime *rt, const char *filename) {
    JSMemoryUsage stats;
    JS_ComputeMemoryUsage(rt, &stats);

    pthread_mutex_lock(&stats_mutex);
    if (stats_count++ == 0) {
        stats_avg = stats_all = stats_min = stats_max = stats;
        stats_min_filename = strdup(filename);
        stats_max_filename = strdup(filename);
    } else {
        if (stats_max.malloc_size < stats.malloc_size) {
            stats_max = stats;
            free(stats_max_filename);
            stats_max_filename = strdup(filename);
        }
        if (stats_min.malloc_size > stats.malloc_size) {
            stats_min = stats;
            free(stats_min_filename);
            stats_min_filename = strdup(filename);
        }

#define update(f)  stats_avg.f = (stats_all.f += stats.f) / stats_count
        update(malloc_count);
        update(malloc_size);
        update(memory_used_count);
        update(memory_used_size);
        update(atom_count);
        update(atom_size);
        update(str_count);
        update(str_size);
        update(obj_count);
        update(obj_size);
        update(prop_count);
        update(prop_size);
        update(shape_count);
        update(shape_size);
        update(js_func_count);
        update(js_func_size);
        update(js_func_code_size);
        update(js_func_pc2line_count);
        update(js_func_pc2line_size);
        update(c_func_count);
        update(array_count);
        update(fast_array_count);
        update(fast_array_elements);
    }
#undef update
    pthread_mutex_unlock(&stats_mutex);
}

int run_test_buf(ThreadLocalStorage *tls,
                 const char *filename, const char *harness, namelist_t *ip,
                 char *buf, size_t buf_len, const char* error_type,
                 int eval_flags, BOOL is_negative, BOOL is_async,
                 BOOL can_block)
{
    JSRuntime *rt;
    JSContext *ctx;
    int i, ret;

    rt = JS_NewRuntime();
    if (rt == NULL) {
        fatal(1, "JS_NewRuntime failure");
    }
    JS_SetRuntimeOpaque(rt, tls);
    ctx = JS_NewContext(rt);
    if (ctx == NULL) {
        JS_FreeRuntime(rt);
        fatal(1, "JS_NewContext failure");
    }
    JS_SetRuntimeInfo(rt, filename);

    JS_SetCanBlock(rt, can_block);

    /* loader for ES6 modules */
    JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader_test, NULL, (void *)filename);

    add_helpers(ctx);

    for (i = 0; i < ip->count; i++) {
        if (eval_file(ctx, harness, ip->array[i],
                      JS_EVAL_TYPE_GLOBAL)) {
            fatal(1, "error including %s for %s", ip->array[i], filename);
        }
    }

    ret = eval_buf(ctx, buf, buf_len, filename, TRUE, is_negative,
                   error_type, outfile, eval_flags, is_async);
    ret = (ret != 0);

    if (dump_memory) {
        update_stats(rt, filename);
    }
    js_agent_free(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    atomic_inc(&test_count);
    if (ret) {
        atomic_inc(&test_failed);
        if (outfile) {
            /* do not output a failure number to minimize diff */
            fprintf(outfile, "  FAILED\n");
        }
    }
    return ret;
}

int run_test(ThreadLocalStorage *tls, const char *filename, int index)
{
    char harnessbuf[1024];
    char *harness;
    char *buf;
    size_t buf_len;
    char *desc, *p;
    char *error_type;
    int ret, eval_flags, use_strict, use_nostrict;
    BOOL is_negative, is_nostrict, is_onlystrict, is_async, is_module, skip;
    BOOL can_block;
    namelist_t include_list = { 0 }, *ip = &include_list;

    is_nostrict = is_onlystrict = is_negative = is_async = is_module = skip = FALSE;
    can_block = TRUE;
    error_type = NULL;
    buf = load_file(filename, &buf_len);

    harness = harness_dir;

    if (new_style) {
        if (!harness) {
            p = strstr(filename, "test/");
            if (p) {
                snprintf(harnessbuf, sizeof(harnessbuf), "%.*s%s",
                         (int)(p - filename), filename, "harness");
            } else {
                pstrcpy(harnessbuf, sizeof(harnessbuf), "");
            }
            harness = harnessbuf;
        }
        namelist_add(ip, NULL, "sta.js");
        namelist_add(ip, NULL, "assert.js");
        /* extract the YAML frontmatter */
        desc = extract_desc(buf, '-');
        if (desc) {
            char *ifile, *option;
            int state;
            p = find_tag(desc, "includes:", &state);
            if (p) {
                while ((ifile = get_option(&p, &state)) != NULL) {
                    // skip unsupported harness files
                    if (find_word(harness_exclude, ifile)) {
                        skip |= 1;
                    } else {
                        namelist_add(ip, NULL, ifile);
                    }
                    free(ifile);
                }
            }
            p = find_tag(desc, "flags:", &state);
            if (p) {
                while ((option = get_option(&p, &state)) != NULL) {
                    if (str_equal(option, "noStrict") ||
                        str_equal(option, "raw")) {
                        is_nostrict = TRUE;
                        skip |= (test_mode == TEST_STRICT);
                    }
                    else if (str_equal(option, "onlyStrict")) {
                        is_onlystrict = TRUE;
                        skip |= (test_mode == TEST_NOSTRICT);
                    }
                    else if (str_equal(option, "async")) {
                        is_async = TRUE;
                        skip |= skip_async;
                    }
                    else if (str_equal(option, "module")) {
                        is_module = TRUE;
                        skip |= skip_module;
                    }
                    else if (str_equal(option, "CanBlockIsFalse")) {
                        can_block = FALSE;
                    }
                    free(option);
                }
            }
            p = find_tag(desc, "negative:", &state);
            if (p) {
                /* XXX: should extract the phase */
                char *q = find_tag(p, "type:", &state);
                if (q) {
                    while (isspace((unsigned char)*q))
                        q++;
                    error_type = strdup_len(q, strcspn(q, " \n"));
                }
                is_negative = TRUE;
            }
            p = find_tag(desc, "features:", &state);
            if (p) {
                while ((option = get_option(&p, &state)) != NULL) {
                    char *p1;
                    if (find_word(harness_features, option)) {
                        /* feature is enabled */
                    } else if ((p1 = find_word(harness_skip_features, option)) != NULL) {
                        /* skip disabled feature */
                        if (harness_skip_features_count)
                            harness_skip_features_count[p1 - harness_skip_features]++;
                        skip |= 1;
                    } else {
                        /* feature is not listed: skip and warn */
                        printf("%s:%d: unknown feature: %s\n", filename, 1, option);
                        skip |= 1;
                    }
                    free(option);
                }
            }
            free(desc);
        }
        if (is_async)
            namelist_add(ip, NULL, "doneprintHandle.js");
    } else {
        char *ifile;

        if (!harness) {
            p = strstr(filename, "test/");
            if (p) {
                snprintf(harnessbuf, sizeof(harnessbuf), "%.*s%s",
                         (int)(p - filename), filename, "test/harness");
            } else {
                pstrcpy(harnessbuf, sizeof(harnessbuf), "");
            }
            harness = harnessbuf;
        }

        namelist_add(ip, NULL, "sta.js");

        /* include extra harness files */
        for (p = buf; (p = strstr(p, "$INCLUDE(\"")) != NULL; p++) {
            p += 10;
            ifile = strdup_len(p, strcspn(p, "\""));
            // skip unsupported harness files
            if (find_word(harness_exclude, ifile)) {
                skip |= 1;
            } else {
                namelist_add(ip, NULL, ifile);
            }
            free(ifile);
        }

        /* locate the old style configuration comment */
        desc = extract_desc(buf, '*');
        if (desc) {
            if (strstr(desc, "@noStrict")) {
                is_nostrict = TRUE;
                skip |= (test_mode == TEST_STRICT);
            }
            if (strstr(desc, "@onlyStrict")) {
                is_onlystrict = TRUE;
                skip |= (test_mode == TEST_NOSTRICT);
            }
            if (strstr(desc, "@negative")) {
                /* XXX: should extract the regex to check error type */
                is_negative = TRUE;
            }
            free(desc);
        }
    }

    if (outfile && index >= 0) {
        fprintf(outfile, "%d: %s%s%s%s%s%s%s\n", index, filename,
                is_nostrict ? "  @noStrict" : "",
                is_onlystrict ? "  @onlyStrict" : "",
                is_async ? "  async" : "",
                is_module ? "  module" : "",
                is_negative ? "  @negative" : "",
                skip ? "  SKIPPED" : "");
        fflush(outfile);
    }

    use_strict = use_nostrict = 0;
    /* XXX: should remove 'test_mode' or simplify it just to force
       strict or non strict mode for single file tests */
    switch (test_mode) {
    case TEST_DEFAULT_NOSTRICT:
        if (is_onlystrict)
            use_strict = 1;
        else
            use_nostrict = 1;
        break;
    case TEST_DEFAULT_STRICT:
        if (is_nostrict)
            use_nostrict = 1;
        else
            use_strict = 1;
        break;
    case TEST_NOSTRICT:
        if (!is_onlystrict)
            use_nostrict = 1;
        break;
    case TEST_STRICT:
        if (!is_nostrict)
            use_strict = 1;
        break;
    case TEST_ALL:
        if (is_module) {
            use_nostrict = 1;
        } else {
            if (!is_nostrict)
                use_strict = 1;
            if (!is_onlystrict)
                use_nostrict = 1;
        }
        break;
    }

    if (skip || use_strict + use_nostrict == 0) {
        atomic_inc(&test_skipped);
        ret = -2;
    } else {
        clock_t clocks;

        if (is_module) {
            eval_flags = JS_EVAL_TYPE_MODULE;
        } else {
            eval_flags = JS_EVAL_TYPE_GLOBAL;
        }
        clocks = clock();
        ret = 0;
        if (use_nostrict) {
            ret = run_test_buf(tls, filename, harness, ip, buf, buf_len,
                               error_type, eval_flags, is_negative, is_async,
                               can_block);
        }
        if (use_strict) {
            ret |= run_test_buf(tls, filename, harness, ip, buf, buf_len,
                                error_type, eval_flags | JS_EVAL_FLAG_STRICT,
                                is_negative, is_async, can_block);
        }
        clocks = clock() - clocks;
        if (outfile && index >= 0 && clocks >= CLOCKS_PER_SEC / 10) {
            /* output timings for tests that take more than 100 ms */
            fprintf(outfile, " time: %d ms\n", (int)(clocks * 1000LL / CLOCKS_PER_SEC));
        }
    }
    namelist_free(&include_list);
    free(error_type);
    free(buf);

    return ret;
}

/* run a test when called by test262-harness+eshost */
int run_test262_harness_test(ThreadLocalStorage *tls,
                             const char *filename, BOOL is_module, BOOL can_block)
{
    JSRuntime *rt;
    JSContext *ctx;
    char *buf;
    size_t buf_len;
    int eval_flags, ret_code, ret;
    JSValue res_val;

    outfile = stdout; /* for js_print */

    rt = JS_NewRuntime();
    if (rt == NULL) {
        fatal(1, "JS_NewRuntime failure");
    }
    JS_SetRuntimeOpaque(rt, tls);
    ctx = JS_NewContext(rt);
    if (ctx == NULL) {
        JS_FreeRuntime(rt);
        fatal(1, "JS_NewContext failure");
    }
    JS_SetRuntimeInfo(rt, filename);

    JS_SetCanBlock(rt, can_block);

    /* loader for ES6 modules */
    JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader_test, NULL, (void *)filename);

    add_helpers(ctx);

    buf = load_file(filename, &buf_len);

    if (is_module) {
      eval_flags = JS_EVAL_TYPE_MODULE;
    } else {
      eval_flags = JS_EVAL_TYPE_GLOBAL;
    }
    res_val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
    ret_code = 0;
    if (JS_IsException(res_val)) {
       js_std_dump_error(ctx);
       ret_code = 1;
    } else {
        JSValue promise = JS_UNDEFINED;
        if (is_module) {
            promise = res_val;
        } else {
            JS_FreeValue(ctx, res_val);
        }
        for(;;) {
            ret = JS_ExecutePendingJob(JS_GetRuntime(ctx), NULL);
            if (ret < 0) {
                js_std_dump_error(ctx);
                ret_code = 1;
            } else if (ret == 0) {
                break;
            }
        }
        /* dump the error if the module returned an error. */
        if (is_module) {
            JSPromiseStateEnum state = JS_PromiseState(ctx, promise);
            if (state == JS_PROMISE_REJECTED) {
                JS_Throw(ctx, JS_PromiseResult(ctx, promise));
                js_std_dump_error(ctx);
                ret_code = 1;
            }
        }
        JS_FreeValue(ctx, promise);
    }
    free(buf);
    js_agent_free(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return ret_code;
}

static int pthread_cond_timedwait2(pthread_cond_t *cond, pthread_mutex_t *mutex, int timeout)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout / 1000;
    ts.tv_nsec += (timeout % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec++;
    }
    return pthread_cond_timedwait(cond, mutex, &ts);
}

void *show_progress(void *opaque)
{
    int test_skipped1, test_failed1, test_count1;

    pthread_mutex_lock(&progress_mutex);
    for(;;) {
        pthread_cond_timedwait2(&progress_cond, &progress_mutex, 50);

        test_failed1 = atomic_load(&test_failed);
        test_count1 = atomic_load(&test_count);
        test_skipped1 = atomic_load(&test_skipped);

        if (compact) {
            static int last_test_skipped;
            static int last_test_failed;
            static int dots;
            char c = '.';
            
            if (test_skipped1 > last_test_skipped)
                c = '-';
            if (test_failed1 > last_test_failed)
                c = '!';
            last_test_skipped = test_skipped1;
            last_test_failed = test_failed1;

            fputc(c, stderr);
            if (progress_exit_request || ++dots % 60 == 0) {
                fprintf(stderr, " %d/%d/%d\n",
                        test_failed1, test_count1, test_skipped1);
            }
        } else {
            /* output progress indicator: erase end of line and return to col 0 */
            fprintf(stderr, "%d/%d/%d\033[K\r",
                    test_failed1, test_count1, test_skipped1);
        }
        fflush(stderr);
        if (progress_exit_request)
            break;
    }
    pthread_mutex_unlock(&progress_mutex);
    return NULL;
}

enum { INCLUDE, EXCLUDE, SKIP };

int include_exclude_or_skip(int i) // naming is hard...
{
    if (namelist_find(&exclude_list, test_list.array[i]) >= 0)
        return EXCLUDE;
    if (i < start_index)
        return SKIP;
    if (stop_index >= 0 && i > stop_index)
        return SKIP;
    return INCLUDE;
}

typedef struct {
    pthread_t tid;
    int thread_index;
} RunTestDirThread;

void *run_test_dir_list(void *opaque)
{
    RunTestDirThread *th = opaque;
    ThreadLocalStorage tls_s, *tls = &tls_s;
    namelist_t *lp = &test_list;
    int i;
    
    init_thread_local_storage(tls);
    
    for (i = th->thread_index; i < lp->count; i += nthreads) {
        const char *p = lp->array[i];
        int ti;
        if (INCLUDE != include_exclude_or_skip(i))
            continue;
        
        if (slow_test_threshold != 0) {
            ti = get_clock_ms();
        } else {
            ti = 0;
        }
        run_test(tls, p, i);
        if (slow_test_threshold != 0) {
            ti = get_clock_ms() - ti;
            if (ti >= slow_test_threshold)
                fprintf(stderr, "\n%s (%d ms)\n", p, ti);
        }
    }
    return NULL;
}

void help(void)
{
    printf("run-test262 version " CONFIG_VERSION "\n"
           "usage: run-test262 [options] {-f file ... | [dir_list] [index range]}\n"
           "-h             help\n"
           "-a             run tests in strict and nostrict modes\n"
           "-m             print memory usage summary\n"
           "-n             use new style harness\n"
           "-N             run test prepared by test262-harness+eshost\n"
           "-s             run tests in strict mode, skip @nostrict tests\n"
           "-E             only run tests from the error file\n"
           "-C             use compact progress indicator\n"
           "-t             show timings\n"
           "-u             update error file\n"
           "-v             verbose: output error messages\n"
           "-D duration    display tests taking more than 'duration' ms\n"
           "-T threads     number of parallel threads\n"
           "-c file        read configuration from 'file'\n"
           "-d dir         run all test files in directory tree 'dir'\n"
           "-e file        load the known errors from 'file'\n"
           "-f file        execute single test from 'file'\n"
           "-r file        set the report file name (default=none)\n"
           "-x file        exclude tests listed in 'file'\n"
           "--no-can-block set [[CanBlock]] to false (Atomics.wait will throw)\n");
    exit(1);
}

char *get_opt_arg(const char *option, char *arg)
{
    if (!arg) {
        fatal(2, "missing argument for option %s", option);
    }
    return arg;
}

int main(int argc, char **argv)
{
    ThreadLocalStorage tls_s, *tls = &tls_s;
    int optind;
    BOOL is_dir_list;
    BOOL only_check_errors = FALSE;
    const char *filename;
    const char *ignore = "";
    BOOL is_test262_harness = FALSE;
    BOOL is_module = FALSE;
    BOOL can_block = TRUE;
    BOOL count_skipped_features = FALSE;
    clock_t clocks;
    
    init_thread_local_storage(tls);
    pthread_mutex_init(&stats_mutex, NULL);
    pthread_mutex_init(&error_list_mutex, NULL);

#if !defined(_WIN32)
    compact = !isatty(STDERR_FILENO);
    /* Date tests assume California local time */
    setenv("TZ", "America/Los_Angeles", 1);
#endif

    optind = 1;
    while (optind < argc) {
        char *arg = argv[optind];
        if (*arg != '-')
            break;
        optind++;
        if (strstr("-c -d -e -x -f -r -E -D -T", arg))
            optind++;
        if (strstr("-d -f", arg))
            ignore = "testdir"; // run only the tests from -d or -f
    }

    /* cannot use getopt because we want to pass the command line to
       the script */
    optind = 1;
    is_dir_list = TRUE;
    while (optind < argc) {
        char *arg = argv[optind];
        if (*arg != '-')
            break;
        optind++;
        if (str_equal(arg, "-h")) {
            help();
        } else if (str_equal(arg, "-m")) {
            dump_memory++;
        } else if (str_equal(arg, "-n")) {
            new_style++;
        } else if (str_equal(arg, "-s")) {
            test_mode = TEST_STRICT;
        } else if (str_equal(arg, "-a")) {
            test_mode = TEST_ALL;
        } else if (str_equal(arg, "-t")) {
            show_timings++;
        } else if (str_equal(arg, "-u")) {
            update_errors++;
        } else if (str_equal(arg, "-v")) {
            verbose++;
        } else if (str_equal(arg, "-C")) {
            compact = 1;
        } else if (str_equal(arg, "-c")) {
            load_config(get_opt_arg(arg, argv[optind++]), ignore);
        } else if (str_equal(arg, "-d")) {
            enumerate_tests(get_opt_arg(arg, argv[optind++]));
        } else if (str_equal(arg, "-e")) {
            error_filename = get_opt_arg(arg, argv[optind++]);
        } else if (str_equal(arg, "-x")) {
            namelist_load(&exclude_list, get_opt_arg(arg, argv[optind++]));
        } else if (str_equal(arg, "-f")) {
            is_dir_list = FALSE;
        } else if (str_equal(arg, "-r")) {
            report_filename = get_opt_arg(arg, argv[optind++]);
        } else if (str_equal(arg, "-E")) {
            only_check_errors = TRUE;
        } else if (str_equal(arg, "-D")) {
            slow_test_threshold = atoi(get_opt_arg(arg, argv[optind++]));
        } else if (str_equal(arg, "-T")) {
            nthreads = atoi(get_opt_arg(arg, argv[optind++]));
        } else if (str_equal(arg, "-N")) {
            is_test262_harness = TRUE;
        } else if (str_equal(arg, "--module")) {
            is_module = TRUE;
        } else if (str_equal(arg, "--no-can-block")) {
            can_block = FALSE;
        } else if (str_equal(arg, "--count_skipped_features")) {
            count_skipped_features = TRUE;
        } else {
            fatal(1, "unknown option: %s", arg);
            break;
        }
    }

    if (optind >= argc && !test_list.count)
        help();

    if (is_test262_harness) {
        return run_test262_harness_test(tls, argv[optind], is_module, can_block);
    }

    if (nthreads == 0) {
        nthreads = cpu_count();
        if (nthreads >= 8) {
            // minus one to not (over)commit the system completely
            nthreads--;
        }
    }
    nthreads = max_int(nthreads, 1);

    if (error_filename) {
        error_file = load_file(error_filename, NULL);
        if (only_check_errors && error_file) {
            namelist_free(&test_list);
            namelist_add_from_error_file(&test_list, error_file);
        }
        if (update_errors) {
            free(error_file);
            error_file = NULL;
        }
    }

    update_exclude_dirs();

    clocks = clock();

    if (count_skipped_features) {
        /* not storage efficient but it is simple */
        size_t size;
        size = sizeof(harness_skip_features_count[0]) * strlen(harness_skip_features);
        harness_skip_features_count = malloc(size);
        memset(harness_skip_features_count, 0, size);
    }
    
    if (is_dir_list) {
        RunTestDirThread *threads;
        int i;
        
        if (optind < argc && !isdigit((unsigned char)argv[optind][0])) {
            filename = argv[optind++];
            namelist_load(&test_list, filename);
        }

        start_index = 0;
        stop_index = -1;
        if (optind < argc) {
            start_index = atoi(argv[optind++]);
            if (optind < argc) {
                stop_index = atoi(argv[optind++]);
            }
        }
        /* XXX: could reorder the report and the errors when nthreads > 1 */
        if (!report_filename || str_equal(report_filename, "none") || nthreads > 1) {
            outfile = NULL;
        } else if (str_equal(report_filename, "-")) {
            outfile = stdout;
        } else {
            outfile = fopen(report_filename, "wb");
            if (!outfile) {
                perror_exit(1, report_filename);
            }
        }

        // exclude_dir_list has already been sorted by update_exclude_dirs()
        namelist_sort(&test_list, TRUE);
        namelist_sort(&exclude_list, TRUE);
        
        for (i = 0; i < test_list.count; i++) {
            switch (include_exclude_or_skip(i)) {
            case EXCLUDE:
                test_excluded++;
                break;
            case SKIP:
                test_skipped++;
                break;
            }
        }

        pthread_cond_init(&progress_cond, NULL);
        pthread_mutex_init(&progress_mutex, NULL);
        pthread_create(&progress_thread, NULL, show_progress, NULL);

        threads = malloc(sizeof(threads[0]) * nthreads);
        for (i = 0; i < nthreads; i++) {
            RunTestDirThread *th = &threads[i];
            pthread_attr_t attr;

            th->thread_index = i;
            
            pthread_attr_init(&attr);
            pthread_attr_setstacksize(&attr, 2 << 20); // 2 MB, glibc default
            pthread_create(&th->tid, &attr, run_test_dir_list, th);
            pthread_attr_destroy(&attr);
        }
        for (i = 0; i < nthreads; i++)
            pthread_join(threads[i].tid, NULL);
        free(threads);

        pthread_mutex_lock(&progress_mutex);
        progress_exit_request = TRUE;
        pthread_cond_signal(&progress_cond);
        pthread_mutex_unlock(&progress_mutex);
        pthread_join(progress_thread, NULL);

        pthread_mutex_destroy(&progress_mutex);
        pthread_cond_destroy(&progress_cond);

        if (outfile && outfile != stdout) {
            fclose(outfile);
            outfile = NULL;
        }
    } else {
        outfile = stdout;
        while (optind < argc) {
            run_test(tls, argv[optind++], -1);
        }
    }

    clocks = clock() - clocks;

    if (dump_memory) {
        if (dump_memory > 1 && stats_count > 1) {
            printf("\nMininum memory statistics for %s:\n\n", stats_min_filename);
            JS_DumpMemoryUsage(stdout, &stats_min, NULL);
            printf("\nMaximum memory statistics for %s:\n\n", stats_max_filename);
            JS_DumpMemoryUsage(stdout, &stats_max, NULL);
        }
        printf("\nAverage memory statistics for %d tests:\n\n", stats_count);
        JS_DumpMemoryUsage(stdout, &stats_avg, NULL);
        printf("\n");
    }

    if (count_skipped_features) {
        size_t i, n, len = strlen(harness_skip_features);
        BOOL disp = FALSE;
        int c;
        for(i = 0; i < len; i++) {
            if (harness_skip_features_count[i] != 0) {
                if (!disp) {
                    disp = TRUE;
                    printf("%-30s %7s\n", "SKIPPED FEATURE", "COUNT");
                }
                for(n = 0; n < 30; n++) {
                    c = harness_skip_features[i + n];
                    if (is_word_sep(c))
                        break;
                    putchar(c);
                }
                for(; n < 30; n++)
                    putchar(' ');
                printf(" %7d\n", harness_skip_features_count[i]);
            }
        }
        printf("\n");
    }
    
    if (is_dir_list) {
        fprintf(stderr, "Result: %d/%d error%s",
                test_failed, test_count, test_count != 1 ? "s" : "");
        if (test_excluded)
            fprintf(stderr, ", %d excluded", test_excluded);
        if (test_skipped)
            fprintf(stderr, ", %d skipped", test_skipped);
        if (error_file) {
            if (new_errors)
                fprintf(stderr, ", %d new", new_errors);
            if (changed_errors)
                fprintf(stderr, ", %d changed", changed_errors);
            if (fixed_errors)
                fprintf(stderr, ", %d fixed", fixed_errors);
        }
        fprintf(stderr, "\n");
        if (show_timings)
            fprintf(stderr, "Total user time: %.3fs (nthreads=%d)\n", (double)clocks / CLOCKS_PER_SEC, nthreads);
    }

    if (update_errors) {
        FILE *error_out = fopen(error_filename, "w");
        int i;
        if (!error_out) {
            perror_exit(1, error_filename);
        }
        /* sort the error list so that its order does not depend on
           the thread scheduling */
        namelist_sort(&error_list, FALSE);
        for (i = 0; i < error_list.count; i++) {
            fputs(error_list.array[i], error_out);
        }
        fclose(error_out);
    }

    namelist_free(&test_list);
    namelist_free(&exclude_list);
    namelist_free(&exclude_dir_list);
    namelist_free(&error_list);
    free(harness_dir);
    free(harness_skip_features);
    free(harness_skip_features_count);
    free(harness_features);
    free(harness_exclude);
    free(error_file);

    /* Signal that the error file is out of date. */
    return new_errors || changed_errors || fixed_errors;
}
