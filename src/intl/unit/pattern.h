/* Number-bearing unit patterns and source-aware native selection.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_UNIT_PATTERN_H
#define QJS_INTL_UNIT_PATTERN_H
#include "data.h"
#include "../common/pair-pattern.h"
#include "../common/unary-pattern.h"

typedef QJSIntlUnaryPattern QJSIntlUnitPattern;

/* Borrow scalar UTF-8 text from validated unit data. Support one {0}; both
   affixes may be empty. Apostrophes are literal, not decimal-pattern quoting.
   Literal-only/empty text returns UNSUPPORTED, not malformed data. Repeated or
   unsupported placeholders/braces return DATA_ERROR. This is the native
   number-bearing adapter's grammar, not a claim about all CLDR templates.
   Allocate nothing, retain nothing, and reset non-NULL output on failure. */
QJSIntlStatus qjs_intl_unit_pattern_parse(QJSIntlBytes, QJSIntlUnitPattern *);

/* Extract one borrowed noun from a unitPattern for a grammatical component.
   Accept literal-only text or a single {0} at a nonspace edge. Remove numeric
   separator whitespace, retaining noun-internal spaces/punctuation. Empty or
   middle-placeholder forms return UNSUPPORTED, malformed scalar text or
   placeholders return DATA_ERROR. Fixed grammatical count selection belongs
   to the caller and must not enable exact numeric selectors. No allocation;
   require disjoint output and reset non-NULL output on failure. */
QJSIntlStatus qjs_intl_unit_noun_parse(QJSIntlBytes, QJSIntlBytes *);

typedef QJSIntlStatus (*QJSIntlUnitCategoryResolver)(void *opaque, QJSIntlBytes source_locale,
                                                   QJSIntlUnitCount *category);

typedef struct QJSIntlUnitPatternRequest {
    QJSIntlBytes case_name; /* Empty means nominative, including its legacy absent case. */
    QJSIntlUnitCategoryResolver resolve;
    void *opaque;
    QJSIntlUnitCount category;
    int exact;
} QJSIntlUnitPatternRequest;

typedef struct QJSIntlSelectedUnitPattern {
    QJSIntlUnitPattern pattern;
    QJSIntlUnitForm form;
    QJSIntlBytes locale;
    QJSIntlUnitWidth width;
    uint32_t group;
} QJSIntlSelectedUnitPattern;

/* Require an unchanged owner and its overlay head (zero is empty).
   Numeric mode: resolve != NULL, category NONE, exact -1/0/1. The callback
   retains the final rounded operands and resolves each source's cardinal
   rules; unknown sources must fail, never substitute requester rules.
   Exact 0/1 precedes category lookup; -1 means neither. Fixed mode: no callback,
   category ZERO..OTHER, exact -1. Use it for grammatical counts or nonfinite
   OTHER, which must not try exact selectors.

   Within each layer, count alternatives surround case alternatives. Retry the
   original request at each parent. Blocked, empty and literal-only leaves
   shadow that exact path in later layers but permit unrelated alternatives.
   These eligibility/shadow rules adapt CLDR data to NumberFormat's number-bearing
   contract. No extra width fallback. Allocate/retain nothing; result borrows
   the owner, and non-NULL output is reset on failure. Callback errors propagate. */
QJSIntlStatus qjs_intl_unit_pattern_select(const QJSIntlUnitData *, uint32_t head,
                                           const QJSIntlUnitPatternRequest *,
                                           QJSIntlSelectedUnitPattern *);

typedef struct QJSIntlSelectedUnitNoun {
    QJSIntlBytes noun;
    QJSIntlUnitForm form;
    QJSIntlBytes locale;
    QJSIntlUnitWidth width;
    uint32_t group;
} QJSIntlSelectedUnitNoun;

/* Select a noun using fixed categories or a source-rule callback as in
   pattern_select, but require exact -1: grammatical components never select
   exact numeric 0/1. Try requested/nominative/absent case before OTHER and each parent layer.
   Literal-only nouns are eligible; empty/blocked/middle-placeholder forms
   shadow their exact path. Require the same unchanged owner/head contract as
   pattern_select. Borrow all text, allocate nothing, reset disjoint output. */
QJSIntlStatus qjs_intl_unit_noun_select(const QJSIntlUnitData *, uint32_t head,
                                        const QJSIntlUnitPatternRequest *,
                                        QJSIntlSelectedUnitNoun *);

typedef struct QJSIntlSelectedUnitTemplate {
    union {
        QJSIntlUnitPattern per_unit;
        QJSIntlPairPattern division;
    } pattern;
    QJSIntlUnitForm form;
    QJSIntlBytes locale;
    QJSIntlUnitWidth width;
    uint32_t group;
} QJSIntlSelectedUnitTemplate;

/* Select PER_PATTERN from a single group (empty case_name required), or
   COMPOUND_PATTERN from a division group with requested/nominative/absent
   case alternatives. Local alternatives precede parents; empty, blocked or
   missing-operand forms shadow their exact inherited path. Malformed consulted
   templates fail. The active pattern member follows form.field. Require the
   unchanged owner/head contract; borrow all text and reset disjoint output.
   No allocation, plural selection or cross-width fallback. */
QJSIntlStatus qjs_intl_unit_template_select(const QJSIntlUnitData *, uint32_t head,
                                            QJSIntlUnitField, QJSIntlBytes case_name,
                                            QJSIntlSelectedUnitTemplate *);
#endif
