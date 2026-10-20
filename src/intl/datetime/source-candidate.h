/* Directly usable native source patterns, not final ECMA Format Records.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_SOURCE_CANDIDATE_H
#define QJS_INTL_DATETIME_SOURCE_CANDIDATE_H
#include "available-source.h"
#include "pattern-info.h"

typedef struct QJSIntlDateSourceCandidate {
    QJSIntlDateAvailableEntry source;
    QJSIntlDateAvailableNode origin;
    QJSIntlDatePattern pattern;
    QJSIntlDatePatternInfo info;
} QJSIntlDateSourceCandidate;

typedef struct QJSIntlDateSourceCandidates {
    QJSIntlDateAvailableData data;
    QJSIntlAllocator allocator;
    QJSIntlDateSourceCandidate *records;
    size_t count;
} QJSIntlDateSourceCandidates;

/* Compile an unchanged resolved PATTERN inventory, selecting NO_PLURAL and
   exactly leaf_alt (empty for ordinary sources), with no alternate fallback.
   Blocked/empty values and unsupported or empty public-component projections
   contribute no candidate; their masked parents stay absent. Check syntax of
   eligible nonempty text, not filtered keys. Propagate malformed syntax, OOM
   and overflow. A populated APPEND inventory is an invalid argument.
   Preserve distinct skeletons and raw fields even for identical components;
   final ECMA grouping, synthesis and renderer admission belong to consumers.
   Copy the owner/provenance and own compiled tokens; borrowed pack bytes must
   outlive the result. The input inventory may be cleared after success.
   Require malloc/free and fresh output disjoint from inputs; zero on failure.
   Empty success retains no storage and does not establish locale support. */
QJSIntlStatus qjs_intl_date_source_candidates_open(const QJSIntlDateAvailableSources *sources,
                                                 QJSIntlBytes leaf_alt,
                                                 const QJSIntlAllocator *allocator,
                                                 QJSIntlDateSourceCandidates *out);
void qjs_intl_date_source_candidates_clear(QJSIntlDateSourceCandidates *candidates);
/* Borrow a const record, without allocating or transferring pattern ownership.
   Do not modify/clear borrowed records. An owner may filter its own mutable
   array, clearing rejected patterns and moving retained records before grouping;
   it must retain the allocator and avoid duplicate pattern ownership.
   Reset non-NULL output on failure. */
QJSIntlStatus qjs_intl_date_source_candidate_at(const QJSIntlDateSourceCandidates *candidates,
                                              size_t index, const QJSIntlDateSourceCandidate **out);
#endif
