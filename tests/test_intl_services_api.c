/*
 * QuickJS native Intl services and tests
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
/* Native Intl allocation failure and cross-context ownership tests.
 * Run only in an enabled build after all four service stages are integrated.
 * Original C test; ICU allocation paths additionally need ASan/LSan CI. */
#include "quickjs.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef CONFIG_INTL
typedef struct FailureState {
    size_t live, calls, fail_at;
    int armed, failed;
} FailureState;
typedef union AllocationHeader {
    max_align_t alignment;
    struct { size_t size; } h;
} AllocationHeader;
static int should_fail(JSMallocState *s)
{
    FailureState *f = s->opaque;
    if (!f->armed) return 0;
    if (f->calls++ == f->fail_at) { f->failed++; return 1; }
    return 0;
}
static void *failure_malloc(JSMallocState *s, size_t size)
{
    FailureState *f = s->opaque;
    AllocationHeader *h;
    if (size > SIZE_MAX - sizeof(*h) || should_fail(s)) return NULL;
    h = malloc(sizeof(*h) + (size ? size : 1));
    if (!h) return NULL;
    h->h.size = size;
    f->live++;
    s->malloc_count++;
    s->malloc_size += size + sizeof(*h);
    return h + 1;
}
static void failure_free(JSMallocState *s, void *ptr)
{
    FailureState *f = s->opaque;
    AllocationHeader *h;
    if (!ptr) return;
    h = (AllocationHeader *)ptr - 1;
    assert(f->live);
    f->live--;
    s->malloc_count--;
    s->malloc_size -= h->h.size + sizeof(*h);
    free(h);
}
static void *failure_realloc(JSMallocState *s, void *ptr, size_t size)
{
    AllocationHeader *h;
    size_t old;
    if (!ptr) return failure_malloc(s, size);
    if (!size) { failure_free(s, ptr); return NULL; }
    if (size > SIZE_MAX - sizeof(*h) || should_fail(s)) return NULL;
    h = (AllocationHeader *)ptr - 1;
    old = h->h.size;
    h = realloc(h, sizeof(*h) + size);
    if (!h) return NULL;
    h->h.size = size;
    s->malloc_size = s->malloc_size - old + size;
    return h + 1;
}
static size_t failure_usable_size(const void *ptr)
{
    return ptr ? ((const AllocationHeader *)ptr - 1)->h.size : 0;
}
static const JSMallocFunctions failure_functions = {
    failure_malloc, failure_free, failure_realloc, failure_usable_size
};
static JSValue evaluate(JSContext *ctx, const char *source, int flags)
{
    return JS_Eval(ctx, source, strlen(source), "intl-api", flags);
}
static void clear_exception(JSContext *ctx)
{
    JS_FreeValue(ctx, JS_GetException(ctx));
}
typedef struct ServiceFixture {
    FailureState failure;
    JSRuntime *rt;
    JSContext *ctx;
} ServiceFixture;

static void open_service(ServiceFixture *fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    fixture->rt = JS_NewRuntime2(&failure_functions, &fixture->failure);
    assert(fixture->rt);
    fixture->ctx = JS_NewContext(fixture->rt);
    assert(fixture->ctx);
}

static void close_service(ServiceFixture *fixture)
{
    assert(!JS_HasException(fixture->ctx));
    JS_RunGC(fixture->rt);
    JS_FreeContext(fixture->ctx);
    JS_FreeRuntime(fixture->rt);
    assert(fixture->failure.live == 0);
}

/* Compile before arming so failures exercise constructor/format/parts paths.
   Keep the original requirement that every injected allocation failure throws.
   Disable injection before consuming the exception and releasing temporaries. */
static size_t call_service(ServiceFixture *fixture, const char *source,
                           size_t fail_at, int expected_failure)
{
    FailureState *f = &fixture->failure;
    JSContext *ctx = fixture->ctx;
    JSValue compiled, result;

    assert(!JS_HasException(ctx));
    compiled = evaluate(ctx, source, JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    f->calls = 0;
    f->failed = 0;
    f->fail_at = fail_at;
    f->armed = 1;
    result = JS_EvalFunction(ctx, compiled);
    f->armed = 0;
    if (f->failed != expected_failure ||
        JS_IsException(result) != expected_failure) {
        fprintf(stderr, "Intl service allocation mismatch fail_at=%zu calls=%zu "
                "failed=%d expected_failure=%d exception=%d live=%zu source=%s\n",
                fail_at, f->calls, f->failed, expected_failure,
                JS_IsException(result), f->live, source);
        fflush(stderr);
    }
    assert(f->failed == expected_failure);
    assert(JS_IsException(result) == expected_failure);
    if (expected_failure)
        clear_exception(ctx);
    JS_FreeValue(ctx, result);
    JS_RunGC(fixture->rt);
    assert(!JS_HasException(ctx));
    return f->calls;
}

/* Fresh realms retain first/middle/last cold failures and the first untouched
   success position, with successful retry and complete runtime teardown.
   Detached locale-cache rollback is covered in test_intl_locale_lookup.c. */
static void cold_failure_boundaries(const char *source, size_t operation)
{
    ServiceFixture fixture;
    size_t count, i, failures[4];

    /* Prime immutable process-wide admission in a discarded realm. The
       measured realm below still has cold context caches and methods. */
    open_service(&fixture);
    call_service(&fixture, source, SIZE_MAX, 0);
    close_service(&fixture);
    open_service(&fixture);
    count = call_service(&fixture, source, SIZE_MAX, 0);
    assert(count < SIZE_MAX - 1);
    close_service(&fixture);
    /* Reusing pool slots can require no host allocation callbacks. In that
       case, all boundaries are the untouched success sentinel at zero. */
    failures[0] = 0;
    failures[1] = count ? (count - 1) / 2 : 0;
    failures[2] = count ? count - 1 : 0;
    failures[3] = count;
    printf("Intl service API operation %zu cold allocations=%zu boundaries=0,%zu,%zu,%zu\n",
           operation, count, failures[1], failures[2], failures[3]);
    fflush(stdout);
    for (i = 0; i < sizeof(failures) / sizeof(*failures); i++) {
        if (i && failures[i] == failures[i - 1])
            continue;
        open_service(&fixture);
        if (failures[i] == count)
            assert(call_service(&fixture, source, failures[i], 0) == count);
        else
            call_service(&fixture, source, failures[i], 1);
        call_service(&fixture, source, SIZE_MAX, 0);
        close_service(&fixture);
    }
}

/* Resolve the service locale inventory, then execute the original operation
   once to realize its lazy prototype methods before measuring the warm count.
   The first call can allocate persistent functions that later calls reuse.
   Every subsequent constructor, method and parts host allocation is swept;
   cold boundaries retain first-use behavior. Reuse the warmed realm without
   rebuilding its 71k-allocation locale list at every fault position under
   GCC ASan. Check cleanup after each fault and retry. */
static void warm_failure_sweep(const char *source, const char *warmup,
                               size_t operation)
{
    ServiceFixture fixture;
    JSValue result;
    size_t count, failure, live;

    open_service(&fixture);
    result = evaluate(fixture.ctx, warmup, JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result));
    assert(JS_ToBool(fixture.ctx, result) == 1);
    JS_FreeValue(fixture.ctx, result);
    JS_RunGC(fixture.rt);
    call_service(&fixture, source, SIZE_MAX, 0);
    count = call_service(&fixture, source, SIZE_MAX, 0);
    assert(count < SIZE_MAX - 1);
    live = fixture.failure.live;
    printf("Intl service API operation %zu warm allocations=%zu sentinel=%zu\n",
           operation, count, count);
    fflush(stdout);
    for (failure = 0; failure <= count; failure++) {
        if (failure == count)
            assert(call_service(&fixture, source, failure, 0) == count);
        else
            call_service(&fixture, source, failure, 1);
        assert(fixture.failure.live == live);
        call_service(&fixture, source, SIZE_MAX, 0);
        assert(fixture.failure.live == live);
    }
    close_service(&fixture);
}
/* Repeat ordinary lifetime work separately from the exhaustive fault sweep.
   No failure is injected here; the original twelve iterations must succeed,
   and destroying each runtime must release all counted engine allocations. */
static void repeated_lifetime(const char *source)
{
    FailureState f = {0};
    JSRuntime *rt = JS_NewRuntime2(&failure_functions, &f);
    JSContext *ctx;
    JSValue compiled, result;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    compiled = evaluate(ctx, source, JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    assert(!JS_IsException(compiled));
    result = JS_EvalFunction(ctx, compiled);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    JS_RunGC(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(f.live == 0);
}
#ifdef CONFIG_INTL_NATIVE
/* Verify provider fallback separately after the original operation's sweeps.
   The fresh fixture cannot pre-create any state used by those measurements. */
static void verify_native_fallback(const char *source, size_t operation)
{
    ServiceFixture fixture;
    JSValue result;
    open_service(&fixture);
    result = evaluate(fixture.ctx, source, JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(result));
    assert(JS_ToBool(fixture.ctx, result) == 1);
    JS_FreeValue(fixture.ctx, result);
    close_service(&fixture);
    printf("Intl locale data: service fixture %zu unavailable; default fallback asserted\n", operation);
}
#endif
/* Each distinct operation retains cold fault boundaries and an exhaustive
   warm sweep. Its original twelve lifetime executions use the identical body. */
#define INTL_PLURAL_OPERATIONS \
    "let p=new Intl.PluralRules('ar',{minimumFractionDigits:2});" \
    "p.select('90071992547409921.005');" \
    "p.selectRange(1,2);" \
    "p.resolvedOptions();"
#define INTL_LIST_OPERATIONS \
    "let p=new Intl.ListFormat('he');" \
    "p.format(['A','','B']);" \
    "p.formatToParts(['','','B']);" \
    "p.resolvedOptions();"
#define INTL_RELATIVE_OPERATIONS \
    "let p=new Intl.RelativeTimeFormat('ar-u-nu-arab');" \
    "p.format(-0,'day');" \
    "p.formatToParts(-12345.67,'day');" \
    "p.resolvedOptions();"
#define INTL_DISPLAY_OPERATIONS \
    "let p=new Intl.DisplayNames('fr',{type:'language'});" \
    "p.of('en-GB');" \
    "p.resolvedOptions();" \
    "new Intl.DisplayNames('fr',{type:'dateTimeField'}).of('month');"
#ifdef CONFIG_INTL_NATIVE
/* This released provider admits en/en-US. Keep the requested-locale operation
   in every cold/warm/lifetime sweep. Warm only the inventory, as before. */
#define INTL_SERVICE_WARMUP(service, locale, options) \
    "Intl." service ".supportedLocalesOf('" locale "').length === 0 &&" \
    "Intl." service ".supportedLocalesOf('en')[0] === 'en'"
#else
#define INTL_SERVICE_WARMUP(service, locale, options) \
    "Intl." service ".supportedLocalesOf('" locale "')[0] === '" locale "'"
#endif
#define INTL_SERVICE_FALLBACK(service, locale, options) \
    "new Intl." service "('" locale "'," options ").resolvedOptions().locale ===" \
    "new Intl." service "(undefined," options ").resolvedOptions().locale"
static const struct {
    const char *once, *repeated, *warmup, *fallback;
} service_sources[] = {
    { "{" INTL_PLURAL_OPERATIONS "}",
      "for(let i=0;i<12;i++){" INTL_PLURAL_OPERATIONS "}",
      INTL_SERVICE_WARMUP("PluralRules", "ar", "undefined"),
      INTL_SERVICE_FALLBACK("PluralRules", "ar", "undefined") },
    { "{" INTL_LIST_OPERATIONS "}",
      "for(let i=0;i<12;i++){" INTL_LIST_OPERATIONS "}",
      INTL_SERVICE_WARMUP("ListFormat", "he", "undefined"),
      INTL_SERVICE_FALLBACK("ListFormat", "he", "undefined") },
    { "{" INTL_RELATIVE_OPERATIONS "}",
      "for(let i=0;i<12;i++){" INTL_RELATIVE_OPERATIONS "}",
      INTL_SERVICE_WARMUP("RelativeTimeFormat", "ar", "undefined"),
      INTL_SERVICE_FALLBACK("RelativeTimeFormat", "ar-u-nu-arab", "undefined") },
    { "{" INTL_DISPLAY_OPERATIONS "}",
      "for(let i=0;i<12;i++){" INTL_DISPLAY_OPERATIONS "}",
      INTL_SERVICE_WARMUP("DisplayNames", "fr", "{type:'language'}"),
      INTL_SERVICE_FALLBACK("DisplayNames", "fr", "{type:'language'}") },
};
#undef INTL_SERVICE_WARMUP
#undef INTL_SERVICE_FALLBACK
#undef INTL_PLURAL_OPERATIONS
#undef INTL_LIST_OPERATIONS
#undef INTL_RELATIVE_OPERATIONS
#undef INTL_DISPLAY_OPERATIONS
int main(void)
{
    for (size_t i = 0; i < sizeof(service_sources) / sizeof(service_sources[0]); i++) {
        cold_failure_boundaries(service_sources[i].once, i + 1);
        warm_failure_sweep(service_sources[i].once, service_sources[i].warmup, i + 1);
        repeated_lifetime(service_sources[i].repeated);
#ifdef CONFIG_INTL_NATIVE
        verify_native_fallback(service_sources[i].fallback, i + 1);
#endif
    }
    puts("Intl service API tests passed");
    return 0;
}
#else
int main(void) { return 0; }
#endif
