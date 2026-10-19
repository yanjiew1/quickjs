/* Intl namespace and runtime-owned locale backend lifetime.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../intl.h"
#include "../../internal/runtime.h"
#include "../../internal/atom.h"

#ifdef CONFIG_INTL
static void *intl_malloc(void *opaque, size_t size)
{
    return js_malloc_rt(opaque, size);
}

static void *intl_realloc(void *opaque, void *ptr, size_t size)
{
    return js_realloc_rt(opaque, ptr, size);
}

static void intl_free(void *opaque, void *ptr)
{
    js_free_rt(opaque, ptr);
}

int js_intl_error(JSContext *ctx, QJSIntlStatus status, const char *operation)
{
    switch (status) {
    case QJS_INTL_OK:
        return 0;
    case QJS_INTL_NO_MEMORY:
    case QJS_INTL_OVERFLOW:
        JS_ThrowOutOfMemory(ctx);
        break;
    case QJS_INTL_INVALID_ARGUMENT:
    case QJS_INTL_UNSUPPORTED:
        JS_ThrowRangeError(ctx, "invalid %s", operation);
        break;
    default:
        JS_ThrowInternalError(ctx, "Intl %s data error", operation);
        break;
    }
    return -1;
}

QJSIntlLocaleService *js_intl_locale_service(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;

    if (!rt->intl_locale) {
        QJSIntlAllocator allocator = {
            rt,
            intl_malloc,
            intl_realloc,
            intl_free,
        };
        QJSIntlLocaleService *service;
        QJSIntlStatus status = qjs_intl_locale_open(&allocator, &service);

        if (js_intl_error(ctx, status, "locale initialization"))
            return NULL;
        rt->intl_locale = service;
    }
    return rt->intl_locale;
}

static const JSCFunctionListEntry intl_functions[] = {
    JS_CFUNC_DEF("getCanonicalLocales", 1, js_intl_get_canonical_locales),
};

int JS_AddIntrinsicIntl(JSContext *ctx)
{
    JSValue object, tag;

    if (JS_IsObject(ctx->intl_intrinsics))
        return 0;
    object = JS_NewObject(ctx);
    if (JS_IsException(object))
        return -1;
    if (JS_SetPropertyFunctionList(ctx, object, intl_functions,
                                   countof(intl_functions)) < 0)
        goto fail;
    tag = JS_NewString(ctx, "Intl");
    if (JS_IsException(tag))
        goto fail;
    if (JS_DefinePropertyValue(ctx, object, JS_ATOM_Symbol_toStringTag, tag,
                               JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0 ||
        JS_DefinePropertyValueStr(
            ctx, ctx->global_obj, "Intl", JS_DupValue(ctx, object),
            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE | JS_PROP_THROW) < 0) {
        goto fail;
    }
    ctx->intl_intrinsics = object;
    return 0;
fail:
    JS_FreeValue(ctx, object);
    return -1;
}
#else
int JS_AddIntrinsicIntl(JSContext *ctx)
{
    JS_ThrowTypeError(ctx, "Intl support is disabled in this build");
    return -1;
}
#endif
