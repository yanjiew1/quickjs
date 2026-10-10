/* Native Intl timezone owner independent of Temporal/ICU zone selection. */
#include "quickjs.h"
#include "../src/quickjs/internal/base.h"
#include "../src/quickjs/internal/runtime.h"
#include "../src/quickjs/builtins/intl/intl-internal.h"
#include "timezone_alignment_allocator.h"
#include <assert.h>
#include <stdint.h>
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
int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *one, *two;
    QJSTzProvider *provider;
    const QJSTimeZone *primary, *alias;
    int32_t offset;
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
    assert(!qjs_tz_provider_open(rt->temporal_tz_provider, "America/New_York", 16, &primary));
    memset(source.bytes, 0xff, sizeof source.bytes);
    assert(!qjs_tz_provider_open(js_intl_native_time_zone_provider(two), "US/Eastern", 10, &alias));
    assert(primary == alias && source.reads == 1 && source.releases == 1);
    assert(qjs_tz_origin(alias) == QJS_TZ_SYSTEM);
    assert(!qjs_tz_offset(alias, 0, &offset) && offset == 1234);
    JS_FreeContext(two); JS_FreeContext(one);
    assert(witness.live == 3);
    JS_FreeRuntime(rt);
    assert(!witness.live && witness.allocations == witness.frees);
    return 0;
}
