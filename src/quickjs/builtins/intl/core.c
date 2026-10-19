/* Intl namespace and runtime-owned locale backend lifetime.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../intl.h"
#include "../../internal/runtime.h"
#include "../../internal/atom.h"
#include "../../internal/object.h"
#include "../../internal/function.h"

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

QJSIntlAllocator js_intl_allocator(JSRuntime *rt)
{
    return (QJSIntlAllocator){rt, intl_malloc, intl_realloc, intl_free};
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
        QJSIntlAllocator allocator = js_intl_allocator(rt);
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

JSValue js_intl_create_from_constructor(JSContext *ctx, JSValueConst new_target,
                                        JSClassID class_id)
{
    JSValue prototype, result;
    prototype = JS_GetProperty(ctx, new_target, JS_ATOM_prototype);
    if (JS_IsException(prototype))
        return prototype;
    if (!JS_IsObject(prototype)) {
        JSContext *realm;
        JS_FreeValue(ctx, prototype);
        realm = JS_GetFunctionRealm(ctx, new_target);
        if (!realm || js_intl_ensure_intrinsics(realm))
            return JS_EXCEPTION;
        prototype = JS_DupValue(ctx, realm->class_proto[class_id]);
    }
    result = JS_NewObjectProtoClass(ctx, prototype, class_id);
    JS_FreeValue(ctx, prototype);
    return result;
}

static void clear_intrinsics(JSContext *ctx)
{
    JS_FreeValue(ctx, ctx->intl_intrinsics);
    ctx->intl_intrinsics = JS_UNDEFINED;
    JS_FreeValue(ctx, ctx->intl_locale_ctor);
    ctx->intl_locale_ctor = JS_UNDEFINED;
    ctx->intl_published = FALSE;
    JS_SetClassProto(ctx, JS_CLASS_INTL_LOCALE, JS_NULL);
    JS_SetClassProto(ctx, JS_CLASS_INTL_LIST_FORMAT, JS_NULL);
}

int js_intl_ensure_intrinsics(JSContext *ctx)
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
        js_intl_init_locale(ctx, object) || js_intl_init_list_format(ctx, object)) {
        goto fail;
    }
    ctx->intl_intrinsics = object;
    return 0;
fail:
    JS_FreeValue(ctx, object);
    clear_intrinsics(ctx);
    return -1;
}

int JS_AddIntrinsicIntl(JSContext *ctx)
{
    BOOL had_intrinsics;

    if (ctx->intl_published)
        return 0;
    had_intrinsics = JS_IsObject(ctx->intl_intrinsics);
    if (js_intl_ensure_intrinsics(ctx))
        return -1;
    if (JS_DefinePropertyValueStr(ctx, ctx->global_obj, "Intl",
                                  JS_DupValue(ctx, ctx->intl_intrinsics),
                                  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE |
                                  JS_PROP_THROW) < 0) {
        if (!had_intrinsics)
            clear_intrinsics(ctx);
        return -1;
    }
    ctx->intl_published = TRUE;
    return 0;
}
#else
int JS_AddIntrinsicIntl(JSContext *ctx)
{
    JS_ThrowTypeError(ctx, "Intl support is disabled in this build");
    return -1;
}
#endif
