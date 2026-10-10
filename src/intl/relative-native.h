/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_RELATIVE_NATIVE_H
#define QJS_INTL_RELATIVE_NATIVE_H
#include "plural-native.h"

typedef enum QJSIntlRelativeUnit {
    QJS_INTL_RELATIVE_SECOND = 0, QJS_INTL_RELATIVE_MINUTE,
    QJS_INTL_RELATIVE_HOUR, QJS_INTL_RELATIVE_DAY, QJS_INTL_RELATIVE_WEEK,
    QJS_INTL_RELATIVE_MONTH, QJS_INTL_RELATIVE_QUARTER, QJS_INTL_RELATIVE_YEAR,
    QJS_INTL_RELATIVE_UNIT_COUNT
} QJSIntlRelativeUnit;
typedef enum QJSIntlRelativeStyle {
    QJS_INTL_RELATIVE_LONG = 0, QJS_INTL_RELATIVE_SHORT, QJS_INTL_RELATIVE_NARROW
} QJSIntlRelativeStyle;
typedef enum QJSIntlRelativeNumeric {
    QJS_INTL_RELATIVE_ALWAYS = 0, QJS_INTL_RELATIVE_AUTO
} QJSIntlRelativeNumeric;
typedef struct QJSIntlRelativeOptions {
    QJSIntlRelativeStyle style;
    QJSIntlRelativeNumeric numeric;
    size_t maximum_output_length; /* explicit resource bound, UTF16 units */
} QJSIntlRelativeOptions;
typedef struct QJSIntlRelativeLiteral {
    QJSIntlRelativeUnit unit;
    int32_t offset;
    QJSIntlBytes text; /* UTF8 scalar text; not a pattern */
} QJSIntlRelativeLiteral;
typedef struct QJSIntlRelativeData {
    /* Selected style, [unit][past=0/future=1][plural category].
     * Missing category entries must already be flattened to other. */
    QJSIntlBytes patterns[8][2][6];
    const QJSIntlRelativeLiteral *literals; /* sorted (unit, signed offset) */
    size_t literal_count;
} QJSIntlRelativeData;
typedef struct QJSIntlRelativeNumberBridge {
    void *opaque;
    /* Borrow raw for this call only; no second rounding. The shared native
     * NumberFormat handle must use standard decimal, signDisplay=auto,
     * selected locale/numbering system, default grouping. Returned number
     * parts partition text. Retain raw.rounded.negative, per the pinned
     * signed mathematical argument. RT normalizes original -0 to 0 after
     * choosing past; negative nonzero rounded to zero retains its sign.
     * Results remain releasable on failure. */
    QJSIntlStatus (*format_rounded)(void *, const QJSIntlDecimalResult *,
                                    QJSIntlFormatted *);
    void (*clear)(void *, QJSIntlFormatted *);
} QJSIntlRelativeNumberBridge;
typedef struct QJSIntlNativeRelative QJSIntlNativeRelative;

/* No provider internals or JS coercion. Open snapshots patterns/rules/options
 * with caller allocator. Bridge opaque is borrowed until close. A bridge
 * caller must keep its own NumberFormat alive; RT never closes it. */
QJSIntlStatus qjs_intl_native_relative_open(const QJSIntlAllocator *,
    const QJSIntlRelativeOptions *, const QJSIntlRelativeData *,
    const QJSIntlPluralRulesData *, const QJSIntlRelativeNumberBridge *,
    QJSIntlNativeRelative **out);
void qjs_intl_native_relative_close(QJSIntlNativeRelative *);
QJSIntlStatus qjs_intl_native_relative_resolved_options(
    const QJSIntlNativeRelative *, QJSIntlRelativeOptions *out);
QJSIntlStatus qjs_intl_native_relative_unit(QJSIntlBytes, QJSIntlRelativeUnit *out);
const char *qjs_intl_native_relative_unit_name(QJSIntlRelativeUnit);
QJSIntlStatus qjs_intl_native_relative_format(const QJSIntlNativeRelative *,
    const QJSIntlMathematicalValue *, QJSIntlRelativeUnit, QJSIntlFormatted *out);
/* Results survive handle close; clear with original allocator. Unit names
 * are static borrowed strings and present on every inserted number part,
 * including number literals. Outer literals have no unit. */
void qjs_intl_native_relative_result_clear(const QJSIntlAllocator *,
                                          QJSIntlFormatted *);
/* Public JS accepts Number through ToNumber; these exact slices are an
 * internal boundary. NaN/infinities -> INVALID_ARGUMENT (frontend RangeError).
 * Use shared qjs_intl_number_to_real_decimal, not the shortest public
 * PluralRules/NumberFormat conversion. Its mathematical -0 is positive zero:
 * the frontend must restore kind=NEGATIVE_ZERO when the original Number
 * was -0, using number_negative and the zero coefficient, before this call.
 * Negative zero chooses past for always and offset0 for auto. Direct RT
 * ResolvePlural(value) rounds the exact binary64 mathematical magnitude.
 * All outputs must be fresh and disjoint from inputs/handle storage. */
#endif
