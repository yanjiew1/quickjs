/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_DECIMAL_H
#define QJS_INTL_NUMBER_DECIMAL_H
#include "decimal.h"

/* Internal C library; the JS frontend has already performed any coercion.
 * This is not a replacement for ToPrimitive or StringNumericLiteral parsing.
 * The value slice borrows the result's owned text. Results cannot be
 * shallow-copied while in use; clear with the creating allocator. Rounding
 * consumers parse only when they need a coefficient.
 * Fresh output required. Errors leave an all-zero, clearable result.
 */
typedef struct QJSIntlNumberDecimal {
    QJSIntlMathematicalValue value;
    char *text;              /* NUL-terminated value.decimal backing */
} QJSIntlNumberDecimal;

/* Number branch of ECMA402 ToIntlMathematicalValue: finite nonzero
 * Numbers use Number::toString(x,10), not the exact binary fraction. The
 * existing js_dtoa free-format implementation supplies that shortest text.
 * -0, NaN and signed infinity have their distinct mathematical kinds.
 * Public NumberFormat and PluralRules select/selectRange use this route.
 */
QJSIntlStatus qjs_intl_number_to_intl_decimal(const QJSIntlAllocator *, double,
                                              QJSIntlNumberDecimal *);

/* Exact mathematical real value of finite binary64. No shortest conversion
 * or floating arithmetic is involved. Mathematical real(-0) is positive 0.
 * The RelativeTimeFormat caller retains its original Number for tense.
 * RelativeTimeFormat's direct PartitionNumberPattern(real(value)) uses this
 * route. NaN/infinity -> INVALID_ARGUMENT; frontend raises its own RangeError.
 * Unsupported double layout -> UNSUPPORTED for either conversion.
 */
QJSIntlStatus qjs_intl_number_to_real_decimal(const QJSIntlAllocator *, double,
                                              QJSIntlNumberDecimal *);
void qjs_intl_number_decimal_clear(const QJSIntlAllocator *, QJSIntlNumberDecimal *);
#endif
