/* Validated native plural conditions and range metadata.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_PLURAL_DATA_H
#define QJS_INTL_PLURAL_DATA_H
#include "rules.h"
#include "../native/data.h"

#define QJS_INTL_PLURAL_RANGE_ABSENT UINT8_MAX

typedef struct QJSIntlPluralData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *maps, *rules, *ranges;
    uint32_t rule_count, range_count;
} QJSIntlPluralData;

typedef struct QJSIntlPluralRuleSet {
    QJSIntlBytes conditions[QJS_INTL_PLURAL_CATEGORY_COUNT];
    unsigned int present;  /* One bit per category; other is present and empty. */
    const uint8_t *ranges; /* 36 start/end results; RANGE_ABSENT stays distinct. */
} QJSIntlPluralRuleSet;

/* Open against an unchanged, successfully opened shared view. Borrow its
   immutable storage; validation scratch is released before return. Check
   structure, category/range maps and every distinct condition's grammar.
   Allocator malloc/free are required. Follow pack.h's non-aliasing contract;
   non-NULL outputs are zeroed on error. No service availability is inferred. */
QJSIntlStatus qjs_intl_plural_data_open(const QJSIntlData *data, const QJSIntlAllocator *allocator,
                                        QJSIntlPluralData *out);
/* Exact shared-locale index and cardinal/ordinal type. Results borrow the
   open storage and allocate nothing. Non-NULL output is zeroed on error.
   Sparse range pairs are metadata; the consumer applies its LDML policy. */
QJSIntlStatus qjs_intl_plural_rules_at(const QJSIntlPluralData *data, uint32_t locale,
                                       QJSIntlPluralType type, QJSIntlPluralRuleSet *out);
#endif
