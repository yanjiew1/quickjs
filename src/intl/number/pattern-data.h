/* Compiled native number patterns; immutable data, not a formatting API.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_PATTERN_DATA_H
#define QJS_INTL_NUMBER_PATTERN_DATA_H
#include "../locale/numbering-data.h"

typedef enum QJSIntlNumberPatternField {
    QJS_INTL_PATTERN_DECIMAL,
    QJS_INTL_PATTERN_SCIENTIFIC,
    QJS_INTL_PATTERN_PERCENT,
    QJS_INTL_PATTERN_CURRENCY_STANDARD,
    QJS_INTL_PATTERN_CURRENCY_ACCOUNTING,
    QJS_INTL_PATTERN_CURRENCY_STANDARD_ALPHA,
    QJS_INTL_PATTERN_CURRENCY_ACCOUNTING_ALPHA,
    QJS_INTL_PATTERN_CURRENCY_STANDARD_NONE,
    QJS_INTL_PATTERN_CURRENCY_ACCOUNTING_NONE,
    QJS_INTL_NUMBER_PATTERN_FIELD_COUNT
} QJSIntlNumberPatternField;

typedef enum QJSIntlNumberTokenKind {
    QJS_INTL_NUMBER_TOKEN_LITERAL,
    QJS_INTL_NUMBER_TOKEN_NUMBER,
    QJS_INTL_NUMBER_TOKEN_MINUS,
    QJS_INTL_NUMBER_TOKEN_PLUS,
    QJS_INTL_NUMBER_TOKEN_PERCENT,
    QJS_INTL_NUMBER_TOKEN_PER_MILLE,
    QJS_INTL_NUMBER_TOKEN_CURRENCY,
    QJS_INTL_NUMBER_TOKEN_CURRENCY_ISO,
    QJS_INTL_NUMBER_TOKEN_CURRENCY_NAME,
    QJS_INTL_NUMBER_TOKEN_CURRENCY_NARROW,
    QJS_INTL_NUMBER_TOKEN_COUNT
} QJSIntlNumberTokenKind;

enum {
    QJS_INTL_NUMBER_PATTERN_VALUE,
    QJS_INTL_NUMBER_PATTERN_ABSENT,
    QJS_INTL_NUMBER_PATTERN_BLOCKED,
    QJS_INTL_NUMBER_PATTERN_EMPTY
};
enum {
    QJS_INTL_NUMBER_PATTERN_POSITIVE,
    QJS_INTL_NUMBER_PATTERN_NEGATIVE,
    QJS_INTL_NUMBER_PATTERN_PLUS,
    QJS_INTL_NUMBER_PATTERN_FORM_COUNT
};

typedef struct QJSIntlNumberAffix {
    const uint8_t *tokens;
    uint32_t count;
} QJSIntlNumberAffix;

typedef struct QJSIntlNumberPattern {
    unsigned int state;
    uint32_t minimum_grouping_digits;
    uint32_t primary_grouping, secondary_grouping;
    uint32_t minimum_integer_digits, integer_placeholders;
    uint32_t minimum_fraction_digits, maximum_fraction_digits;
    uint32_t decimal_point, exponent_digits, exponent_plus;
    QJSIntlNumberAffix forms[QJS_INTL_NUMBER_PATTERN_FORM_COUNT];
} QJSIntlNumberPattern;

typedef struct QJSIntlNumberPatternToken {
    QJSIntlNumberTokenKind kind;
    QJSIntlBytes text; /* Borrowed, nonempty only for LITERAL. */
} QJSIntlNumberPatternToken;

typedef struct QJSIntlNumberPatternData {
    QJSIntlNumberingData numbering;
    QJSIntlSection section;
    const uint8_t *systems, *locale_rows, *rows, *vectors;
    const uint8_t *overrides, *patterns, *affixes, *tokens;
    uint32_t system_count, vector_count, row_count;
    uint32_t pattern_count, affix_count, token_count;
    uint32_t override_count;
} QJSIntlNumberPatternData;

/* Require an unchanged, validated numbering view. Borrow its immutable pack;
   scratch allocation is released before returning. No locale fallback or
   formatter capability is implied. Outputs must be disjoint from inputs and
   storage, as in pack.h; non-NULL outputs are reset on failure. */
QJSIntlStatus qjs_intl_number_pattern_data_open(const QJSIntlNumberingData *,
                                                const QJSIntlAllocator *,
                                                QJSIntlNumberPatternData *);

/* Exact shared locale/numbering-definition indexes, no allocation. Algorithmic
   definitions return UNSUPPORTED. Optional ABSENT/BLOCKED/EMPTY results retain
   minimum_grouping_digits but have no numeric metadata or affix views. Pattern
   digit settings are source metadata: ECMA-402 options govern actual rounding.
   PLUS follows LDML; additional signDisplay insertion is service policy. */
QJSIntlStatus qjs_intl_number_pattern_at(const QJSIntlNumberPatternData *, uint32_t locale,
                                         uint32_t system, QJSIntlNumberPatternField,
                                         QJSIntlNumberPattern *);

/* Exact raw per-currency pattern returned by the currency metadata owner.
   Look up its precompiled representation; this is not a runtime pattern
   parser. No allocation or locale fallback. The caller supplies the owning
   locale's minimum-grouping preference when applying the result. */
QJSIntlStatus qjs_intl_number_pattern_override(const QJSIntlNumberPatternData *, QJSIntlBytes raw,
                                               QJSIntlNumberPattern *);

/* Require an affix returned by this same unchanged open view. No allocation;
   non-NULL output is reset on errors. Affix/token/string storage is borrowed. */
QJSIntlStatus qjs_intl_number_pattern_token_at(const QJSIntlNumberPatternData *,
                                               const QJSIntlNumberAffix *, uint32_t index,
                                               QJSIntlNumberPatternToken *);
#endif
