/* Native Intl timezone owner independent of Temporal/ICU zone selection. */
#include "quickjs.h"
#include "../src/quickjs/internal/base.h"
#include "../src/quickjs/internal/runtime.h"
#include "../src/quickjs/builtins/intl/intl-internal.h"
#include "timezone_alignment_allocator.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Source {
    unsigned char bytes[110];
    int reads, releases;
} Source;
static void put32(unsigned char *p, int32_t value)
{
    uint32_t bits = (uint32_t)value;
    p[0] = bits >> 24; p[1] = bits >> 16; p[2] = bits >> 8; p[3] = bits;
}
static int read_source(void *opaque, const char *name, const unsigned char **bytes, size_t *length)
{
    Source *source = opaque;
    assert(!strcmp(name, "America/New_York")); source->reads++;
    *bytes = source->bytes; *length = sizeof source->bytes; return QJS_TZ_OK;
}
static void release_source(void *opaque, const unsigned char *bytes, size_t length)
{
    Source *source = opaque;
    assert(bytes == source->bytes && length == sizeof source->bytes); source->releases++;
}
static void initialize(Source *source)
{
    int block;
    memset(source, 0, sizeof *source);
    for (block = 0; block < 2; block++) {
        unsigned char *p = source->bytes + block * 54;
        memcpy(p, "TZif2", 5); put32(p + 36, 1); put32(p + 40, 4);
        put32(p + 44, 1234); memcpy(p + 50, "FIX", 4);
    }
    memcpy(source->bytes + 108, "\n\n", 2);
}
/* A global-only marker proves the creating realm is collected while its
 * formatter remains rooted in the other realm. */
static JSClassID realm_marker_class_id;
static void realm_marker_finalizer(JSRuntime *rt, JSValue value)
{
    int *finalized = JS_GetOpaque(value, realm_marker_class_id);
    (void)rt;
    assert(finalized);
    (*finalized)++;
}
static void set_global(JSContext *ctx, const char *name, JSValue value)
{
    JSValue global = JS_GetGlobalObject(ctx);
    assert(!JS_IsException(global));
    assert(JS_SetPropertyStr(ctx, global, name, value) == 1);
    JS_FreeValue(ctx, global);
}
static void install_realm_marker(JSContext *ctx, int *finalized)
{
    static const JSClassDef class_def = {
        .class_name = "TimezoneSnapshotRealmMarker",
        .finalizer = realm_marker_finalizer,
    };
    JSValue marker;
    JS_NewClassID(&realm_marker_class_id);
    assert(!JS_NewClass(JS_GetRuntime(ctx), realm_marker_class_id, &class_def));
    marker = JS_NewObjectProtoClass(ctx, JS_NULL, realm_marker_class_id);
    assert(!JS_IsException(marker));
    JS_SetOpaque(marker, finalized);
    set_global(ctx, "timezoneSnapshotRealmMarker", marker);
}
static JSValue evaluate(JSContext *ctx, const char *script)
{
    JSValue value = JS_Eval(ctx, script, strlen(script),
                            "intl-timezone-snapshot.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(value)) {
        JSValue exception = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, exception);
        fprintf(stderr, "Intl timezone snapshot: %s\n",
                message ? message : "exception");
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, exception);
        abort();
    }
    return value;
}
static void evaluate_ok(JSContext *ctx, const char *script)
{
    JS_FreeValue(ctx, evaluate(ctx, script));
}
static JSValue new_formatter(JSContext *ctx, const char *zone)
{
    char script[512];
    int length = snprintf(script, sizeof script,
        "new Intl.DateTimeFormat('en-US', {timeZone:'%s',"
        "calendar:'gregory', numberingSystem:'latn', hourCycle:'h23',"
        "year:'numeric', month:'numeric', day:'numeric',"
        "hour:'numeric', minute:'numeric', second:'numeric'})", zone);
    assert(length > 0 && (size_t)length < sizeof script);
    return evaluate(ctx, script);
}
int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *one, *two;
    QJSTzProvider *provider;
    const QJSTimeZone *primary, *alias;
    int32_t offset;
    int creating_realm_finalized = 0;
    JSValue formatter;
    char host[256];
    Source source;
    TzAlignmentAllocator witness = {0};
    QJSTzAllocator allocator = {&witness, tz_alignment_allocate, tz_alignment_deallocate};
    QJSTzSource input = {&source, read_source, release_source};
    assert(rt); one = JS_NewContext(rt); two = JS_NewContext(rt);
    assert(one && two && !rt->temporal_tz_provider);
    assert(!qjs_tz_system_identifier(host, sizeof host));
    assert(!js_intl_ensure_context(one) && !js_intl_ensure_context(two));
    assert(!strcmp(js_intl_default_time_zone(one), host));
    assert(!strcmp(js_intl_default_time_zone(two), host));
    JS_SetMemoryLimit(rt, 0);
    assert(!js_intl_native_time_zone_provider(one) && !rt->temporal_tz_provider);
    { JSValue exception = JS_GetException(one); JS_FreeValue(one, exception); }
    JS_SetMemoryLimit(rt, (size_t)-1);
    provider = js_intl_native_time_zone_provider(one);
    assert(provider && provider == js_intl_native_time_zone_provider(two));
    /* Swap only the test-owned provider before opening any zone. */
    qjs_tz_provider_free(provider); rt->temporal_tz_provider = NULL;
    initialize(&source);
    assert(!qjs_tz_provider_create(&rt->temporal_tz_provider, &input, &allocator));
    assert(js_intl_native_time_zone_provider(one) == rt->temporal_tz_provider);
    provider = rt->temporal_tz_provider;
    /* Let the frontend perform the first open of the controlled snapshot. */
    formatter = new_formatter(one, "America/New_York");
    assert(JS_IsObject(formatter));
    assert(source.reads == 1 && source.releases == 1);
    /* Calling the other realm's method preserves the formatter's internal
     * slots without keeping this realm through its prototype or bound format. */
    assert(JS_SetPrototype(one, formatter, JS_NULL) == 1);
    set_global(two, "retainedFormatter", formatter);
    install_realm_marker(one, &creating_realm_finalized);
    evaluate_ok(two,
        "function checkSnapshot(formatter, value = 0) {"
        "  const expected = {year:1970, month:1, day:1,"
        "                    hour:0, minute:20, second:34};"
        "  const actual = Object.create(null);"
        "  const parts = Intl.DateTimeFormat.prototype.formatToParts.call(formatter, value);"
        "  for (const part of parts) {"
        "    if (Object.prototype.hasOwnProperty.call(expected, part.type)) {"
        "      if (Object.prototype.hasOwnProperty.call(actual, part.type))"
        "        throw Error('duplicate numeric part: ' + part.type);"
        "      actual[part.type] = Number(part.value);"
        "    }"
        "  }"
        "  for (const key of Object.keys(expected))"
        "    if (actual[key] !== expected[key])"
        "      throw Error('snapshot numeric part: ' + key + ' = ' + actual[key]);"
        "}"
        "checkSnapshot(retainedFormatter);");
    assert(!qjs_tz_provider_open(rt->temporal_tz_provider, "America/New_York", 16, &primary));
    assert(source.reads == 1 && source.releases == 1);
    assert(!creating_realm_finalized);
    JS_FreeContext(one);
    JS_RunGC(rt);
    assert(creating_realm_finalized == 1);
    /* The consumed bytes may change; cached TZif and formatter data persist. */
    memset(source.bytes, 0xff, sizeof source.bytes);
    assert(!qjs_tz_provider_open(js_intl_native_time_zone_provider(two), "US/Eastern", 10, &alias));
    assert(primary == alias && source.reads == 1 && source.releases == 1);
    assert(qjs_tz_origin(alias) == QJS_TZ_SYSTEM);
    assert(!qjs_tz_offset(alias, 0, &offset) && offset == 1234);
    set_global(two, "aliasFormatter", new_formatter(two, "US/Eastern"));
    evaluate_ok(two,
        "checkSnapshot(retainedFormatter);"
        "checkSnapshot(aliasFormatter);");
#ifdef CONFIG_TEMPORAL
    evaluate_ok(two,
        "(() => {"
        "  const instant = Temporal.Instant.fromEpochNanoseconds(0n);"
        "  checkSnapshot(retainedFormatter, instant);"
        "  checkSnapshot(aliasFormatter, instant);"
        "  for (const zone of ['America/New_York', 'US/Eastern']) {"
        "    const z = instant.toZonedDateTimeISO(zone);"
        "    if (z.epochNanoseconds !== 0n || z.offsetNanoseconds !== 1234000000000 ||"
        "        z.year !== 1970 || z.month !== 1 || z.day !== 1 ||"
        "        z.hour !== 0 || z.minute !== 20 || z.second !== 34 ||"
        "        z.millisecond !== 0 || z.microsecond !== 0 || z.nanosecond !== 0)"
        "      throw Error('Temporal shared snapshot: ' + zone);"
        "  }"
        "})();");
#endif
    assert(js_intl_native_time_zone_provider(two) == provider);
    assert(primary == alias && source.reads == 1 && source.releases == 1);
    assert(!qjs_tz_offset(alias, 0, &offset) && offset == 1234);
    JS_FreeContext(two);
    assert(witness.live == 3);
    JS_FreeRuntime(rt);
    assert(!witness.live && witness.allocations == witness.frees);
    assert(source.reads == 1 && source.releases == 1);
    return 0;
}
