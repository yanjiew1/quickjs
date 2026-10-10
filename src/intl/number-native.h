/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_NATIVE_H
#define QJS_INTL_NUMBER_NATIVE_H
#include "decimal.h"
#include "plural-native.h"

typedef enum QJSIntlNumberStyle {
    QJS_INTL_NUMBER_DECIMAL = 0, QJS_INTL_NUMBER_PERCENT,
    QJS_INTL_NUMBER_CURRENCY, QJS_INTL_NUMBER_UNIT
} QJSIntlNumberStyle;
typedef enum QJSIntlNumberNotation {
    QJS_INTL_NUMBER_STANDARD = 0, QJS_INTL_NUMBER_SCIENTIFIC,
    QJS_INTL_NUMBER_ENGINEERING, QJS_INTL_NUMBER_COMPACT
} QJSIntlNumberNotation;
typedef enum QJSIntlNumberGrouping {
    QJS_INTL_NUMBER_GROUP_OFF = 0, QJS_INTL_NUMBER_GROUP_AUTO,
    QJS_INTL_NUMBER_GROUP_ALWAYS, QJS_INTL_NUMBER_GROUP_MIN2
} QJSIntlNumberGrouping;
typedef enum QJSIntlNumberSign {
    QJS_INTL_NUMBER_SIGN_AUTO = 0, QJS_INTL_NUMBER_SIGN_NEVER,
    QJS_INTL_NUMBER_SIGN_ALWAYS, QJS_INTL_NUMBER_SIGN_EXCEPT_ZERO,
    QJS_INTL_NUMBER_SIGN_NEGATIVE
} QJSIntlNumberSign;
typedef enum QJSIntlCurrencyDisplay {
    QJS_INTL_CURRENCY_SYMBOL = 0, QJS_INTL_CURRENCY_NARROW_SYMBOL,
    QJS_INTL_CURRENCY_CODE, QJS_INTL_CURRENCY_NAME
} QJSIntlCurrencyDisplay;
typedef enum QJSIntlUnitDisplay {
    QJS_INTL_UNIT_LONG = 0, QJS_INTL_UNIT_SHORT, QJS_INTL_UNIT_NARROW
} QJSIntlUnitDisplay;
typedef enum QJSIntlCompactDisplay {
    QJS_INTL_COMPACT_SHORT = 0, QJS_INTL_COMPACT_LONG
} QJSIntlCompactDisplay;
typedef struct QJSIntlNumberOptions {
    QJSIntlNumberStyle style;
    QJSIntlNumberNotation notation;
    QJSIntlNumberGrouping grouping;
    QJSIntlNumberSign sign_display;
    QJSIntlCurrencyDisplay currency_display;
    QJSIntlUnitDisplay unit_display;
    QJSIntlCompactDisplay compact_display;
    uint8_t currency_accounting;
    QJSIntlBytes currency;          /* validated uppercase ISO4217-shaped code */
    QJSIntlBytes unit;              /* validated sanctioned unit identifier */
    QJSIntlDecimalOptions digits;   /* frontend-resolved defaults, no getters */
    size_t maximum_output_length;   /* UTF16 code units; required, nonzero */
} QJSIntlNumberOptions;

enum {
    QJS_INTL_NUMBER_SYMBOL_DECIMAL = 0, QJS_INTL_NUMBER_SYMBOL_GROUP,
    QJS_INTL_NUMBER_SYMBOL_PLUS, QJS_INTL_NUMBER_SYMBOL_MINUS,
    QJS_INTL_NUMBER_SYMBOL_PERCENT, QJS_INTL_NUMBER_SYMBOL_EXPONENTIAL,
    QJS_INTL_NUMBER_SYMBOL_INFINITY, QJS_INTL_NUMBER_SYMBOL_NAN,
    QJS_INTL_NUMBER_SYMBOL_CURRENCY_DECIMAL,
    QJS_INTL_NUMBER_SYMBOL_CURRENCY_GROUP,
    QJS_INTL_NUMBER_SYMBOL_COUNT
};
typedef struct QJSIntlNumberPattern {
    QJSIntlBytes zero, negative, positive;
    uint8_t primary_group, secondary_group; /* zero means no pattern grouping */
} QJSIntlNumberPattern;
typedef struct QJSIntlNumberScalarRange {
    uint32_t first, last, flags;    /* bit0 L; bit1 !S&&!Z; bit2 Nd */
} QJSIntlNumberScalarRange;
/* Frontend/data adapter selects short/long before open. The decimal CLDR
 * notation pattern is shared by all four styles. Each threshold is 10^m;
 * exponent is m-(number of zero placeholders)+1. A CLDR pattern consisting
 * only of "0" has exponent0 and deliberately disables compact notation.
 * Nonzero rows contain scalar UTF8 notation templates: {number} zero or one
 * times, with affix text classified as compact and surrounding whitespace
 * retained as literal. Exact count="1" overrides precede cardinal rules.
 */
typedef struct QJSIntlNumberCompact {
    int32_t magnitude, exponent;
    QJSIntlBytes patterns[QJS_INTL_PLURAL_CATEGORY_COUNT];
    QJSIntlBytes exact_one;         /* optional LDML count="1" override */
} QJSIntlNumberCompact;
typedef struct QJSIntlNumberData {
    uint32_t digits[10];            /* distinct Unicode scalars, numeric radix10 */
    QJSIntlBytes symbols[QJS_INTL_NUMBER_SYMBOL_COUNT];
    uint8_t minimum_grouping_digits; /* CLDR 1..9 */
    QJSIntlNumberPattern pattern;   /* selected style; unit/name uses decimal */
    QJSIntlNumberPattern alpha_pattern; /* optional alphaNextToNumber variant */
    QJSIntlBytes currency_symbol, currency_narrow_symbol;
    QJSIntlBytes currency_names[QJS_INTL_PLURAL_CATEGORY_COUNT];
    QJSIntlBytes currency_name_patterns[QJS_INTL_PLURAL_CATEGORY_COUNT];
    QJSIntlBytes unit_patterns[QJS_INTL_PLURAL_CATEGORY_COUNT];
    QJSIntlBytes before_currency, after_currency; /* CLDR spacing insertions */
    const QJSIntlNumberScalarRange *classes;
    size_t class_count;
    const QJSIntlNumberCompact *compact;
    size_t compact_count;         /* increasing unique nonnegative magnitude */
    QJSIntlBytes approximately, range; /* optional exact raw LDML patterns */
    const QJSIntlNativePlural *cardinal; /* borrowed until close; see below */
} QJSIntlNumberData;
typedef struct QJSIntlNativeNumber QJSIntlNativeNumber;

/* No locale matching, JS coercion/getters, floating point, ICU or runtime
 * XML/JSON. All strings are UTF8 scalar slices; templates use {number},
 * {currency}, {percentSign}, {minusSign}, {plusSign}, no literal braces.
 * Open copies options, all strings and Unicode class ranges. A cardinal
 * handle is borrowed until close for unit/name/compact, otherwise optional.
 * Caller must close NumberFormat before its borrowed cardinal handle.
 * Results are owned by this handle's explicit malloc/free allocator; realloc
 * is unused. Failure leaves a clearable zero output. Fresh results required.
 * Compact rows and all their strings are copied. Missing compact data returns
 * UNSUPPORTED. Compact rounding defaults are resolved by the frontend.
 * Unit data contains the final six templates for a sanctioned simple or
 * compound identifier. Compound templates are composed by the data adapter;
 * grammar and denominator labels are never guessed by the generic engine.
 */
QJSIntlStatus qjs_intl_native_number_open(const QJSIntlAllocator *,
    const QJSIntlNumberOptions *, const QJSIntlNumberData *, QJSIntlNativeNumber **);
void qjs_intl_native_number_close(QJSIntlNativeNumber *);
void qjs_intl_native_number_result_clear(const QJSIntlNativeNumber *, QJSIntlFormatted *);
QJSIntlStatus qjs_intl_native_number_resolved_options(const QJSIntlNativeNumber *,
                                                     QJSIntlNumberOptions *);
QJSIntlStatus qjs_intl_native_number_format(const QJSIntlNativeNumber *,
    const QJSIntlMathematicalValue *, QJSIntlFormatted *);
/* Standard decimal only: render already rounded plain unsigned ASCII text
 * using rounded.negative, retained fractional zeros, localized digits,
 * grouping and sign patterns. No rounding or percent scaling. RT uses this
 * with the default signDisplay auto and its own shared decimal result.
 */
QJSIntlStatus qjs_intl_native_number_format_rounded(const QJSIntlNativeNumber *,
    const QJSIntlDecimalResult *, QJSIntlFormatted *);
/* NaN -> INVALID_ARGUMENT for frontend RangeError. Equal formatted strings
 * use the locale approximation pattern with every source shared. Distinct
 * strings use exact locale range pattern with endpoint source annotations.
 * CollapseNumberRange intentionally preserves both complete endpoints, an
 * explicitly conforming implementation in pinned16.5.21. Missing range data
 * returns UNSUPPORTED; no guessed joins or ambiguity-producing collapse.
 */
QJSIntlStatus qjs_intl_native_number_format_range(const QJSIntlNativeNumber *,
    const QJSIntlMathematicalValue *, const QJSIntlMathematicalValue *, QJSIntlFormatted *);
/* Allocation-free template/UTF8 gate used by the authoritative reader.
 * Modes0general,1required number-only,2currency name,3unit number-optional.
 * CLDR singular/dual unit forms may omit the numeric placeholder.
 */
int qjs_intl_number_template_validate(QJSIntlBytes, unsigned int unit_mode);
/* Allocation-free compact grammar gate: optional {number}, no other tokens;
 * nonzero exponent requires nonempty affix text. */
int qjs_intl_number_compact_template_validate(QJSIntlBytes, int32_t exponent);
#endif
