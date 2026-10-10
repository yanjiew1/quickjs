/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DECIMAL_H
#define QJS_INTL_DECIMAL_H
#include "provider.h"

/* Internal C library, no JS or floating point conversion. The frontend owns
 * coercion to exact mathematical values and observable option validation.
 * ASCII grammar: [+-]?(digits[.digits?]?|.digits)([eE][+-]?digits)?
 * No whitespace, radix prefixes, Infinity or NaN in a finite decimal slice.
 * Input exponent and resulting coefficient exponent are bounded to +/-1e9.
 * Normalized leading zeros are removed; trailing zeros remain exact.
 */
#define QJS_INTL_DECIMAL_EXPONENT_LIMIT INT32_C(1000000000)
typedef struct QJSIntlDecimal {
    char *digits;                 /* owned ASCII digits, terminated by NUL */
    size_t length;                /* >=1; no leading zero except zero itself */
    int32_t exponent;             /* magnitude = integer(digits) * 10^exponent */
    uint8_t negative;             /* retained for zero */
} QJSIntlDecimal;

typedef enum QJSIntlRoundingMode {
    QJS_INTL_ROUND_CEIL = 0, QJS_INTL_ROUND_FLOOR,
    QJS_INTL_ROUND_EXPAND, QJS_INTL_ROUND_TRUNC,
    QJS_INTL_ROUND_HALF_CEIL, QJS_INTL_ROUND_HALF_FLOOR,
    QJS_INTL_ROUND_HALF_EXPAND, QJS_INTL_ROUND_HALF_TRUNC,
    QJS_INTL_ROUND_HALF_EVEN
} QJSIntlRoundingMode;
typedef enum QJSIntlRoundingType {
    QJS_INTL_ROUND_FRACTION = 0, QJS_INTL_ROUND_SIGNIFICANT,
    QJS_INTL_ROUND_MORE_PRECISION, QJS_INTL_ROUND_LESS_PRECISION
} QJSIntlRoundingType;
typedef enum QJSIntlTrailingZeroDisplay {
    QJS_INTL_TRAILING_ZERO_AUTO = 0, QJS_INTL_TRAILING_ZERO_STRIP_IF_INTEGER
} QJSIntlTrailingZeroDisplay;
typedef struct QJSIntlDecimalOptions {
    uint8_t minimum_integer_digits; /* 1..21 */
    uint8_t minimum_fraction_digits, maximum_fraction_digits; /* 0..100 */
    uint8_t minimum_significant_digits, maximum_significant_digits; /* 1..21 */
    uint16_t rounding_increment;    /* 1,2,5,10,20,25,50,100,200,250,500,
                                      1000,2000,2500,5000 */
    QJSIntlRoundingMode rounding_mode;
    QJSIntlRoundingType rounding_type;
    QJSIntlTrailingZeroDisplay trailing_zero_display;
    size_t maximum_output_length;   /* required nonzero caller resource bound */
} QJSIntlDecimalOptions;
typedef struct QJSIntlDecimalResult {
    QJSIntlDecimal rounded;         /* owned, includes sign and negative zero */
    char *text;                     /* owned unsigned plain ASCII, NUL at length */
    size_t length;
    size_t integer_digits;          /* before minimum integer padding */
    int32_t rounding_magnitude;     /* ECMA402 [[RoundingMagnitude]] */
} QJSIntlDecimalResult;

/* Allocator malloc/free required; realloc unused. Results are zeroed before
 * work and stay clearable on all failures. Outputs must be fresh and disjoint
 * from inputs. Clear with the same allocator. Calls borrow input slices only.
 * OUT_OF_RANGE exponent/output expansion -> OVERFLOW, syntax -> INVALID_ARGUMENT.
 */
QJSIntlStatus qjs_intl_decimal_parse(const QJSIntlAllocator *, QJSIntlBytes,
                                    QJSIntlDecimal *out);
QJSIntlStatus qjs_intl_decimal_from_value(const QJSIntlAllocator *,
                              const QJSIntlMathematicalValue *, QJSIntlDecimal *);
void qjs_intl_decimal_clear(const QJSIntlAllocator *, QJSIntlDecimal *);
void qjs_intl_decimal_result_clear(const QJSIntlAllocator *, QJSIntlDecimalResult *);
QJSIntlStatus qjs_intl_decimal_options_validate(const QJSIntlDecimalOptions *);

/* Raw operations apply the signed mode via GetUnsignedRoundingMode using
 * input.negative. Their text is unsigned, matching the spec's raw record.
 */
QJSIntlStatus qjs_intl_decimal_to_raw_fixed(const QJSIntlAllocator *,
    const QJSIntlDecimal *, unsigned int minimum_fraction, unsigned int maximum_fraction,
    unsigned int increment, QJSIntlRoundingMode, size_t maximum_output_length,
    QJSIntlDecimalResult *);
QJSIntlStatus qjs_intl_decimal_to_raw_precision(const QJSIntlAllocator *,
    const QJSIntlDecimal *, unsigned int minimum_precision, unsigned int maximum_precision,
    QJSIntlRoundingMode, size_t maximum_output_length, QJSIntlDecimalResult *);
QJSIntlStatus qjs_intl_decimal_format_numeric(const QJSIntlAllocator *,
    const QJSIntlDecimal *, const QJSIntlDecimalOptions *, QJSIntlDecimalResult *);
#endif
