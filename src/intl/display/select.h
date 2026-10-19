/* Native label-selection policy over immutable source layers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DISPLAY_SELECT_H
#define QJS_INTL_DISPLAY_SELECT_H
#include "data.h"
#include "display.h"

typedef struct QJSIntlNameLabel {
    QJSIntlNameEntry entry;
    QJSIntlBytes origin_locale;
    unsigned int found;
} QJSIntlNameLabel;

/* Exact normalized code, with native style alternatives within each layer
   before following parents. Ignore menu/scope variants. standalone is a boolean
   selecting standalone-script alternatives and must be zero for other groups.
   BLOCK masks only its exact alternative path; empty/code-identical text remains
   present. No API code fallback, composition or locale admission. Borrow all
   returned slices, allocate nothing and reset non-NULL output on failure.
   Follow pack.h's disjoint-output and unchanged-view contract. */
QJSIntlStatus qjs_intl_name_select(const QJSIntlNameData *, uint32_t locale,
                                   QJSIntlNameGroup, QJSIntlNameStyle, int standalone,
                                   QJSIntlBytes normalized_code, QJSIntlNameLabel *);
#endif
