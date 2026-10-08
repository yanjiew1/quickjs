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
/* Intl.RelativeTimeFormat, ECMA402 #sec-Intl.RelativeTimeFormat.
 * Spec 7ae78cfdf8255468ffc8ebda33dafaea952808dd, 2026-10-06.
 * Original native C implementation; ICU supplies locale data. */
#include "intl-service-common.h"
#ifdef CONFIG_INTL
#include <unicode/ureldatefmt.h>
#include <unicode/unumberformatter.h>
#include <unicode/uformattedvalue.h>
typedef struct JSIntlRelativeTimeFormat {
    char *locale, *numbering_system;
    URelativeDateTimeFormatter *formatter;
    int style, numeric;
} JSIntlRelativeTimeFormat;
static const char *const relative_numeric[] = { "always", "auto" };
static const char *const relative_units[] = {
    "year", "quarter", "month", "week", "day", "hour", "minute", "second"
};
static void js_intl_relative_finalizer(JSRuntime *rt, JSValue obj)
{
    JSIntlRelativeTimeFormat *s = JS_GetOpaque(obj, JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    if (s) {
        if (s->formatter) ureldatefmt_close(s->formatter);
        js_free_rt(rt, s->locale);
        js_free_rt(rt, s->numbering_system);
        js_free_rt(rt, s);
    }
}
static JSValue js_intl_relative_constructor(JSContext *ctx, JSValueConst new_target,
                                          int argc, JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED, options = JS_UNDEFINED;
    JSIntlLocaleList requested = {0};
    JSIntlResolvedLocale resolved = {0};
    JSIntlResolutionKey key = { "nu", NULL, FALSE };
    JSIntlRelativeTimeFormat *s = NULL;
    UNumberFormat *number = NULL;
    UErrorCode status = U_ZERO_ERROR;
    char *numbering = NULL;
    int matcher;
    if (JS_IsUndefined(new_target))
        return JS_ThrowTypeError(ctx, "Intl.RelativeTimeFormat requires new");
    obj = js_intl_new_object(ctx, new_target, JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    if (JS_IsException(obj)) goto fail;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) goto fail;
    JS_SetOpaque(obj, s);
    if (js_intl_service_options(ctx,
            argc > 0 ? argv[0] : JS_UNDEFINED,
            argc > 1 ? argv[1] : JS_UNDEFINED, 1, &requested,
                               &options, &matcher) < 0 ||
        js_intl_get_string_option_alloc(ctx, options, "numberingSystem", &numbering) < 0)
        goto fail;
    if (numbering && !js_intl_unicode_type(numbering)) {
        JS_ThrowRangeError(ctx, "invalid numberingSystem");
        goto fail;
    }
    key.option = numbering;
    if (js_intl_resolve_locale(ctx, JS_INTL_RELATIVE_TIME_FORMAT, &requested,
            js_intl_matchers[matcher], &key, 1, &resolved) < 0 ||
        js_intl_get_string_option(ctx, options, "style", js_intl_styles,
            countof(js_intl_styles), 0, &s->style) < 0 ||
        js_intl_get_string_option(ctx, options, "numeric", relative_numeric,
            countof(relative_numeric), 0, &s->numeric) < 0)
        goto fail;
    s->locale = resolved.locale;
    resolved.locale = NULL;
    s->numbering_system = resolved.values[0];
    resolved.values[0] = NULL;
    number = unum_open(UNUM_DECIMAL, NULL, 0, resolved.icu_locale, NULL, &status);
    if (js_intl_icu_error(ctx, status, "RelativeTimeFormat number") < 0) goto fail;
    unum_setAttribute(number, UNUM_MIN_INTEGER_DIGITS, 1);
    unum_setAttribute(number, UNUM_MIN_FRACTION_DIGITS, 0);
    unum_setAttribute(number, UNUM_MAX_FRACTION_DIGITS, 3);
    unum_setAttribute(number, UNUM_ROUNDING_MODE, UNUM_ROUND_HALFUP);
    /* ureldatefmt_open adopts number, including its failure path. */
    s->formatter = ureldatefmt_open(resolved.icu_locale, number,
        (UDateRelativeDateTimeFormatterStyle)s->style,
        UDISPCTX_CAPITALIZATION_NONE, &status);
    number = NULL;
    if (js_intl_icu_error(ctx, status, "RelativeTimeFormat") < 0) goto fail;
    JS_FreeValue(ctx, options);
    js_free(ctx, numbering);
    js_intl_locale_list_free(ctx, &requested);
    js_intl_resolved_locale_free(ctx, &resolved);
    return obj;
 fail:
    if (number) unum_close(number);
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, options);
    js_free(ctx, numbering);
    js_intl_locale_list_free(ctx, &requested);
    js_intl_resolved_locale_free(ctx, &resolved);
    return JS_EXCEPTION;
}
/* Enumerated number fields overlap: group beats integer; smaller spans win.
   Numeric pattern literals retain the unit, outer pattern literals omit it. */
typedef struct JSIntlRelativeField {
    unsigned char type, unit;
    int32_t span;
} JSIntlRelativeField;
static const char *const relative_part_types[] = {
    "literal", "integer", "fraction", "decimal", "group", "plusSign", "minusSign"
};
static int js_intl_relative_field_type(int32_t field, const UChar *text,
                                      int32_t start, int32_t limit)
{
    switch (field) {
    case UNUM_INTEGER_FIELD: return 1;
    case UNUM_FRACTION_FIELD: return 2;
    case UNUM_DECIMAL_SEPARATOR_FIELD: return 3;
    case UNUM_GROUPING_SEPARATOR_FIELD: return 4;
    case UNUM_SIGN_FIELD:
        return start < limit && text[start] == '+' ? 5 : 6;
    default: return 0;
    }
}
static JSValue js_intl_relative_format(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv, int parts)
{
    JSIntlRelativeTimeFormat *s = JS_GetOpaque2(ctx, this_val,
                                               JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    UFormattedRelativeDateTime *result = NULL;
    const UFormattedValue *formatted;
    UConstrainedFieldPosition *position = NULL;
    JSIntlRelativeField *fields = NULL;
    const UChar *text;
    const char *unit = NULL;
    UChar *unit_text = NULL;
    int32_t unit_length, length, start, limit, i, j, field, category, type;
    UErrorCode status = U_ZERO_ERROR;
    JSValue value = JS_UNDEFINED, unit_value = JS_UNDEFINED;
    double number;
    uint32_t part = 0;
    if (!s) return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &number, argv[0]) < 0 ||
        js_intl_to_uchar(ctx, argv[1], &unit_text, &unit_length) < 0) goto fail;
    /* ToString(unit) precedes both finite-number and unit validation. */
    if (!isfinite(number)) {
        JS_ThrowRangeError(ctx, "relative time value must be finite");
        goto fail;
    }
    for (i = 0; i < countof(relative_units); i++) {
        int32_t n = strlen(relative_units[i]);
        if (unit_length != n && unit_length != n + 1) continue;
        for (j = 0; j < n && unit_text[j] == relative_units[i][j]; j++) {}
        if (j == n && (unit_length == n || unit_text[n] == 's')) {
            unit = relative_units[i];
            break;
        }
    }
    if (!unit) {
        JS_ThrowRangeError(ctx, "invalid relative time unit");
        goto fail;
    }
    result = ureldatefmt_openResult(&status);
    /* ICU auto uses a 1% epsilon. ECMA402 requires an exact literal key. */
    if (s->numeric && number == trunc(number))
        ureldatefmt_formatToResult(s->formatter, number,
            (URelativeDateTimeUnit)i, result, &status);
    else
        ureldatefmt_formatNumericToResult(s->formatter, number,
            (URelativeDateTimeUnit)i, result, &status);
    formatted = ureldatefmt_resultAsValue(result, &status);
    text = ufmtval_getString(formatted, &length, &status);
    if (js_intl_icu_error(ctx, status, "RelativeTimeFormat.format") < 0) goto fail;
    if (!parts) {
        value = js_intl_from_uchar(ctx, text, length);
        goto done;
    }
    value = JS_NewArray(ctx);
    unit_value = JS_NewString(ctx, unit);
    if (JS_IsException(value) || JS_IsException(unit_value)) goto fail;
    if ((size_t)length > SIZE_MAX / sizeof(*fields)) {
        JS_ThrowOutOfMemory(ctx);
        goto fail;
    }
    fields = js_mallocz(ctx, (size_t)(length ? length : 1) * sizeof(*fields));
    if (!fields) goto fail;
    for (i = 0; i < length; i++) fields[i].span = INT32_MAX;
    position = ucfpos_open(&status);
    while (ufmtval_nextPosition(formatted, position, &status)) {
        category = ucfpos_getCategory(position, &status);
        field = ucfpos_getField(position, &status);
        ucfpos_getIndexes(position, &start, &limit, &status);
        if (js_intl_icu_error(ctx, status, "RelativeTimeFormat parts") < 0) goto fail;
        if (start < 0 || limit > length || start > limit) {
            JS_ThrowInternalError(ctx, "invalid ICU relative time field");
            goto fail;
        }
        if (category == UFIELD_CATEGORY_RELATIVE_DATETIME &&
                field == UDAT_REL_NUMERIC_FIELD) {
            for (i = start; i < limit; i++) fields[i].unit = 1;
        } else if (category == UFIELD_CATEGORY_NUMBER) {
            type = js_intl_relative_field_type(field, text, start, limit);
            for (i = start; i < limit; i++) {
                fields[i].unit = 1;
                if (limit - start < fields[i].span ||
                        (limit - start == fields[i].span && type > fields[i].type)) {
                    fields[i].type = type;
                    fields[i].span = limit - start;
                }
            }
        }
    }
    if (js_intl_icu_error(ctx, status, "RelativeTimeFormat parts") < 0) goto fail;
    for (i = 0; i < length; i = j) {
        for (j = i + 1; j < length && fields[j].type == fields[i].type &&
                fields[j].unit == fields[i].unit; j++) {}
        if (js_intl_add_part_uchar(ctx, value, part++,
            relative_part_types[fields[i].type], text + i, j - i,
            fields[i].unit ? "unit" : NULL, unit_value) < 0) goto fail;
    }
    goto done;
 fail:
    JS_FreeValue(ctx, value);
    value = JS_EXCEPTION;
 done:
    JS_FreeValue(ctx, unit_value);
    js_free(ctx, unit_text);
    js_free(ctx, fields);
    if (position) ucfpos_close(position);
    if (result) ureldatefmt_closeResult(result);
    return value;
}
static JSValue js_intl_relative_resolved(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv)
{
    JSIntlRelativeTimeFormat *s = JS_GetOpaque2(ctx, this_val,
                                               JS_CLASS_INTL_RELATIVE_TIME_FORMAT);
    JSValue result;
    if (!s) return JS_EXCEPTION;
    result = JS_NewObject(ctx);
    if (JS_IsException(result)) return result;
    if (js_intl_define_string(ctx, result, "locale", s->locale) < 0 ||
        js_intl_define_string(ctx, result, "style", js_intl_styles[s->style]) < 0 ||
        js_intl_define_string(ctx, result, "numeric", relative_numeric[s->numeric]) < 0 ||
        js_intl_define_string(ctx, result, "numberingSystem", s->numbering_system) < 0) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
    }
    return result;
}
static JSValue js_intl_relative_supported(JSContext *ctx, JSValueConst this_val,
                                         int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_RELATIVE_TIME_FORMAT,
        argc > 0 ? argv[0] : JS_UNDEFINED,
        argc > 1 ? argv[1] : JS_UNDEFINED);
}
static const JSClassDef js_intl_relative_class = {
    "Intl.RelativeTimeFormat", .finalizer = js_intl_relative_finalizer,
};
static const JSCFunctionListEntry js_intl_relative_static[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, js_intl_relative_supported),
};
static const JSCFunctionListEntry js_intl_relative_prototype[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, js_intl_relative_resolved),
    JS_CFUNC_MAGIC_DEF("format", 2, js_intl_relative_format, 0),
    JS_CFUNC_MAGIC_DEF("formatToParts", 2, js_intl_relative_format, 1),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.RelativeTimeFormat", JS_PROP_CONFIGURABLE),
};
int js_intl_init_relative_time_format(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_RELATIVE_TIME_FORMAT,
        &js_intl_relative_class, "RelativeTimeFormat", js_intl_relative_constructor, 0,
        JS_CFUNC_constructor, js_intl_relative_static, countof(js_intl_relative_static),
        js_intl_relative_prototype, countof(js_intl_relative_prototype));
}
#endif
