/* Runtime provider snapshots, allocation retries, realm and Date witnesses.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE. */
#include "../src/quickjs/internal/base.h"
#include "../src/quickjs/builtins/temporal/temporal-internal.h"
#include "../src/timezone/timezone.h"

/* Link the native owner once with only its system-zone symbol renamed. */
int qjs_temporal_system_zone(QJSTemporalZone *result)
{
    return qjs_temporal_zone_parse(result, "America/New_York", 16);
}

#if defined(CONFIG_TEMPORAL) && !defined(CONFIG_ICU)
#include "timezone_alignment_allocator.h"
typedef struct Fixture {
    unsigned char bytes[110];
    int status, reads, releases;
} Fixture;
typedef struct AllocatorWitness {
    JSRuntime *rt;
    int live, allocations, frees, fail_next;
} AllocatorWitness;

static void put32(unsigned char *p, int32_t value)
{
    uint32_t bits = (uint32_t)value;
    p[0] = bits >> 24; p[1] = bits >> 16;
    p[2] = bits >> 8; p[3] = bits;
}
static void fixture_data(Fixture *f, int32_t offset)
{
    int block;
    memset(f->bytes, 0, sizeof(f->bytes));
    for (block = 0; block < 2; block++) {
        unsigned char *p = f->bytes + block * 54;
        memcpy(p, "TZif2", 5);
        put32(p + 36, 1);
        put32(p + 40, 4);
        put32(p + 44, offset);
        memcpy(p + 50, "FIX", 4);
    }
    memcpy(f->bytes + 108, "\n\n", 2);
}
static int fixture_read(void *opaque, const char *id,
                         const unsigned char **bytes, size_t *size)
{
    Fixture *f = opaque;
    assert(!strcmp(id, "America/New_York"));
    f->reads++;
    if (f->status) return f->status;
    *bytes = f->bytes; *size = sizeof(f->bytes);
    return QJS_TZ_OK;
}
static void fixture_release(void *opaque, const unsigned char *bytes, size_t size)
{
    Fixture *f = opaque;
    assert(bytes == f->bytes && size == sizeof(f->bytes));
    f->releases++;
}
static void *witness_allocate(void *opaque, size_t size)
{
    AllocatorWitness *w = opaque;
    void *p;
    if (w->fail_next) { w->fail_next--; return NULL; }
    p = js_malloc_rt(w->rt, size);
    if (p) { w->live++; w->allocations++; }
    return p;
}
static void witness_deallocate(void *opaque, void *p)
{
    AllocatorWitness *w = opaque;
    if (p) { assert(w->live > 0); w->live--; w->frees++; }
    js_free_rt(w->rt, p);
}
static void install_fixture(JSRuntime *rt, Fixture *f, AllocatorWitness *w)
{
    QJSTzSource source = { f, fixture_read, fixture_release };
    QJSTzAllocator allocator = { w, witness_allocate, witness_deallocate };
    assert(!rt->temporal_tz_provider);
    w->rt = rt;
    assert(!qjs_tz_provider_create(&rt->temporal_tz_provider, &source, &allocator));
}
static void eval_ok(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source),
                            "runtime-zone-provider.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(value)) {
        JSValue error = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, error);
        fprintf(stderr, "runtime zone provider: %s\n", message ? message : "exception");
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, error);
        abort();
    }
    JS_FreeValue(ctx, value);
}
static void test_eight_byte_aligned_provider(void)
{
    Fixture f = { {0}, 0, 0, 0 };
    TzAlignmentAllocator w = {0};
    QJSTzSource source = {&f, fixture_read, fixture_release};
    QJSTzAllocator allocator = {
        &w, tz_alignment_allocate, tz_alignment_deallocate
    };
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx && !rt->temporal_tz_provider);
    fixture_data(&f, 1234);
    assert(!qjs_tz_provider_create(&rt->temporal_tz_provider,
                                   &source, &allocator));
    assert(w.live == 1 && !f.reads);
    /* timeZoneId binds the host snapshot before Date and ZonedDateTime
       use it. Every provider allocation is 8 aligned and not 16 aligned. */
    eval_ok(ctx,
        "if (Temporal.Now.timeZoneId() !== 'America/New_York')"
        "  throw Error('host zone identifier was lost');"
        "const z = new Temporal.ZonedDateTime(0n, 'US/Eastern');"
        "const d = new Date(0);"
        "if (z.offsetNanoseconds !== 1234000000000 ||"
        "    d.getMinutes() !== 20 || d.getSeconds() !== 34 ||"
        "    d.getTimezoneOffset() !== -1234 / 60 ||"
        "    new Date(1970, 0, 1, 0, 20, 34).getTime() !== 0)"
        "  throw Error('8-byte provider lost Date/Temporal selection');");
    assert(w.live == 3 && f.reads == 1 && f.releases == 1);
    JS_FreeContext(ctx);
    JS_RunGC(rt);
    assert(w.live == 3);
    JS_FreeRuntime(rt);
    assert(!w.live && w.allocations == w.frees);
}

static void test_lazy_creation_retry(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    QJSTemporalZone zone, unchanged;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx && !rt->temporal_tz_provider);
    assert(!js_temporal_parse_time_zone(ctx, &zone, "UTC", 3));
    assert(!js_temporal_parse_time_zone(ctx, &zone, "+05:30", 6));
    assert(!rt->temporal_tz_provider && !zone.provider);
    unchanged = zone;
    JS_SetMemoryLimit(rt, 0);
    assert(js_temporal_parse_time_zone(ctx, &zone, "America/New_York", 16) ==
           QJS_TEMPORAL_ERROR_MEMORY);
    assert(!rt->temporal_tz_provider);
    assert(!strcmp(zone.identifier, unchanged.identifier));
    assert(zone.is_offset == unchanged.is_offset &&
           zone.offset_nanoseconds == unchanged.offset_nanoseconds &&
           zone.provider == unchanged.provider);
    assert(!JS_HasException(ctx)); /* Native error code; callers throw once. */
    JS_SetMemoryLimit(rt, SIZE_MAX);
    assert(!js_temporal_parse_time_zone(ctx, &zone, "America/New_York", 16));
    assert(zone.provider == rt->temporal_tz_provider && zone.provider);
    JS_FreeContext(ctx);
    JS_RunGC(rt);
    {
        int64_t offset;
        assert(!qjs_temporal_zone_offset(&zone, qjs_temporal_epoch_ns_from_int64(0), &offset));
        assert(zone.provider == rt->temporal_tz_provider);
    }
    JS_FreeRuntime(rt);
}
static void test_runtime_snapshot_and_realm(void)
{
    Fixture f = { {0}, 0, 0, 0 };
    AllocatorWitness one_w = {0}, two_w = {0};
    JSRuntime *one = JS_NewRuntime(), *two;
    JSContext *owner, *survivor, *fresh;
    QJSTemporalZone zone, unchanged;
    const QJSTimeZone *primary, *alias;
    JSValue global, held;
    assert(one);
    owner = JS_NewContext(one); survivor = JS_NewContext(one);
    assert(owner && survivor);
    fixture_data(&f, 1234);
    install_fixture(one, &f, &one_w);
    assert(!qjs_temporal_zone_parse(&zone, "America/New_York", 16));
    unchanged = zone;
    one_w.fail_next = 1;
    assert(js_temporal_bind_time_zone(owner, &zone) == QJS_TEMPORAL_ERROR_MEMORY);
    assert(zone.provider == unchanged.provider && !zone.provider);
    f.status = QJS_TZ_MEMORY;
    assert(js_temporal_bind_time_zone(owner, &zone) == QJS_TEMPORAL_ERROR_MEMORY);
    assert(!zone.provider && f.releases == 0);
    f.status = QJS_TZ_OK;
    assert(!js_temporal_bind_time_zone(owner, &zone));
    assert(zone.provider == one->temporal_tz_provider && f.releases == 1);
    eval_ok(owner,
        "globalThis.held = new Temporal.ZonedDateTime(0n, 'US/Eastern');"
        "if (held.offsetNanoseconds !== 1234000000000 ||"
        "    new Date(0).getMinutes() !== 20 || new Date(0).getSeconds() !== 34 ||"
        "    new Date(0).getTimezoneOffset() !== -1234 / 60)"
        "  throw Error('Date and Temporal must select fixture system data');"
        "if (new Date(1970, 0, 1, 0, 20, 34).getTime() !== 0 ||"
        "    Date.parse('1970-01-01T00:20:34') !== 0)"
        "  throw Error('Date UTC conversion must use the same fixture');");
    global = JS_GetGlobalObject(owner);
    held = JS_GetPropertyStr(owner, global, "held");
    assert(!JS_IsException(held));
    assert(((JSTemporalZonedDateTimeData *)JS_GetOpaque(held,
             JS_CLASS_TEMPORAL_ZONED_DATE_TIME))->time_zone.provider == one->temporal_tz_provider);
    JS_FreeValue(owner, global);
    global = JS_GetGlobalObject(survivor);
    assert(JS_SetPropertyStr(survivor, global, "held", held) >= 0);
    JS_FreeValue(survivor, global);
    JS_FreeContext(owner); JS_RunGC(one);
    fixture_data(&f, 5678); /* Replacing source bytes cannot alter a retained snapshot. */
    eval_ok(survivor,
        "if (held.withTimeZone('America/New_York').offsetNanoseconds !== 1234000000000 ||"
        "    held.add({hours: 1}).offsetNanoseconds !== 1234000000000 ||"
        "    new Temporal.ZonedDateTime(0n, 'America/New_York').offsetNanoseconds !== 1234000000000 ||"
        "    new Date(0).getSeconds() !== 34)"
        "  throw Error('context cleanup or alias bypassed immutable snapshot');");
    assert(!qjs_tz_provider_open(one->temporal_tz_provider, "America/New_York", 16, &primary));
    assert(!qjs_tz_provider_open(one->temporal_tz_provider, "US/Eastern", 10, &alias));
    assert(primary == alias && qjs_tz_origin(alias) == QJS_TZ_SYSTEM);
    two = JS_NewRuntime(); assert(two);
    fresh = JS_NewContext(two); assert(fresh);
    install_fixture(two, &f, &two_w);
    eval_ok(fresh,
        "if (new Temporal.ZonedDateTime(0n, 'US/Eastern').offsetNanoseconds !== 5678000000000 ||"
        "    new Date(0).getHours() !== 1 || new Date(0).getMinutes() !== 34 ||"
        "    new Date(0).getSeconds() !== 38)"
        "  throw Error('fresh runtime must own an independent selection');");
    assert(one->temporal_tz_provider != two->temporal_tz_provider);
    JS_FreeContext(fresh); JS_FreeRuntime(two);
    assert(two_w.live == 0 && two_w.allocations == two_w.frees);
    JS_FreeContext(survivor); JS_RunGC(one); JS_FreeRuntime(one);
    assert(one_w.live == 0 && one_w.allocations == one_w.frees);
}
static void test_date_string_offsets(void)
{
    static const struct {
        int32_t offset;
        const char *string;
    } cases[] = {
        { 1234, "Thu Jan 01 1970 00:20:34 GMT+0020 (UTC+00:20:34)" },
        { -20, "Wed Dec 31 1969 23:59:40 GMT+0000 (UTC-00:00:20)" },
        { 17762, "Thu Jan 01 1970 04:56:02 GMT+0456 (UTC+04:56:02)" },
        { -17762, "Wed Dec 31 1969 19:03:58 GMT-0456 (UTC-04:56:02)" },
        { 19800, "Thu Jan 01 1970 05:30:00 GMT+0530" },
        { -18000, "Wed Dec 31 1969 19:00:00 GMT-0500" },
        { 0, "Thu Jan 01 1970 00:00:00 GMT+0000" },
    };
    size_t i;

    /* JavaScript cannot select a host provider with an arbitrary second offset.
       A fresh runtime per case also respects immutable provider snapshots. */
    for (i = 0; i < countof(cases); i++) {
        Fixture f = { {0}, 0, 0, 0 };
        AllocatorWitness w = {0};
        JSRuntime *rt = JS_NewRuntime();
        JSContext *ctx;
        JSValue global;
        assert(rt); ctx = JS_NewContext(rt); assert(ctx);
        fixture_data(&f, cases[i].offset);
        install_fixture(rt, &f, &w);
        global = JS_GetGlobalObject(ctx);
        assert(JS_SetPropertyStr(ctx, global, "expectedString",
                                JS_NewString(ctx, cases[i].string)) >= 0);
        assert(JS_SetPropertyStr(ctx, global, "expectedOffset",
                                JS_NewInt32(ctx, cases[i].offset)) >= 0);
        JS_FreeValue(ctx, global);
        eval_ok(ctx,
            "const date = new Date(0);"
            "if (date.toString() !== expectedString ||"
            "    date.toTimeString() !== expectedString.slice(16) ||"
            "    date.getTimezoneOffset() !== -expectedOffset / 60)"
            "  throw Error('exact offset string or sign was lost');"
            "for (const value of [0, -1000, 1000, Date.UTC(1880, 0, 1),"
            "                     -62167219200000,"
            "                     -62198755200000, 253402300800000,"
            "                     -8640000000000000, 8640000000000000]) {"
            "  const d = new Date(value);"
            "  if (Date.parse(d.toString()) !== value ||"
            "      Date.parse(d.toUTCString()) !== value ||"
            "      Date.parse(d.toISOString()) !== value)"
            "    throw Error('zero-millisecond Date string did not roundtrip');"
            "}"
            "if (date.toUTCString() !== 'Thu, 01 Jan 1970 00:00:00 GMT' ||"
            "    date.toISOString() !== '1970-01-01T00:00:00.000Z')"
            "  throw Error('UTC or ISO output changed');");
        assert(f.reads == 1 && f.releases == 1);
        JS_FreeContext(ctx); JS_FreeRuntime(rt);
        assert(w.live == 0 && w.allocations == w.frees);
    }
}
static void test_fallback_snapshot(void)
{
    Fixture f = { {0}, QJS_TZ_ABSENT, 0, 0 };
    AllocatorWitness w = {0};
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    const QJSTimeZone *snapshot;
    assert(rt); ctx = JS_NewContext(rt); assert(ctx);
    install_fixture(rt, &f, &w);
    eval_ok(ctx,
        "if (new Temporal.ZonedDateTime(0n, 'US/Eastern').offsetNanoseconds !== -18000000000000 ||"
        "    new Date(0).getHours() !== 19 || new Date(0).getTimezoneOffset() !== 300)"
        "  throw Error('Date and Temporal must share embedded fallback');");
    assert(f.reads == 1 && f.releases == 0);
    fixture_data(&f, 1234); f.status = QJS_TZ_OK;
    eval_ok(ctx,
        "if (new Temporal.ZonedDateTime(0n, 'America/New_York').offsetNanoseconds !== -18000000000000 ||"
        "    new Date(0).getTimezoneOffset() !== 300)"
        "  throw Error('later system availability must not replace fallback snapshot');");
    assert(f.reads == 1);
    assert(!qjs_tz_provider_open(rt->temporal_tz_provider, "America/New_York", 16, &snapshot));
    assert(qjs_tz_origin(snapshot) == QJS_TZ_EMBEDDED);
    JS_FreeContext(ctx); JS_FreeRuntime(rt);
    assert(w.live == 0 && w.allocations == w.frees);
}
#endif

int main(void)
{
#if defined(CONFIG_TEMPORAL) && !defined(CONFIG_ICU)
    test_eight_byte_aligned_provider();
    test_lazy_creation_retry();
    test_runtime_snapshot_and_realm();
    test_date_string_offsets();
    test_fallback_snapshot();
#endif
    return 0;
}
