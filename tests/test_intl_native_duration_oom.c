/*
 * QuickJS native Intl embedding and allocator ownership tests
 *
 * Copyright (c) 2026 Yan-Jie Wang
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "intl-allocation-probe.h"
#include "../src/quickjs/builtins/intl/intl-internal.h"
/* Cold constructors sample first/middle/last/sentinel allocation boundaries.
 * Warm closures retain DurationFormat so exhaustive sweeps measure formatting,
 * JS result adaptation and record parsing without rebuilding20 Number owners. */
static const char *const cold_operations[] = {
    "() => Intl.DurationFormat.supportedLocalesOf(['en','en-US'])",
    "() => new Intl.DurationFormat('en', {style:'long'})",
    "() => new Intl.DurationFormat('en', {style:'short'})",
    "() => new Intl.DurationFormat('en', {style:'narrow'})",
    "() => new Intl.DurationFormat('en', {style:'digital',fractionalDigits:9})",
#ifdef CONFIG_TEMPORAL
    "() => Temporal.Duration.from('PT1H2M3.004005006S').toLocaleString('en',{style:'digital',fractionalDigits:9})",
#endif
};
static const char *const warm_operations[] = {
    "(()=>{const f=new Intl.DurationFormat('en',{style:'digital',fractionalDigits:9});return()=>f.format({hours:1,minutes:2,seconds:3,nanoseconds:1});})()",
    "(()=>{const f=new Intl.DurationFormat('en',{style:'digital',fractionalDigits:9});return()=>f.formatToParts({hours:0,minutes:-2,seconds:-3,nanoseconds:-1});})()",
    "(()=>{const f=new Intl.DurationFormat('en',{style:'long'});return()=>f.formatToParts({years:1,months:2,weeks:3,days:4,hours:5,minutes:6,seconds:7,milliseconds:8,microseconds:9,nanoseconds:10});})()",
    "(()=>{const f=new Intl.DurationFormat('en',{style:'short'});return()=>f.formatToParts({years:1,days:2,seconds:3});})()",
    "(()=>{const f=new Intl.DurationFormat('en',{style:'narrow'});return()=>f.formatToParts({years:1,days:2,seconds:3});})()",
    "(()=>{const f=new Intl.DurationFormat('en',{seconds:'numeric',fractionalDigits:9});return()=>f.formatToParts({nanoseconds:1000000000000000128});})()",
    "(()=>{const f=new Intl.DurationFormat('en',{style:'digital',fractionalDigits:9});return()=>f.format('PT1H2M3.004005006S');})()",
#ifdef CONFIG_TEMPORAL
    "(()=>{const f=new Intl.DurationFormat('en',{style:'digital',fractionalDigits:9});const d=Temporal.Duration.from('PT1H2M3.004005006S');return()=>f.formatToParts(d);})()",
#endif
};

typedef struct OperationFixture {
    AllocationProbe probe;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue function;
} OperationFixture;

typedef struct CacheSnapshot {
    const JSIntlLocaleList *owner;
    JSIntlLocaleList list;
    uint64_t contents;
} CacheSnapshot;

static void open_operation(OperationFixture *fixture, const char *source)
{
    memset(fixture, 0, sizeof(*fixture));
    fixture->rt = JS_NewRuntime2(&probe_functions, &fixture->probe);
    assert(fixture->rt);
    fixture->ctx = JS_NewContext(fixture->rt);
    assert(fixture->ctx);
    fixture->function = JS_Eval(fixture->ctx, source, strlen(source),
                                "intl-oom-probe", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(fixture->function));
    assert(!JS_HasException(fixture->ctx));
}

static void close_operation(OperationFixture *fixture)
{
    assert(!JS_HasException(fixture->ctx));
    JS_FreeValue(fixture->ctx, fixture->function);
    JS_RunGC(fixture->rt);
    JS_FreeContext(fixture->ctx);
    JS_FreeRuntime(fixture->rt);
    assert(!fixture->probe.live);
}

/* Count only the call, including any error cleanup performed by the engine.
   Disable injection before releasing the result or consuming its exception. */
static size_t call_operation(OperationFixture *fixture, size_t failure)
{
    AllocationProbe *probe = &fixture->probe;
    JSContext *ctx = fixture->ctx;
    JSValue value;

    assert(!JS_HasException(ctx));
    assert(JS_GetRuntime(ctx) == fixture->rt);
    probe->attempts = 0;
    probe->failed = 0;
    probe->failure_at = failure;
    value = JS_Call(ctx, fixture->function, JS_UNDEFINED, 0, NULL);
    probe->failure_at = 0;
    if (JS_IsException(value)) {
        JSValue error;
        assert(probe->failed); /* Baseline failures must remain visible. */
        assert(JS_HasException(ctx));
        error = JS_GetException(ctx);
        JS_FreeValue(ctx, error);
    }
    assert(!JS_HasException(ctx));
    JS_FreeValue(ctx, value);
    JS_RunGC(fixture->rt);
    return probe->attempts;
}

static uint64_t cache_contents(const JSIntlLocaleList *list)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t i;
    assert(list->count <= list->capacity);
    for (i = 0; i < list->count; i++) {
        const unsigned char *p = (const unsigned char *)list->items[i];
        assert(p && *p);
        do {
            hash ^= *p;
            hash *= UINT64_C(1099511628211);
        } while (*p++);
    }
    return hash;
}

static void snapshot_caches(JSContext *ctx, CacheSnapshot *snapshots)
{
    int i;
    for (i = 0; i < JS_INTL_SERVICE_COUNT; i++) {
        const JSIntlLocaleList *list = js_intl_available_locale_cache(ctx, i);
        assert(list && !JS_HasException(ctx));
        snapshots[i].owner = list;
        snapshots[i].list = *list;
        snapshots[i].contents = cache_contents(list);
    }
}

static void check_caches(JSContext *ctx, const CacheSnapshot *snapshots)
{
    int i;
    for (i = 0; i < JS_INTL_SERVICE_COUNT; i++) {
        const JSIntlLocaleList *list = js_intl_available_locale_cache(ctx, i);
        assert(list == snapshots[i].owner && !JS_HasException(ctx));
        assert(list->items == snapshots[i].list.items);
        assert(list->count == snapshots[i].list.count);
        assert(list->capacity == snapshots[i].list.capacity);
        assert(cache_contents(list) == snapshots[i].contents);
    }
}

/* Locale lookup tests cover detached-cache rollback. These boundaries also
   exercise cold constructor/method paths for each service and successful retry. */
static void check_cold_failures(const char *source, size_t operation)
{
    OperationFixture fixture;
    size_t count, i;
    size_t failures[4];

    open_operation(&fixture, source);
    count = call_operation(&fixture, SIZE_MAX);
    assert(count && count < SIZE_MAX - 1 && !fixture.probe.failed);
    close_operation(&fixture);
    failures[0] = 1;
    failures[1] = count / 2 + count % 2;
    failures[2] = count;
    failures[3] = count + 1;
    printf("Intl OOM operation %zu cold allocations=%zu boundaries=1,%zu,%zu,%zu\n",
           operation, count, failures[1], failures[2], failures[3]);
    fflush(stdout);
    for (i = 0; i < sizeof(failures) / sizeof(*failures); i++) {
        if (i && failures[i] == failures[i - 1])
            continue;
        open_operation(&fixture, source);
        call_operation(&fixture, failures[i]);
        assert(fixture.probe.failed == (failures[i] <= count));
        if (failures[i] > count)
            assert(fixture.probe.attempts == count);
        call_operation(&fixture, 0); /* Every fault requires a successful retry. */
        assert(!fixture.probe.failed);
        close_operation(&fixture);
    }
}

/* Reuse one realm after warming this pure operation. Completed context caches
   survive; each call's temporary objects and opaque state must be released. */
static void check_warm_failures(const char *source, size_t operation)
{
    OperationFixture fixture;
    CacheSnapshot snapshots[JS_INTL_SERVICE_COUNT];
    size_t count, failure, live;

    open_operation(&fixture, source);
    call_operation(&fixture, 0);
    snapshot_caches(fixture.ctx, snapshots);
    count = call_operation(&fixture, SIZE_MAX);
    assert(count < SIZE_MAX - 1 && !fixture.probe.failed);
    check_caches(fixture.ctx, snapshots);
    live = fixture.probe.live;
    printf("Intl OOM operation %zu warm allocations=%zu sentinel=%zu\n",
           operation, count, count + 1);
    fflush(stdout);
    for (failure = 1; failure <= count + 1; failure++) {
        call_operation(&fixture, failure);
        assert(fixture.probe.failed == (failure <= count));
        if (failure > count)
            assert(fixture.probe.attempts == count);
        assert(fixture.probe.live == live);
        check_caches(fixture.ctx, snapshots);
        call_operation(&fixture, 0);
        assert(!fixture.probe.failed);
        assert(fixture.probe.live == live);
        check_caches(fixture.ctx, snapshots);
    }
    close_operation(&fixture);
}

#endif
int main(void)
{
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
    size_t i;
    for (i = 0; i < sizeof(cold_operations)/sizeof(cold_operations[0]); i++)
        check_cold_failures(cold_operations[i], i + 1);
    for (i = 0; i < sizeof(warm_operations)/sizeof(warm_operations[0]); i++)
        check_warm_failures(warm_operations[i], i + 1);
#endif
    return 0;
}
