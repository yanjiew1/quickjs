/* qjsc-generated context/main native allocation-failure regression.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
#include <assert.h>
#include <string.h>
#include "quickjs-libc.h"
#include "qjsc-allocation-probe.h"

enum FailureSite {
    FAIL_NONE, FAIL_RUNTIME, FAIL_RAW_CONTEXT, FAIL_BASE_OBJECTS,
#ifdef CONFIG_TEMPORAL
    FAIL_TEMPORAL,
#endif
#ifdef CONFIG_INTL
    FAIL_INTL,
#endif
    FAIL_SITE_COUNT
};

static AllocationProbe allocation;
static enum FailureSite selected_failure;
static int handlers_created, handlers_freed, contexts_created, contexts_freed;
static int runtimes_freed, helpers_called, scripts_called, loops_called;

static void arm_failure(enum FailureSite site)
{
    if (selected_failure == site) {
        allocation.attempts = 0;
        allocation.failure_at = 1;
    }
}

static JSRuntime *fault_new_runtime(void)
{
    JSRuntime *rt;
    arm_failure(FAIL_RUNTIME);
    rt = JS_NewRuntime2(&probe_functions, &allocation);
    allocation.failure_at = 0;
    return rt;
}

static JSContext *fault_new_raw_context(JSRuntime *rt)
{
    JSContext *ctx;
    arm_failure(FAIL_RAW_CONTEXT);
    ctx = JS_NewContextRaw(rt);
    allocation.failure_at = 0;
    if (ctx)
        contexts_created++;
    return ctx;
}

static int fault_add_base_objects(JSContext *ctx)
{
    int result;
    arm_failure(FAIL_BASE_OBJECTS);
    result = JS_AddIntrinsicBaseObjects(ctx);
    allocation.failure_at = 0;
    return result;
}

#ifdef CONFIG_TEMPORAL
static int fault_add_temporal(JSContext *ctx)
{
    int result;
    arm_failure(FAIL_TEMPORAL);
    result = JS_AddIntrinsicTemporal(ctx);
    allocation.failure_at = 0;
    return result;
}
#endif
#ifdef CONFIG_INTL
static int fault_add_intl(JSContext *ctx)
{
    int result;
    arm_failure(FAIL_INTL);
    result = JS_AddIntrinsicIntl(ctx);
    allocation.failure_at = 0;
    return result;
}
#endif

static void fault_init_handlers(JSRuntime *rt)
{
    assert(rt);
    handlers_created++;
    js_std_init_handlers(rt);
}

static void fault_free_handlers(JSRuntime *rt)
{
    handlers_freed++;
    assert(handlers_freed == handlers_created);
    js_std_free_handlers(rt);
}

static void fault_free_context(JSContext *ctx)
{
    assert(ctx);
    contexts_freed++;
    JS_FreeContext(ctx);
}

static void fault_free_runtime(JSRuntime *rt)
{
    assert(rt);
    runtimes_freed++;
    JS_FreeRuntime(rt);
}

static void fault_add_helpers(JSContext *ctx, int argc, char **argv)
{
    JSValue global;
    assert(ctx && !JS_HasException(ctx));
    helpers_called++;
    global = JS_GetGlobalObject(ctx);
#ifdef CONFIG_TEMPORAL
    {
        JSValue feature = JS_GetPropertyStr(ctx, global, "Temporal");
        assert(JS_IsObject(feature));
        JS_FreeValue(ctx, feature);
    }
#endif
#ifdef CONFIG_INTL
    {
        JSValue feature = JS_GetPropertyStr(ctx, global, "Intl");
        assert(JS_IsObject(feature));
        JS_FreeValue(ctx, feature);
    }
#endif
    JS_FreeValue(ctx, global);
    js_std_add_helpers(ctx, argc, argv);
}

static void fault_eval_binary(JSContext *ctx, const uint8_t *buf,
                              size_t length, int flags)
{
    assert(ctx && !JS_HasException(ctx));
    scripts_called++;
    js_std_eval_binary(ctx, buf, length, flags);
}

static void fault_loop(JSContext *ctx)
{
    assert(ctx && !JS_HasException(ctx));
    loops_called++;
    js_std_loop(ctx);
}

/* Headers were included before these macros. Only the generated call sites
   are redirected; each wrapper calls the real engine/libc implementation. */
#define main qjsc_generated_main
#define JS_NewRuntime fault_new_runtime
#define JS_NewContextRaw fault_new_raw_context
#define JS_AddIntrinsicBaseObjects fault_add_base_objects
#ifdef CONFIG_TEMPORAL
#define JS_AddIntrinsicTemporal fault_add_temporal
#endif
#ifdef CONFIG_INTL
#define JS_AddIntrinsicIntl fault_add_intl
#endif
#define JS_FreeContext fault_free_context
#define JS_FreeRuntime fault_free_runtime
#define js_std_init_handlers fault_init_handlers
#define js_std_free_handlers fault_free_handlers
#define js_std_add_helpers fault_add_helpers
#define js_std_eval_binary fault_eval_binary
#define js_std_loop fault_loop
#include "qjsc-context-generated.c"
#undef main
#undef JS_NewRuntime
#undef JS_NewContextRaw
#undef JS_AddIntrinsicBaseObjects
#ifdef CONFIG_TEMPORAL
#undef JS_AddIntrinsicTemporal
#endif
#ifdef CONFIG_INTL
#undef JS_AddIntrinsicIntl
#endif
#undef JS_FreeContext
#undef JS_FreeRuntime
#undef js_std_init_handlers
#undef js_std_free_handlers
#undef js_std_add_helpers
#undef js_std_eval_binary
#undef js_std_loop

int main(void)
{
    enum FailureSite site;
    char program_name[] = "qjsc-context-fault";
    char *argv[] = { program_name, NULL };

    for (site = FAIL_NONE; site < FAIL_SITE_COUNT; site++) {
        int result;
        memset(&allocation, 0, sizeof(allocation));
        selected_failure = site;
        handlers_created = handlers_freed = 0;
        contexts_created = contexts_freed = runtimes_freed = 0;
        helpers_called = scripts_called = loops_called = 0;
        result = qjsc_generated_main(1, argv);
        assert(!allocation.live);
        assert(handlers_created == handlers_freed);
        assert(contexts_created == contexts_freed);
        if (site == FAIL_NONE) {
            assert(result == 0 && !allocation.failed);
            assert(handlers_created == 1 && runtimes_freed == 1);
            assert(contexts_created == 1);
            assert(helpers_called == 1 && scripts_called == 1);
            assert(loops_called == 1);
        } else {
            assert(result == 1 && allocation.failed);
            assert(helpers_called == 0 && scripts_called == 0);
            assert(loops_called == 0);
            assert(runtimes_freed == (site != FAIL_RUNTIME));
            assert(handlers_created == (site != FAIL_RUNTIME));
            assert(contexts_created == (site != FAIL_RUNTIME &&
                                        site != FAIL_RAW_CONTEXT));
        }
    }
    return 0;
}
