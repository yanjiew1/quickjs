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
#include "test262_harness.h"
#include <unistd.h>
#include <ctype.h>

void init_thread_local_storage(ThreadLocalStorage *tls)
{
    memset(tls, 0, sizeof(*tls));
    pthread_mutex_init(&tls->agent_mutex, NULL);
    pthread_cond_init(&tls->agent_cond, NULL);
    init_list_head(&tls->agent_list);

    pthread_mutex_init(&tls->report_mutex, NULL);
    init_list_head(&tls->report_list);
}

static void js_print_value_write(void *opaque, const char *buf, size_t len)
{
    FILE *fo = opaque;
    fwrite(buf, 1, len, fo);
}

JSValue js_print(JSContext *ctx, JSValueConst this_val,
                int argc, JSValueConst *argv)
{
    ThreadLocalStorage *tls = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    int i;
    JSValueConst v;
    
    for (i = 0; i < argc; i++) {
        if (i != 0 && outfile)
            fputc(' ', outfile);
        v = argv[i];
        if (JS_IsString(v)) {
            const char *str;
            size_t len;
            str = JS_ToCStringLen(ctx, &len, v);
            if (!str)
                return JS_EXCEPTION;
            if (!strcmp(str, "Test262:AsyncTestComplete")) {
                tls->async_done++;
            } else if (strstart(str, "Test262:AsyncTestFailure", NULL)) {
                tls->async_done = 2; /* force an error */
            }
            if (outfile) {
                fwrite(str, 1, len, outfile);
            }
            JS_FreeCString(ctx, str);
        } else {
            if (outfile) {
                JS_PrintValue(ctx, js_print_value_write, outfile, v, NULL);
            }
        }
    }
    if (outfile)
        fputc('\n', outfile);
    return JS_UNDEFINED;
}

static JSValue js_detachArrayBuffer(JSContext *ctx, JSValue this_val,
                                    int argc, JSValue *argv)
{
    JS_DetachArrayBuffer(ctx, argv[0]);
    return JS_UNDEFINED;
}

static JSValue js_evalScript(JSContext *ctx, JSValue this_val,
                             int argc, JSValue *argv)
{
    const char *str;
    size_t len;
    JSValue ret;
    str = JS_ToCStringLen(ctx, &len, argv[0]);
    if (!str)
        return JS_EXCEPTION;
    ret = JS_Eval(ctx, str, len, "<evalScript>", JS_EVAL_TYPE_GLOBAL);
    JS_FreeCString(ctx, str);
    return ret;
}

/* forward decl */
/* forward decl */

static void *agent_start(void *arg)
{
    Test262Agent *agent = arg;
    ThreadLocalStorage *tls = agent->tls;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue ret_val;
    int ret;

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
    JS_SetContextOpaque(ctx, agent);
    JS_SetRuntimeInfo(rt, "agent");
    JS_SetCanBlock(rt, TRUE);

    add_helpers(ctx);
    ret_val = JS_Eval(ctx, agent->script, strlen(agent->script),
                      "<evalScript>", JS_EVAL_TYPE_GLOBAL);
    free(agent->script);
    agent->script = NULL;
    if (JS_IsException(ret_val))
        js_std_dump_error(ctx);
    JS_FreeValue(ctx, ret_val);

    for(;;) {
        ret = JS_ExecutePendingJob(JS_GetRuntime(ctx), NULL);
        if (ret < 0) {
            js_std_dump_error(ctx);
            break;
        } else if (ret == 0) {
            if (JS_IsUndefined(agent->broadcast_func)) {
                break;
            } else {
                JSValue args[2];

                pthread_mutex_lock(&tls->agent_mutex);
                while (!agent->broadcast_pending) {
                    pthread_cond_wait(&tls->agent_cond, &tls->agent_mutex);
                }

                agent->broadcast_pending = FALSE;
                pthread_cond_signal(&tls->agent_cond);

                pthread_mutex_unlock(&tls->agent_mutex);

                args[0] = JS_NewArrayBuffer(ctx, agent->broadcast_sab_buf,
                                            agent->broadcast_sab_size,
                                            NULL, NULL, TRUE);
                args[1] = JS_NewInt32(ctx, agent->broadcast_val);
                ret_val = JS_Call(ctx, agent->broadcast_func, JS_UNDEFINED,
                                  2, (JSValueConst *)args);
                JS_FreeValue(ctx, args[0]);
                JS_FreeValue(ctx, args[1]);
                if (JS_IsException(ret_val))
                    js_std_dump_error(ctx);
                JS_FreeValue(ctx, ret_val);
                JS_FreeValue(ctx, agent->broadcast_func);
                agent->broadcast_func = JS_UNDEFINED;
            }
        }
    }
    JS_FreeValue(ctx, agent->broadcast_func);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return NULL;
}

static JSValue js_agent_start(JSContext *ctx, JSValue this_val,
                              int argc, JSValue *argv)
{
    ThreadLocalStorage *tls = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    const char *script;
    Test262Agent *agent;
    pthread_attr_t attr;

    if (JS_GetContextOpaque(ctx) != NULL)
        return JS_ThrowTypeError(ctx, "cannot be called inside an agent");

    script = JS_ToCString(ctx, argv[0]);
    if (!script)
        return JS_EXCEPTION;
    agent = malloc(sizeof(*agent));
    memset(agent, 0, sizeof(*agent));
    agent->tls = tls;
    agent->broadcast_func = JS_UNDEFINED;
    agent->broadcast_sab = JS_UNDEFINED;
    agent->script = strdup(script);
    JS_FreeCString(ctx, script);
    list_add_tail(&agent->link, &tls->agent_list);
    pthread_attr_init(&attr);
    // musl libc gives threads 80 kb stacks, much smaller than
    // JS_DEFAULT_STACK_SIZE (256 kb)
    pthread_attr_setstacksize(&attr, 2 << 20); // 2 MB, glibc default
    pthread_create(&agent->tid, &attr, agent_start, agent);
    pthread_attr_destroy(&attr);
    return JS_UNDEFINED;
}

void js_agent_free(JSContext *ctx)
{
    ThreadLocalStorage *tls = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    struct list_head *el, *el1;
    Test262Agent *agent;

    list_for_each_safe(el, el1, &tls->agent_list) {
        agent = list_entry(el, Test262Agent, link);
        pthread_join(agent->tid, NULL);
        JS_FreeValue(ctx, agent->broadcast_sab);
        list_del(&agent->link);
        free(agent);
    }
}

static JSValue js_agent_leaving(JSContext *ctx, JSValue this_val,
                                int argc, JSValue *argv)
{
    Test262Agent *agent = JS_GetContextOpaque(ctx);
    if (!agent)
        return JS_ThrowTypeError(ctx, "must be called inside an agent");
    /* nothing to do */
    return JS_UNDEFINED;
}

BOOL is_broadcast_pending(ThreadLocalStorage *tls)
{
    struct list_head *el;
    Test262Agent *agent;
    list_for_each(el, &tls->agent_list) {
        agent = list_entry(el, Test262Agent, link);
        if (agent->broadcast_pending)
            return TRUE;
    }
    return FALSE;
}

static JSValue js_agent_broadcast(JSContext *ctx, JSValue this_val,
                                  int argc, JSValue *argv)
{
    ThreadLocalStorage *tls = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    JSValueConst sab = argv[0];
    struct list_head *el;
    Test262Agent *agent;
    uint8_t *buf;
    size_t buf_size;
    int32_t val;

    if (JS_GetContextOpaque(ctx) != NULL)
        return JS_ThrowTypeError(ctx, "cannot be called inside an agent");

    buf = JS_GetArrayBuffer(ctx, &buf_size, sab);
    if (!buf)
        return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &val, argv[1]))
        return JS_EXCEPTION;

    /* broadcast the values and wait until all agents have started
       calling their callbacks */
    pthread_mutex_lock(&tls->agent_mutex);
    list_for_each(el, &tls->agent_list) {
        agent = list_entry(el, Test262Agent, link);
        agent->broadcast_pending = TRUE;
        /* the shared array buffer is used by the thread, so increment
           its refcount */
        agent->broadcast_sab = JS_DupValue(ctx, sab);
        agent->broadcast_sab_buf = buf;
        agent->broadcast_sab_size = buf_size;
        agent->broadcast_val = val;
    }
    pthread_cond_broadcast(&tls->agent_cond);

    while (is_broadcast_pending(tls)) {
        pthread_cond_wait(&tls->agent_cond, &tls->agent_mutex);
    }
    pthread_mutex_unlock(&tls->agent_mutex);
    return JS_UNDEFINED;
}

static JSValue js_agent_receiveBroadcast(JSContext *ctx, JSValue this_val,
                                         int argc, JSValue *argv)
{
    Test262Agent *agent = JS_GetContextOpaque(ctx);
    if (!agent)
        return JS_ThrowTypeError(ctx, "must be called inside an agent");
    if (!JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "expecting function");
    JS_FreeValue(ctx, agent->broadcast_func);
    agent->broadcast_func = JS_DupValue(ctx, argv[0]);
    return JS_UNDEFINED;
}

static JSValue js_agent_sleep(JSContext *ctx, JSValue this_val,
                              int argc, JSValue *argv)
{
    uint32_t duration;
    if (JS_ToUint32(ctx, &duration, argv[0]))
        return JS_EXCEPTION;
    usleep(duration * 1000);
    return JS_UNDEFINED;
}

int64_t get_clock_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
}

static JSValue js_agent_monotonicNow(JSContext *ctx, JSValue this_val,
                                     int argc, JSValue *argv)
{
    return JS_NewInt64(ctx, get_clock_ms());
}

static JSValue js_agent_getReport(JSContext *ctx, JSValue this_val,
                                  int argc, JSValue *argv)
{
    ThreadLocalStorage *tls = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    AgentReport *rep;
    JSValue ret;

    pthread_mutex_lock(&tls->report_mutex);
    if (list_empty(&tls->report_list)) {
        rep = NULL;
    } else {
        rep = list_entry(tls->report_list.next, AgentReport, link);
        list_del(&rep->link);
    }
    pthread_mutex_unlock(&tls->report_mutex);
    if (rep) {
        ret = JS_NewString(ctx, rep->str);
        free(rep->str);
        free(rep);
    } else {
        ret = JS_NULL;
    }
    return ret;
}

static JSValue js_agent_report(JSContext *ctx, JSValue this_val,
                               int argc, JSValue *argv)
{
    ThreadLocalStorage *tls = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    const char *str;
    AgentReport *rep;

    str = JS_ToCString(ctx, argv[0]);
    if (!str)
        return JS_EXCEPTION;
    rep = malloc(sizeof(*rep));
    rep->str = strdup(str);
    JS_FreeCString(ctx, str);

    pthread_mutex_lock(&tls->report_mutex);
    list_add_tail(&rep->link, &tls->report_list);
    pthread_mutex_unlock(&tls->report_mutex);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_agent_funcs[] = {
    /* only in main */
    JS_CFUNC_DEF("start", 1, js_agent_start ),
    JS_CFUNC_DEF("getReport", 0, js_agent_getReport ),
    JS_CFUNC_DEF("broadcast", 2, js_agent_broadcast ),
    /* only in agent */
    JS_CFUNC_DEF("report", 1, js_agent_report ),
    JS_CFUNC_DEF("leaving", 0, js_agent_leaving ),
    JS_CFUNC_DEF("receiveBroadcast", 1, js_agent_receiveBroadcast ),
    /* in both */
    JS_CFUNC_DEF("sleep", 1, js_agent_sleep ),
    JS_CFUNC_DEF("monotonicNow", 0, js_agent_monotonicNow ),
};

JSValue js_new_agent(JSContext *ctx)
{
    JSValue agent;
    agent = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, agent, js_agent_funcs,
                               countof(js_agent_funcs));
    return agent;
}

static JSValue js_createRealm(JSContext *ctx, JSValue this_val,
                              int argc, JSValue *argv)
{
    JSContext *ctx1;
    JSValue ret;

    ctx1 = JS_NewContext(JS_GetRuntime(ctx));
    if (!ctx1)
        return JS_ThrowOutOfMemory(ctx);
    ret = add_helpers1(ctx1);
    /* ctx1 has a refcount so it stays alive */
    JS_FreeContext(ctx1);
    return ret;
}

static JSValue js_IsHTMLDDA(JSContext *ctx, JSValue this_val,
                            int argc, JSValue *argv)
{
    return JS_NULL;
}

static JSValue js_gc(JSContext *ctx, JSValueConst this_val,
                     int argc, JSValueConst *argv)
{
    JS_RunGC(JS_GetRuntime(ctx));
    return JS_UNDEFINED;
}

JSValue add_helpers1(JSContext *ctx)
{
    JSValue global_obj;
    JSValue obj262, obj;

    global_obj = JS_GetGlobalObject(ctx);

    JS_SetPropertyStr(ctx, global_obj, "print",
                      JS_NewCFunction(ctx, js_print, "print", 1));

    /* $262 special object used by the tests */
    obj262 = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj262, "detachArrayBuffer",
                      JS_NewCFunction(ctx, js_detachArrayBuffer,
                                      "detachArrayBuffer", 1));
    JS_SetPropertyStr(ctx, obj262, "evalScript",
                      JS_NewCFunction(ctx, js_evalScript,
                                      "evalScript", 1));
    JS_SetPropertyStr(ctx, obj262, "codePointRange",
                      JS_NewCFunction(ctx, js_string_codePointRange,
                                      "codePointRange", 2));
    JS_SetPropertyStr(ctx, obj262, "agent", js_new_agent(ctx));

    JS_SetPropertyStr(ctx, obj262, "global",
                      JS_DupValue(ctx, global_obj));
    JS_SetPropertyStr(ctx, obj262, "createRealm",
                      JS_NewCFunction(ctx, js_createRealm,
                                      "createRealm", 0));
    obj = JS_NewCFunction(ctx, js_IsHTMLDDA, "IsHTMLDDA", 0);
    JS_SetIsHTMLDDA(ctx, obj);
    JS_SetPropertyStr(ctx, obj262, "IsHTMLDDA", obj);
    JS_SetPropertyStr(ctx, obj262, "gc",
                      JS_NewCFunction(ctx, js_gc, "gc", 0));

    JS_SetPropertyStr(ctx, global_obj, "$262", JS_DupValue(ctx, obj262));

    JS_FreeValue(ctx, global_obj);
    return obj262;
}

void add_helpers(JSContext *ctx)
{
    JS_FreeValue(ctx, add_helpers1(ctx));
}

char *load_file(const char *filename, size_t *lenp)
{
    char *buf;
    size_t buf_len;
    buf = (char *)js_load_file(NULL, &buf_len, filename);
    if (!buf)
        perror_exit(1, filename);
    if (lenp)
        *lenp = buf_len;
    return buf;
}

static int json_module_init_test(JSContext *ctx, JSModuleDef *m)
{
    JSValue val;
    val = JS_GetModulePrivateValue(ctx, m);
    JS_SetModuleExport(ctx, m, "default", val);
    return 0;
}

JSModuleDef *js_module_loader_test(JSContext *ctx,
                                          const char *module_name, void *opaque,
                                          JSValueConst attributes)
{
    size_t buf_len;
    uint8_t *buf;
    JSModuleDef *m;
    char *filename, *slash, path[1024];

    // interpret import("bar.js") from path/to/foo.js as
    // import("path/to/bar.js") but leave import("./bar.js") untouched
    filename = opaque;
    if (!strchr(module_name, '/')) {
        slash = strrchr(filename, '/');
        if (slash) {
            snprintf(path, sizeof(path), "%.*s/%s",
                     (int)(slash - filename), filename, module_name);
            module_name = path;
        }
    }

    buf = js_load_file(ctx, &buf_len, module_name);
    if (!buf) {
        JS_ThrowReferenceError(ctx, "could not load module filename '%s'",
                               module_name);
        return NULL;
    }

    if (js_module_test_json(ctx, attributes) == 1) {
        /* compile as JSON */
        JSValue val;
        val = JS_ParseJSON(ctx, (char *)buf, buf_len, module_name);
        js_free(ctx, buf);
        if (JS_IsException(val))
            return NULL;
        m = JS_NewCModule(ctx, module_name, json_module_init_test);
        if (!m) {
            JS_FreeValue(ctx, val);
            return NULL;
        }
        /* only export the "default" symbol which will contain the JSON object */
        JS_AddModuleExport(ctx, m, "default");
        JS_SetModulePrivateValue(ctx, m, val);
    } else {
        JSValue func_val;
        /* compile the module */
        func_val = JS_Eval(ctx, (char *)buf, buf_len, module_name,
                           JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        js_free(ctx, buf);
        if (JS_IsException(func_val))
            return NULL;
        /* the module is already referenced, so we must free it */
        m = JS_VALUE_GET_PTR(func_val);
        JS_FreeValue(ctx, func_val);
    }
    return m;
}

int is_line_sep(char c)
{
    return (c == '\0' || c == '\n' || c == '\r');
}

char *find_line(const char *str, const char *line)
{
    if (str) {
        const char *p;
        int len = strlen(line);
        for (p = str; (p = strstr(p, line)) != NULL; p += len + 1) {
            if ((p == str || is_line_sep(p[-1])) && is_line_sep(p[len]))
                return (char *)p;
        }
    }
    return NULL;
}

int is_word_sep(char c)
{
    return (c == '\0' || isspace((unsigned char)c) || c == ',');
}

char *find_word(const char *str, const char *word)
{
    const char *p;
    int len = strlen(word);
    if (str && len) {
        for (p = str; (p = strstr(p, word)) != NULL; p += len) {
            if ((p == str || is_word_sep(p[-1])) && is_word_sep(p[len]))
                return (char *)p;
        }
    }
    return NULL;
}

char *extract_desc(const char *buf, char style)
{
    const char *p, *desc_start;
    char *desc;
    int len;

    p = buf;
    while (*p != '\0') {
        if (p[0] == '/' && p[1] == '*' && p[2] == style && p[3] != '/') {
            p += 3;
            desc_start = p;
            while (*p != '\0' && (p[0] != '*' || p[1] != '/'))
                p++;
            if (*p == '\0') {
                warning("Expecting end of desc comment");
                return NULL;
            }
            len = p - desc_start;
            desc = malloc(len + 1);
            memcpy(desc, desc_start, len);
            desc[len] = '\0';
            return desc;
        } else {
            p++;
        }
    }
    return NULL;
}

char *find_tag(char *desc, const char *tag, int *state)
{
    char *p;
    p = strstr(desc, tag);
    if (p) {
        p += strlen(tag);
        *state = 0;
    }
    return p;
}

char *get_option(char **pp, int *state)
{
    char *p, *p0, *option = NULL;
    if (*pp) {
        for (p = *pp;; p++) {
            switch (*p) {
            case '[':
                *state += 1;
                continue;
            case ']':
                *state -= 1;
                if (*state > 0)
                    continue;
                p = NULL;
                break;
            case ' ':
            case '\t':
            case '\r':
            case ',':
            case '-':
                continue;
            case '\n':
                if (*state > 0 || p[1] == ' ')
                    continue;
                p = NULL;
                break;
            case '\0':
                p = NULL;
                break;
            default:
                p0 = p;
                p += strcspn(p0, " \t\r\n,]");
                option = strdup_len(p0, p - p0);
                break;
            }
            break;
        }
        *pp = p;
    }
    return option;
}
