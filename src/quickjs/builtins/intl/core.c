/*
 * QuickJS native internationalization support
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
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
#include "intl-internal.h"

#ifndef CONFIG_INTL
/* The additive embedding entry point is a documented disabled-build no-op. */
int JS_AddIntrinsicIntl(JSContext *ctx)
{
    (void)ctx;
    return 0;
}
#else
#include "../../../intl/libintl.h"

#define JS_INTL_CLASS_COUNT (JS_CLASS_INTL_END - JS_CLASS_INTL_LOCALE)

struct JSIntlContext {
    char *default_locale;
    char *default_time_zone;
    JSValue fallback_symbol;
    JSValue constructors[JS_INTL_CLASS_COUNT];
    BOOL installed;
};

/* Snapshot host defaults per realm; locale data and process globals are owned
   by ICU. The engine never mutates ICU defaults or calls u_cleanup. */
static char *intl_default_locale_snapshot(JSContext *ctx)
{
    char *tag;
    tag = js_intl_locale_from_icu(ctx, intl_backend_default_locale());
    if (!tag)
        return NULL;
    intl_remove_unicode_extension(tag);
    return tag;
}

static char *intl_default_time_zone_snapshot(JSContext *ctx)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t required, i;
    UChar *zone;
    char *ascii, *result;
    char offset[7];
    int parsed;

    required = intl_backend_default_time_zone(NULL, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR &&
        js_intl_icu_error(ctx, status, "default timezone preflight"))
        return NULL;
    zone = js_intl_alloc_uchar(ctx, required);
    if (!zone)
        return NULL;
    status = U_ZERO_ERROR;
    required = intl_backend_default_time_zone(zone, required + 1, &status);
    if (js_intl_icu_error(ctx, status, "default timezone")) {
        js_free(ctx, zone);
        return NULL;
    }
    ascii = js_intl_alloc_char(ctx, required);
    if (!ascii) {
        js_free(ctx, zone);
        return NULL;
    }
    for (i = 0; i < required; i++) {
        if (zone[i] > 0x7f || !zone[i]) {
            js_free(ctx, zone);
            js_free(ctx, ascii);
            JS_ThrowInternalError(ctx, "ICU default timezone is not an identifier");
            return NULL;
        }
        ascii[i] = zone[i];
    }
    ascii[required] = 0;
    js_free(ctx, zone);
    /* ICU uses this sentinel when the host's timezone cannot be identified. */
    parsed = intl_backend_parse_offset_time_zone(ascii, required, offset);
    if (!strcmp(ascii, "Etc/Unknown"))
        result = js_intl_strdup(ctx, "UTC");
    else if (parsed > 0)
        result = js_intl_strdup(ctx, offset);
    else if (parsed < 0) {
        JS_ThrowInternalError(ctx, "ICU default timezone has an invalid offset");
        result = NULL;
    } else
        result = js_intl_canonicalize_time_zone(ctx, ascii, required);
    js_free(ctx, ascii);
    return result;
}

int js_intl_ensure_context(JSContext *ctx)
{
    struct JSIntlContext *state;
    UErrorCode status = U_ZERO_ERROR;
    int i;

    if (ctx->intl)
        return 0;
    intl_backend_initialize(&status);
    if (js_intl_icu_error(ctx, status, "data initialization"))
        return -1;
    state = js_mallocz(ctx, sizeof(*state));
    if (!state)
        return -1;
    state->fallback_symbol = JS_UNDEFINED;
    for (i = 0; i < JS_INTL_CLASS_COUNT; i++)
        state->constructors[i] = JS_UNDEFINED;
    state->default_locale = intl_default_locale_snapshot(ctx);
    if (!state->default_locale)
        goto fail;
    state->default_time_zone = intl_default_time_zone_snapshot(ctx);
    if (!state->default_time_zone)
        goto fail;
    state->fallback_symbol = JS_NewSymbol(ctx, "IntlLegacyConstructedSymbol", FALSE);
    if (JS_IsException(state->fallback_symbol))
        goto fail;
    ctx->intl = state;
    return 0;
fail:
    js_free(ctx, state->default_locale);
    js_free(ctx, state->default_time_zone);
    JS_FreeValue(ctx, state->fallback_symbol);
    js_free(ctx, state);
    return -1;
}

const char *js_intl_default_locale(JSContext *ctx)
{
    assert(ctx->intl);
    return ctx->intl->default_locale;
}

const char *js_intl_default_time_zone(JSContext *ctx)
{
    assert(ctx->intl);
    return ctx->intl->default_time_zone;
}

JSValueConst js_intl_constructor(JSContext *ctx, JSClassID class_id)
{
    assert(class_id >= JS_CLASS_INTL_LOCALE && class_id < JS_CLASS_INTL_END);
    if (!ctx->intl)
        return JS_UNDEFINED;
    return ctx->intl->constructors[class_id - JS_CLASS_INTL_LOCALE];
}

JSValueConst js_intl_fallback_symbol(JSContext *ctx)
{
    assert(ctx->intl);
    return ctx->intl->fallback_symbol;
}

void js_intl_context_mark(JSRuntime *rt, JSContext *ctx,
                          JS_MarkFunc *mark_func)
{
    int i;
    if (!ctx->intl)
        return;
    JS_MarkValue(rt, ctx->intl->fallback_symbol, mark_func);
    for (i = 0; i < JS_INTL_CLASS_COUNT; i++)
        JS_MarkValue(rt, ctx->intl->constructors[i], mark_func);
}

void js_intl_context_free(JSContext *ctx)
{
    struct JSIntlContext *state = ctx->intl;
    int i;
    if (!state)
        return;
    ctx->intl = NULL;
    JS_FreeValue(ctx, state->fallback_symbol);
    for (i = 0; i < JS_INTL_CLASS_COUNT; i++)
        JS_FreeValue(ctx, state->constructors[i]);
    js_free(ctx, state->default_locale);
    js_free(ctx, state->default_time_zone);
    js_free(ctx, state);
}

void js_intl_context_memory_usage(JSContext *ctx, JSMemoryUsage *usage)
{
    struct JSIntlContext *state = ctx->intl;
    if (!state)
        return;
    usage->memory_used_count += 3;
    usage->memory_used_size += sizeof(*state) + strlen(state->default_locale) +
        strlen(state->default_time_zone) + 2;
}

int js_intl_register_class(JSContext *ctx, JSClassID class_id,
                           const JSClassDef *class_def)
{
    if (!JS_IsRegisteredClass(ctx->rt, class_id) &&
        JS_NewClass(ctx->rt, class_id, class_def)) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    return 0;
}

static int intl_ensure_class(JSContext *ctx, JSClassID class_id);

JSValue js_intl_new_object(JSContext *ctx, JSValueConst new_target,
                           JSClassID class_id)
{
    JSValue prototype, object;
    JSContext *realm;
    if (JS_IsUndefined(new_target)) {
        prototype = JS_DupValue(ctx, ctx->class_proto[class_id]);
    } else {
        /* GetPrototypeFromConstructor performs one observable prototype Get. */
        prototype = JS_GetProperty(ctx, new_target, JS_ATOM_prototype);
        if (JS_IsException(prototype))
            return prototype;
        if (!JS_IsObject(prototype)) {
            JS_FreeValue(ctx, prototype);
            realm = JS_GetFunctionRealm(ctx, new_target);
            if (!realm || intl_ensure_class(realm, class_id))
                return JS_EXCEPTION;
            prototype = JS_DupValue(ctx, realm->class_proto[class_id]);
        }
    }
    object = JS_NewObjectProtoClass(ctx, prototype, class_id);
    JS_FreeValue(ctx, prototype);
    return object;
}

int js_intl_init_constructor(JSContext *ctx, JSValueConst intl,
                            JSClassID class_id, const JSClassDef *class_def,
                            const char *name, JSCFunction *constructor,
                            int length, JSCFunctionEnum cproto,
                            const JSCFunctionListEntry *constructor_functions,
                            int constructor_function_count,
                            const JSCFunctionListEntry *prototype_functions,
                            int prototype_function_count)
{
    JSValue proto = JS_UNDEFINED, ctor = JS_UNDEFINED, old_proto;
    JSValue *slot;
    int result = -1;

    assert(class_id >= JS_CLASS_INTL_LOCALE && class_id < JS_CLASS_INTL_END);
    if (js_intl_ensure_context(ctx) ||
        js_intl_register_class(ctx, class_id, class_def))
        return -1;
    slot = &ctx->intl->constructors[class_id - JS_CLASS_INTL_LOCALE];
    if (!JS_IsUndefined(*slot))
        return JS_DefinePropertyValueStr(ctx, intl, name,
                                         JS_DupValue(ctx, *slot),
                                         JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE)
                < 0 ? -1 : 0;
    proto = JS_NewObject(ctx);
    if (JS_IsException(proto))
        goto done;
    if (JS_SetPropertyFunctionList(ctx, proto, prototype_functions,
                                   prototype_function_count))
        goto done;
    ctor = JS_NewCFunction2(ctx, constructor, name, length, cproto, 0);
    if (JS_IsException(ctor))
        goto done;
    if (JS_SetPropertyFunctionList(ctx, ctor, constructor_functions,
                                   constructor_function_count) ||
        JS_SetConstructor2(ctx, ctor, proto, 0,
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE))
        goto done;
    if (JS_DefinePropertyValueStr(ctx, intl, name, JS_DupValue(ctx, ctor),
                                  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE) < 0)
        goto done;
    old_proto = ctx->class_proto[class_id];
    ctx->class_proto[class_id] = JS_DupValue(ctx, proto);
    JS_FreeValue(ctx, old_proto);
    *slot = JS_DupValue(ctx, ctor);
    result = 0;
done:
    JS_FreeValue(ctx, proto);
    JS_FreeValue(ctx, ctor);
    return result;
}

typedef struct JSIntlModule {
    int service; /* Locale is not a service constructor; its selector is -1. */
    JSClassID class_id;
    int (*initialize)(JSContext *ctx, JSValueConst intl);
} JSIntlModule;

static const JSIntlModule intl_modules[] = {
#define JS_INTL_MODULE(service, class_id, initialize) { service, class_id, initialize },
#include "intl-module-defs.h"
#undef JS_INTL_MODULE
};

static int intl_ensure_class(JSContext *ctx, JSClassID class_id)
{
    JSValue intl;
    int result, i;
    if (js_intl_ensure_context(ctx))
        return -1;
    for (i = 0; i < countof(intl_modules); i++) {
        if (intl_modules[i].class_id != class_id)
            continue;
        if (!JS_IsUndefined(js_intl_constructor(ctx, class_id)))
            return 0;
        intl = JS_NewObject(ctx);
        if (JS_IsException(intl))
            return -1;
        result = intl_modules[i].initialize(ctx, intl);
        JS_FreeValue(ctx, intl);
        return result;
    }
    JS_ThrowInternalError(ctx, "Intl class is not in this experimental build");
    return -1;
}

int js_intl_ensure_service(JSContext *ctx, JSIntlService service)
{
    JSValue intl;
    int result, i;
    if (js_intl_ensure_context(ctx))
        return -1;
    if (service == JS_INTL_COLLATOR_SEARCH)
        service = JS_INTL_COLLATOR;
    for (i = 0; i < countof(intl_modules); i++) {
        if (intl_modules[i].service != service)
            continue;
        if (!JS_IsUndefined(js_intl_constructor(ctx, intl_modules[i].class_id)))
            return 0;
        intl = JS_NewObject(ctx);
        if (JS_IsException(intl))
            return -1;
        result = intl_modules[i].initialize(ctx, intl);
        JS_FreeValue(ctx, intl);
        return result;
    }
    JS_ThrowInternalError(ctx, "Intl service is not in this experimental build");
    return -1;
}

static const JSCFunctionListEntry intl_functions[] = {
    JS_CFUNC_DEF("getCanonicalLocales", 1, js_intl_get_canonical_locales),
    JS_CFUNC_DEF("supportedValuesOf", 1, js_intl_supported_values_of),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl", JS_PROP_CONFIGURABLE),
};

int JS_AddIntrinsicIntl(JSContext *ctx)
{
    JSValue intl;
    int i;
    if (ctx->intl && ctx->intl->installed)
        return 0;
    if (js_intl_ensure_context(ctx))
        return -1;
    intl = JS_NewObject(ctx);
    if (JS_IsException(intl))
        return -1;
    if (JS_SetPropertyFunctionList(ctx, intl, intl_functions,
                                   countof(intl_functions))) {
        JS_FreeValue(ctx, intl);
        return -1;
    }
    for (i = 0; i < countof(intl_modules); i++) {
        if (intl_modules[i].initialize(ctx, intl)) {
            JS_FreeValue(ctx, intl);
            return -1;
        }
    }
    if (JS_DefinePropertyValueStr(ctx, ctx->global_obj, "Intl", intl,
                                  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE |
                                  JS_PROP_THROW) < 0)
        return -1;
    ctx->intl->installed = TRUE;
    return 0;
}
#endif
