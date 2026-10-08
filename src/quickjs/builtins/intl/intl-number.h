/* Native ECMA-402 number algorithms; ICU types remain private. */
#ifndef QUICKJS_INTL_NUMBER_H
#define QUICKJS_INTL_NUMBER_H
#include "intl-internal.h"
#ifdef CONFIG_INTL
#include <unicode/unumberformatter.h>
#include <unicode/unumberrangeformatter.h>
enum JSIntlRoundingMode { JS_INTL_CEIL, JS_INTL_FLOOR, JS_INTL_EXPAND,
    JS_INTL_TRUNC, JS_INTL_HALF_CEIL, JS_INTL_HALF_FLOOR,
    JS_INTL_HALF_EXPAND, JS_INTL_HALF_TRUNC, JS_INTL_HALF_EVEN };
enum JSIntlRoundingType { JS_INTL_FRACTION, JS_INTL_SIGNIFICANT,
    JS_INTL_MORE_PRECISION, JS_INTL_LESS_PRECISION };
typedef struct JSIntlDigitOptions {
    int minimum_integer_digits, minimum_fraction_digits, maximum_fraction_digits;
    int minimum_significant_digits, maximum_significant_digits;
    int rounding_increment, rounding_mode, rounding_type, rounding_priority;
    int trailing_zero_display;
} JSIntlDigitOptions;
enum JSIntlMVKind { JS_INTL_MV_FINITE, JS_INTL_MV_NAN,
    JS_INTL_MV_POSITIVE_INFINITY, JS_INTL_MV_NEGATIVE_INFINITY,
    JS_INTL_MV_NEGATIVE_ZERO };
typedef struct JSIntlMathematicalValue {
    enum JSIntlMVKind kind;
    char *decimal; /* owned, ASCII exact decimal, including exponent */
    size_t length;
} JSIntlMathematicalValue;
int js_intl_set_digit_options(JSContext *, JSValueConst, int, int, int,
                              JSIntlDigitOptions *);
int js_intl_digit_skeleton(JSContext *, DynBuf *, const JSIntlDigitOptions *);
int js_intl_to_mathematical_value(JSContext *, JSValueConst,
                                 JSIntlMathematicalValue *);
void js_intl_free_mathematical_value(JSContext *, JSIntlMathematicalValue *);
int js_intl_format_mathematical_value(JSContext *, const UNumberFormatter *,
                                    const JSIntlMathematicalValue *,
                                    UFormattedNumber *, UErrorCode *);
int js_intl_format_mathematical_range(JSContext *, const UNumberRangeFormatter *,
                                    const JSIntlMathematicalValue *,
                                    const JSIntlMathematicalValue *,
                                    UFormattedNumberRange *, UErrorCode *);
JSValue js_intl_number_format_value(JSContext *, JSValueConst, JSValueConst,
                                  const JSIntlMathematicalValue *, int);
JSValue js_intl_number_to_locale_string(JSContext *, JSValueConst,
                                       JSValueConst, JSValueConst);
int js_intl_number_unit_valid(const char *);
JSValue js_intl_number_parts(JSContext *, const UFormattedValue *,
                             const JSIntlMathematicalValue *,
                             const JSIntlMathematicalValue *, int);
#ifdef CONFIG_TEMPORAL
JSValue js_intl_temporal_duration_to_locale_string(JSContext *, JSValueConst,
                                                   JSValueConst, JSValueConst);
#endif
int js_intl_init_number_format(JSContext *, JSValueConst);
int js_intl_init_duration_format(JSContext *, JSValueConst);
#endif /* CONFIG_INTL */
#endif
