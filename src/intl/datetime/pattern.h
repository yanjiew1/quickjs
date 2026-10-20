/* Source-preserving LDML syntax, independent of ECMA format admission.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_PATTERN_H
#define QJS_INTL_DATETIME_PATTERN_H
#include "../common/types.h"
#include "../common/pair-pattern.h"

typedef enum QJSIntlDateIntervalOrder {
    QJS_INTL_DATE_INTERVAL_DEFAULT,
    QJS_INTL_DATE_INTERVAL_EARLIEST_FIRST,
    QJS_INTL_DATE_INTERVAL_LATEST_FIRST
} QJSIntlDateIntervalOrder;

typedef struct QJSIntlDatePatternToken {
    QJSIntlBytes literal;
    size_t width;
    unsigned int symbol;
} QJSIntlDatePatternToken;

typedef struct QJSIntlDatePattern {
    QJSIntlDatePatternToken *tokens;
    size_t count;
    void *storage;
    QJSIntlDateIntervalOrder order;
} QJSIntlDatePattern;

/* Copy all decoded scalar UTF-8 literal bytes into one owned allocation.
   Literal tokens have symbol/width zero and nonempty coalesced text. Fields
   retain the original symbol and full positive run length, with empty text.
   Combination slots use symbols '0' (time) / '1' (date), width zero, empty text.
   Recognized quarter/week/long-fraction fields remain valid source tokens;
   legal render widths, ECMA projection and locale policy belong to consumers.
   Input must remain unchanged during the call and disjoint from output.
   Require malloc/free, reset output on failure, and release with the original
   allocator. No source pointer is retained. */
QJSIntlStatus qjs_intl_date_pattern_compile(const QJSIntlAllocator *allocator,
                                          QJSIntlBytes source, QJSIntlDatePattern *out);
/* Exactly one unquoted {0} and {1}; apostrophe quoting also applies to slots.
   Other braces remain literal, matching the source collector's grammar. */
QJSIntlStatus qjs_intl_date_combination_compile(const QJSIntlAllocator *allocator,
                                              QJSIntlBytes source, QJSIntlDatePattern *out);
/* Preserve an optional earliestFirst:/latestFirst: directive and repeated
   field tokens. The consumer owns interval splitting and logical equivalence. */
QJSIntlStatus qjs_intl_date_interval_compile(const QJSIntlAllocator *allocator,
                                           QJSIntlBytes source, QJSIntlDatePattern *out);
/* Copy an unchanged compiled pattern, retaining its grammar/order metadata.
   Own one allocation, independent of all input token/literal storage. Require
   malloc/free and fresh disjoint output; reset output on failure and release
   with the same allocator through pattern_clear. */
QJSIntlStatus qjs_intl_date_pattern_copy(const QJSIntlAllocator *allocator,
                                       const QJSIntlDatePattern *pattern, QJSIntlDatePattern *out);
/* Expand a compiled connector's time/date slots with compiled ordinary
   patterns. Quoted slot text remains literal, inserted tokens are not parsed
   again, and adjacent literals coalesce across input boundaries. Copy raw
   symbols/widths without choosing styles or adapting hour cycles. Require
   unchanged inputs from their matching compile operations, disjoint output
   and malloc/free. Reset output on failure; release via pattern_clear. */
QJSIntlStatus qjs_intl_date_pattern_combine(const QJSIntlAllocator *allocator,
                                          const QJSIntlDatePattern *connector,
                                          const QJSIntlDatePattern *time,
                                          const QJSIntlDatePattern *date, QJSIntlDatePattern *out);
/* Apply a parsed two-operand literal template to ordinary compiled programs.
   Unlike LDML combination sources, template letters and apostrophes are literal.
   Require a pair-pattern parse over validated scalar UTF-8, unchanged inputs and
   fresh disjoint output. Own one copied allocation; reset output on failure. */
QJSIntlStatus qjs_intl_date_pattern_join(const QJSIntlAllocator *allocator,
                                       const QJSIntlPairPattern *layout,
                                       const QJSIntlDatePattern *first,
                                       const QJSIntlDatePattern *second, QJSIntlDatePattern *out);
/* Replace exactly one field token by an ordinary compiled program. Preserve
   surrounding decoded literals and never reparse replacement text. Both inputs
   are unchanged ordinary programs; symbol is a nonzero field, not a slot.
   Missing/repeated target is INVALID_ARGUMENT. Own one copied allocation;
   require malloc/free and fresh disjoint output, reset errors. */
QJSIntlStatus qjs_intl_date_pattern_replace(const QJSIntlAllocator *allocator,
                                          const QJSIntlDatePattern *pattern, unsigned int symbol,
                                          const QJSIntlDatePattern *replacement, QJSIntlDatePattern *out);
void qjs_intl_date_pattern_clear(const QJSIntlAllocator *allocator, QJSIntlDatePattern *pattern);
#endif
