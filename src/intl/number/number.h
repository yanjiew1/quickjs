/* Compile-time number service, independent of engine values and JS coercion.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_SERVICE_H
#define QJS_INTL_NUMBER_SERVICE_H
#include "../common/decimal.h"
#include "../common/format.h"

typedef enum QJSIntlNumberGrouping {
    QJS_INTL_NUMBER_GROUP_OFF,
    QJS_INTL_NUMBER_GROUP_AUTO,
    QJS_INTL_NUMBER_GROUP_ALWAYS,
    QJS_INTL_NUMBER_GROUP_MIN2
} QJSIntlNumberGrouping;

typedef enum QJSIntlNumberSign {
    QJS_INTL_NUMBER_SIGN_AUTO,
    QJS_INTL_NUMBER_SIGN_NEVER,
    QJS_INTL_NUMBER_SIGN_ALWAYS,
    QJS_INTL_NUMBER_SIGN_EXCEPT_ZERO,
    QJS_INTL_NUMBER_SIGN_NEGATIVE
} QJSIntlNumberSign;

typedef enum QJSIntlNumberNotation {
    QJS_INTL_NUMBER_STANDARD,
    QJS_INTL_NUMBER_SCIENTIFIC,
    QJS_INTL_NUMBER_ENGINEERING,
    QJS_INTL_NUMBER_COMPACT
} QJSIntlNumberNotation;

typedef enum QJSIntlNumberStyle {
    QJS_INTL_NUMBER_DECIMAL,
    QJS_INTL_NUMBER_PERCENT,
    QJS_INTL_NUMBER_CURRENCY
} QJSIntlNumberStyle;

typedef enum QJSIntlNumberCurrencyDisplay {
    QJS_INTL_NUMBER_CURRENCY_SYMBOL,
    QJS_INTL_NUMBER_CURRENCY_NARROW,
    QJS_INTL_NUMBER_CURRENCY_CODE,
    QJS_INTL_NUMBER_CURRENCY_NAME
} QJSIntlNumberCurrencyDisplay;

typedef enum QJSIntlNumberCurrencySign {
    QJS_INTL_NUMBER_CURRENCY_STANDARD,
    QJS_INTL_NUMBER_CURRENCY_ACCOUNTING
} QJSIntlNumberCurrencySign;

typedef struct QJSIntlNumberOptions {
    QJSIntlNumberStyle style;
    QJSIntlNumberNotation notation;
    QJSIntlNumberGrouping grouping;
    QJSIntlNumberSign sign_display;
    unsigned int compact_long;
    QJSIntlDecimalOptions digits;
    size_t maximum_output_length;
    QJSIntlBytes currency; /* Canonical uppercase code; copied when used. */
    QJSIntlNumberCurrencyDisplay currency_display;
    QJSIntlNumberCurrencySign currency_sign;
} QJSIntlNumberOptions;

typedef struct QJSIntlNumberService QJSIntlNumberService;
typedef struct QJSIntlNumberFormatter QJSIntlNumberFormatter;

/* This internal service implements decimal/percent formatting with standard,
   scientific, engineering and compact notation, plus standard-notation
   currency names and standard/scientific/engineering currency symbols/codes.
   Compact currency, nonstandard currency names, units and ranges are
   still required before the complete Intl.NumberFormat binding can register.
   Bindings must not register NumberFormat on the strength of this subset.

   Copy the allocator; malloc/free required, realloc unused. Its callbacks and
   opaque state outlive corresponding formatters and result clearing. Opening
   validates the selected immutable profile. Errors clear *out; NULL closes. */
QJSIntlStatus qjs_intl_number_open(const QJSIntlAllocator *, QJSIntlNumberService **out);
void qjs_intl_number_close(QJSIntlNumberService *);
/* Borrow canonical sorted extension-free tags, excluding root, until close.
   No locale matching occurs. Failure clears both outputs. */
QJSIntlStatus qjs_intl_number_available(const QJSIntlNumberService *, const QJSIntlBytes **,
                                        size_t *count);
/* Exact available-locale index; no matching or Unicode extension negotiation.
   Borrow the default numeric system name from immutable compiled storage. */
QJSIntlStatus qjs_intl_number_default_system(const QJSIntlNumberService *, size_t locale,
                                             QJSIntlBytes *out);
/* CurrencyDigits for a canonical uppercase code, including unknown codes.
   Return regular fraction digits, never cash digits or a rounding increment.
   The frontend uses this to resolve digit options before formatter_open.
   Allocation-free; failure resets output to zero. */
QJSIntlStatus qjs_intl_number_currency_digits(const QJSIntlNumberService *, QJSIntlBytes code,
                                              uint32_t *out);
/* Empty system chooses the locale default. An explicit system is exact;
   absent/algorithmic definitions return UNSUPPORTED, without fallback. The
   frontend resolves defaults, options and Unicode extensions before opening.
   A formatter may outlive its service; it retains no service-owned arrays. */
QJSIntlStatus qjs_intl_number_formatter_open(const QJSIntlNumberService *, size_t locale,
                                             QJSIntlBytes system, const QJSIntlNumberOptions *,
                                             QJSIntlNumberFormatter **out);
void qjs_intl_number_formatter_close(QJSIntlNumberFormatter *);
/* Borrow an already coerced exact mathematical value. Round once at the final
   notation scale (after percent multiplication), select sign from the rounded value,
   localize the numeric body, and apply its selected outer pattern.
   Fresh, disjoint output required; failures reset it. Results own text/parts
   and survive formatter/service close. Clear using the creating allocator. */
QJSIntlStatus qjs_intl_number_format(const QJSIntlNumberFormatter *,
                                     const QJSIntlMathematicalValue *, QJSIntlFormatted *);
void qjs_intl_number_result_clear(const QJSIntlAllocator *, QJSIntlFormatted *);
#endif
