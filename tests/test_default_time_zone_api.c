/* Shared runtime default policy, including Intl without Temporal bindings.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../include/quickjs.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void assert_embedding_error(JSContext *ctx, const char *expected)
{
    JSValue error = JS_GetException(ctx), name = JS_GetPropertyStr(ctx, error, "name");
    const char *text = JS_ToCString(ctx, name);
    assert(text && !strcmp(text, expected));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, name);
    JS_FreeValue(ctx, error);
}

#ifdef CONFIG_TEMPORAL
static JSValue embedding_eval(JSContext *ctx, const char *source)
{
    JSValue result = JS_Eval(ctx, source, strlen(source), "default-zone-test", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue error = JS_GetException(ctx);
        const char *text = JS_ToCString(ctx, error);
        fprintf(stderr, "%s\n", text ? text : "exception");
        JS_FreeCString(ctx, text);
        JS_FreeValue(ctx, error);
        assert(0);
    }
    return result;
}

/* A zero host-memory limit still permits spare arena blocks to be reused.
   Retain them so the operation must fail if it requests any allocation. */
static void exhaust_spare_blocks(JSRuntime *rt, void **padding)
{
    size_t size;
    void *ptr;

    JS_SetMemoryLimit(rt, 0);
    for (size = sizeof(void *); size <= 512; size += sizeof(void *)) {
        while ((ptr = js_malloc_rt(rt, size)) != NULL) {
            memcpy(ptr, padding, sizeof(*padding));
            *padding = ptr;
        }
    }
}

static void assert_default_zone(JSContext *ctx, const char *expected)
{
    JSValue value = JS_GetDefaultTimeZone(ctx);
    const char *text;

    assert(!JS_IsException(value));
    text = JS_ToCString(ctx, value);
    assert(text && !strcmp(text, expected));
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, value);
}

#if defined(CONFIG_TIMEZONE_NATIVE) && !defined(_WIN32)
static void test_default_zone_snapshot(void)
{
    const char *original = getenv("TZ");
    char *saved = original ? strdup(original) : NULL;
    JSRuntime *rt, *other_rt;
    JSContext *first, *second, *other;

    assert(!original || saved);
    assert(!setenv("TZ", ":US/Eastern", 1));
    rt = JS_NewRuntime();
    assert(rt);
    first = JS_NewContext(rt);
    second = JS_NewContext(rt);
    assert(first && second);
    assert_default_zone(first, "America/New_York");
    assert(!setenv("TZ", "UTC-5:30", 1));
    assert_default_zone(first, "America/New_York");
    assert_default_zone(second, "America/New_York");
    other_rt = JS_NewRuntime();
    assert(other_rt);
    other = JS_NewContext(other_rt);
    assert(other);
    assert_default_zone(other, "+05:30");
    JS_FreeContext(other);
    JS_FreeRuntime(other_rt);
    JS_FreeContext(first);
    JS_FreeContext(second);
    JS_FreeRuntime(rt);
    if (saved) {
        assert(!setenv("TZ", saved, 1));
        free(saved);
    } else {
        assert(!unsetenv("TZ"));
    }
}
#endif

static void test_default_zone(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *first, *second;
    JSValue value;
    void *padding = NULL, *next;

    assert(rt);
#if defined(CONFIG_TIMEZONE_NATIVE) && !defined(_WIN32)
    test_default_zone_snapshot();
#endif
    first = JS_NewContext(rt);
    second = JS_NewContext(rt);
    assert(first && second);
    value = JS_GetDefaultTimeZone(first);
    assert(!JS_IsException(value) && JS_IsString(value));
    {
        const char *identifier = JS_ToCString(first, value);
        assert(identifier);
        assert_default_zone(second, identifier);
        JS_FreeCString(first, identifier);
    }
    JS_FreeValue(first, value);
    assert(!JS_SetDefaultTimeZone(first, "UTC"));
    assert_default_zone(first, "UTC");
#ifdef CONFIG_TIMEZONE_NATIVE
    assert(!JS_SetDefaultTimeZone(first, "us/eastern"));
    assert_default_zone(second, "America/New_York");
    value = embedding_eval(second,
        "if (new Date('2024-01-01T00:00Z').getTimezoneOffset() !== 300 ||"
        " new Date('2024-07-01T00:00Z').getTimezoneOffset() !== 240 ||"
        " (typeof Temporal !== 'undefined' && Temporal.Now.timeZoneId() !== 'America/New_York'))"
        " throw Error('named Date/Temporal default mismatch');");
    assert(!JS_IsException(value));
    JS_FreeValue(second, value);
    assert(!JS_SetDefaultTimeZone(first, "UTC"));
#endif
    assert(!JS_SetDefaultTimeZone(first, "+0530"));
    assert_default_zone(second, "+05:30");
    value = embedding_eval(second,
        "if (new Date(0).getHours() !== 5 || new Date(0).getMinutes() !== 30 ||"
        " new Date(1970,0,1).getTime() !== -19800000 ||"
        " Date.parse('1970-01-01T00:00') !== -19800000 ||"
        " (typeof Temporal !== 'undefined' && Temporal.Now.timeZoneId() !== '+05:30') ||"
        " (typeof Temporal !== 'undefined' && Temporal.Now.zonedDateTimeISO().timeZoneId !== '+05:30'))"
        " throw Error('Date/Temporal default mismatch');");
    assert(!JS_IsException(value));
    JS_FreeValue(second, value);
    assert(JS_SetDefaultTimeZone(first, NULL) == -1);
    assert_embedding_error(first, "RangeError");
    assert_default_zone(second, "+05:30");
    assert(JS_SetDefaultTimeZone(first, "not/a-zone") == -1);
    assert_embedding_error(first, "RangeError");
    assert_default_zone(second, "+05:30");
    JS_RunGC(rt);
    JS_SetGCThreshold(rt, (size_t)-1);
    exhaust_spare_blocks(rt, &padding);
    assert(JS_SetDefaultTimeZone(first, "UTC") == -1);
    assert(JS_HasException(first));
    JS_SetMemoryLimit(rt, (size_t)-1);
    value = JS_GetException(first);
    JS_FreeValue(first, value);
    assert_default_zone(second, "+05:30");
    while (padding) {
        memcpy(&next, padding, sizeof(next));
        js_free_rt(rt, padding);
        padding = next;
    }
    assert(!JS_SetDefaultTimeZone(first, "-00:00"));
    assert_default_zone(second, "+00:00");
    assert(!JS_SetDefaultTimeZone(second, "uTc"));
    assert_default_zone(first, "UTC");
    JS_FreeContext(first);
    JS_FreeContext(second);
    JS_FreeRuntime(rt);
}

static void test_date_boundaries(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue result;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    assert(!JS_SetDefaultTimeZone(ctx, "+01:00"));
    result = embedding_eval(ctx,
        "if (new Date(275760,8,13,1).getTime() !== 8640000000000000 ||"
        " !Number.isNaN(new Date(275760,8,13,1,0,0,1).getTime()))"
        " throw Error('upper wall boundary recovery');");
    JS_FreeValue(ctx, result);
    assert(!JS_SetDefaultTimeZone(ctx, "-01:00"));
    result = embedding_eval(ctx,
        "if (new Date(-271821,3,19,23).getTime() !== -8640000000000000 ||"
        " !Number.isNaN(new Date(-271821,3,19,22,59,59,999).getTime()))"
        " throw Error('lower wall boundary recovery');");
    JS_FreeValue(ctx, result);
#ifdef CONFIG_TIMEZONE_NATIVE
    assert(!JS_SetDefaultTimeZone(ctx, "America/New_York"));
    result = embedding_eval(ctx,
        "if (new Date(2024,2,10,2,30).toISOString() !== '2024-03-10T07:30:00.000Z' ||"
        " new Date(2024,10,3,1,30).toISOString() !== '2024-11-03T05:30:00.000Z')"
        " throw Error('compatible disambiguation');");
    JS_FreeValue(ctx, result);
    assert(!JS_SetDefaultTimeZone(ctx, "Europe/Paris"));
    result = embedding_eval(ctx,
        "let t = Date.parse('1911-03-10T23:50:39Z');"
        "if (new Date(t-1).getTimezoneOffset() !== -561/60 ||"
        " new Date(t).getTimezoneOffset() !== 0 ||"
        " new Date(t-1).getMilliseconds() !== 999)"
        " throw Error('negative-epoch transition flooring');");
    JS_FreeValue(ctx, result);
    assert(!JS_SetDefaultTimeZone(ctx, "Africa/Monrovia"));
    result = embedding_eval(ctx,
        "let d = new Date('1970-01-01T00:00:00.000Z');"
        "if (d.getTimezoneOffset() !== 44.5 || Date.parse(d.toString()) !== d.getTime())"
        " throw Error('historical seconds');");
    JS_FreeValue(ctx, result);
#endif
    assert(!JS_SetDefaultTimeZone(ctx, "UTC"));
    result = embedding_eval(ctx,
        "if (new Date(-1).getSeconds() !== 59 ||"
        " new Date(-271821,3,20).getTime() !== -8640000000000000 ||"
        " new Date(275760,8,13).getTime() !== 8640000000000000)"
        " throw Error('Date boundary');");
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}
#endif

int main(void)
{
#ifdef CONFIG_TEMPORAL
    test_default_zone();
    test_date_boundaries();
#else
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue value;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    assert(JS_SetDefaultTimeZone(ctx, "UTC") == -1 && JS_HasException(ctx));
    assert_embedding_error(ctx, "TypeError");
    value = JS_GetDefaultTimeZone(ctx);
    assert(JS_IsException(value) && JS_HasException(ctx));
    assert_embedding_error(ctx, "TypeError");
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
#endif
    return 0;
}
