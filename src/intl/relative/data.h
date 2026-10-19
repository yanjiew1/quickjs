/* Resolved native relative-time source facts; no formatting fallback policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_RELATIVE_DATA_H
#define QJS_INTL_RELATIVE_DATA_H
#include "../native/data.h"
#include "../plural/rules.h"

typedef enum QJSIntlRelativeUnit {
    QJS_INTL_RELATIVE_SECOND,
    QJS_INTL_RELATIVE_MINUTE,
    QJS_INTL_RELATIVE_HOUR,
    QJS_INTL_RELATIVE_DAY,
    QJS_INTL_RELATIVE_WEEK,
    QJS_INTL_RELATIVE_MONTH,
    QJS_INTL_RELATIVE_QUARTER,
    QJS_INTL_RELATIVE_YEAR,
    QJS_INTL_RELATIVE_UNIT_COUNT
} QJSIntlRelativeUnit;

typedef enum QJSIntlRelativeWidth {
    QJS_INTL_RELATIVE_LONG,
    QJS_INTL_RELATIVE_SHORT,
    QJS_INTL_RELATIVE_NARROW,
    QJS_INTL_RELATIVE_WIDTH_COUNT
} QJSIntlRelativeWidth;

typedef enum QJSIntlRelativeTense {
    QJS_INTL_RELATIVE_PAST,
    QJS_INTL_RELATIVE_FUTURE
} QJSIntlRelativeTense;

typedef struct QJSIntlRelativeData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *keys, *locale_rows, *rows, *maps, *entries;
    uint32_t key_count, row_count, map_count, entry_count;
} QJSIntlRelativeData;

typedef struct QJSIntlRelativeMap {
    const uint8_t *entries;
    uint32_t count;
} QJSIntlRelativeMap;

typedef struct QJSIntlRelativeForm {
    QJSIntlBytes text, origin_locale;
    QJSIntlRelativeUnit origin_unit;
    QJSIntlRelativeWidth origin_width;
    unsigned int blocked;
} QJSIntlRelativeForm;

/* Borrow an unchanged validated shared owner. Check section shape, canonical
   sharing, source identities, literal keys, counted template syntax and required
   long other forms. No formatter availability or selection policy is inferred.
   malloc/free are required; all validation scratch is released before return.
   Follow pack.h's disjoint-output contract; non-NULL outputs reset on error. */
QJSIntlStatus qjs_intl_relative_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                          QJSIntlRelativeData *);
/* Exact shared-locale index and unit/width. Queries borrow storage and allocate
   nothing; no width, category or locale fallback. Require a map returned by
   this unchanged owner. Absent selectors return UNSUPPORTED; explicit blocked
   and empty values return OK and remain distinct. Literal query keys are
   arbitrary explicit-length Number strings, including non-integer misses. */
QJSIntlStatus qjs_intl_relative_map_at(const QJSIntlRelativeData *, uint32_t locale,
                                       QJSIntlRelativeUnit, QJSIntlRelativeWidth,
                                       QJSIntlRelativeMap *);
QJSIntlStatus qjs_intl_relative_pattern_find(const QJSIntlRelativeData *, const QJSIntlRelativeMap *,
                                             QJSIntlRelativeTense, QJSIntlPluralCategory,
                                             QJSIntlRelativeForm *);
QJSIntlStatus qjs_intl_relative_literal_find(const QJSIntlRelativeData *, const QJSIntlRelativeMap *,
                                             QJSIntlBytes key, QJSIntlRelativeForm *);
#endif
