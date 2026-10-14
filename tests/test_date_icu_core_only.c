/* ICU Date backend with both JavaScript frontends disabled.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 * This is an isolated process test. Production never changes ICU defaults. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unicode/ucal.h>
#include "quickjs.h"

#if !defined(CONFIG_ICU) || defined(CONFIG_INTL) || defined(CONFIG_TEMPORAL)
#error "test_date_icu_core_only requires ICU and no Intl or Temporal frontend"
#endif

static int eval_ok(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "date-icu-core-only.js",
                            JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(value)) {
        JSValue error = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, error);
        fprintf(stderr, "ICU core-only Date: %s\n", message ? message : "exception");
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, error);
        return -1;
    }
    JS_FreeValue(ctx, value);
    return JS_HasException(ctx) ? -1 : 0;
}

static int clear_type_error(JSContext *ctx)
{
    JSValue error, name;
    const char *text;
    int result;
    if (!JS_HasException(ctx))
        return -1;
    error = JS_GetException(ctx);
    name = JS_GetPropertyStr(ctx, error, "name");
    text = JS_IsException(name) ? NULL : JS_ToCString(ctx, name);
    result = text && !strcmp(text, "TypeError") ? 0 : -1;
    JS_FreeCString(ctx, text);
    JS_FreeValue(ctx, name);
    JS_FreeValue(ctx, error);
    return JS_HasException(ctx) ? -1 : result;
}

static int check_api_stubs(JSContext *ctx)
{
    JSValue value;
    if (JS_SetDefaultTimeZone(ctx, "UTC") != -1 || clear_type_error(ctx))
        return -1;
    if (JS_SetDefaultTimeZone(ctx, NULL) != -1 || clear_type_error(ctx))
        return -1;
    value = JS_GetDefaultTimeZone(ctx);
    if (!JS_IsException(value)) {
        JS_FreeValue(ctx, value);
        return -1;
    }
    return clear_type_error(ctx);
}

int main(void)
{
    static const UChar new_york[] = {
        'A','m','e','r','i','c','a','/','N','e','w','_','Y','o','r','k',0
    };
    UChar original[512];
    UErrorCode status = U_ZERO_ERROR;
    JSRuntime *rt = NULL;
    JSContext *ctx = NULL;
    time_t epoch = 0;
    struct tm *os_local;
    const char *os_tz = getenv("TZ");
    int result = 1;
    int32_t original_length;

    /* A UTC OS path cannot accidentally satisfy the New York expectations. */
    if (!os_tz || strcmp(os_tz, "UTC")) {
        fprintf(stderr, "ICU core-only Date requires process TZ=UTC\n");
        return 1;
    }
    os_local = localtime(&epoch);
    if (!os_local || os_local->tm_hour || os_local->tm_min || os_local->tm_sec) {
        fprintf(stderr, "ICU core-only Date requires an actual UTC OS default\n");
        return 1;
    }
    original_length = ucal_getDefaultTimeZone(original,
        (int32_t)(sizeof(original) / sizeof(*original)), &status);
    if (U_FAILURE(status) || original_length <= 0 ||
        (size_t)original_length >= sizeof(original) / sizeof(*original)) {
        fprintf(stderr, "cannot retain ICU default: %s\n", u_errorName(status));
        return 1;
    }
    status = U_ZERO_ERROR;
    ucal_setDefaultTimeZone(new_york, &status);
    if (U_FAILURE(status)) {
        fprintf(stderr, "cannot select ICU New York default: %s\n", u_errorName(status));
        goto restore;
    }
    /* Set ICU before runtime creation; no runtime policy helper is enabled. */
    rt = JS_NewRuntime();
    if (!rt)
        goto restore;
    ctx = JS_NewContext(rt);
    if (!ctx || check_api_stubs(ctx))
        goto restore;
    if (eval_ok(ctx,
        "if ('Intl' in globalThis || 'Temporal' in globalThis)"
        " throw Error('frontends must be absent');"
        "function equal(actual, expected, message) {"
        " if (actual !== expected) throw Error(message + ': ' + actual + ' != ' + expected);"
        "}"
        "const t = Date.UTC(1880,0,1), historical = new Date(t);"
        "equal(historical.getTimezoneOffset(), 17762/60, 'New York historical seconds');"
        "equal(historical.getFullYear(), 1879, 'historical year');"
        "equal(historical.getMonth(), 11, 'historical month');"
        "equal(historical.getDate(), 31, 'historical day');"
        "equal(historical.getHours(), 19, 'historical hour');"
        "equal(historical.getMinutes(), 3, 'historical minute');"
        "equal(historical.getSeconds(), 58, 'historical second');"
        "equal(new Date(1879,11,31,19,3,58).getTime(), t, 'historical local constructor');"
        "equal(Date.parse('1879-12-31T19:03:58'), t, 'historical local parse');"
        "equal(new Date(historical.getFullYear(), historical.getMonth(),"
        " historical.getDate(), historical.getHours(), historical.getMinutes(),"
        " historical.getSeconds(), historical.getMilliseconds()).getTime(),"
        " t, 'historical local field round trip');"
        "equal(Date.parse(historical.toString()), t, 'historical emitted string round trip');"
        "const fields = new Date(t);"
        "equal(fields.setSeconds(58,123), t+123, 'historical local setter');"
        "equal(fields.setMilliseconds(0), t, 'historical local setter round trip');"
        "const gap = new Date(2024,2,10,2,30);"
        "equal(gap.getTime(), Date.UTC(2024,2,10,7,30), 'gap compatible epoch');"
        "equal(gap.getHours(), 3, 'gap advances local hour');"
        "equal(gap.getMinutes(), 30, 'gap preserves minutes');"
        "equal(Date.parse('2024-03-10T02:30:00'), gap.getTime(), 'gap compatible parse');"
        "const gapSetter = new Date(2024,2,10,1,30);"
        "equal(gapSetter.setHours(2,30,0,0), gap.getTime(), 'gap compatible setter');"
        "const overlap = new Date(2024,10,3,1,30);"
        "equal(overlap.getTime(), Date.UTC(2024,10,3,5,30), 'overlap chooses earlier epoch');"
        "equal(overlap.getTimezoneOffset(), 240, 'overlap earlier offset');"
        "equal(Date.parse('2024-11-03T01:30:00'), overlap.getTime(), 'overlap compatible parse');"
        "const overlapSetter = new Date(Date.UTC(2024,10,3,6,30));"
        "equal(overlapSetter.setHours(1,30,0,0), overlap.getTime(), 'overlap compatible setter');"
        "equal(new Date('2024-01-01T00:00:00Z').getTimezoneOffset(), 300,"
        " 'ICU ownership survives disabled policy calls');"))
        goto restore;
    if (check_api_stubs(ctx) || eval_ok(ctx,
        "equal(new Date('2024-07-01T00:00:00Z').getTimezoneOffset(), 240,"
        " 'disabled public APIs preserve ICU default');"))
        goto restore;
    result = 0;
restore:
    if (ctx)
        JS_FreeContext(ctx);
    if (rt)
        JS_FreeRuntime(rt);
    status = U_ZERO_ERROR;
    ucal_setDefaultTimeZone(original, &status);
    if (U_FAILURE(status)) {
        fprintf(stderr, "cannot restore ICU default: %s\n", u_errorName(status));
        result = 1;
    }
    return result;
}
