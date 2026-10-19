/* Exercise internal locale-key resolution before individual API registration.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../include/quickjs-libc.h"
#include "../src/quickjs/builtins/intl.h"
#include "oom-allocator.h"
#include "pool-padding.h"
#include <assert.h>
#include <string.h>

static QJSIntlStatus supported(void *opaque, QJSIntlBytes value, int *out)
{
    const char *key = opaque;
    const char *const number[] = {"latn", "arab", "mathbold"};
    const char *const calendar[] = {"gregory", "iso8601", "islamic-civil"};
    const char *const boolean[] = {"true", "false"};
    const char *const *values = !strcmp(key, "nu") ? number
                               : !strcmp(key, "ca") ? calendar : boolean;
    size_t count = !strcmp(key, "kn") ? 2 : 3, i;
    *out = 0;
    if (value.length == 5 && !memcmp(value.data, "error", 5))
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < count; i++)
        if (value.length == strlen(values[i]) && !memcmp(value.data, values[i], value.length))
            *out = 1;
    return QJS_INTL_OK;
}

static JSValue read_options(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    static const QJSIntlBytes available[] = {{"en", 2}, {"en-US", 5}};
    const char *key = "nu", *property = "numberingSystem", *fallback = "latn";
    JSIntlLocaleList requested = {0};
    JSIntlResolvedKeyword resolved = {0};
    QJSIntlLocaleMatch match;
    QJSIntlBytes option;
    const char *owned = NULL;
    JSValue options = JS_UNDEFINED, result = JS_EXCEPTION;
    (void)this_val;
    if (argc > 2) {
        if (JS_IsString(argv[2])) {
            const char *text = JS_ToCString(ctx, argv[2]);
            if (!text)
                return JS_EXCEPTION;
            if (!strcmp(text, "ca")) {
                key = "ca";
                property = "calendar";
                fallback = "gregory";
            } else if (!strcmp(text, "kn")) {
                key = "kn";
                property = "numeric";
                fallback = "false";
            } else {
                assert(!strcmp(text, "nu"));
            }
            JS_FreeCString(ctx, text);
        }
    }
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED, &requested))
        goto done;
    options = js_intl_options_object(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, TRUE);
    if (JS_IsException(options) || js_intl_locale_matcher(ctx, options) < 0 ||
        js_intl_unicode_type_option(ctx, options, property, &option, &owned) ||
        js_intl_resolve_locale_match(ctx, available, 2, &requested, &match) ||
        js_intl_resolve_locale_key(ctx, available[match.available_index], match.extension,
                                    key, (QJSIntlBytes){fallback, strlen(fallback)}, option,
                                    supported, (void *)key, &resolved))
        goto done;
    result = JS_NewObject(ctx);
    if (JS_IsException(result))
        goto done;
    if (JS_SetPropertyStr(ctx, result, "locale", JS_NewString(ctx, resolved.locale)) < 0 ||
        JS_SetPropertyStr(ctx, result, "value", JS_NewString(ctx, resolved.value)) < 0 ||
        JS_SetPropertyStr(ctx, result, "requestIndex",
                          JS_NewInt32(ctx, match.requested_index == SIZE_MAX
                                               ? -1 : (int)match.requested_index)) < 0) {
        JS_FreeValue(ctx, result);
        result = JS_EXCEPTION;
    }
done:
    js_free(ctx, resolved.locale);
    js_free(ctx, resolved.value);
    JS_FreeCString(ctx, owned);
    JS_FreeValue(ctx, options);
    js_intl_locale_list_clear(JS_GetRuntime(ctx), &requested);
    return result;
}

static void test_allocation_failures(int operation)
{
    static const char *const extensions[] = {"", "-u-nu-arab", "-u-nu-arab",
                                              "-u-ca-islamic-civil"};
    static const char *const options[] = {NULL, NULL, "MATHBOLD", "islamicc"};
    static const char *const locales[] = {"en", "en-u-nu-arab", "en", "en-u-ca-islamic-civil"};
    static const char *const values[] = {"latn", "arab", "mathbold", "islamic-civil"};
    const char *key = operation == 3 ? "ca" : "nu";
    const char *fallback = operation == 3 ? "gregory" : "latn";
    QJSIntlBytes extension = {extensions[operation], strlen(extensions[operation])};
    QJSIntlBytes option = {options[operation], options[operation] ? strlen(options[operation]) : 0};
    size_t step, calls = 0, failures = 0;
    for (step = 0; step <= calls; step++) {
        OOMTestAllocator allocator;
        JSRuntime *rt = oom_test_runtime(&allocator);
        JSContext *ctx = JS_NewContextRaw(rt);
        JSIntlResolvedKeyword out;
        void *padding;
        int error;
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx) && js_intl_locale_service(ctx));
        padding = test_pool_padding(rt);
        memset(&out, 0xff, sizeof(out));
        oom_test_begin(&allocator, step);
        error = js_intl_resolve_locale_key(ctx, (QJSIntlBytes){"en", 2}, extension, key,
                                             (QJSIntlBytes){fallback, strlen(fallback)}, option,
                                             supported, (void *)key, &out);
        oom_test_end(&allocator);
        test_pool_padding_free(rt, padding);
        if (!step) {
            calls = allocator.calls;
            assert(calls && calls < 128 && !error);
        } else {
            assert(allocator.failures == 1);
        }
        if (error) {
            failures++;
            assert(!out.locale && !out.value);
            oom_test_check_exception(ctx);
            assert(!js_intl_resolve_locale_key(ctx, (QJSIntlBytes){"en", 2}, extension, key,
                                                 (QJSIntlBytes){fallback, strlen(fallback)}, option,
                                                 supported, (void *)key, &out));
        }
        assert(!JS_HasException(ctx));
        assert(!strcmp(out.locale, locales[operation]) && !strcmp(out.value, values[operation]));
        js_free(ctx, out.locale);
        js_free(ctx, out.value);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!allocator.live);
    }
    assert(failures);
}

int main(int argc, char **argv)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx;
    JSValue global, result;
    size_t length;
    uint8_t *script;
    assert(argc == 2 && rt);
    ctx = JS_NewContext(rt);
    assert(ctx && !JS_SetIntlDefaultLocale(ctx, "en-US"));
    global = JS_GetGlobalObject(ctx);
    assert(JS_SetPropertyStr(ctx, global, "readLocaleOptions",
                             JS_NewCFunction(ctx, read_options, "readLocaleOptions", 2)) >= 0);
    JS_FreeValue(ctx, global);
    script = js_load_file(ctx, &length, argv[1]);
    assert(script);
    result = JS_Eval(ctx, (const char *)script, length, argv[1], JS_EVAL_TYPE_GLOBAL);
    js_free(ctx, script);
    if (JS_IsException(result))
        js_std_dump_error(ctx);
    assert(!JS_IsException(result));
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    for (int operation = 0; operation < 4; operation++)
        test_allocation_failures(operation);
    return 0;
}
