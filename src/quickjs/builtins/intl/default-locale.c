/* One runtime default locale shared by Intl services and realms.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../../internal/runtime.h"
#include "../intl.h"
#ifdef CONFIG_INTL
#include "../../../intl/locale/system.h"

static QJSIntlStatus resolve_default(JSRuntime *rt, QJSIntlLocaleService *service,
                                     const char *input, char **out)
{
    QJSIntlBytes selected;
    QJSIntlStatus status;
    char *canonical;
    *out = NULL;
    status = qjs_intl_locale_canonicalize(service, (QJSIntlBytes){input, strlen(input)},
                                          &canonical);
    if (status)
        return status;
    status = qjs_intl_locale_profile_match(
        service, (QJSIntlBytes){canonical, strlen(canonical)}, &selected);
    qjs_intl_locale_result_free(service, canonical);
    if (status)
        return status;
    *out = js_malloc_rt(rt, selected.length + 1);
    if (!*out)
        return QJS_INTL_NO_MEMORY;
    memcpy(*out, selected.data, selected.length);
    (*out)[selected.length] = 0;
    return QJS_INTL_OK;
}

const char *js_intl_default_locale(JSContext *ctx)
{
    JSRuntime *rt = ctx->rt;
    if (!rt->intl_default_locale) {
        QJSIntlLocaleService *service = js_intl_locale_service(ctx);
        QJSIntlStatus status;
        char host[128], *locale;
        const char *input;
        if (!service)
            return NULL;
        input = qjs_intl_locale_system_default(host, sizeof(host)) == QJS_INTL_OK
                    ? host
                    : "en";
        status = resolve_default(rt, service, input, &locale);
        if (status == QJS_INTL_INVALID_ARGUMENT || status == QJS_INTL_UNSUPPORTED)
            status = resolve_default(rt, service, "en", &locale);
        if (js_intl_error(ctx, status, "default locale"))
            return NULL;
        rt->intl_default_locale = locale;
    }
    return rt->intl_default_locale;
}

int JS_SetIntlDefaultLocale(JSContext *ctx, const char *locale)
{
    JSRuntime *rt = ctx->rt;
    char *copy = NULL, *old;
    if (locale) {
        QJSIntlLocaleService *service = js_intl_locale_service(ctx);
        QJSIntlStatus status;
        if (!service)
            return -1;
        status = resolve_default(rt, service, locale, &copy);
        if (js_intl_error(ctx, status, "default locale"))
            return -1;
    }
    old = rt->intl_default_locale;
    rt->intl_default_locale = copy;
    js_free_rt(rt, old);
    return 0;
}

JSValue JS_GetIntlDefaultLocale(JSContext *ctx)
{
    const char *locale = js_intl_default_locale(ctx);
    return locale ? JS_NewString(ctx, locale) : JS_EXCEPTION;
}
#else
int JS_SetIntlDefaultLocale(JSContext *ctx, const char *locale)
{
    (void)locale;
    JS_ThrowTypeError(ctx, "Intl support is disabled in this build");
    return -1;
}
JSValue JS_GetIntlDefaultLocale(JSContext *ctx)
{
    return JS_ThrowTypeError(ctx, "Intl support is disabled in this build");
}
#endif
