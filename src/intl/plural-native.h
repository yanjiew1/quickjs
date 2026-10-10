/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_PLURAL_NATIVE_H
#define QJS_INTL_PLURAL_NATIVE_H
#include "decimal.h"
#include "plural.h"

typedef enum QJSIntlPluralCategory {
    QJS_INTL_PLURAL_ZERO = 0, QJS_INTL_PLURAL_ONE, QJS_INTL_PLURAL_TWO,
    QJS_INTL_PLURAL_FEW, QJS_INTL_PLURAL_MANY, QJS_INTL_PLURAL_OTHER,
    QJS_INTL_PLURAL_CATEGORY_COUNT
} QJSIntlPluralCategory;
typedef enum QJSIntlPluralType {
    QJS_INTL_PLURAL_CARDINAL = 0, QJS_INTL_PLURAL_ORDINAL
} QJSIntlPluralType;
typedef struct QJSIntlPluralRule {
    QJSIntlPluralCategory category;
    QJSIntlBytes relation;         /* exact rule, no samples required */
} QJSIntlPluralRule;
typedef struct QJSIntlPluralRange {
    QJSIntlPluralCategory start, end, result;
} QJSIntlPluralRange;
typedef struct QJSIntlPluralRulesData {
    const QJSIntlPluralRule *rules; /* 1..6 sorted unique categories, other last */
    size_t rule_count;
    const QJSIntlPluralRange *ranges; /* sorted (start,end), optional overrides */
    size_t range_count;            /* absent pairs -> LDML default other */
} QJSIntlPluralRulesData;
typedef struct QJSIntlPluralOptions {
    QJSIntlPluralType type;
    QJSIntlDecimalOptions digits;
} QJSIntlPluralOptions;
typedef struct QJSIntlNativePlural QJSIntlNativePlural;

/* No locale lookup, coercion, JS getters, or Intl provider activation here.
 * Open copies rules, ranges and validated options; input storage may be freed.
 * No ICU dependency. Internal handles use the allocator copied at open.
 * Rules are parsed at open and malformed data returns DATA_ERROR.
 * Function outputs are zeroed before work and stay clearable on failure.
 */
QJSIntlStatus qjs_intl_native_plural_open(const QJSIntlAllocator *,
    const QJSIntlPluralOptions *, const QJSIntlPluralRulesData *, QJSIntlNativePlural **out);
void qjs_intl_native_plural_close(QJSIntlNativePlural *);
QJSIntlStatus qjs_intl_native_plural_resolved_options(const QJSIntlNativePlural *,
    QJSIntlPluralOptions *out);
/* Categories in spec order zero,one,two,few,many,other. No allocation. */
QJSIntlStatus qjs_intl_native_plural_categories(const QJSIntlNativePlural *,
    QJSIntlPluralCategory out[QJS_INTL_PLURAL_CATEGORY_COUNT], size_t *count);
const char *qjs_intl_plural_category_name(QJSIntlPluralCategory);
QJSIntlStatus qjs_intl_native_plural_select(const QJSIntlNativePlural *,
    const QJSIntlMathematicalValue *, QJSIntlPluralCategory *out);
/* NaN -> INVALID_ARGUMENT for frontend RangeError. Infinities -> other.
 * Range endpoints need not be ordered. Equal unsigned formatted strings
 * return the start category, per pinned ResolvePluralRange, before lookup.
 */
QJSIntlStatus qjs_intl_native_plural_select_range(const QJSIntlNativePlural *,
    const QJSIntlMathematicalValue *start, const QJSIntlMathematicalValue *end,
    QJSIntlPluralCategory *out);
/* NumberFormat/RelativeTimeFormat may supply an already rounded unsigned
 * decimal plus notation exponent c/e. Visible fractional zeros are retained.
 * Caller owns scaling/compact-pattern selection and supplies the exact CLDR
 * operand decimal (not an arbitrary scientific string). Standard exponent0.
 */
QJSIntlStatus qjs_intl_native_plural_select_operands(const QJSIntlNativePlural *,
    const JSIntlPluralOperands *, QJSIntlPluralCategory *out);
QJSIntlStatus qjs_intl_native_plural_range_categories(const QJSIntlNativePlural *,
    QJSIntlPluralCategory start, QJSIntlPluralCategory end, QJSIntlPluralCategory *out);
#endif
