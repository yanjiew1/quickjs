/* Shared native currency metadata; no formatter or availability policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_CURRENCY_DATA_H
#define QJS_INTL_CURRENCY_DATA_H
#include "../native/data.h"

typedef enum QJSIntlCurrencyField {
    QJS_INTL_CURRENCY_NAME,
    QJS_INTL_CURRENCY_NAME_EXACT_ZERO,
    QJS_INTL_CURRENCY_NAME_EXACT_ONE,
    QJS_INTL_CURRENCY_NAME_ZERO,
    QJS_INTL_CURRENCY_NAME_ONE,
    QJS_INTL_CURRENCY_NAME_TWO,
    QJS_INTL_CURRENCY_NAME_FEW,
    QJS_INTL_CURRENCY_NAME_MANY,
    QJS_INTL_CURRENCY_NAME_OTHER,
    QJS_INTL_CURRENCY_SYMBOL,
    QJS_INTL_CURRENCY_NARROW,
    QJS_INTL_CURRENCY_FORMAL,
    QJS_INTL_CURRENCY_VARIANT,
    QJS_INTL_CURRENCY_DECIMAL,
    QJS_INTL_CURRENCY_GROUP,
    QJS_INTL_CURRENCY_PATTERN,
    QJS_INTL_CURRENCY_FIELD_COUNT
} QJSIntlCurrencyField;

enum {
    QJS_INTL_CURRENCY_VALUE,
    QJS_INTL_CURRENCY_ABSENT,
    QJS_INTL_CURRENCY_BLOCKED,
    QJS_INTL_CURRENCY_EMPTY
};

typedef struct QJSIntlCurrencyText {
    QJSIntlBytes text;
    unsigned int state;
    unsigned int from_root; /* Counted name originated at root, including markers. */
} QJSIntlCurrencyText;

typedef struct QJSIntlCurrencyFractions {
    uint32_t digits, rounding, cash_digits, cash_rounding;
} QJSIntlCurrencyFractions;

typedef struct QJSIntlCurrencyData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *codes, *fractions, *locale_maps, *maps, *vectors;
    uint32_t code_count, fraction_count, map_count, vector_count;
    QJSIntlCurrencyFractions defaults;
} QJSIntlCurrencyData;

/* Require an unchanged validated data view. Borrow its immutable storage;
   all temporary validation memory is released before return. Follow pack.h's
   disjoint output contract. No plural, display fallback or currency availability
   decision is made here. Non-NULL outputs are reset on failure. */
QJSIntlStatus qjs_intl_currency_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                          QJSIntlCurrencyData *);

/* Code inventory includes historical and supplemental-only codes. It is not
   the supportedValuesOf currency list. Find requires exactly three uppercase
   ASCII letters; a well-formed absent code returns UNSUPPORTED. */
QJSIntlStatus qjs_intl_currency_code_at(const QJSIntlCurrencyData *, uint32_t index,
                                        QJSIntlBytes *);
QJSIntlStatus qjs_intl_currency_code_find(const QJSIntlCurrencyData *, QJSIntlBytes,
                                          uint32_t *index);

/* Exact shared locale/code indexes. Missing vectors return ABSENT. Empty and
   blocked fields have zeroed text slices, distinct from VALUE. from_root marks
   counted names for the consumer's root-excluding fallback; other fields use 0.
   Returned VALUE slices borrow the unchanged owner; no lookup allocates. */
QJSIntlStatus qjs_intl_currency_field_at(const QJSIntlCurrencyData *, uint32_t locale,
                                         uint32_t code, QJSIntlCurrencyField,
                                         QJSIntlCurrencyText *);

/* Supplemental facts, including cash data not used by ECMA-402 CurrencyDigits.
   Well-formed codes lacking a fraction record return the stored DEFAULT facts,
   including codes absent from the display inventory. No availability is implied. */
QJSIntlStatus qjs_intl_currency_fractions(const QJSIntlCurrencyData *, QJSIntlBytes,
                                          QJSIntlCurrencyFractions *);
#endif
