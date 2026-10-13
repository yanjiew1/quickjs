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
/* Native C service helpers; ECMA402 7ae78cf (2026-10-06).
 * Original implementation, no QuickJS NG source copied. */
#ifndef QUICKJS_INTL_SERVICE_COMMON_H
#define QUICKJS_INTL_SERVICE_COMMON_H
#include "intl-internal.h"
#ifdef CONFIG_INTL
static const char *const js_intl_matchers[] = { "lookup", "best fit" };
static const char *const js_intl_styles[] = { "long", "short", "narrow" };
/* GetOptionsObject by default; PR/RTF coerce, DN requires options. */
static inline int js_intl_service_options(JSContext *ctx, JSValueConst locales,
    JSValueConst input, int mode, JSIntlLocaleList *requested,
    JSValue *options, int *matcher)
{
    if (js_intl_canonicalize_locale_list(ctx, locales, requested) < 0)
        return -1;
    if (mode == 2 && JS_IsUndefined(input)) {
        JS_ThrowTypeError(ctx, "Intl.DisplayNames requires options");
        return -1;
    }
    *options = mode == 1 ? js_intl_coerce_options(ctx, input)
                        : js_intl_get_options(ctx, input);
    if (JS_IsException(*options))
        return -1;
    return js_intl_get_string_option(ctx, *options, "localeMatcher",
        js_intl_matchers, countof(js_intl_matchers), 1, matcher);
}
static inline int js_intl_ascii_alpha(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static inline int js_intl_ascii_digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}
static inline int js_intl_unicode_type(const char *s)
{
    int n = 0;
    for (;; s++) {
        if (*s == '-' || !*s) {
            if (n < 3 || n > 8) return 0;
            if (!*s) return 1;
            n = 0;
        } else {
            if (!js_intl_ascii_alpha(*s) && !js_intl_ascii_digit(*s))
                return 0;
            n++;
        }
    }
}
#endif
#endif
