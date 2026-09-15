/*
 * ECMA Test 262 Runner agent and $262 harness
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
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../../cutils.h"
#include "../../list.h"
#include "../../quickjs-libc.h"
#include "harness.h"

#define CMD_NAME "run-test262"

struct Test262Harness {
    pthread_mutex_t agent_mutex;
    pthread_cond_t agent_cond;
    /* list of Test262Agent.link */
    struct list_head agent_list;

    pthread_mutex_t report_mutex;
    /* list of AgentReport.link */
    struct list_head report_list;

    FILE *output;
    int async_done;
};

typedef struct {
    struct list_head link;
    Test262Harness *harness;
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

static void harness_fatal(const char *message)
{
    fflush(stdout);
    fprintf(stderr, "%s: %s\n", CMD_NAME, message);
    exit(1);
}

Test262Harness *test262_harness_new(void)
{
    Test262Harness *harness = calloc(1, sizeof(*harness));

    if (!harness)
        harness_fatal("allocation failure");
    pthread_mutex_init(&harness->agent_mutex, NULL);
    pthread_cond_init(&harness->agent_cond, NULL);
    init_list_head(&harness->agent_list);
    pthread_mutex_init(&harness->report_mutex, NULL);
    init_list_head(&harness->report_list);
    return harness;
}

void test262_harness_free(Test262Harness *harness)
{
    struct list_head *el, *el1;
    AgentReport *rep;

    if (!harness)
        return;
    assert(list_empty(&harness->agent_list));
    list_for_each_safe(el, el1, &harness->report_list) {
        rep = list_entry(el, AgentReport, link);
        list_del(&rep->link);
        free(rep->str);
        free(rep);
    }
    pthread_mutex_destroy(&harness->report_mutex);
    pthread_cond_destroy(&harness->agent_cond);
    pthread_mutex_destroy(&harness->agent_mutex);
    free(harness);
}

void test262_harness_set_output(Test262Harness *harness, FILE *output)
{
    harness->output = output;
}

void test262_harness_reset_async(Test262Harness *harness)
{
    harness->async_done = 0;
}

int test262_harness_async_state(const Test262Harness *harness)
{
    return harness->async_done;
}

static void js_print_value_write(void *opaque, const char *buf, size_t len)
{
    FILE *fo = opaque;
    fwrite(buf, 1, len, fo);
}

static JSValue js_print(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv)
{
    Test262Harness *harness = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    FILE *output = harness->output;
    int i;
    JSValueConst v;

    for (i = 0; i < argc; i++) {
        if (i != 0 && output)
            fputc(' ', output);
        v = argv[i];
        if (JS_IsString(v)) {
            const char *str;
            size_t len;
            str = JS_ToCStringLen(ctx, &len, v);
            if (!str)
                return JS_EXCEPTION;
            if (!strcmp(str, "Test262:AsyncTestComplete")) {
                harness->async_done++;
            } else if (strstart(str, "Test262:AsyncTestFailure", NULL)) {
                harness->async_done = 2; /* force an error */
            }
            if (output)
                fwrite(str, 1, len, output);
            JS_FreeCString(ctx, str);
        } else {
            if (output)
                JS_PrintValue(ctx, js_print_value_write, output, v, NULL);
        }
    }
    if (output)
        fputc('\n', output);
    return JS_UNDEFINED;
}

void test262_harness_print_value(JSContext *ctx, JSValueConst value)
{
    js_print(ctx, JS_NULL, 1, &value);
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

static JSValue add_helpers1(JSContext *ctx);

static void *agent_start(void *arg)
{
    Test262Agent *agent = arg;
    Test262Harness *harness = agent->harness;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue ret_val;
    int ret;

    rt = JS_NewRuntime();
    if (rt == NULL)
        harness_fatal("JS_NewRuntime failure");
    JS_SetRuntimeOpaque(rt, harness);
    ctx = JS_NewContext(rt);
    if (ctx == NULL) {
        JS_FreeRuntime(rt);
        harness_fatal("JS_NewContext failure");
    }
    JS_SetContextOpaque(ctx, agent);
    JS_SetRuntimeInfo(rt, "agent");
    JS_SetCanBlock(rt, TRUE);

    test262_harness_add_helpers(ctx);
    ret_val = JS_Eval(ctx, agent->script, strlen(agent->script),
                      "<evalScript>", JS_EVAL_TYPE_GLOBAL);
    free(agent->script);
    agent->script = NULL;
    if (JS_IsException(ret_val))
        js_std_dump_error(ctx);
    JS_FreeValue(ctx, ret_val);

    for (;;) {
        ret = JS_ExecutePendingJob(JS_GetRuntime(ctx), NULL);
        if (ret < 0) {
            js_std_dump_error(ctx);
            break;
        } else if (ret == 0) {
            if (JS_IsUndefined(agent->broadcast_func)) {
                break;
            } else {
                JSValue args[2];

                pthread_mutex_lock(&harness->agent_mutex);
                while (!agent->broadcast_pending) {
                    pthread_cond_wait(&harness->agent_cond,
                                      &harness->agent_mutex);
                }

                agent->broadcast_pending = FALSE;
                pthread_cond_signal(&harness->agent_cond);

                pthread_mutex_unlock(&harness->agent_mutex);

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
    Test262Harness *harness = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
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
    agent->harness = harness;
    agent->broadcast_func = JS_UNDEFINED;
    agent->broadcast_sab = JS_UNDEFINED;
    agent->script = strdup(script);
    JS_FreeCString(ctx, script);
    list_add_tail(&agent->link, &harness->agent_list);
    pthread_attr_init(&attr);
    // musl libc gives threads 80 kb stacks, much smaller than
    // JS_DEFAULT_STACK_SIZE (256 kb)
    pthread_attr_setstacksize(&attr, 2 << 20); // 2 MB, glibc default
    pthread_create(&agent->tid, &attr, agent_start, agent);
    pthread_attr_destroy(&attr);
    return JS_UNDEFINED;
}

void test262_harness_free_agents(JSContext *ctx)
{
    Test262Harness *harness = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    struct list_head *el, *el1;
    Test262Agent *agent;

    list_for_each_safe(el, el1, &harness->agent_list) {
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

static BOOL is_broadcast_pending(Test262Harness *harness)
{
    struct list_head *el;
    Test262Agent *agent;
    list_for_each(el, &harness->agent_list) {
        agent = list_entry(el, Test262Agent, link);
        if (agent->broadcast_pending)
            return TRUE;
    }
    return FALSE;
}

static JSValue js_agent_broadcast(JSContext *ctx, JSValue this_val,
                                  int argc, JSValue *argv)
{
    Test262Harness *harness = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
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
    pthread_mutex_lock(&harness->agent_mutex);
    list_for_each(el, &harness->agent_list) {
        agent = list_entry(el, Test262Agent, link);
        agent->broadcast_pending = TRUE;
        /* the shared array buffer is used by the thread, so increment
           its refcount */
        agent->broadcast_sab = JS_DupValue(ctx, sab);
        agent->broadcast_sab_buf = buf;
        agent->broadcast_sab_size = buf_size;
        agent->broadcast_val = val;
    }
    pthread_cond_broadcast(&harness->agent_cond);

    while (is_broadcast_pending(harness)) {
        pthread_cond_wait(&harness->agent_cond, &harness->agent_mutex);
    }
    pthread_mutex_unlock(&harness->agent_mutex);
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

static int64_t get_clock_ms(void)
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
    Test262Harness *harness = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    AgentReport *rep;
    JSValue ret;

    pthread_mutex_lock(&harness->report_mutex);
    if (list_empty(&harness->report_list)) {
        rep = NULL;
    } else {
        rep = list_entry(harness->report_list.next, AgentReport, link);
        list_del(&rep->link);
    }
    pthread_mutex_unlock(&harness->report_mutex);
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
    Test262Harness *harness = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    const char *str;
    AgentReport *rep;

    str = JS_ToCString(ctx, argv[0]);
    if (!str)
        return JS_EXCEPTION;
    rep = malloc(sizeof(*rep));
    rep->str = strdup(str);
    JS_FreeCString(ctx, str);

    pthread_mutex_lock(&harness->report_mutex);
    list_add_tail(&rep->link, &harness->report_list);
    pthread_mutex_unlock(&harness->report_mutex);
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

static JSValue js_new_agent(JSContext *ctx)
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

static JSValue add_helpers1(JSContext *ctx)
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

void test262_harness_add_helpers(JSContext *ctx)
{
    JS_FreeValue(ctx, add_helpers1(ctx));
}
