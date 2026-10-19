/* NumberFormat unit option processing, ECMA-402
 * 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "number-format-options.h"
#include "../intl.h"
#include "../../../intl/unit/identifier.h"
#include <assert.h>

static int string_value(JSContext *ctx, JSValueConst options, const char *name,
                         QJSIntlBytes *out, const char **owned)
{
    JSValue value = JS_GetPropertyStr(ctx, options, name);
    *out = (QJSIntlBytes){0};
    *owned = NULL;
    if (JS_IsException(value))
        return -1;
    if (JS_IsUndefined(value))
        return 0;
    *owned = JS_ToCStringLen(ctx, &out->length, value);
    JS_FreeValue(ctx, value);
    out->data = *owned;
    return *owned ? 0 : -1;
}

int js_intl_number_unit_options(JSContext *ctx, JSValueConst options,
                                 JSIntlNumberUnitOptions *out)
{
    static const char *const styles[] = {"decimal", "percent", "currency", "unit"};
    static const char *const currency_displays[] = {"symbol", "narrowSymbol", "code", "name"};
    static const char *const currency_signs[] = {"standard", "accounting"};
    static const char *const unit_displays[] = {"short", "narrow", "long"};
    JSIntlNumberUnitOptions result = {0};
    QJSIntlBytes currency, unit;
    QJSIntlUnitIdentifier identifier;
    const char *owned_currency = NULL, *owned_unit = NULL;
    int style, display, sign, unit_display, ret = -1;
    size_t i;
    assert(out && JS_IsObject(options));
    memset(out, 0, sizeof(*out));
    style = js_intl_string_option(ctx, options, "style", styles, countof(styles), 0);
    if (style < 0 || string_value(ctx, options, "currency", &currency, &owned_currency))
        goto done;
    if (!currency.data) {
        if (style == QJS_INTL_NUMBER_CURRENCY) {
            JS_ThrowTypeError(ctx, "currency style requires currency");
            goto done;
        }
    } else {
        if (currency.length != 3)
            goto invalid_currency;
        for (i = 0; i < 3; i++) {
            unsigned char c = currency.data[i];
            if (c >= 'a' && c <= 'z')
                c -= 'a' - 'A';
            else if (c < 'A' || c > 'Z')
                goto invalid_currency;
            if (style == QJS_INTL_NUMBER_CURRENCY)
                result.currency[i] = c;
        }
    }
    display = js_intl_string_option(ctx, options, "currencyDisplay", currency_displays,
                                      countof(currency_displays), 0);
    if (display < 0)
        goto done;
    sign = js_intl_string_option(ctx, options, "currencySign", currency_signs,
                                   countof(currency_signs), 0);
    if (sign < 0 || string_value(ctx, options, "unit", &unit, &owned_unit))
        goto done;
    if (!unit.data) {
        if (style == QJS_INTL_NUMBER_UNIT) {
            JS_ThrowTypeError(ctx, "unit style requires unit");
            goto done;
        }
    } else if (qjs_intl_unit_identifier_parse(unit, &identifier)) {
        JS_ThrowRangeError(ctx, "invalid Intl unit");
        goto done;
    }
    unit_display = js_intl_string_option(ctx, options, "unitDisplay", unit_displays,
                                           countof(unit_displays), 0);
    if (unit_display < 0)
        goto done;
    result.style = style;
    if (style == QJS_INTL_NUMBER_CURRENCY) {
        result.currency_display = display;
        result.currency_sign = sign;
    } else if (style == QJS_INTL_NUMBER_UNIT) {
        result.unit = js_strndup(ctx, unit.data, unit.length);
        if (!result.unit)
            goto done;
        result.unit_display = unit_display;
    }
    *out = result;
    ret = 0;
    goto done;
invalid_currency:
    JS_ThrowRangeError(ctx, "invalid Intl currency");
done:
    JS_FreeCString(ctx, owned_currency);
    JS_FreeCString(ctx, owned_unit);
    return ret;
}

static int grouping_option(JSContext *ctx, JSValueConst options, int fallback)
{
    static const char *const names[] = {"auto", "always", "min2", "true", "false"};
    JSValue value = JS_GetPropertyStr(ctx, options, "useGrouping");
    const char *text;
    size_t length, i;
    if (JS_IsException(value))
        return -1;
    if (JS_IsUndefined(value))
        return fallback;
    if (JS_IsBool(value) && JS_ToBool(ctx, value))
        return QJS_INTL_NUMBER_GROUP_ALWAYS;
    if (!JS_ToBool(ctx, value)) {
        JS_FreeValue(ctx, value);
        return QJS_INTL_NUMBER_GROUP_OFF;
    }
    text = JS_ToCStringLen(ctx, &length, value);
    JS_FreeValue(ctx, value);
    if (!text)
        return -1;
    for (i = 0; i < countof(names); i++) {
        if (length == strlen(names[i]) && !memcmp(text, names[i], length))
            break;
    }
    JS_FreeCString(ctx, text);
    if (i == countof(names)) {
        JS_ThrowRangeError(ctx, "invalid Intl useGrouping");
        return -1;
    }
    return i >= 3 ? fallback : (int)i + QJS_INTL_NUMBER_GROUP_AUTO;
}

int js_intl_number_format_options(JSContext *ctx, JSValueConst options,
                                   const QJSIntlNumberService *service,
                                   JSIntlNumberFormatOptions *out)
{
    static const char *const notations[] = {"standard", "scientific", "engineering", "compact"};
    static const char *const compact_displays[] = {"short", "long"};
    static const char *const signs[] = {"auto", "never", "always", "exceptZero", "negative"};
    JSIntlNumberFormatOptions result = {0};
    int notation, compact, grouping, sign, minimum = 0, maximum = 3;
    assert(service && out && JS_IsObject(options));
    memset(out, 0, sizeof(*out));
    if (js_intl_number_unit_options(ctx, options, &result.unit))
        return -1;
    notation = js_intl_string_option(ctx, options, "notation", notations, countof(notations), 0);
    if (notation < 0)
        goto fail;
    if (result.unit.style == QJS_INTL_NUMBER_CURRENCY && notation == QJS_INTL_NUMBER_STANDARD) {
        uint32_t digits;
        if (js_intl_error(ctx, qjs_intl_number_currency_digits(service,
                                  (QJSIntlBytes){result.unit.currency, 3}, &digits), "currency digits"))
            goto fail;
        minimum = maximum = digits;
    } else if (result.unit.style == QJS_INTL_NUMBER_PERCENT) {
        maximum = 0;
    }
    if (js_intl_set_digit_options(ctx, options, minimum, maximum,
                                    notation == QJS_INTL_NUMBER_COMPACT, &result.digits))
        goto fail;
    compact = js_intl_string_option(ctx, options, "compactDisplay", compact_displays,
                                      countof(compact_displays), 0);
    if (compact < 0)
        goto fail;
    grouping = grouping_option(ctx, options, notation == QJS_INTL_NUMBER_COMPACT
                                               ? QJS_INTL_NUMBER_GROUP_MIN2
                                               : QJS_INTL_NUMBER_GROUP_AUTO);
    if (grouping < 0)
        goto fail;
    sign = js_intl_string_option(ctx, options, "signDisplay", signs, countof(signs), 0);
    if (sign < 0)
        goto fail;
    result.notation = notation;
    result.compact_long = notation == QJS_INTL_NUMBER_COMPACT ? compact : 0;
    result.grouping = grouping;
    result.sign_display = sign;
    *out = result;
    return 0;
fail:
    js_free(ctx, result.unit.unit);
    return -1;
}
