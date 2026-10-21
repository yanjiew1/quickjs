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
    QJS_INTL_NUMBER_CURRENCY,
    QJS_INTL_NUMBER_UNIT
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

typedef enum QJSIntlNumberUnitDisplay {
    QJS_INTL_NUMBER_UNIT_SHORT,
    QJS_INTL_NUMBER_UNIT_NARROW,
    QJS_INTL_NUMBER_UNIT_LONG
} QJSIntlNumberUnitDisplay;

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
    QJSIntlBytes unit; /* Sanctioned single/division identifier; not retained. */
    QJSIntlNumberUnitDisplay unit_display;
} QJSIntlNumberOptions;

typedef struct QJSIntlNumberService QJSIntlNumberService;
typedef struct QJSIntlNumberFormatter QJSIntlNumberFormatter;

/* This internal service implements decimal/percent/currency and sanctioned units
   with standard, scientific, engineering and compact notation, including ranges.
   Observable constructor/options/coercion, JS result conversion and bound
   function ownership belong to the engine binding.

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
/* Test an exact canonical system identifier against this service's capabilities,
   not merely numbering metadata. No allocation or fallback; unknown, reserved
   (native/traditio/finance) and algorithmic systems produce supported=0, OK.
   Empty names are unsupported. Reset supported on failure. */
QJSIntlStatus qjs_intl_number_system_supported(const QJSIntlNumberService *, size_t locale,
                                               QJSIntlBytes name, int *supported);
/* Global executable repertoire: supported in at least one admitted locale.
   No allocation or metadata-only/algorithmic entries. next enumerates sorted
   unique names borrowed until close. Start cursor at zero; success/exhaustion
   advances it, other errors preserve it. Exhaustion is UNSUPPORTED. Non-NULL
   outputs are cleared on errors; callers use fresh disjoint output storage. */
QJSIntlStatus qjs_intl_number_system_available(const QJSIntlNumberService *,
                                               QJSIntlBytes name, int *out);
QJSIntlStatus qjs_intl_number_system_next(const QJSIntlNumberService *,
                                          uint32_t *cursor, QJSIntlBytes *out);
/* CurrencyDigits for a canonical uppercase code, including unknown codes.
   Return regular fraction digits, never cash digits or a rounding increment.
   The frontend uses this to resolve digit options before formatter_open.
   Allocation-free; failure resets output to zero. */
QJSIntlStatus qjs_intl_number_currency_digits(const QJSIntlNumberService *, QJSIntlBytes code,
                                              uint32_t *out);
/* Currency-code formatting accepts every three-letter uppercase code, with
   the standard unknown-code symbol/name/fraction fallback. No allocation;
   malformed codes are OK/false and operational errors clear output. */
QJSIntlStatus qjs_intl_number_currency_supported(const QJSIntlNumberService *,
                                                 QJSIntlBytes code, int *out);
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
/* Format both already-coerced endpoints independently. NaN is INVALID_ARGUMENT
   (the binding throws RangeError); descending ranges are allowed. Displayed
   text equality, including equal inputs/infinities, uses approximation with
   shared sources. Other results preserve both endpoints without collapsing
   parts. Native prefix approximation is an ILND policy; see range.h.
   Ownership, bounds and result lifetime follow format's disjoint contract. */
QJSIntlStatus qjs_intl_number_format_range(const QJSIntlNumberFormatter *,
                                         const QJSIntlMathematicalValue *start,
                                         const QJSIntlMathematicalValue *end,
                                         QJSIntlFormattedRange *);
/* Range results own text and source-tagged parts. Clear with the creating
   allocator; NULL and already-cleared results are harmless. */
void qjs_intl_number_range_result_clear(const QJSIntlAllocator *, QJSIntlFormattedRange *);
#endif
