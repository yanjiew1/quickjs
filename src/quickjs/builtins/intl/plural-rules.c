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
/* Intl.PluralRules, ECMA402 #sec-intl.pluralrules.
 * Spec 7ae78cfdf8255468ffc8ebda33dafaea952808dd, 2026-10-06.
 * Original native C implementation; shared native number algorithms. */
#include "intl-service-common.h"
#ifdef CONFIG_INTL
#include "intl-number.h"
#include "../../../intl/plural-icu.h"
#include <unicode/upluralrules.h>
#include <unicode/uenum.h>
#include <unicode/uformattedvalue.h>
#include <unicode/uformattednumber.h>
#include <unicode/ures.h>
#include <unicode/uloc.h>
typedef struct JSIntlPluralRules {
    char *locale, *icu_locale;
    UPluralRules *rules;
    UNumberFormatter *formatter, *raw_formatter;
    uint8_t range_categories[6][6];
    JSIntlPluralRuleSet exact_rules;
    JSIntlDigitOptions digits;
    int type, notation, compact_display;
} JSIntlPluralRules;
static const char *const plural_types[] = { "cardinal", "ordinal" };
static const char *const plural_notations[] = {
    "standard", "scientific", "engineering", "compact"
};
static const char *const plural_compact[] = { "short", "long" };
static const char *const plural_categories[] = { "zero", "one", "two", "few", "many", "other" };
static const char *const plural_round_modes[] = {
    "ceil", "floor", "expand", "trunc", "halfCeil", "halfFloor", "halfExpand", "halfTrunc", "halfEven"
};
static const char *const plural_priorities[] = { "auto", "morePrecision", "lessPrecision" };
static void js_intl_plural_finalizer(JSRuntime *rt, JSValue obj)
{
    JSIntlPluralRules *s = JS_GetOpaque(obj, JS_CLASS_INTL_PLURAL_RULES);
    if (s) {
        if (s->rules) uplrules_close(s->rules);
        if (s->formatter) unumf_close(s->formatter);
        if (s->raw_formatter) unumf_close(s->raw_formatter);
        js_intl_plural_icu_free(&s->exact_rules);
        js_free_rt(rt, s->locale);
        js_free_rt(rt, s->icu_locale);
        js_free_rt(rt, s);
    }
}
/* ResolvePlural compares unsigned FormatNumericToString, before notation.
   Use the exact same digit options with invariant ASCII output and no scaling.
   sign-never affects presentation only: signed input still controls rounding. */
static UNumberFormatter *js_intl_plural_raw_formatter(JSContext *ctx,
    DynBuf *buffer, UErrorCode *status)
{
    static const char sign[] = "sign-never ";
    UChar *skeleton;
    UNumberFormatter *formatter;
    int32_t length;
    if (dbuf_error(buffer) || buffer->size > INT32_MAX - sizeof(sign)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    length = buffer->size + sizeof(sign) - 1;
    skeleton = js_intl_alloc_uchar(ctx, length);
    if (!skeleton) return NULL;
    for (size_t i = 0; i < buffer->size; i++) skeleton[i] = buffer->buf[i];
    for (size_t i = 0; i < sizeof(sign); i++) skeleton[buffer->size + i] = sign[i];
    formatter = unumf_openForSkeletonAndLocale(skeleton, length,
                                               "en_US@numbers=latn", status);
    js_free(ctx, skeleton);
    return formatter;
}
/* This backend consumes only ResolvePlural's already rounded decimal string.
   ICU notation supplies locale scaling. It must not round the input again. */
static void js_intl_plural_notation_skeleton(DynBuf *buffer,
                                            const JSIntlPluralRules *s)
{
    dbuf_putstr(buffer, "group-off sign-never ");
    if (s->notation == 1) dbuf_putstr(buffer, "scientific ");
    if (s->notation == 2) dbuf_putstr(buffer, "engineering ");
    if (s->notation == 3) dbuf_putstr(buffer,
        s->compact_display ? "compact-long " : "compact-short ");
}

static int js_intl_plural_keyword(JSContext *ctx, const UChar *text,
                                  int32_t length)
{
    int i, j;
    for (i = 0; i < countof(plural_categories); i++) {
        if ((size_t)length != strlen(plural_categories[i])) continue;
        for (j = 0; j < length && text[j] == plural_categories[i][j]; j++) {}
        if (j == length) return i;
    }
    JS_ThrowInternalError(ctx, "invalid ICU plural category");
    return -1;
}

/* DecimalQuantity's public decimal result is its notation-scaled mantissa.
   Compare magnitudes to determine scaling without binary64 conversion. */
static int js_intl_plural_decimal_magnitude(const char *text, int32_t length,
                                           int64_t *magnitude)
{
    int32_t at = 0, digits = 0, integer_digits = 0, first = -1;
    int seen_point = 0, exponent_negative = 0;
    int64_t exponent = 0;
    if (at < length && (text[at] == '+' || text[at] == '-')) at++;
    for (; at < length && text[at] != 'e' && text[at] != 'E'; at++) {
        if (text[at] == '.' && !seen_point) { seen_point = 1; continue; }
        if (text[at] < '0' || text[at] > '9') return -1;
        if (first < 0 && text[at] != '0') first = digits;
        if (!seen_point) integer_digits++;
        digits++;
    }
    if (!digits) return -1;
    if (at < length) {
        at++;
        if (at < length && (text[at] == '+' || text[at] == '-'))
            exponent_negative = text[at++] == '-';
        if (at == length) return -1;
        for (; at < length; at++) {
            if (text[at] < '0' || text[at] > '9' ||
                exponent > (INT32_MAX - (text[at] - '0')) / 10) return -1;
            exponent = exponent * 10 + text[at] - '0';
        }
    }
    if (exponent_negative) exponent = -exponent;
    *magnitude = first < 0 ? 0 : (int64_t)integer_digits - first - 1 + exponent;
    return 0;
}

/* Runtime-owned allocation remains valid after the creation context dies. */
static void *js_intl_plural_realloc(void *opaque, void *ptr, size_t size)
{
    return js_realloc_rt((JSRuntime *)opaque, ptr, size);
}

static int32_t js_intl_plural_category(JSContext *ctx, JSIntlPluralRules *s,
    UFormattedNumber *raw, UChar *category, int32_t capacity)
{
    UErrorCode status = U_ZERO_ERROR;
    UFormattedNumber *formatted = NULL;
    const UFormattedValue *value;
    const UChar *text;
    char *decimal = NULL, *mantissa = NULL;
    JSIntlMathematicalValue number = {0};
    JSIntlPluralOperands operands;
    int32_t length, mantissa_length, result = -1;
    int64_t magnitude, mantissa_magnitude, exponent = 0;
    int selected;
    value = unumf_resultAsValue(raw, &status);
    text = ufmtval_getString(value, &length, &status);
    if (js_intl_icu_error(ctx, status, "PluralRules raw decimal") < 0) goto done;
    decimal = js_intl_alloc_char(ctx, length);
    if (!decimal) goto done;
    for (int32_t i = 0; i < length; i++) {
        if ((text[i] < '0' || text[i] > '9') && text[i] != '.') {
            JS_ThrowInternalError(ctx, "invalid raw plural decimal"); goto done;
        }
        decimal[i] = text[i];
    }
    decimal[length] = 0;
    if (s->notation) {
        if (js_intl_plural_decimal_magnitude(decimal, length, &magnitude) < 0) {
            JS_ThrowInternalError(ctx, "invalid raw plural magnitude"); goto done;
        }
        number.decimal = decimal;
        number.length = length;
        formatted = unumf_openResult(&status);
        if (js_intl_format_mathematical_value(ctx, s->formatter, &number,
                                              formatted, &status) < 0) goto done;
        mantissa_length = unumf_resultToDecimalNumber(formatted, NULL, 0, &status);
        if (status != U_BUFFER_OVERFLOW_ERROR &&
            js_intl_icu_error(ctx, status, "PluralRules scaled decimal") < 0) goto done;
        status = U_ZERO_ERROR;
        mantissa = js_intl_alloc_char(ctx, mantissa_length);
        if (!mantissa) goto done;
        mantissa_length = unumf_resultToDecimalNumber(formatted, mantissa,
                                                       mantissa_length + 1, &status);
        if (js_intl_icu_error(ctx, status, "PluralRules scaled decimal") < 0) goto done;
        if (js_intl_plural_decimal_magnitude(mantissa, mantissa_length,
                                              &mantissa_magnitude) < 0) {
            JS_ThrowInternalError(ctx, "invalid scaled plural magnitude"); goto done;
        }
        exponent = magnitude - mantissa_magnitude;
        if (exponent < INT32_MIN || exponent > INT32_MAX) {
            JS_ThrowInternalError(ctx, "invalid plural notation exponent"); goto done;
        }
    }
    /* CLDR n/i/v/w/f/t describe the full unscaled decimal string; c/e record
       notation scaling. Read visible zeros directly instead of asking ICU
       to reconstruct fractional padding or converting operands to double. */
    operands = (JSIntlPluralOperands){ decimal, length, exponent };
    if (js_intl_plural_icu_select(&s->exact_rules, &operands, &selected) < 0) {
        JS_ThrowInternalError(ctx, "invalid exact plural operands"); goto done;
    }
    length = strlen(plural_categories[selected]);
    if (length >= capacity) {
        JS_ThrowInternalError(ctx, "plural category buffer too small"); goto done;
    }
    for (int32_t i = 0; i <= length; i++) category[i] = plural_categories[selected][i];
    result = length;
 done:
    if (formatted) unumf_closeResult(formatted);
    js_free(ctx, mantissa);
    js_free(ctx, decimal);
    return result;
}

/* Public ICU resource APIs expose the category-pair table. Consume selected
   categories directly, rather than reformatting the original range inputs. */
static int js_intl_plural_range_data(JSContext *ctx, JSIntlPluralRules *s,
                                     const char *locale)
{
    UErrorCode status = U_ZERO_ERROR, lookup = U_ZERO_ERROR;
    UResourceBundle *bundle = NULL, *locales = NULL, *rules = NULL;
    UResourceBundle *set = NULL, *row = NULL;
    const UChar *text;
    char language[ULOC_LANG_CAPACITY], key[32];
    int32_t length, count;
    int categories[3], result = -1;
    memset(s->range_categories, 5, sizeof(s->range_categories));
    uloc_getLanguage(locale, language, sizeof(language), &status);
    bundle = ures_openDirect(NULL, "pluralRanges", &status);
    locales = ures_getByKey(bundle, "locales", NULL, &status);
    if (js_intl_icu_error(ctx, status, "PluralRules range locale") < 0) goto done;
    text = ures_getStringByKey(locales, language, &length, &lookup);
    if (lookup == U_MISSING_RESOURCE_ERROR) { result = 0; goto done; }
    if (js_intl_icu_error(ctx, lookup, "PluralRules range locale") < 0) goto done;
    if (length < 0 || length >= (int32_t)sizeof(key)) {
        JS_ThrowInternalError(ctx, "invalid ICU plural range set"); goto done;
    }
    for (int32_t i = 0; i < length; i++) {
        if (text[i] > 127 || !text[i]) {
            JS_ThrowInternalError(ctx, "invalid ICU plural range set"); goto done;
        }
        key[i] = text[i];
    }
    key[length] = 0;
    rules = ures_getByKey(bundle, "rules", NULL, &status);
    set = ures_getByKey(rules, key, NULL, &status);
    if (js_intl_icu_error(ctx, status, "PluralRules range rules") < 0) goto done;
    count = ures_getSize(set);
    for (int32_t i = 0; i < count; i++) {
        row = ures_getByIndex(set, i, NULL, &status);
        if (js_intl_icu_error(ctx, status, "PluralRules range row") < 0) goto done;
        if (ures_getSize(row) != 3) {
            JS_ThrowInternalError(ctx, "invalid ICU plural range row"); goto done;
        }
        for (int j = 0; j < 3; j++) {
            text = ures_getStringByIndex(row, j, &length, &status);
            if (js_intl_icu_error(ctx, status, "PluralRules range category") < 0) goto done;
            categories[j] = js_intl_plural_keyword(ctx, text, length);
            if (categories[j] < 0) goto done;
        }
        s->range_categories[categories[0]][categories[1]] = categories[2];
        ures_close(row); row = NULL;
    }
    result = 0;
 done:
    if (row) ures_close(row);
    if (set) ures_close(set);
    if (rules) ures_close(rules);
    if (locales) ures_close(locales);
    if (bundle) ures_close(bundle);
    return result;
}

static JSValue js_intl_plural_constructor(JSContext *ctx, JSValueConst new_target,
                                         int argc, JSValueConst *argv)
{
    JSValue obj = JS_UNDEFINED, options = JS_UNDEFINED;
    JSIntlLocaleList requested = {0};
    JSIntlResolvedLocale resolved = {0};
    JSIntlPluralRules *s = NULL;
    UErrorCode status = U_ZERO_ERROR;
    UChar *skeleton = NULL;
    DynBuf buffer;
    int32_t skeleton_length;
    int matcher;
    js_dbuf_init(ctx, &buffer);
    if (JS_IsUndefined(new_target)) {
        JS_ThrowTypeError(ctx, "Intl.PluralRules requires new");
        goto fail;
    }
    obj = js_intl_new_object(ctx, new_target, JS_CLASS_INTL_PLURAL_RULES);
    if (JS_IsException(obj)) goto fail;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) goto fail;
    JS_SetOpaque(obj, s);
    if (js_intl_service_options(ctx,
            argc > 0 ? argv[0] : JS_UNDEFINED,
            argc > 1 ? argv[1] : JS_UNDEFINED, 1, &requested,
                               &options, &matcher) < 0 ||
        js_intl_resolve_locale(ctx, JS_INTL_PLURAL_RULES, &requested,
            js_intl_matchers[matcher], NULL, 0, &resolved) < 0 ||
        js_intl_get_string_option(ctx, options, "type", plural_types,
            countof(plural_types), 0, &s->type) < 0 ||
        js_intl_get_string_option(ctx, options, "notation", plural_notations,
            countof(plural_notations), 0, &s->notation) < 0 ||
        js_intl_get_string_option(ctx, options, "compactDisplay", plural_compact,
            countof(plural_compact), 0, &s->compact_display) < 0 ||
        js_intl_set_digit_options(ctx, options, 0, 3, s->notation == 3,
            &s->digits) < 0)
        goto fail;
    if (js_intl_digit_skeleton(ctx, &buffer, &s->digits) < 0) goto fail;
    dbuf_putstr(&buffer, "group-off ");
    s->raw_formatter = js_intl_plural_raw_formatter(ctx, &buffer, &status);
    if (!s->raw_formatter) {
        if (U_FAILURE(status)) js_intl_icu_error(ctx, status, "PluralRules raw digits");
        goto fail;
    }
    dbuf_free(&buffer);
    js_dbuf_init(ctx, &buffer);
    dbuf_putstr(&buffer, "precision-unlimited ");
    js_intl_plural_notation_skeleton(&buffer, s);
    if (dbuf_error(&buffer) || buffer.size > INT32_MAX) {
        JS_ThrowOutOfMemory(ctx);
        goto fail;
    }
    skeleton_length = buffer.size;
    skeleton = js_intl_alloc_uchar(ctx, skeleton_length);
    if (!skeleton) goto fail;
    for (int32_t i = 0; i < skeleton_length; i++) skeleton[i] = buffer.buf[i];
    skeleton[skeleton_length] = 0;
    s->locale = resolved.locale;
    resolved.locale = NULL;
    s->rules = uplrules_openForType(resolved.icu_locale,
        s->type ? UPLURAL_TYPE_ORDINAL : UPLURAL_TYPE_CARDINAL, &status);
    s->formatter = unumf_openForSkeletonAndLocale(skeleton, skeleton_length,
        resolved.icu_locale, &status);
    if (js_intl_plural_icu_load(&s->exact_rules, resolved.icu_locale, s->type,
            js_intl_plural_realloc, JS_GetRuntime(ctx), &status) < 0 ||
        js_intl_icu_error(ctx, status, "PluralRules") < 0 ||
        js_intl_plural_range_data(ctx, s, resolved.icu_locale) < 0) {
        if (U_FAILURE(status)) js_intl_icu_error(ctx, status, "PluralRules exact rules");
        goto fail;
    }
    s->icu_locale = resolved.icu_locale;
    resolved.icu_locale = NULL;
    JS_FreeValue(ctx, options);
    js_free(ctx, skeleton);
    dbuf_free(&buffer);
    js_intl_locale_list_free(ctx, &requested);
    js_intl_resolved_locale_free(ctx, &resolved);
    return obj;
 fail:
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, options);
    js_free(ctx, skeleton);
    dbuf_free(&buffer);
    js_intl_locale_list_free(ctx, &requested);
    js_intl_resolved_locale_free(ctx, &resolved);
    return JS_EXCEPTION;
}
static JSValue js_intl_plural_select(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    JSIntlPluralRules *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_PLURAL_RULES);
    JSIntlMathematicalValue number = {0};
    UFormattedNumber *formatted = NULL;
    UErrorCode status = U_ZERO_ERROR;
    UChar category[16];
    int32_t length;
    JSValue result = JS_UNDEFINED;
    if (!s) return JS_EXCEPTION;
    if (js_intl_to_mathematical_value(ctx, argv[0], &number) < 0) goto fail;
    if (number.kind == JS_INTL_MV_NAN ||
        number.kind == JS_INTL_MV_POSITIVE_INFINITY ||
        number.kind == JS_INTL_MV_NEGATIVE_INFINITY) {
        result = JS_NewString(ctx, "other");
        goto done;
    }
    formatted = unumf_openResult(&status);
    if (js_intl_format_mathematical_value(ctx, s->raw_formatter, &number,
                                          formatted, &status) < 0) goto fail;
    length = js_intl_plural_category(ctx, s, formatted, category, countof(category));
    if (length < 0) goto fail;
    if (js_intl_icu_error(ctx, status, "PluralRules.select") < 0) goto fail;
    result = js_intl_from_uchar(ctx, category, length);
    goto done;
 fail:
    result = JS_EXCEPTION;
 done:
    if (formatted) unumf_closeResult(formatted);
    js_intl_free_mathematical_value(ctx, &number);
    return result;
}
static JSValue js_intl_plural_select_range(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv)
{
    JSIntlPluralRules *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_PLURAL_RULES);
    JSIntlMathematicalValue x = {0}, y = {0};
    UFormattedNumber *first = NULL, *second = NULL;
    int first_category, second_category;
    UErrorCode status = U_ZERO_ERROR;
    const UFormattedValue *first_value, *second_value;
    const UChar *first_text, *second_text;
    UChar category[16];
    int32_t first_length, second_length, category_length;
    int same, x_finite, y_finite;
    JSValue result = JS_UNDEFINED;
    if (!s) return JS_EXCEPTION;
    if (JS_IsUndefined(argv[0]) || JS_IsUndefined(argv[1]))
        return JS_ThrowTypeError(ctx, "PluralRules.selectRange requires two values");
    /* Both conversions precede NaN validation; no range-order restriction. */
    if (js_intl_to_mathematical_value(ctx, argv[0], &x) < 0 ||
        js_intl_to_mathematical_value(ctx, argv[1], &y) < 0) goto fail;
    if (x.kind == JS_INTL_MV_NAN || y.kind == JS_INTL_MV_NAN) {
        JS_ThrowRangeError(ctx, "PluralRules.selectRange does not accept NaN");
        goto fail;
    }
    first = unumf_openResult(&status);
    second = unumf_openResult(&status);
    if (js_intl_format_mathematical_value(ctx, s->raw_formatter, &x, first, &status) < 0 ||
        js_intl_format_mathematical_value(ctx, s->raw_formatter, &y, second, &status) < 0)
        goto fail;
    first_value = unumf_resultAsValue(first, &status);
    second_value = unumf_resultAsValue(second, &status);
    first_text = ufmtval_getString(first_value, &first_length, &status);
    second_text = ufmtval_getString(second_value, &second_length, &status);
    if (js_intl_icu_error(ctx, status, "PluralRules.selectRange values") < 0) goto fail;
    x_finite = x.kind == JS_INTL_MV_FINITE || x.kind == JS_INTL_MV_NEGATIVE_ZERO;
    y_finite = y.kind == JS_INTL_MV_FINITE || y.kind == JS_INTL_MV_NEGATIVE_ZERO;
    if (x_finite && y_finite)
        same = first_length == second_length &&
            !memcmp(first_text, second_text, (size_t)first_length * sizeof(UChar));
    else
        same = !x_finite && !y_finite && x.kind == y.kind;
    if (same) {
        if (x.kind == JS_INTL_MV_POSITIVE_INFINITY ||
            x.kind == JS_INTL_MV_NEGATIVE_INFINITY) {
            result = JS_NewString(ctx, "other");
            goto done;
        }
        category_length = js_intl_plural_category(ctx, s, first,
                                                  category, countof(category));
        if (category_length < 0) goto fail;
    } else {
        first_category = second_category = 5; /* Infinite values use other. */
        if (x_finite) {
            category_length = js_intl_plural_category(ctx, s, first,
                                                      category, countof(category));
            if (category_length < 0) goto fail;
            first_category = js_intl_plural_keyword(ctx, category, category_length);
            if (first_category < 0) goto fail;
        }
        if (y_finite) {
            category_length = js_intl_plural_category(ctx, s, second,
                                                      category, countof(category));
            if (category_length < 0) goto fail;
            second_category = js_intl_plural_keyword(ctx, category, category_length);
            if (second_category < 0) goto fail;
        }
        result = JS_NewString(ctx,
            plural_categories[s->range_categories[first_category][second_category]]);
        goto done;
    }
    if (js_intl_icu_error(ctx, status, "PluralRules.selectRange") < 0) goto fail;
    result = js_intl_from_uchar(ctx, category, category_length);
    goto done;
 fail:
    result = JS_EXCEPTION;
 done:
    if (first) unumf_closeResult(first);
    if (second) unumf_closeResult(second);
    js_intl_free_mathematical_value(ctx, &x);
    js_intl_free_mathematical_value(ctx, &y);
    return result;
}
static JSValue js_intl_plural_resolved(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv)
{
    JSIntlPluralRules *s = JS_GetOpaque2(ctx, this_val, JS_CLASS_INTL_PLURAL_RULES);
    UEnumeration *keywords = NULL;
    UErrorCode status = U_ZERO_ERROR;
    const char *keyword;
    unsigned int mask = 0;
    uint32_t index = 0;
    JSValue result = JS_UNDEFINED, categories = JS_UNDEFINED;
    if (!s) return JS_EXCEPTION;
    keywords = uplrules_getKeywords(s->rules, &status);
    if (js_intl_icu_error(ctx, status, "PluralRules categories") < 0) goto fail;
    while ((keyword = uenum_next(keywords, NULL, &status))) {
        for (int i = 0; i < countof(plural_categories); i++)
            if (!strcmp(keyword, plural_categories[i])) mask |= 1U << i;
    }
    if (js_intl_icu_error(ctx, status, "PluralRules categories") < 0) goto fail;
    categories = JS_NewArray(ctx);
    result = JS_NewObject(ctx);
    if (JS_IsException(categories) || JS_IsException(result)) goto fail;
    for (int i = 0; i < countof(plural_categories); i++) {
        if (mask & (1U << i)) {
            JSValue category = JS_NewString(ctx, plural_categories[i]);
            if (JS_IsException(category)) goto fail;
            if (JS_DefinePropertyValueUint32(ctx, categories, index++,
                category, JS_PROP_C_W_E) < 0) goto fail;
        }
    }
    if (js_intl_define_string(ctx, result, "locale", s->locale) < 0 ||
        js_intl_define_string(ctx, result, "type", plural_types[s->type]) < 0 ||
        js_intl_define_string(ctx, result, "notation", plural_notations[s->notation]) < 0 ||
        (s->notation == 3 && js_intl_define_string(ctx, result, "compactDisplay",
            plural_compact[s->compact_display]) < 0) ||
        js_intl_define_int(ctx, result, "minimumIntegerDigits", s->digits.minimum_integer_digits) < 0)
        goto fail;
#define PR_DIGIT(property, field) \
    if (s->digits.field >= 0 && js_intl_define_int(ctx, result, property, s->digits.field) < 0) goto fail
    PR_DIGIT("minimumFractionDigits", minimum_fraction_digits);
    PR_DIGIT("maximumFractionDigits", maximum_fraction_digits);
    PR_DIGIT("minimumSignificantDigits", minimum_significant_digits);
    PR_DIGIT("maximumSignificantDigits", maximum_significant_digits);
#undef PR_DIGIT
    if (JS_DefinePropertyValueStr(ctx, result, "pluralCategories", categories, JS_PROP_C_W_E) < 0) {
        categories = JS_UNDEFINED;
        goto fail;
    }
    categories = JS_UNDEFINED;
    if (js_intl_define_int(ctx, result, "roundingIncrement", s->digits.rounding_increment) < 0 ||
        js_intl_define_string(ctx, result, "roundingMode", plural_round_modes[s->digits.rounding_mode]) < 0 ||
        js_intl_define_string(ctx, result, "roundingPriority", plural_priorities[s->digits.rounding_priority]) < 0 ||
        js_intl_define_string(ctx, result, "trailingZeroDisplay",
            s->digits.trailing_zero_display ? "stripIfInteger" : "auto") < 0) goto fail;
    goto done;
 fail:
    JS_FreeValue(ctx, result);
    result = JS_EXCEPTION;
 done:
    if (keywords) uenum_close(keywords);
    JS_FreeValue(ctx, categories);
    return result;
}
static JSValue js_intl_plural_supported(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv)
{
    return js_intl_supported_locales(ctx, JS_INTL_PLURAL_RULES,
        argc > 0 ? argv[0] : JS_UNDEFINED,
        argc > 1 ? argv[1] : JS_UNDEFINED);
}
static const JSClassDef js_intl_plural_class = {
    "Intl.PluralRules", .finalizer = js_intl_plural_finalizer,
};
static const JSCFunctionListEntry js_intl_plural_static[] = {
    JS_CFUNC_DEF("supportedLocalesOf", 1, js_intl_plural_supported),
};
static const JSCFunctionListEntry js_intl_plural_prototype[] = {
    JS_CFUNC_DEF("resolvedOptions", 0, js_intl_plural_resolved),
    JS_CFUNC_DEF("select", 1, js_intl_plural_select),
    JS_CFUNC_DEF("selectRange", 2, js_intl_plural_select_range),
    JS_PROP_STRING_DEF("[Symbol.toStringTag]", "Intl.PluralRules", JS_PROP_CONFIGURABLE),
};
int js_intl_init_plural_rules(JSContext *ctx, JSValueConst intl)
{
    return js_intl_init_constructor(ctx, intl, JS_CLASS_INTL_PLURAL_RULES,
        &js_intl_plural_class, "PluralRules", js_intl_plural_constructor, 0,
        JS_CFUNC_constructor, js_intl_plural_static, countof(js_intl_plural_static),
        js_intl_plural_prototype, countof(js_intl_plural_prototype));
}
#endif
