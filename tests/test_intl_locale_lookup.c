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

int main(void)
{
    const char *prior = uloc_getDefault();
    const char *private_default = "en-US-x-private-u-ca-gregory";
    size_t length = strlen(prior);
    char *saved_default = malloc(length + 1);
    char backend_default[128];
    int32_t parsed = 0;
    UErrorCode status = U_ZERO_ERROR;
    JSRuntime *rt;
    JSContext *ctx;

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
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    status = U_ZERO_ERROR;
    uloc_setDefault(saved_default, &status);
    assert(U_SUCCESS(status));
    free(saved_default);
    return 0;
}
#else
int main(void)
{
    return 0;
}
#endif
