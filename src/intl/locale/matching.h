/* Fixed lookup matching; independent of backend data and JS objects.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_LOCALE_MATCHING_H
#define QJS_INTL_LOCALE_MATCHING_H
#include "../common/types.h"

typedef struct QJSIntlLocaleMatch {
    int found;
    size_t available_index;
    size_t requested_index;
    QJSIntlBytes extension; /* Borrowed original '-u-...' sequence or absent. */
} QJSIntlLocaleMatch;

/* LookupMatchingLocaleByPrefix. Available tags are a strictly code-unit-sorted
   unique inventory of canonical locale IDs with no extensions or private use.
   Requested tags are a canonical language priority list. Both arrays and all
   slices remain unchanged during the call. This does not validate an inventory,
   advertise service availability, choose a default, or perform best-fit matching.
   Allocate nothing; return OK with found=0 when unmatched. Output storage must
   not overlap inputs and is zeroed on error. */
QJSIntlStatus qjs_intl_lookup_matching_locale(const QJSIntlBytes *available,
                                              size_t available_count,
                                              const QJSIntlBytes *requested,
                                              size_t requested_count,
                                              QJSIntlLocaleMatch *out);
#endif
