/* Fixed lookup matching; independent of backend data and JS objects.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
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
