/* Native number coercion and rounding options. ECMA-402 7ae78cf (2026-10-06).
 * Original implementation: no QuickJS-NG Intl source was available at a6b82a3. */
#include "intl-number.h"
#include "../../internal/number.h"
#include "../../internal/bigint.h"

static const char *const rounding_modes[] = {
    "ceil", "floor", "expand", "trunc", "halfCeil", "halfFloor",
    "halfExpand", "halfTrunc", "halfEven"
};
static const char *const rounding_priorities[] = {
    "auto", "morePrecision", "lessPrecision"
};
static const char *const trailing_zeros[] = { "auto", "stripIfInteger" };

int js_intl_set_digit_options(JSContext *ctx, JSValueConst options,
                              int mnfd_default, int mxfd_default, int compact,
                              JSIntlDigitOptions *d)
{
    JSValue mnfd = JS_UNDEFINED, mxfd = JS_UNDEFINED;
    JSValue mnsd = JS_UNDEFINED, mxsd = JS_UNDEFINED;
    int has_sd, has_fd, need_sd, need_fd, i, ret = -1;
    static const int increments[] = {
        1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000
    };
    memset(d, 0, sizeof(*d));
    d->minimum_fraction_digits = d->maximum_fraction_digits = -1;
    d->minimum_significant_digits = d->maximum_significant_digits = -1;
    if (js_intl_get_number_option(ctx, options, "minimumIntegerDigits",
                                  1, 21, 1, &d->minimum_integer_digits) < 0)
        goto done;
    /* Read these four raw values before rounding options or conversion. */
    mnfd = JS_GetPropertyStr(ctx, options, "minimumFractionDigits");
    if (JS_IsException(mnfd)) goto done;
    mxfd = JS_GetPropertyStr(ctx, options, "maximumFractionDigits");
    if (JS_IsException(mxfd)) goto done;
    mnsd = JS_GetPropertyStr(ctx, options, "minimumSignificantDigits");
    if (JS_IsException(mnsd)) goto done;
    mxsd = JS_GetPropertyStr(ctx, options, "maximumSignificantDigits");
    if (JS_IsException(mxsd)) goto done;
    if (js_intl_get_number_option(ctx, options, "roundingIncrement", 1, 5000,
                                  1, &d->rounding_increment) < 0)
        goto done;
    for (i = 0; i < countof(increments); i++)
        if (d->rounding_increment == increments[i]) break;
    if (i == countof(increments)) {
        JS_ThrowRangeError(ctx, "invalid roundingIncrement");
        goto done;
    }
    if (js_intl_get_string_option(ctx, options, "roundingMode", rounding_modes,
                                  countof(rounding_modes), JS_INTL_HALF_EXPAND,
                                  &d->rounding_mode) < 0 ||
        js_intl_get_string_option(ctx, options, "roundingPriority",
                                  rounding_priorities, countof(rounding_priorities),
                                  0, &d->rounding_priority) < 0 ||
        js_intl_get_string_option(ctx, options, "trailingZeroDisplay", trailing_zeros,
                                  countof(trailing_zeros), 0,
                                  &d->trailing_zero_display) < 0)
        goto done;
    if (d->rounding_increment != 1) mxfd_default = mnfd_default;
    has_sd = !JS_IsUndefined(mnsd) || !JS_IsUndefined(mxsd);
    has_fd = !JS_IsUndefined(mnfd) || !JS_IsUndefined(mxfd);
    need_sd = need_fd = 1;
    if (d->rounding_priority == 0) {
        need_sd = has_sd;
        if (need_sd || (!has_fd && compact)) need_fd = 0;
    }
    if (need_sd) {
        if (js_intl_default_number_option(ctx, mnsd, 1, 21, 1,
                                          &d->minimum_significant_digits) < 0 ||
            js_intl_default_number_option(ctx, mxsd,
                                          d->minimum_significant_digits, 21, 21,
                                          &d->maximum_significant_digits) < 0)
            goto done;
    }
    if (need_fd) {
        if (has_fd) {
            if (js_intl_default_number_option(ctx, mnfd, 0, 100, -1,
                                              &d->minimum_fraction_digits) < 0 ||
                js_intl_default_number_option(ctx, mxfd, 0, 100, -1,
                                              &d->maximum_fraction_digits) < 0)
                goto done;
            if (d->minimum_fraction_digits < 0)
                d->minimum_fraction_digits = min_int(mnfd_default,
                                                     d->maximum_fraction_digits);
            else if (d->maximum_fraction_digits < 0)
                d->maximum_fraction_digits = max_int(mxfd_default,
                                                     d->minimum_fraction_digits);
            else if (d->minimum_fraction_digits > d->maximum_fraction_digits) {
                JS_ThrowRangeError(ctx, "minimumFractionDigits exceeds maximumFractionDigits");
                goto done;
            }
        } else {
            d->minimum_fraction_digits = mnfd_default;
            d->maximum_fraction_digits = mxfd_default;
        }
    }
    if (!need_sd && !need_fd) {
        d->minimum_fraction_digits = d->maximum_fraction_digits = 0;
        d->minimum_significant_digits = 1;
        d->maximum_significant_digits = 2;
        d->rounding_type = JS_INTL_MORE_PRECISION;
        d->rounding_priority = 1;
    } else if (d->rounding_priority == 1) {
        d->rounding_type = JS_INTL_MORE_PRECISION;
    } else if (d->rounding_priority == 2) {
        d->rounding_type = JS_INTL_LESS_PRECISION;
    } else {
        d->rounding_type = has_sd ? JS_INTL_SIGNIFICANT : JS_INTL_FRACTION;
    }
    if (d->rounding_increment != 1) {
        if (d->rounding_type != JS_INTL_FRACTION) {
            JS_ThrowTypeError(ctx, "roundingIncrement requires fraction precision");
            goto done;
        }
        if (d->minimum_fraction_digits != d->maximum_fraction_digits) {
            JS_ThrowRangeError(ctx, "roundingIncrement requires fixed fraction precision");
            goto done;
        }
    }
    ret = 0;
 done:
    JS_FreeValue(ctx, mnfd); JS_FreeValue(ctx, mxfd);
    JS_FreeValue(ctx, mnsd); JS_FreeValue(ctx, mxsd);
    return ret;
}

static void repeat(DynBuf *b, int c, int count)
{
    while (count-- > 0) dbuf_putc(b, c);
}

int js_intl_digit_skeleton(JSContext *ctx, DynBuf *b,
                          const JSIntlDigitOptions *d)
{
    static const char *const modes[] = {
        "ceiling", "floor", "up", "down", "half-ceiling", "half-floor",
        "half-up", "half-down", "half-even"
    };
    if (d->rounding_increment != 1) {
        char increment[8];
        int len, point;
        len = snprintf(increment, sizeof(increment), "%d", d->rounding_increment);
        point = len - d->maximum_fraction_digits;
        dbuf_putstr(b, "precision-increment/");
        if (point > 0) {
            dbuf_put(b, (const uint8_t *)increment, point);
            if (point < len) {
                dbuf_putc(b, '.');
                dbuf_put(b, (const uint8_t *)increment + point, len - point);
            }
        } else {
            dbuf_putstr(b, "0.");
            repeat(b, '0', -point);
            dbuf_put(b, (const uint8_t *)increment, len);
        }
    } else if (d->rounding_type == JS_INTL_SIGNIFICANT) {
        repeat(b, '@', d->minimum_significant_digits);
        repeat(b, '#', d->maximum_significant_digits - d->minimum_significant_digits);
    } else {
        dbuf_putc(b, '.');
        repeat(b, '0', d->minimum_fraction_digits);
        repeat(b, '#', d->maximum_fraction_digits - d->minimum_fraction_digits);
        if (d->rounding_type == JS_INTL_MORE_PRECISION ||
            d->rounding_type == JS_INTL_LESS_PRECISION) {
            dbuf_putc(b, '/');
            repeat(b, '@', d->minimum_significant_digits);
            repeat(b, '#', d->maximum_significant_digits - d->minimum_significant_digits);
            dbuf_putc(b, d->rounding_type == JS_INTL_MORE_PRECISION ? 'r' : 's');
        }
    }
    if (d->trailing_zero_display) dbuf_putstr(b, "/w");
    dbuf_printf(b, " rounding-mode-%s integer-width/+", modes[d->rounding_mode]);
    repeat(b, '0', d->minimum_integer_digits);
    dbuf_putc(b, ' ');
    if (dbuf_error(b)) { JS_ThrowOutOfMemory(ctx); return -1; }
    return 0;
}

void js_intl_free_mathematical_value(JSContext *ctx, JSIntlMathematicalValue *mv)
{
    js_free(ctx, mv->decimal);
    memset(mv, 0, sizeof(*mv));
}

int js_intl_to_mathematical_value(JSContext *ctx, JSValueConst value,
                                 JSIntlMathematicalValue *mv)
{
    JSValue prim, text = JS_UNDEFINED;
    const char *s = NULL, *begin;
    size_t len, n;
    double number;
    int ret = -1;
    memset(mv, 0, sizeof(*mv));
    prim = JS_ToPrimitive(ctx, value, HINT_NUMBER);
    if (JS_IsException(prim)) return -1;
    if (JS_IsBigInt(ctx, prim)) {
        text = JS_ToString(ctx, prim);
        if (JS_IsException(text)) goto done;
    } else {
        /* The engine's StringNumericLiteral parser also handles Unicode
           whitespace, prefixes, NULs, invalid syntax, overflow and underflow. */
        if (JS_ToFloat64(ctx, &number, prim) < 0) goto done;
        if (isnan(number)) { mv->kind = JS_INTL_MV_NAN; ret = 0; goto done; }
        if (isinf(number)) {
            mv->kind = number < 0 ? JS_INTL_MV_NEGATIVE_INFINITY :
                                    JS_INTL_MV_POSITIVE_INFINITY;
            ret = 0; goto done;
        }
        if (number == 0) {
            if (signbit(number)) mv->kind = JS_INTL_MV_NEGATIVE_ZERO;
            else {
                mv->decimal = js_intl_strdup(ctx, "0");
                if (!mv->decimal) goto done;
                mv->length = 1;
            }
            ret = 0; goto done;
        }
        if (JS_IsString(prim)) {
            s = JS_ToCStringLen(ctx, &len, prim);
            if (!s) goto done;
            begin = s + skip_spaces(s);
            /* ToNumber above has already checked the complete grammar. */
            if (begin[0] == '0' && (begin[1] == 'x' || begin[1] == 'X' ||
                                   begin[1] == 'b' || begin[1] == 'B' ||
                                   begin[1] == 'o' || begin[1] == 'O')) {
                JSValue integer = JS_ToBigInt(ctx, prim);
                if (JS_IsException(integer)) goto done;
                text = JS_ToString(ctx, integer);
                JS_FreeValue(ctx, integer);
                if (JS_IsException(text)) goto done;
                JS_FreeCString(ctx, s); s = NULL;
            } else {
                n = 0;
                while (begin[n] && ((begin[n] >= '0' && begin[n] <= '9') ||
                       begin[n] == '+' || begin[n] == '-' || begin[n] == '.' ||
                       begin[n] == 'e' || begin[n] == 'E')) n++;
                mv->decimal = js_malloc(ctx, n + 1);
                if (!mv->decimal) goto done;
                memcpy(mv->decimal, begin, n); mv->decimal[n] = 0;
                mv->length = n;
                ret = 0; goto done;
            }
        } else {
            text = JS_ToString(ctx, JS_NewFloat64(ctx, number));
            if (JS_IsException(text)) goto done;
        }
    }
    s = JS_ToCStringLen(ctx, &len, text);
    if (!s) goto done;
    mv->decimal = js_malloc(ctx, len + 1);
    if (!mv->decimal) goto done;
    memcpy(mv->decimal, s, len + 1); mv->length = len;
    ret = 0;
 done:
    if (s) JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, text); JS_FreeValue(ctx, prim);
    if (ret < 0) js_intl_free_mathematical_value(ctx, mv);
    return ret;
}

static const char *mv_decimal(const JSIntlMathematicalValue *mv)
{
    static const char *const special[] = { NULL, "NaN", "Infinity", "-Infinity", "-0" };
    return mv->kind == JS_INTL_MV_FINITE ? mv->decimal : special[mv->kind];
}

int js_intl_format_mathematical_value(JSContext *ctx,
                                    const UNumberFormatter *formatter,
                                    const JSIntlMathematicalValue *mv,
                                    UFormattedNumber *result, UErrorCode *status)
{
    const char *decimal = mv_decimal(mv);
    size_t length = mv->kind == JS_INTL_MV_FINITE ? mv->length : strlen(decimal);
    if (length > INT32_MAX) { JS_ThrowOutOfMemory(ctx); return -1; }
    unumf_formatDecimal(formatter, decimal, (int32_t)length, result, status);
    return js_intl_icu_error(ctx, *status, "format number");
}

int js_intl_format_mathematical_range(JSContext *ctx,
                                    const UNumberRangeFormatter *formatter,
                                    const JSIntlMathematicalValue *x,
                                    const JSIntlMathematicalValue *y,
                                    UFormattedNumberRange *result,
                                    UErrorCode *status)
{
    const char *a = mv_decimal(x), *b = mv_decimal(y);
    size_t alen = x->kind == JS_INTL_MV_FINITE ? x->length : strlen(a);
    size_t blen = y->kind == JS_INTL_MV_FINITE ? y->length : strlen(b);
    if (alen > INT32_MAX || blen > INT32_MAX) { JS_ThrowOutOfMemory(ctx); return -1; }
    unumrf_formatDecimalRange(formatter, a, (int32_t)alen, b, (int32_t)blen,
                             result, status);
    return js_intl_icu_error(ctx, *status, "format number range");
}
