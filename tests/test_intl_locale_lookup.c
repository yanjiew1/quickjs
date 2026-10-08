/* Native host-default and AvailableLocales regression.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#include "quickjs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef CONFIG_INTL
#include "../src/quickjs/builtins/intl/locale-private.h"
#include <unicode/uloc.h>
#include "intl-allocation-probe.h"

static void check_lookup(JSContext *ctx, const char *request,
                         const char *expected)
{
    char *matched = intl_lookup_locale(ctx, JS_INTL_NUMBER_FORMAT, request);
    assert(matched && !JS_HasException(ctx));
    if (strcmp(matched, expected))
        fprintf(stderr, "Locale lookup %s: got %s, expected %s\n",
                request, matched, expected);
    assert(!strcmp(matched, expected));
    js_free(ctx, matched);
}

static int cache_contains(const JSIntlLocaleList *list, const char *locale)
{
    size_t i;
    for (i = 0; i < list->count; i++) {
        if (!strcmp(list->items[i], locale))
            return 1;
    }
    return 0;
}

static void check_complete_cache(const JSIntlLocaleList *list)
{
    size_t i;
    assert(list && list->items && list->count);
    assert(list->count <= list->capacity);
    for (i = 0; i < list->count; i++) {
        assert(list->items[i] && *list->items[i]);
        if (i)
            assert(strcmp(list->items[i - 1], list->items[i]) < 0);
    }
}

static void check_cache_reuse(JSContext *ctx, const char *default_locale)
{
    const JSIntlLocaleList *number, *collator, *search;
    char *const *number_items;
    JSMemoryUsage before, after;
    char *matched;
    int i;

    number = js_intl_available_locale_cache(ctx, JS_INTL_NUMBER_FORMAT);
    check_complete_cache(number);
    assert(cache_contains(number, default_locale));
    number_items = number->items;
    JS_ComputeMemoryUsage(JS_GetRuntime(ctx), &before);
    matched = intl_lookup_locale(ctx, JS_INTL_COLLATOR, default_locale);
    assert(matched && !strcmp(matched, default_locale));
    js_free(ctx, matched);
    collator = js_intl_available_locale_cache(ctx, JS_INTL_COLLATOR);
    check_complete_cache(collator);
    matched = intl_lookup_locale(ctx, JS_INTL_COLLATOR_SEARCH, default_locale);
    assert(matched && !strcmp(matched, default_locale));
    js_free(ctx, matched);
    search = js_intl_available_locale_cache(ctx, JS_INTL_COLLATOR_SEARCH);
    check_complete_cache(search);
    assert(collator->items != number->items);
    assert(search->items != collator->items);
    assert(search->items != number->items);
    JS_ComputeMemoryUsage(JS_GetRuntime(ctx), &after);
    assert(after.memory_used_count > before.memory_used_count);
    assert(after.memory_used_size > before.memory_used_size);
    JS_RunGC(JS_GetRuntime(ctx));
    for (i = 0; i < 8; i++) {
        check_lookup(ctx, default_locale, default_locale);
        assert(number->items == number_items);
        check_complete_cache(number);
    }
}

static void clear_cache_error(JSContext *ctx)
{
    JSValue error;
    assert(JS_HasException(ctx));
    error = JS_GetException(ctx);
    JS_FreeValue(ctx, error);
    assert(!JS_HasException(ctx));
}

/* The first member preserves the existing probe callback layout. Cold
   counting/injection stops once the detached locale list is published. */
typedef struct CacheAllocationProbe {
    AllocationProbe allocation;
    const JSIntlLocaleList *cache;
    int cold_only;
} CacheAllocationProbe;

static void *cache_probe_malloc(JSMallocState *state, size_t size)
{
    CacheAllocationProbe *probe = state->opaque;
    size_t failure_at = probe->allocation.failure_at;
    void *result;

    if (probe->cold_only && probe->cache && probe->cache->items)
        probe->allocation.failure_at = 0;
    result = probe_malloc(state, size);
    probe->allocation.failure_at = failure_at;
    return result;
}

static void *cache_probe_realloc(JSMallocState *state, void *ptr, size_t size)
{
    CacheAllocationProbe *probe = state->opaque;
    size_t failure_at = probe->allocation.failure_at;
    void *result;

    if (probe->cold_only && probe->cache && probe->cache->items)
        probe->allocation.failure_at = 0;
    result = probe_realloc(state, ptr, size);
    probe->allocation.failure_at = failure_at;
    return result;
}

static const JSMallocFunctions cache_probe_functions = {
    cache_probe_malloc, probe_free, cache_probe_realloc, NULL,
};

static size_t count_cold_cache_allocations(void)
{
    CacheAllocationProbe probe = { 0 };
    JSRuntime *rt = JS_NewRuntime2(&cache_probe_functions, &probe);
    JSContext *ctx;
    size_t count;

    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    probe.cache = js_intl_available_locale_cache(ctx, JS_INTL_NUMBER_FORMAT);
    assert(probe.cache && !probe.cache->items);
    probe.cold_only = 1;
    probe.allocation.failure_at = SIZE_MAX;
    check_lookup(ctx, "en-US", "en-US");
    count = probe.allocation.attempts;
    probe.allocation.failure_at = 0;
    assert(count && !probe.allocation.failed);
    check_complete_cache(probe.cache);
    probe.cache = NULL;
    probe.cold_only = 0;
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(!probe.allocation.live);
    return count;
}

static void check_cache_allocation_failures(void)
{
    size_t cold_count = count_cold_cache_allocations();
    size_t failures[] = { 1, cold_count / 2 + cold_count % 2, cold_count };
    char long_request[1024];
    size_t i, length = strlen("en-US-x");

    memcpy(long_request, "en-US-x", length);
    while (length + 9 < sizeof(long_request)) {
        memcpy(long_request + length, "-abcdefgh", 9);
        length += 9;
    }
    long_request[length] = 0;
    /* Exceed the current 512-byte small-block allocator bound, so a warm
       owned-match copy must reach the host callback despite spare arenas. */
    assert(length > 512);
    assert(intl_unicode_locale_well_formed(long_request, length));
    printf("Intl cache cold allocation positions: %zu, %zu, %zu\n",
           failures[0], failures[1], failures[2]);
    for (i = 0; i < sizeof(failures) / sizeof(*failures); i++) {
        CacheAllocationProbe probe = { 0 };
        JSRuntime *rt;
        JSContext *ctx;
        const JSIntlLocaleList *cache;
        char *const *items;
        char *matched;

        if (i && failures[i] == failures[i - 1])
            continue;
        rt = JS_NewRuntime2(&cache_probe_functions, &probe);
        assert(rt);
        ctx = JS_NewContext(rt);
        assert(ctx);
        cache = js_intl_available_locale_cache(ctx, JS_INTL_NUMBER_FORMAT);
        assert(cache && !cache->items && !cache->count && !cache->capacity);
        probe.cache = cache;
        probe.cold_only = 1;
        probe.allocation.attempts = 0;
        probe.allocation.failure_at = failures[i];
        matched = intl_lookup_locale(ctx, JS_INTL_NUMBER_FORMAT, "en-US");
        probe.allocation.failure_at = 0;
        if (!probe.allocation.failed || matched) {
            fprintf(stderr, "cold cache fault %zu/%zu: attempts=%zu "
                    "failed=%d matched=%s exception=%d\n",
                    failures[i], cold_count, probe.allocation.attempts,
                    probe.allocation.failed, matched ? matched : "NULL",
                    JS_HasException(ctx));
        }
        assert(probe.allocation.failed && !matched);
        clear_cache_error(ctx);
        /* Partially built detached arrays must never be retained. */
        assert(!cache->items && !cache->count && !cache->capacity);
        check_lookup(ctx, "en-US", "en-US");
        check_complete_cache(cache);
        items = cache->items;

        /* A warm lookup can still throw OOM while copying its owned match,
           but it must neither free nor replace the completed cache. */
        probe.cold_only = 0;
        probe.allocation.failed = 0;
        probe.allocation.attempts = 0;
        probe.allocation.failure_at = 1;
        matched = intl_lookup_locale(ctx, JS_INTL_NUMBER_FORMAT, long_request);
        probe.allocation.failure_at = 0;
        if (!probe.allocation.failed || matched) {
            fprintf(stderr, "warm cache fault: attempts=%zu failed=%d "
                    "matched=%s exception=%d\n", probe.allocation.attempts,
                    probe.allocation.failed, matched ? matched : "NULL",
                    JS_HasException(ctx));
        }
        assert(probe.allocation.failed && !matched);
        clear_cache_error(ctx);
        assert(cache->items == items);
        check_complete_cache(cache);
        JS_RunGC(rt);
        check_lookup(ctx, long_request, "en-US");
        assert(cache->items == items);
        probe.cache = NULL;
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!probe.allocation.live);
    }
}

int main(void)
{
    const char *prior = uloc_getDefault();
    const char *private_default = "en-US-x-private-u-ca-gregory";
    size_t length = strlen(prior);
    char *saved_default = malloc(length + 1);
    char backend_default[128];
    int32_t parsed = 0;
    UErrorCode status = U_ZERO_ERROR;
    JSRuntime *rt, *other_rt;
    JSContext *ctx, *other;

    assert(saved_default);
    memcpy(saved_default, prior, length + 1);
    uloc_forLanguageTag(private_default, backend_default,
                        sizeof(backend_default), &parsed, &status);
    assert(U_SUCCESS(status) && parsed == (int32_t)strlen(private_default));
    /* The standalone host test controls ICU defaults; the engine never does. */
    uloc_setDefault(backend_default, &status);
    assert(U_SUCCESS(status));
    rt = JS_NewRuntime();
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    assert(!strcmp(js_intl_default_locale(ctx), private_default));
    check_lookup(ctx, "az-Latn-AZ", "az-Latn-AZ");
    check_lookup(ctx, "az-AZ", "az-AZ");
    check_lookup(ctx, private_default, private_default);
    check_lookup(ctx, "en-US-x-private-u-ca-gregory-a-b", private_default);
    check_lookup(ctx, "en-US-u-ca-gregory-x-private-u-ca-gregory",
                  private_default);
    check_cache_reuse(ctx, private_default);
    {
        const char *other_default = "fr-FR-x-second";
        const JSIntlLocaleList *first_cache, *second_cache;
        char *const *second_items;

        status = U_ZERO_ERROR;
        uloc_forLanguageTag(other_default, backend_default,
                            sizeof(backend_default), &parsed, &status);
        assert(U_SUCCESS(status) && parsed == (int32_t)strlen(other_default));
        /* Only this standalone host fixture changes ICU configuration,
           before creating the second runtime; no engine operation does. */
        uloc_setDefault(backend_default, &status);
        assert(U_SUCCESS(status));
        other_rt = JS_NewRuntime();
        assert(other_rt);
        other = JS_NewContext(other_rt);
        assert(other);
        assert(!strcmp(js_intl_default_locale(ctx), private_default));
        assert(!strcmp(js_intl_default_locale(other), other_default));
        check_lookup(ctx, private_default, private_default);
        check_lookup(other, other_default, other_default);
        first_cache = js_intl_available_locale_cache(ctx, JS_INTL_NUMBER_FORMAT);
        second_cache = js_intl_available_locale_cache(other, JS_INTL_NUMBER_FORMAT);
        assert(first_cache->items != second_cache->items);
        assert(cache_contains(first_cache, private_default));
        assert(!cache_contains(first_cache, other_default));
        assert(cache_contains(second_cache, other_default));
        assert(!cache_contains(second_cache, private_default));
        second_items = second_cache->items;
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        JS_RunGC(other_rt);
        check_lookup(other, other_default, other_default);
        assert(second_cache->items == second_items);
        JS_FreeContext(other);
        JS_FreeRuntime(other_rt);
    }
    status = U_ZERO_ERROR;
    uloc_setDefault(saved_default, &status);
    assert(U_SUCCESS(status));
    free(saved_default);
    check_cache_allocation_failures();
    return 0;
}
#else
int main(void)
{
    return 0;
}
#endif
