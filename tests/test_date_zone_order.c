/* Native provider faults exercise Date's observable argument order.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#include "../src/temporal/time-zone.h"

/* The owner TU is linked once with just its provider renamed for this unit. */
int qjs_temporal_system_zone_test_backend(QJSTemporalZone *result);
static int provider_mode;
static int provider_calls;
int qjs_temporal_system_zone(QJSTemporalZone *result)
{
    provider_calls++;
    if (provider_mode == 1)
        return QJS_TEMPORAL_ERROR_MEMORY;
    if (provider_mode == 2)
        return qjs_temporal_zone_parse(result, "+05:30", 6);
    return qjs_temporal_system_zone_test_backend(result);
}

static void report_exception(JSContext *ctx, const char *operation)
{
    JSValue error;
    const char *message;
    if (!JS_HasException(ctx)) {
        fprintf(stderr, "Date zone unit %s: no retained exception\n", operation);
        return;
    }
    error = JS_GetException(ctx);
    message = JS_ToCString(ctx, error);
    fprintf(stderr, "Date zone unit %s: %s\n", operation,
            message ? message : "exception unavailable");
    JS_FreeCString(ctx, message);
    JS_FreeValue(ctx, error);
}

static JSContext *new_context_or_abort(JSRuntime *rt)
{
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        /* The failed normal initializer frees its context, but the runtime
           retains its exception. A raw context can report it without trying
           the failing Intl initialization again. */
        JSContext *diagnostic = JS_NewContextRaw(rt);
        if (diagnostic) {
            report_exception(diagnostic, "context initialization");
            JS_FreeContext(diagnostic);
        } else {
            fprintf(stderr, "Date zone unit: no diagnostic context\n");
        }
        abort();
    }
    return ctx;
}

static void eval_ok(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "date-zone-order.js",
                            JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(value)) {
        report_exception(ctx, "evaluation");
        abort();
    }
    JS_FreeValue(ctx, value);
}

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    assert(rt);
    ctx = new_context_or_abort(rt);
    provider_mode = 1;
    provider_calls = 0;
    eval_ok(ctx,
        "for (const method of ['setDate', 'setFullYear', 'setHours', "
        "'setMilliseconds', 'setMinutes', 'setMonth', 'setSeconds']) {"
        " const marker = {}; const date = new Date(0); let caught;"
        " try { date[method]({ valueOf() { throw marker; } }); }"
        " catch (error) { caught = error; }"
        " if (caught !== marker) throw Error('first argument order: ' + method);"
        "}");
    assert(provider_calls == 0);
    eval_ok(ctx,
        "{ const date = new Date(0); let called = 0, caught;"
        " try { date.setFullYear(1, { valueOf() { called++; return 1; } }); }"
        " catch (error) { caught = error; }"
        " if (called || !(caught instanceof InternalError))"
        "   throw Error('setFullYear optional argument order'); }");
    assert(provider_calls == 1);
    provider_calls = 0;
    eval_ok(ctx,
        "for (const method of ['setHours', 'setMinutes', 'setMonth', 'setSeconds']) {"
        " const marker = {}; const date = new Date(0); let caught;"
        " try { date[method](1, { valueOf() { throw marker; } }); }"
        " catch (error) { caught = error; }"
        " if (caught !== marker) throw Error('optional argument order: ' + method);"
        "}");
    assert(provider_calls == 0);
    eval_ok(ctx,
        "{ const date = new Date(0); let called = 0, caught;"
        " try { date.setYear({ valueOf() { called++; return 1; } }); }"
        " catch (error) { caught = error; }"
        " if (called || !(caught instanceof InternalError))"
        "   throw Error('Annex B LocalTime precedes year coercion'); }");
    assert(provider_calls == 1);
    JS_FreeContext(ctx);
    provider_mode = 2;
    /* Intl captures its embedding default during this fresh realm creation. */
    ctx = new_context_or_abort(rt);
    eval_ok(ctx,
        "if (new Date(0).getHours() !== 5 ||"
        "    new Date(0).getMinutes() !== 30 ||"
        "    new Date(0).getTimezoneOffset() !== -330)"
        "  throw Error('Date must consume the configured native provider');"
        "if (typeof Temporal !== 'undefined' &&"
        "    Temporal.Now.timeZoneId() !== '+05:30')"
        "  throw Error('Temporal must consume the same provider');"
        "if (typeof Intl !== 'undefined' &&"
        "    new Intl.DateTimeFormat('en').resolvedOptions().timeZone !== '+05:30')"
        "  throw Error('Intl must consume the same provider');");
    provider_mode = 0;
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
