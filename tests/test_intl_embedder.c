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
#include "../src/quickjs/internal/allocator.h"
#ifdef CONFIG_INTL
#include "../src/intl/libintl.h"
#include "../src/intl/locale-data.h"
#endif

static void evaluate(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "intl-embedder",
                            JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(value)) {
        JSValue error = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, error);
        fprintf(stderr, "Intl embedder test: %s\n", message ? message : "exception");
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, error);
        abort();
    }
    JS_FreeValue(ctx, value);
}

static void check_global(JSContext *ctx, int present)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue intl = JS_GetPropertyStr(ctx, global, "Intl");
    assert(!JS_IsException(intl));
    assert(present ? JS_IsObject(intl) : JS_IsUndefined(intl));
    if (present) {
        assert(!JS_IsFunction(ctx, intl));
        assert(!JS_IsConstructor(ctx, intl));
    }
    JS_FreeValue(ctx, intl);
    JS_FreeValue(ctx, global);
}

#ifdef CONFIG_INTL
/* A foreign newTarget with a non-object prototype selects the raw realm's
   intrinsic Locale prototype without publishing that realm's global Intl. */
static JSValue raw_locale_target(JSContext *ctx, JSValueConst new_target,
                                int argc, JSValueConst *argv)
{
    (void)ctx;
    (void)new_target;
    (void)argc;
    (void)argv;
    assert(!"the newTarget callback must not execute");
    return JS_UNDEFINED;
}

static void check_blocked_publication(JSRuntime *rt, JSContext *normal,
                                      int nonextensible)
{
    JSContext *raw = JS_NewContextRaw(rt);
    JSValue global, normal_global, intl, constructor, target, argument;
    JSValue locale, prototype, cached_constructor;
    JSClassID class_id;
    JSAtom intl_atom;
    int attempt;

    assert(raw && !JS_AddIntrinsicBaseObjects(raw));
    check_global(raw, 0);
    global = JS_GetGlobalObject(raw);
    normal_global = JS_GetGlobalObject(normal);
    intl = JS_GetPropertyStr(normal, normal_global, "Intl");
    assert(JS_IsObject(intl));
    constructor = JS_GetPropertyStr(normal, intl, "Locale");
    assert(JS_IsConstructor(normal, constructor));
    target = JS_NewCFunction2(raw, raw_locale_target, "RawTarget", 0,
                             JS_CFUNC_constructor, 0);
    assert(JS_IsConstructor(raw, target));
    assert(JS_DefinePropertyValueStr(raw, target, "prototype",
                                    JS_NewInt32(raw, 0),
                                    JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE |
                                    JS_PROP_THROW) == 1);
    argument = JS_NewString(normal, "en");
    assert(!JS_IsException(argument));
    locale = JS_CallConstructor2(normal, constructor, target, 1, &argument);
    assert(JS_IsObject(locale));
    class_id = JS_GetClassID(locale);
    prototype = JS_GetClassProto(raw, class_id);
    assert(JS_IsObject(prototype));
    {
        JSValue actual = JS_GetPrototype(normal, locale);
        assert(JS_IsObject(actual));
        assert(JS_VALUE_GET_PTR(actual) == JS_VALUE_GET_PTR(prototype));
        JS_FreeValue(normal, actual);
    }
    cached_constructor = JS_GetPropertyStr(raw, prototype, "constructor");
    assert(JS_IsConstructor(raw, cached_constructor));
    check_global(raw, 0);
    intl_atom = JS_NewAtom(raw, "Intl");
    assert(intl_atom != JS_ATOM_NULL);
    if (nonextensible) {
        assert(JS_PreventExtensions(raw, global) == 1);
        assert(JS_IsExtensible(raw, global) == 0);
    } else {
        assert(JS_DefinePropertyValueStr(raw, global, "Intl", JS_UNDEFINED,
                                        JS_PROP_THROW) == 1);
    }

    for (attempt = 0; attempt < 2; attempt++) {
        JSPropertyDescriptor descriptor;
        JSValue error, name, current_prototype, current_constructor;
        const char *error_name;
        int own;

        assert(!JS_HasException(raw));
        assert(JS_AddIntrinsicIntl(raw) == -1);
        assert(JS_HasException(raw));
        error = JS_GetException(raw);
        name = JS_GetPropertyStr(raw, error, "name");
        error_name = JS_ToCString(raw, name);
        assert(error_name && !strcmp(error_name, "TypeError"));
        JS_FreeCString(raw, error_name);
        JS_FreeValue(raw, name);
        JS_FreeValue(raw, error);
        assert(!JS_HasException(raw));
        own = JS_GetOwnProperty(raw, &descriptor, global, intl_atom);
        assert(own == !nonextensible);
        if (own) {
            assert(JS_IsUndefined(descriptor.value));
            assert(!(descriptor.flags & (JS_PROP_WRITABLE |
                     JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE)));
            JS_FreeValue(raw, descriptor.value);
            JS_FreeValue(raw, descriptor.getter);
            JS_FreeValue(raw, descriptor.setter);
        }
        check_global(raw, 0);
        current_prototype = JS_GetClassProto(raw, class_id);
        assert(JS_IsObject(current_prototype));
        assert(JS_VALUE_GET_PTR(current_prototype) ==
               JS_VALUE_GET_PTR(prototype));
        current_constructor = JS_GetPropertyStr(raw, current_prototype,
                                                "constructor");
        assert(JS_IsConstructor(raw, current_constructor));
        assert(JS_VALUE_GET_PTR(current_constructor) ==
               JS_VALUE_GET_PTR(cached_constructor));
        JS_FreeValue(raw, current_constructor);
        JS_FreeValue(raw, current_prototype);
        JS_RunGC(rt);
    }

    /* Reusing the existing intrinsic remains possible after refused
       publication and collection; the global property stays unchanged. */
    JS_FreeValue(normal, locale);
    locale = JS_CallConstructor2(normal, constructor, target, 1, &argument);
    assert(JS_IsObject(locale));
    {
        JSValue actual = JS_GetPrototype(normal, locale);
        assert(JS_IsObject(actual));
        assert(JS_VALUE_GET_PTR(actual) == JS_VALUE_GET_PTR(prototype));
        JS_FreeValue(normal, actual);
    }
    check_global(raw, 0);
    JS_FreeAtom(raw, intl_atom);
    JS_FreeValue(raw, cached_constructor);
    JS_FreeValue(raw, prototype);
    JS_FreeValue(normal, locale);
    JS_FreeValue(normal, argument);
    JS_FreeValue(raw, target);
    JS_FreeValue(normal, constructor);
    JS_FreeValue(normal, intl);
    JS_FreeValue(normal, normal_global);
    JS_FreeValue(raw, global);
    JS_FreeContext(raw);
}

/* Identifier lookup and primary mapping serve different observable slots. */
static void check_time_zone_identifiers(void)
{
    static const struct {
        const char *input;
        const char *identifier;
        const char *primary;
    } cases[] = {
        { "utc", "UTC", "UTC" },
        { "eTc/gMt", "Etc/GMT", "UTC" },
        { "ETC/utc", "Etc/UTC", "UTC" },
        { "gmt", "GMT", "UTC" },
        { "aSiA/cAlCuTtA", "Asia/Calcutta", "Asia/Kolkata" },
        { "aSiA/kOlKaTa", "Asia/Kolkata", "Asia/Kolkata" },
        { "us/eastern", "US/Eastern", "America/New_York" }
    };
    size_t i, j;

    for (i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        const char *identifier = intl_iana_zone_name(cases[i].input,
                                                    strlen(cases[i].input));
        UChar primary[128];
        UErrorCode status = U_ZERO_ERROR;
        int32_t length;

        assert(identifier && !strcmp(identifier, cases[i].identifier));
        length = intl_iana_zone_primary(identifier, primary,
                                       sizeof(primary) / sizeof(*primary),
                                       &status);
        assert(U_SUCCESS(status));
        assert(length == (int32_t)strlen(cases[i].primary));
        for (j = 0; j < (size_t)length; j++)
            assert(primary[j] == (unsigned char)cases[i].primary[j]);
        assert(primary[length] == 0);
        /* The borrowed Identifier remains unchanged by primary lookup. */
        assert(!strcmp(identifier, cases[i].identifier));
    }
    assert(!intl_iana_zone_name("UTC\0junk", 8));
    assert(!intl_iana_zone_name("GMT+5", 5));
    assert(!intl_iana_zone_name("not-a-zone", 10));
}

static void check_backend(void)
{
    char offset[7];
    char private_tag[] = "en-x-u-private";
    char extension_tag[] = "en-u-ca-gregory-x-u-private";
    assert(!intl_remove_unicode_extension(private_tag));
    assert(!strcmp(private_tag, "en-x-u-private"));
    assert(intl_remove_unicode_extension(extension_tag));
    assert(!strcmp(extension_tag, "en-x-u-private"));
    assert(intl_backend_parse_offset_time_zone("GMT+05:30", 9, offset) == 1);
    assert(!strcmp(offset, "+05:30"));
    assert(intl_backend_parse_offset_time_zone("GMT-0", 5, offset) == 1);
    assert(!strcmp(offset, "+00:00"));
    assert(intl_backend_parse_offset_time_zone("GMT-0330", 8, offset) == 1);
    assert(!strcmp(offset, "-03:30"));
    assert(intl_backend_parse_offset_time_zone("Etc/GMT+5", 9, offset) == 0);
    assert(intl_backend_parse_offset_time_zone("GMT+24:00", 9, offset) == -1);
    /* SystemTimeZoneIdentifier returns canonical fixed offsets without GMT. */
    assert(intl_backend_parse_offset_time_zone("+05:30", 6, offset) == 1);
    assert(!strcmp(offset, "+05:30"));
    assert(intl_backend_parse_offset_time_zone("-03:30", 6, offset) == 1);
    assert(!strcmp(offset, "-03:30"));
    assert(intl_backend_parse_offset_time_zone("-00:00", 6, offset) == 1);
    assert(!strcmp(offset, "+00:00"));
    assert(intl_backend_parse_offset_time_zone("+24:00", 6, offset) == -1);
    assert(intl_backend_parse_offset_time_zone("+05:60", 6, offset) == -1);
    assert(intl_backend_parse_offset_time_zone("+05:30:00", 9, offset) == -1);
    assert(intl_backend_parse_offset_time_zone("+", 1, offset) == -1);
    assert(intl_backend_parse_offset_time_zone("+05", 3, offset) == -1);
    IntlBackendVersions versions;
    UErrorCode status = U_ZERO_ERROR;
    char icu[U_MAX_VERSION_STRING_LENGTH];
    char unicode[U_MAX_VERSION_STRING_LENGTH];
    char cldr[U_MAX_VERSION_STRING_LENGTH];
    intl_backend_initialize(&status);
    intl_backend_versions(&versions, &status);
    assert(U_SUCCESS(status));
    assert(versions.icu[0] > 78 ||
           (versions.icu[0] == 78 && versions.icu[1] >= 3));
    assert(versions.unicode[0] >= 17);
    assert(versions.cldr[0] >= 48);
    assert(versions.tzdata && *versions.tzdata);
    u_versionToString(versions.icu, icu);
    u_versionToString(versions.unicode, unicode);
    u_versionToString(versions.cldr, cldr);
    printf("Intl backend ICU=%s Unicode=%s CLDR=%s tzdata=%s\n",
           icu, unicode, cldr, versions.tzdata);
}

#include "intl-allocation-probe.h"

/* Fail each engine allocation during publication, then retry the same realm.
   ICU's process-global allocator is deliberately not replaced by QuickJS. */
static void check_initialization_failures(void)
{
    size_t failure;
    for (failure = 1; failure < 4096; failure++) {
        AllocationProbe probe = { 0 };
        JSRuntime *rt = JS_NewRuntime2(&probe_functions, &probe);
        JSContext *ctx;
        int result;
        assert(rt);
        ctx = JS_NewContextRaw(rt);
        assert(ctx && !JS_AddIntrinsicBaseObjects(ctx));
        check_global(ctx, 0);
        probe.failure_at = failure;
        result = JS_AddIntrinsicIntl(ctx);
        probe.failure_at = 0;
        if (result < 0) {
            JSValue error = JS_GetException(ctx);
            assert(probe.failed);
            JS_FreeValue(ctx, error);
            check_global(ctx, 0);
            assert(!JS_AddIntrinsicIntl(ctx));
        }
        check_global(ctx, 1);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        assert(!probe.live);
        if (!probe.failed)
            return;
    }
    assert(!"Intl initialization exceeds fault-injection bound");
}

#endif

int main(void)
{
    JSRuntime *rt = JS_NewRuntime();
    JSContext *normal, *raw;
    assert(rt);
    normal = JS_NewContext(rt);
    raw = JS_NewContextRaw(rt);
    assert(normal && raw);
    check_global(raw, 0);
    assert(!JS_AddIntrinsicBaseObjects(raw));
    assert(!JS_AddIntrinsicEval(raw));
#ifdef CONFIG_INTL
    check_backend();
    check_time_zone_identifiers();
    check_global(normal, 1);
    check_blocked_publication(rt, normal, 0);
    check_blocked_publication(rt, normal, 1);
    assert(!JS_AddIntrinsicIntl(raw));
    assert(!JS_AddIntrinsicIntl(raw));
    evaluate(raw,
        "let desc=Object.getOwnPropertyDescriptor(globalThis,'Intl');"
        "if(!desc.writable||desc.enumerable||!desc.configurable) throw Error('descriptor');"
        "if(Object.getPrototypeOf(Intl)!==Object.prototype) throw Error('prototype');"
        "if(Object.prototype.toString.call(Intl)!=='[object Intl]') throw Error('tag');"
        "if(Intl.getCanonicalLocales('EN-us')[0]!=='en-US') throw Error('canonical');"
        "let saved=Intl.Locale; globalThis.Intl={};"
        "if(new saved('en').baseName!=='en') throw Error('root');");
#else
    check_global(normal, 0);
    assert(!JS_AddIntrinsicIntl(normal));
    assert(!JS_AddIntrinsicIntl(raw));
    check_global(normal, 0);
    check_global(raw, 0);
    evaluate(normal, "if ('a'.localeCompare('b')>=0) throw Error('fallback');");
#endif
    JS_FreeContext(raw);
    JS_FreeContext(normal);
    JS_FreeRuntime(rt);
#ifdef CONFIG_INTL
    check_initialization_failures();
#endif
    return 0;
}
