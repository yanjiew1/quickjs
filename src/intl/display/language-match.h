/* Native dialect-name matching over actual source candidates.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DISPLAY_LANGUAGE_MATCH_H
#define QJS_INTL_DISPLAY_LANGUAGE_MATCH_H
#include "select.h"

typedef struct QJSIntlLanguageNameMatch {
    QJSIntlNameLabel label;
    QJSIntlBytes code; /* Normalized matched source key, not the API identity. */
    size_t subtags; /* Includes the primary language; zero when absent. */
} QJSIntlLanguageNameMatch;

/* Input is a fully validated, canonical Unicode Language ID, without extensions
   or private use. Caller handles aliases and duplicate variants before this
   operation. dialect=0 selects the primary only; dialect=1 chooses the present
   source candidate with most ordered included subtags, then earlier matching
   input positions and alphabetical key. Resolve each candidate's style and
   inheritance before ranking. Missing base yields OK with a zero result.

   Query only the same-primary-language inventory range, without enumerating
   variant subsets. Allocate nothing and borrow all results from the unchanged
   data owner. No qualifier composition, code fallback or locale admission.
   Follow pack.h's disjoint-output contract; reset output on failure. */
QJSIntlStatus qjs_intl_language_name_match(const QJSIntlNameData *, uint32_t locale,
                                         QJSIntlNameStyle, int dialect,
                                         QJSIntlBytes canonical, QJSIntlLanguageNameMatch *);
#endif
