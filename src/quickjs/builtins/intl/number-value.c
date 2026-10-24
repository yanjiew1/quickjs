/* ECMA402 730ca2b88c305862db7be2f83c510dcca33ab48c, 2026-10-19:
 * ToIntlMathematicalValue. Copyright (c) 2026 Yan-Jie Wang. MIT license. */
#include "number-value.h"
#include "../intl.h"
#include "../../internal/bigint.h"
#include "../../internal/number.h"
#include "../../internal/runtime.h"
#include "../../value/conversion.h"
#include <math.h>

static int exact_decimal(JSContext *ctx, const char *text, size_t length,
                         QJSIntlNumberDecimal *out)
{
    QJSIntlStatus status;
    out->text = js_strndup(ctx, text, length);
    if (!out->text)
        return -1;
    out->value.decimal = (QJSIntlBytes){out->text, length};
    status = qjs_intl_decimal_validate(out->value.decimal);
    if (js_intl_error(ctx, status, "mathematical value"))
        return -1;
    return 0;
}

int js_intl_to_mathematical_value(JSContext *ctx, JSValueConst value, QJSIntlNumberDecimal *out)
{
    QJSIntlAllocator allocator = js_intl_allocator(ctx->rt);
    JSValue primitive, text = JS_UNDEFINED;
    const char *bytes = NULL;
    size_t length;
    double number;
    int result = -1;
    memset(out, 0, sizeof(*out));
    primitive = JS_ToPrimitive(ctx, value, HINT_NUMBER);
    if (JS_IsException(primitive))
        return -1;
    if (JS_IsBigInt(ctx, primitive)) {
        text = JS_ToString(ctx, primitive);
    } else {
        /* For strings this validates the whole StringNumericLiteral grammar
           and implements its RoundMVResult infinity/zero boundaries. It must
           not replace a finite nonzero string's exact mathematical value. */
        if (JS_ToFloat64(ctx, &number, primitive))
            goto done;
        if (!JS_IsString(primitive) || !isfinite(number) || number == 0) {
            result = js_intl_error(ctx, qjs_intl_number_to_intl_decimal(&allocator, number, out),
                                   "mathematical value");
            goto done;
        }
        bytes = JS_ToCStringLen(ctx, &length, primitive);
        if (!bytes)
            goto done;
        {
            const char *begin = bytes + skip_spaces(bytes);
            if (begin[0] == '0' && (begin[1] == 'x' || begin[1] == 'X' || begin[1] == 'o' ||
                                    begin[1] == 'O' || begin[1] == 'b' || begin[1] == 'B')) {
                JSValue integer = JS_ToBigInt(ctx, primitive);
                if (JS_IsException(integer))
                    goto done;
                text = JS_ToString(ctx, integer);
                JS_FreeValue(ctx, integer);
            } else {
                size_t count = 0;
                /* Grammar is already validated. Stop before trailing Unicode
                   whitespace rather than interpreting UTF-8 as decimal text. */
                while ((begin[count] >= '0' && begin[count] <= '9') || begin[count] == '+' ||
                       begin[count] == '-' || begin[count] == '.' || begin[count] == 'e' ||
                       begin[count] == 'E')
                    count++;
                result = exact_decimal(ctx, begin, count, out);
                goto done;
            }
        }
        JS_FreeCString(ctx, bytes);
        bytes = NULL;
    }
    if (JS_IsException(text))
        goto done;
    bytes = JS_ToCStringLen(ctx, &length, text);
    if (bytes)
        result = exact_decimal(ctx, bytes, length, out);
done:
    JS_FreeCString(ctx, bytes);
    JS_FreeValue(ctx, text);
    JS_FreeValue(ctx, primitive);
    if (result)
        qjs_intl_number_decimal_clear(&allocator, out);
    return result;
}
