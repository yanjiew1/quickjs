/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATOR_NATIVE_H
#define QJS_INTL_COLLATOR_NATIVE_H
#include "collator-native-data.h"

typedef enum QJSIntlCollatorUsage {
    QJS_INTL_COLLATOR_SORT = 0,
    QJS_INTL_COLLATOR_USAGE_SEARCH = 1
} QJSIntlCollatorUsage;
typedef enum QJSIntlCollatorSensitivity {
    QJS_INTL_COLLATOR_BASE = 0,
    QJS_INTL_COLLATOR_ACCENT = 1,
    QJS_INTL_COLLATOR_CASE = 2,
    QJS_INTL_COLLATOR_VARIANT = 3
} QJSIntlCollatorSensitivity;
typedef enum QJSIntlCollatorCaseFirst {
    QJS_INTL_COLLATOR_CASE_FALSE = 0,
    QJS_INTL_COLLATOR_CASE_UPPER = 1,
    QJS_INTL_COLLATOR_CASE_LOWER = 2
} QJSIntlCollatorCaseFirst;
typedef struct QJSIntlCollatorOptions {
    QJSIntlCollatorUsage usage;
    QJSIntlCollatorSensitivity sensitivity;
    QJSIntlCollatorCaseFirst case_first;
    uint8_t numeric, ignore_punctuation;
} QJSIntlCollatorOptions;
typedef struct QJSIntlCollatorLimits {
    size_t max_input_units;   /* per operand; 0 means arithmetic limit */
    size_t max_work_bytes;    /* both operands combined; 0 means SIZE_MAX */
} QJSIntlCollatorLimits;
typedef struct QJSIntlNativeCollator QJSIntlNativeCollator;

/* Original plain C module. Frontend owns coercion, observable option reads,
 * locale resolution, boundCompare, resolvedOptions, brands and realm errors.
 * locale_index is an already resolved section16 row. collation is empty or
 * "default" for the advertised implementation-defined comparison policy.
 * This packet deliberately uses the CLDR root comparator for both usages.
 * Search has its own explicit capability/default row; this is not a claim
 * that the distinct CLDR root/search tailoring has been compiled. Other
 * collation values and absent (locale,usage) pairs return UNSUPPORTED.
 * Allocator callbacks/opaque are copied; opaque lives through close. Handle
 * copies options/limits/data descriptors and borrows the immutable blob only.
 * All three allocator callbacks are mandatory. Never realloc(ptr,0).
 */
/* Retrieve the usage-specific locale defaults before frontend option
 * resolution. Output is zeroed on failure. On success usage is preserved;
 * sensitivity and ignore_punctuation come from the explicit capability row,
 * numeric=false and case_first=false. The frontend publishes co=[null] for
 * both LocaleData records and one AvailableLocales set containing only
 * locales with both usages. This API does not advertise JS locale coverage.
 */
QJSIntlStatus qjs_intl_native_collator_defaults(const QJSIntlCollationData *,
                                               uint32_t locale_index,
                                               QJSIntlCollatorUsage,
                                               QJSIntlCollatorOptions *out);
QJSIntlStatus qjs_intl_native_collator_open(const QJSIntlAllocator *,
                                           const QJSIntlCollationData *,
                                           uint32_t locale_index,
                                           QJSIntlBytes collation,
                                           const QJSIntlCollatorOptions *,
                                           const QJSIntlCollatorLimits *,
                                           QJSIntlNativeCollator **out);
void qjs_intl_native_collator_close(QJSIntlNativeCollator *);
/* Explicit UTF16 lengths preserve NUL/unpaired units. Canonically equivalent
 * strings compare0 at every sensitivity. Strengths: base=primary,
 * accent=primary+secondary, case=primary+case, variant=primary+secondary+
 * tertiary. No code-point/identical-level tiebreak. Successful result is
 * exactly -1/0/+1. Output0 on failure. Inputs remain unchanged and borrowed
 * only for the call; caller does not overlap output with inputs/handle/data.
 * Per-call scratch is bounded by limits and arithmetic; failure is neutral
 * NO_MEMORY/OVERFLOW/DATA_ERROR and frees all temporary allocations.
 * Concurrent compare calls are independent if allocator opaque is safe.
 */
QJSIntlStatus qjs_intl_native_collator_compare(const QJSIntlNativeCollator *,
                                              QJSIntlUTF16 left,
                                              QJSIntlUTF16 right, int *out);
#endif
