/* QuickJS Intl binding ownership.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QUICKJS_BUILTINS_INTL_H
#define QUICKJS_BUILTINS_INTL_H
#include "../internal/base.h"
#ifdef CONFIG_INTL
#include "../../intl/locale/locale.h"

typedef struct JSIntlLocaleList {
    char **items;
    size_t count, capacity;
} JSIntlLocaleList;

QJSIntlLocaleService *js_intl_locale_service(JSContext *ctx);
const char *js_intl_default_locale(JSContext *ctx);
QJSIntlAllocator js_intl_allocator(JSRuntime *rt);
int js_intl_ensure_intrinsics(JSContext *ctx);
int js_intl_init_locale(JSContext *ctx, JSValueConst namespace_object);
const char *js_intl_locale_tag(JSValueConst value);
int js_intl_error(JSContext *ctx, QJSIntlStatus status, const char *operation);
int js_intl_canonicalize_locale_list(JSContext *ctx, JSValueConst locales,
                                     JSIntlLocaleList *out);
void js_intl_locale_list_clear(JSRuntime *rt, JSIntlLocaleList *list);
JSValue js_intl_get_canonical_locales(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv);
#endif
#endif
