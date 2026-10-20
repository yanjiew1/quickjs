/* Native source-to-component projection, separate from candidate admission.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_PATTERN_INFO_H
#define QJS_INTL_DATETIME_PATTERN_INFO_H
#include "components.h"
#include "pattern.h"

enum {
    QJS_INTL_DATE_YEAR_CALENDAR = 1,
    QJS_INTL_DATE_YEAR_RELATED = 2,
    QJS_INTL_DATE_YEAR_CYCLIC = 4
};

typedef struct QJSIntlDatePatternInfo {
    QJSIntlDateTimeComponents components;
    unsigned int hour_symbol; /* h/H/K/k, or zero; retain the exact source cycle. */
    unsigned int period_symbol; /* a/b/B, or zero. a is not a DAY_PERIOD option. */
    size_t period_width;
    unsigned int year_kinds;
} QJSIntlDatePatternInfo;

/* Input must be an unchanged compiled ordinary pattern. Allocate/retain nothing;
   require output disjoint from input and clear output on failure. Unsupported
   fields, widths or repeated components return UNSUPPORTED, not malformed data.
   Literal-only and a-only patterns may yield empty public components. This is
   metadata projection, not candidate admission, source fallback or matching.
   Keep the original tokens for name contexts and widths. In particular source
   yyyy does not override ECMA numeric minimum digits, and rr/UU are not yy. */
QJSIntlStatus qjs_intl_date_pattern_info(const QJSIntlDatePattern *pattern,
                                       QJSIntlDatePatternInfo *out);
#endif
