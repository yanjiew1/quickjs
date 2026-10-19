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
