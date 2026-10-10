/* Per-context Intl default locale embedding tests.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license as in the QuickJS source tree.
 * This driver invokes JavaScript through C; qjs alone cannot configure defaults.
 * Source preparation only. Run each backend/disabled profile separately. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"
#ifdef CONFIG_INTL
#include "intl-allocation-probe.h"
#include "../src/quickjs/builtins/intl/intl-internal.h"
#endif

static void eval_ok(JSContext *ctx, const char *source)
{
    JSValue result = JS_Eval(ctx, source, strlen(source),
                            "intl-default-locale-api", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue error = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, error);
        fprintf(stderr, "Intl default locale API: %s\n", message ? message : "exception");
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, error);
        abort();
    }
    JS_FreeValue(ctx, result);
    assert(!JS_HasException(ctx));
}

static void check_string(JSContext *ctx, JSValueConst value, const char *expected)
{
    const char *string;
    assert(JS_IsString(value));
    string = JS_ToCString(ctx, value);
    assert(string && !strcmp(string, expected));
    JS_FreeCString(ctx, string);
    assert(!JS_HasException(ctx));
}

static void clear_error(JSContext *ctx, const char *name)
{
    JSValue error, value;
    assert(JS_HasException(ctx));
    error = JS_GetException(ctx);
    if (name) {
        value = JS_GetPropertyStr(ctx, error, "name");
        assert(!JS_IsException(value));
        check_string(ctx, value, name);
        JS_FreeValue(ctx, value);
    }
    JS_FreeValue(ctx, error);
    assert(!JS_HasException(ctx));
}

static void check_global_intl_absent(JSContext *ctx)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue value = JS_GetPropertyStr(ctx, global, "Intl");
    assert(JS_IsUndefined(value));
    JS_FreeValue(ctx, value);
    JS_FreeValue(ctx, global);
}

#ifdef CONFIG_INTL
static void check_locale(JSContext *ctx, const char *expected)
{
    JSValue locale = JS_GetIntlDefaultLocale(ctx);
    assert(!JS_IsException(locale));
    check_string(ctx, locale, expected);
    JS_FreeValue(ctx, locale);
#ifdef CONFIG_INTL_NATIVE
    {
        QJSIntlBytes provider_locale = qjs_intl_provider_default_locale(
            js_intl_native_provider(ctx));
        assert(provider_locale.length == strlen(expected));
        assert(!memcmp(provider_locale.data, expected, provider_locale.length));
    }
#endif
}

typedef struct CacheSnapshot {
    JSIntlLocaleList list;
    uint64_t contents;
} CacheSnapshot;

static uint64_t cache_hash(const JSIntlLocaleList *list)
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
        JSIntlLocaleList *cache = js_intl_available_locale_cache(ctx, i);
        assert(cache);
        snapshots[i].list = *cache;
        snapshots[i].contents = cache_hash(cache);
    }
}

static void check_caches_unchanged(JSContext *ctx, const CacheSnapshot *snapshots)
{
    int i;
    for (i = 0; i < JS_INTL_SERVICE_COUNT; i++) {
        JSIntlLocaleList *cache = js_intl_available_locale_cache(ctx, i);
        assert(cache && cache->items == snapshots[i].list.items);
        assert(cache->count == snapshots[i].list.count);
        assert(cache->capacity == snapshots[i].list.capacity);
        assert(cache_hash(cache) == snapshots[i].contents);
    }
}

static void check_caches_empty(JSContext *ctx)
{
    int i;
    for (i = 0; i < JS_INTL_SERVICE_COUNT; i++) {
        JSIntlLocaleList *cache = js_intl_available_locale_cache(ctx, i);
        assert(cache && !cache->items && !cache->count && !cache->capacity);
    }
}

static void warm_services(JSContext *ctx)
{
#ifdef CONFIG_INTL_NATIVE
    eval_ok(ctx, "Intl.ListFormat.supportedLocalesOf(['en', 'en-US']);");
#else
    eval_ok(ctx,
        "for (const name of ['Collator','Segmenter','NumberFormat','DateTimeFormat',"
        "'PluralRules','ListFormat','RelativeTimeFormat','DisplayNames','DurationFormat'])"
        " Intl[name].supportedLocalesOf(['en','en-US']);");
    /* The search service has a distinct cache and supported key data. */
    eval_ok(ctx, "new Intl.Collator(undefined, {usage:'search'});");
#endif
}

static void make_old_objects(JSContext *ctx)
{
    eval_ok(ctx,
        "var oldList = new Intl.ListFormat();"
        "var oldListText = oldList.format(['a','b','c']);"
        "var oldListParts = JSON.stringify(oldList.formatToParts(['a','b','c']));");
#ifndef CONFIG_INTL_NATIVE
    eval_ok(ctx,
        "var serviceNames = ['Collator','Segmenter','NumberFormat','DateTimeFormat',"
        "'PluralRules','ListFormat','RelativeTimeFormat','DisplayNames','DurationFormat'];"
        "var oldObjects = serviceNames.map(n => new Intl[n](undefined,"
        " n === 'DisplayNames' ? {type:'language'} : undefined));"
        "var oldLocales = oldObjects.map(o => o.resolvedOptions().locale);"
        "var oldCompare = oldObjects[0].compare;"
        "var oldNumber = oldObjects[2].format;"
        "var oldDate = oldObjects[3].format;"
        "var oldNumberText = oldNumber(1234.5); var oldDateText = oldDate(0);"
        "var oldTemporalDtf, oldTemporalValue, oldTemporalText;"
        "if (typeof Temporal === 'object') {"
        " oldTemporalDtf = new Intl.DateTimeFormat(undefined,"
        "   {year:'numeric',month:'numeric',day:'numeric',timeZone:'UTC'});"
        " oldTemporalValue = new Temporal.PlainDate(2024,3,4);"
        " oldTemporalText = oldTemporalDtf.format(oldTemporalValue);"
        "}");
#endif
}

static void check_old_objects(JSContext *ctx)
{
    eval_ok(ctx,
        "if (oldList.resolvedOptions().locale !== 'en-US' ||"
        " oldList.format(['a','b','c']) !== oldListText ||"
        " JSON.stringify(oldList.formatToParts(['a','b','c'])) !== oldListParts)"
        " throw Error('old ListFormat snapshot changed');"
        "if (new Intl.ListFormat().resolvedOptions().locale !== 'en')"
        " throw Error('new ListFormat did not use the new default');");
#ifndef CONFIG_INTL_NATIVE
    eval_ok(ctx,
        "oldObjects.forEach((o,i) => {"
        " if (o.resolvedOptions().locale !== oldLocales[i])"
        "  throw Error('old service snapshot changed: ' + serviceNames[i]);"
        " const fresh = new Intl[serviceNames[i]](undefined,"
        "  serviceNames[i] === 'DisplayNames' ? {type:'language'} : undefined);"
        " if (fresh.resolvedOptions().locale !== 'en')"
        "  throw Error('new service missed default: ' + serviceNames[i]);"
        "});"
        "if (oldCompare('a','b') >= 0 || oldNumber(1234.5) !== oldNumberText ||"
        " oldDate(0) !== oldDateText) throw Error('old bound formatter changed');"
        "if (oldTemporalDtf && oldTemporalDtf.format(oldTemporalValue) !== oldTemporalText)"
        " throw Error('old Temporal formatter bank changed');");
#endif
}

static void check_basic_contract(void)
{
    AllocationProbe probe = { 0 };
    JSRuntime *rt = JS_NewRuntime2(&probe_functions, &probe);
    JSContext *a, *b;
    JSValue owned, object;
    CacheSnapshot snapshots[JS_INTL_SERVICE_COUNT];
    assert(rt);
    a = JS_NewContextRaw(rt);
    assert(a && !JS_AddIntrinsicBaseObjects(a));
    assert(!JS_SetIntlDefaultLocale(a, "EN-us-u-kn-true"));
    check_global_intl_absent(a); /* Configuration does not publish an intrinsic. */
    check_locale(a, "en-US");
    assert(!JS_AddIntrinsicEval(a) && !JS_AddIntrinsicJSON(a));
    assert(!JS_AddIntrinsicProxy(a));
#if defined(CONFIG_TEMPORAL) && !defined(CONFIG_INTL_NATIVE)
    assert(!JS_AddIntrinsicDate(a) && !JS_AddIntrinsicTemporal(a));
#endif
    assert(!JS_AddIntrinsicIntl(a));
    b = JS_NewContext(rt);
    assert(b && !JS_SetIntlDefaultLocale(b, "en"));
    eval_ok(a, "if (new Intl.ListFormat().resolvedOptions().locale !== 'en-US') throw Error('realm a');");
    eval_ok(b, "if (new Intl.ListFormat().resolvedOptions().locale !== 'en') throw Error('realm b');");
    warm_services(a);
    make_old_objects(a);
    eval_ok(a,
        "var listGetterOrder = [];"
        "new Intl.ListFormat(undefined, new Proxy({}, {get(t,k) {"
        " listGetterOrder.push(k); return undefined; }}));"
        "var listGetterBefore = listGetterOrder.join();");
    owned = JS_GetIntlDefaultLocale(a);
    assert(!JS_IsException(owned));
    snapshot_caches(a, snapshots);
    assert(JS_SetIntlDefaultLocale(a, NULL) == -1);
    clear_error(a, "RangeError");
    check_locale(a, "en-US");
    check_caches_unchanged(a, snapshots);
    assert(JS_SetIntlDefaultLocale(a, "en_US") == -1);
    clear_error(a, "RangeError");
    check_locale(a, "en-US");
    check_caches_unchanged(a, snapshots);
#ifdef CONFIG_INTL_NATIVE
    assert(JS_SetIntlDefaultLocale(a, "fr-FR") == -1);
    clear_error(a, "RangeError");
    check_locale(a, "en-US");
    check_caches_unchanged(a, snapshots);
#endif
    assert(!JS_SetIntlDefaultLocale(a, "en"));
    check_caches_empty(a);
    check_string(a, owned, "en-US");
    check_locale(a, "en");
    check_locale(b, "en");
    check_old_objects(a);
    eval_ok(a,
        "listGetterOrder = [];"
        "new Intl.ListFormat(undefined, new Proxy({}, {get(t,k) {"
        " listGetterOrder.push(k); return undefined; }}));"
        "if (listGetterOrder.join() !== listGetterBefore ||"
        " listGetterBefore !== 'localeMatcher,type,style')"
        " throw Error('locale policy affected option getter order');");
    eval_ok(a,
        "if (new Intl.ListFormat('qzz').resolvedOptions().locale !== 'en')"
        " throw Error('unsupported request must use configured default');"
        "if (Intl.ListFormat.supportedLocalesOf(['en-US','en']).join() !== 'en-US,en')"
        " throw Error('missing default/fallback availability');");
#ifndef CONFIG_INTL_NATIVE
    assert(!JS_SetIntlDefaultLocale(a, "fr-FR"));
    check_locale(a, "fr-FR");
    eval_ok(a,
        "if (oldNumber(1234.5) !== oldNumberText || oldDate(0) !== oldDateText)"
        " throw Error('old bound formatter changed after language change');"
        "if (new Intl.NumberFormat().format(1234.5) === oldNumberText)"
        " throw Error('new formatter missed French policy');"
        "if (oldTemporalDtf && oldTemporalDtf.format(oldTemporalValue) !== oldTemporalText)"
        " throw Error('Temporal bank changed after language change');");
    /* Tags outside the ICU inventory use real provider fallback. All completed
       constructors must work while reporting the admitted default identifier. */
    assert(!JS_SetIntlDefaultLocale(a, "qzz-Latn-ZZ-u-kn-true"));
    check_locale(a, "qzz-Latn-ZZ");
    eval_ok(a,
        "for (const n of serviceNames) {"
        " const o = new Intl[n](undefined, n === 'DisplayNames' ? {type:'language'} : undefined);"
        " if (o.resolvedOptions().locale !== 'qzz-Latn-ZZ') throw Error('fallback: '+n);"
        " if (Intl[n].supportedLocalesOf(['qzz-Latn-ZZ','qzz-ZZ','qzz']).length !== 3)"
        "  throw Error('fallback closure: '+n);"
        "}"
        "new Intl.NumberFormat().format(1234.5);"
        "new Intl.DateTimeFormat().format(0);"
        "new Intl.PluralRules().select(2);"
        "new Intl.ListFormat().format(['a','b']);"
        "new Intl.RelativeTimeFormat().format(2,'day');"
        "new Intl.DisplayNames(undefined,{type:'language'}).of('en');"
        "new Intl.DurationFormat().format({hours:1,minutes:2});"
        "Array.from(new Intl.Segmenter().segment('hello world'));");
    assert(!JS_SetIntlDefaultLocale(a, "en-US-a-foo-u-kn-true-x-u-private"));
    check_locale(a, "en-US-a-foo-x-u-private");
#endif
    /* Hold a service object and the owned getter value past context teardown;
       finalizers and JS string release use runtime-owned allocation state. */
    object = JS_Eval(a, "oldList", 7, "intl-locale-lifetime", JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(object));
    /* Remove the prototype's constructor/realm references so GC can actually
       destroy the old context while this formatter handle remains retained. */
    assert(JS_SetPrototype(a, object, JS_NULL) == 1);
    JS_FreeContext(a);
    JS_RunGC(rt);
    check_string(b, owned, "en-US");
    JS_FreeValueRT(rt, owned);
    JS_FreeValueRT(rt, object);
    JS_FreeContext(b);
    JS_RunGC(rt);
    JS_FreeRuntime(rt);
    assert(!probe.live);
}

/* Cold initialization retains first/middle/last failure and success sentinel.
   Warm setters reject every measured host allocation. Successful retries may
   clear the inventories, so each warm fixture is recreated before injection.
   All rejected calls preserve existing policy/cache data and all blocks free. */
static size_t setter_once(size_t failure, int warm, int raw, int expected_failure)
{
    AllocationProbe probe = { 0 };
    JSRuntime *rt = JS_NewRuntime2(&probe_functions, &probe);
    JSContext *ctx;
    CacheSnapshot snapshots[JS_INTL_SERVICE_COUNT];
    size_t count;
    int result;
    assert(rt);
    ctx = raw ? JS_NewContextRaw(rt) : JS_NewContext(rt);
    assert(ctx);
    if (raw) {
        assert(!JS_AddIntrinsicBaseObjects(ctx));
        check_global_intl_absent(ctx);
    } else {
        assert(!JS_SetIntlDefaultLocale(ctx, "en-US"));
        if (warm)
            warm_services(ctx);
        snapshot_caches(ctx, snapshots);
    }
    probe.attempts = 0;
    probe.failed = 0;
    probe.failure_at = failure;
    result = JS_SetIntlDefaultLocale(ctx, "en-u-kn-true");
    probe.failure_at = 0;
    count = probe.attempts;
    assert(probe.failed == expected_failure);
    if (probe.failed) {
        assert(result == -1);
        clear_error(ctx, NULL);
        if (!raw) {
            check_locale(ctx, "en-US");
            check_caches_unchanged(ctx, snapshots);
        }
    } else {
        assert(result == 0 && !JS_HasException(ctx));
        check_locale(ctx, "en");
        check_caches_empty(ctx);
    }
    assert(!JS_SetIntlDefaultLocale(ctx, "en-u-kn-true"));
    check_locale(ctx, "en");
    if (raw) {
        check_global_intl_absent(ctx);
        assert(!JS_AddIntrinsicIntl(ctx));
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(!probe.live);
    return count;
}

static void check_setter_oom(void)
{
    int raw, warm;
    for (raw = 0; raw < 2; raw++) {
        for (warm = 0; warm < (raw ? 1 : 2); warm++) {
            size_t count = setter_once(SIZE_MAX, warm, raw, 0), failure;
            assert(count < SIZE_MAX - 1);
            if (warm) {
                for (failure = 1; failure <= count + 1; failure++)
                    setter_once(failure, warm, raw, failure <= count);
            } else {
                size_t boundaries[4] = { 1, count ? (count + 1) / 2 : 1,
                                         count ? count : 1, count + 1 };
                size_t i, j;
                for (i = 0; i < 4; i++) {
                    for (j = 0; j < i; j++)
                        if (boundaries[j] == boundaries[i])
                            break;
                    if (j == i)
                        setter_once(boundaries[i], warm, raw,
                                    boundaries[i] <= count);
                }
            }
            printf("Intl default setter: raw=%d warm=%d allocations=%zu mode=%s\n",
                   raw, warm, count, warm ? "exhaustive" : "cold-boundaries");
            fflush(stdout);
        }
    }
}

static size_t getter_once(size_t failure, int raw, int expected_failure)
{
    AllocationProbe probe = { 0 };
    JSRuntime *rt = JS_NewRuntime2(&probe_functions, &probe);
    JSContext *ctx;
    CacheSnapshot snapshots[JS_INTL_SERVICE_COUNT];
    JSValue value;
    size_t count;
    assert(rt);
    ctx = raw ? JS_NewContextRaw(rt) : JS_NewContext(rt);
    assert(ctx);
    if (raw) {
        assert(!JS_AddIntrinsicBaseObjects(ctx));
    } else {
        assert(!JS_SetIntlDefaultLocale(ctx, "en-US"));
        warm_services(ctx);
        snapshot_caches(ctx, snapshots);
    }
    probe.attempts = 0;
    probe.failed = 0;
    probe.failure_at = failure;
    value = JS_GetIntlDefaultLocale(ctx);
    probe.failure_at = 0;
    count = probe.attempts;
    assert(probe.failed == expected_failure);
    if (probe.failed) {
        assert(JS_IsException(value));
        clear_error(ctx, NULL);
    } else {
        assert(!JS_IsException(value));
        if (!raw)
            check_string(ctx, value, "en-US");
    }
    JS_FreeValue(ctx, value);
    if (!raw) {
        check_caches_unchanged(ctx, snapshots);
        check_locale(ctx, "en-US");
    } else {
        check_global_intl_absent(ctx);
        value = JS_GetIntlDefaultLocale(ctx);
        assert(JS_IsString(value));
        JS_FreeValue(ctx, value);
    }
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(!probe.live);
    return count;
}

static void check_getter_oom(void)
{
    int raw;
    for (raw = 0; raw < 2; raw++) {
        size_t count = getter_once(SIZE_MAX, raw, 0), failure;
        assert(count < SIZE_MAX - 1);
        if (!raw) {
            for (failure = 1; failure <= count + 1; failure++)
                getter_once(failure, raw, failure <= count);
        } else {
            size_t boundaries[4] = { 1, count ? (count + 1) / 2 : 1,
                                     count ? count : 1, count + 1 };
            size_t i, j;
            for (i = 0; i < 4; i++) {
                for (j = 0; j < i; j++)
                    if (boundaries[j] == boundaries[i])
                        break;
                if (j == i)
                    getter_once(boundaries[i], raw, boundaries[i] <= count);
            }
        }
        printf("Intl default getter: raw=%d allocations=%zu mode=%s\n",
               raw, count, raw ? "cold-boundaries" : "exhaustive");
        fflush(stdout);
    }
}
#endif

int main(void)
{
#ifdef CONFIG_INTL
    check_basic_contract();
    check_setter_oom();
    check_getter_oom();
#else
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue locale;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    assert(JS_SetIntlDefaultLocale(ctx, "en") == -1);
    clear_error(ctx, "TypeError");
    locale = JS_GetIntlDefaultLocale(ctx);
    assert(JS_IsException(locale));
    clear_error(ctx, "TypeError");
    check_global_intl_absent(ctx);
    eval_ok(ctx, "if ((1+2) !== 3) throw Error('ordinary engine behavior');");
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
#endif
    return 0;
}
