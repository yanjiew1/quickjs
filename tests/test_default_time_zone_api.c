/* Shared runtime default time-zone embedding regressions.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#include "allocation-probe.h"
#ifdef CONFIG_TEMPORAL
#include "../src/quickjs/internal/runtime.h"
#include "../src/quickjs/internal/default-time-zone.h"
#include "../src/quickjs/internal/string.h"
#endif

static void eval_ok(JSContext *ctx, const char *source)
{
    JSValue result = JS_Eval(ctx, source, strlen(source), "default-time-zone-api",
                             JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue error = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, error);
        fprintf(stderr, "default time zone API: %s\n", message ? message : "exception");
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, error);
        abort();
    }
    JS_FreeValue(ctx, result);
    assert(!JS_HasException(ctx));
}
static void string_is(JSContext *ctx, JSValueConst value, const char *expected)
{
    const char *string;
    assert(JS_IsString(value));
    string = JS_ToCString(ctx, value);
    assert(string && !strcmp(string, expected));
    JS_FreeCString(ctx, string);
}
static void clear_error(JSContext *ctx, const char *name)
{
    JSValue error, value;
    assert(JS_HasException(ctx));
    error = JS_GetException(ctx);
    if (name) {
        value = JS_GetPropertyStr(ctx, error, "name");
        assert(!JS_IsException(value));
        string_is(ctx, value, name);
        JS_FreeValue(ctx, value);
    }
    JS_FreeValue(ctx, error);
    assert(!JS_HasException(ctx));
}
#ifdef CONFIG_TEMPORAL
/* Host allocation callbacks do not see allocations from existing arenas.
 * Retain this size class's free slots before arming the callback failure. */
typedef struct ReservedAllocation {
    struct ReservedAllocation *next;
    void *pointer;
} ReservedAllocation;

static ReservedAllocation *arm_allocation_failure(JSRuntime *rt,
                                                  AllocationProbe *probe,
                                                  size_t size)
{
    ReservedAllocation *reserved = NULL;
    assert(size && !probe->failure_at);
    for (;;) {
        ReservedAllocation *entry = malloc(sizeof(*entry));
        assert(entry);
        probe->attempts = 0;
        probe->failure_at = 1;
        probe->failed = 0;
        entry->pointer = js_malloc_rt(rt, size);
        probe->failure_at = 0;
        if (!entry->pointer) {
            assert(probe->failed);
            free(entry);
            break;
        }
        assert(!probe->failed);
        entry->next = reserved;
        reserved = entry;
    }
    probe->attempts = 0;
    probe->failure_at = 1;
    probe->failed = 0;
    return reserved;
}

static void release_reserved_allocations(JSRuntime *rt,
                                         ReservedAllocation *reserved)
{
    while (reserved) {
        ReservedAllocation *next = reserved->next;
        js_free_rt(rt, reserved->pointer);
        free(reserved);
        reserved = next;
    }
}

static void zone_is(JSContext *ctx, const char *expected)
{
    JSValue value = JS_GetDefaultTimeZone(ctx);
    assert(!JS_IsException(value));
    string_is(ctx, value, expected);
    JS_FreeValue(ctx, value);
}
static void check_policy(JSContext *ctx, const char *zone, int offset)
{
    char script[1024];
    snprintf(script, sizeof(script),
        "if (new Date('2024-01-01T00:00:00Z').getTimezoneOffset() !== %d)"
        " throw Error('Date default zone');"
        "if (typeof Temporal === 'object' && Temporal.Now.timeZoneId() !== '%s')"
        " throw Error('Temporal.Now default zone');", offset, zone);
    zone_is(ctx, zone);
    eval_ok(ctx, script);
}
static void failed_setting_preserves(JSContext *ctx, AllocationProbe *probe,
                                     const char *input, int oom)
{
    char *old_policy = ctx->rt->default_time_zone;
    ReservedAllocation *reserved = NULL;
    QJSTzProvider *old_provider = ctx->rt->temporal_tz_provider;
    if (oom)
        reserved = arm_allocation_failure(ctx->rt, probe, strlen(input) + 1);
    assert(JS_SetDefaultTimeZone(ctx, input) == -1);
    if (oom) {
        probe->failure_at = 0;
        assert(probe->failed);
        release_reserved_allocations(ctx->rt, reserved);
    }
    clear_error(ctx, oom ? NULL : "RangeError");
    assert(ctx->rt->default_time_zone == old_policy);
    assert(ctx->rt->temporal_tz_provider == old_provider);
    check_policy(ctx, "America/New_York", 300);
}
static void test_enabled(JSRuntime *rt, AllocationProbe *probe)
{
    JSContext *a, *b, *other, *raw = JS_NewContextRaw(rt);
    JSRuntime *rt2;
    JSValue retained, getter;
    ReservedAllocation *reserved;
    char mutable_input[] = "uS/eAsTeRn";
    assert(raw);
    assert(JS_AddIntrinsicBaseObjects(raw) == 0);
    {
        JSValue global = JS_GetGlobalObject(raw);
        JSValue intl = JS_GetPropertyStr(raw, global, "Intl");
        JSValue temporal = JS_GetPropertyStr(raw, global, "Temporal");
        assert(JS_IsUndefined(intl) && JS_IsUndefined(temporal));
        JS_FreeValue(raw, intl); JS_FreeValue(raw, temporal); JS_FreeValue(raw, global);
    }
    assert(JS_SetDefaultTimeZone(raw, "Mars/Phobos") == -1);
    clear_error(raw, "RangeError");
    assert(!raw->rt->default_time_zone);
    reserved = arm_allocation_failure(rt, probe, sizeof("UTC"));
    assert(JS_SetDefaultTimeZone(raw, "UTC") == -1);
    probe->failure_at = 0;
    assert(probe->failed && !raw->rt->default_time_zone);
    release_reserved_allocations(rt, reserved);
    clear_error(raw, NULL);
    assert(JS_SetDefaultTimeZone(raw, "+0545") == 0);
    zone_is(raw, "+05:45");
    /* Configuration before Date/Intl/Temporal initialization must be retained. */
    assert(JS_AddIntrinsicDate(raw) == 0 && JS_AddIntrinsicEval(raw) == 0);
#ifdef CONFIG_TEMPORAL
    assert(JS_AddIntrinsicTemporal(raw) == 0);
#endif
    check_policy(raw, "+05:45", -345);
    a = JS_NewContext(rt); b = JS_NewContext(rt);
    assert(a && b);
    check_policy(a, "+05:45", -345);
    check_policy(b, "+05:45", -345);
    rt2 = JS_NewRuntime2(&probe_functions, probe);
    assert(rt2);
    other = JS_NewContext(rt2);
    assert(other);
    assert(JS_SetDefaultTimeZone(other, "UTC") == 0);
    assert(JS_SetDefaultTimeZone(a, mutable_input) == 0);
    memset(mutable_input, 'x', sizeof(mutable_input) - 1);
    check_policy(a, "America/New_York", 300);
    check_policy(b, "America/New_York", 300);
    check_policy(other, "UTC", 0);
    /* Check Date construction/parsing and fields, beyond getTimezoneOffset. */
    eval_ok(a,
        "if (new Date(2024,0,1).toISOString() !== '2024-01-01T05:00:00.000Z' ||"
        " Date.parse('2024-01-01T00:00:00') !== Date.UTC(2024,0,1,5) ||"
        " new Date(0).getHours() !== 19) throw Error('Date local policy');"
        "if (typeof Temporal === 'object') {"
        " let now = Temporal.Now.zonedDateTimeISO();"
        " if (now.timeZoneId !== 'America/New_York') throw Error('Now zone slot');"
        " let plain = Temporal.Now.plainDateTimeISO();"
        " if (!(plain instanceof Temporal.PlainDateTime)) throw Error('Now plain');"
        "}");
    failed_setting_preserves(a, probe, NULL, 0);
    failed_setting_preserves(a, probe, "Mars/Phobos", 0);
    failed_setting_preserves(a, probe, "+05:45:01", 0);
    failed_setting_preserves(a, probe, "+24:00", 0);
    failed_setting_preserves(a, probe, "Europe/Paris", 1);
    /* Getter OOM leaves the committed policy alive and allows retry. */
    reserved = arm_allocation_failure(rt, probe,
        sizeof(JSString) + strlen(rt->default_time_zone) + 1);
    getter = JS_GetDefaultTimeZone(a);
    probe->failure_at = 0;
    assert(probe->failed && JS_IsException(getter));
    release_reserved_allocations(rt, reserved);
    clear_error(a, NULL);
    check_policy(a, "America/New_York", 300);
    assert(JS_SetDefaultTimeZone(a, "Asia/Kathmandu") == 0);
    check_policy(a, "Asia/Kathmandu", -345);
    check_policy(b, "Asia/Kathmandu", -345);
    check_policy(other, "UTC", 0);
    /* Preserve seconds from named-zone data; fixed-offset input stays minutes. */
    assert(JS_SetDefaultTimeZone(a, "Europe/Paris") == 0);
    eval_ok(a,
        "if (new Date('1900-01-01T00:00:00Z').getTimezoneOffset() !== -561 / 60)"
        " throw Error('historical second offset');"
        "if (typeof Temporal === 'object' &&"
        " new Temporal.ZonedDateTime(-2208988800000000000n,"
        " Temporal.Now.timeZoneId()).offsetNanoseconds !== 561000000000)"
        " throw Error('Temporal historical second offset');");
    assert(JS_SetDefaultTimeZone(b, "-00:00") == 0);
    check_policy(b, "+00:00", 0);
    check_policy(raw, "+00:00", 0);
    check_policy(other, "UTC", 0);
    check_policy(a, "+00:00", 0);
    retained = JS_GetDefaultTimeZone(a);
    assert(!JS_IsException(retained));
    JS_FreeContext(a);
    string_is(b, retained, "+00:00");
    JS_FreeValueRT(rt, retained);
    JS_FreeContext(raw);
    JS_FreeContext(b);
    JS_FreeContext(other);
    JS_FreeRuntime(rt2);
}
#else
static void test_disabled(JSRuntime *rt)
{
    JSContext *ctx = JS_NewContext(rt);
    assert(ctx);
    assert(JS_SetDefaultTimeZone(ctx, "UTC") == -1);
    clear_error(ctx, "TypeError");
    assert(JS_IsException(JS_GetDefaultTimeZone(ctx)));
    clear_error(ctx, "TypeError");
    eval_ok(ctx, "if (typeof Date !== 'function') throw Error('Date disabled');");
    JS_FreeContext(ctx);
}
#endif
int main(void)
{
    AllocationProbe probe = {0};
    JSRuntime *rt = JS_NewRuntime2(&probe_functions, &probe);
    assert(rt);
#ifdef CONFIG_TEMPORAL
    test_enabled(rt, &probe);
#else
    test_disabled(rt);
#endif
    JS_FreeRuntime(rt);
    assert(!probe.live);
    return 0;
}
