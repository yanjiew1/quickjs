/* Prescribed ECMA-402 format matching, independent of backend candidates.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DATETIME_BASIC_MATCH_H
#define QJS_INTL_DATETIME_BASIC_MATCH_H
#include "components.h"
#include "../common/types.h"

/* No allocation or backend data. Validate legal component values and reset
   output on failure. Outputs must not alias input records or arrays. */
QJSIntlStatus qjs_intl_datetime_basic_score(const QJSIntlDateTimeComponents *requested,
                                            const QJSIntlDateTimeComponents *candidate, int *out);
/* Empty candidate lists return UNSUPPORTED. Equal scores retain the earliest
   candidate, following BasicFormatMatcher's strict best-score comparison. */
QJSIntlStatus qjs_intl_datetime_basic_match(const QJSIntlDateTimeComponents *requested,
                                            const QJSIntlDateTimeComponents *candidates,
                                            size_t count, size_t *out);
#endif
