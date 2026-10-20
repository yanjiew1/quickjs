/* Group source programs without transferring their pattern ownership.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_FORMAT_SOURCE_H
#define QJS_INTL_DATETIME_FORMAT_SOURCE_H
#include "source-candidate.h"

typedef struct QJSIntlDateFormatSourceGroup QJSIntlDateFormatSourceGroup;
typedef struct QJSIntlDateFormatSourceMember QJSIntlDateFormatSourceMember;
typedef struct QJSIntlDateFormatSources {
    QJSIntlAllocator allocator;
    QJSIntlDateFormatSourceGroup *groups;
    QJSIntlDateFormatSourceMember *members;
    size_t count, source_count;
} QJSIntlDateFormatSources;

/* Group all eleven component values from unchanged source candidates. Keep
   every source index, including different raw contexts/year/hour semantics.
   Order groups by first input occurrence and members by original input index.
   Own only index/group storage; source patterns remain owned by the caller.
   Indexes require the original pool alive and unchanged when resolved, but
   no pointer to its stack-local view is retained. Allocate at most two blocks;
   empty input needs none. Require malloc/free and fresh disjoint output,
   reset on failure. This does not finalize Pattern/Pattern12, ranges, required
   subset coverage or renderer admission and is not an ECMA formats list. */
QJSIntlStatus qjs_intl_date_format_sources_open(const QJSIntlDateSourceCandidates *sources,
                                              const QJSIntlAllocator *allocator,
                                              QJSIntlDateFormatSources *out);
void qjs_intl_date_format_sources_clear(QJSIntlDateFormatSources *sources);
/* Copy component metadata or an original source index, allocating nothing.
   Reset every non-NULL output on failure; all indexes are zero-based. */
QJSIntlStatus qjs_intl_date_format_source_group(const QJSIntlDateFormatSources *sources,
                                              size_t group, QJSIntlDateTimeComponents *components,
                                              size_t *member_count);
QJSIntlStatus qjs_intl_date_format_source_member(const QJSIntlDateFormatSources *sources,
                                               size_t group, size_t member, size_t *source_index);
#endif
