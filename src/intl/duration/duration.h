/* DurationFormat service, independent of JS coercion and locale matching.
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
#ifndef QJS_INTL_DURATION_SERVICE_H
#define QJS_INTL_DURATION_SERVICE_H
#include "format.h"
#include "options.h"

typedef struct QJSIntlDurationService QJSIntlDurationService;
typedef struct QJSIntlDurationFormatter QJSIntlDurationFormatter;

/* Compile-time selected service. Copy allocator callbacks/opaque; all three
   callbacks required by the Number/List composition. Their lifetime covers
   all handles/results. Validate the profile and Number/List dependencies.
   Errors reset *out; NULL closes. */
QJSIntlStatus qjs_intl_duration_open(const QJSIntlAllocator *allocator,
                                     const struct QJSIntlData *checked_data,
                                     QJSIntlDurationService **out);
void qjs_intl_duration_close(QJSIntlDurationService *);
/* Borrow sorted extension-free locale tags, excluding root, until close.
   Exact inventory indexes, no matching/fallback or allocation in queries. */
QJSIntlStatus qjs_intl_duration_available(const QJSIntlDurationService *, const QJSIntlBytes **, size_t *);
QJSIntlStatus qjs_intl_duration_default_system(const QJSIntlDurationService *, size_t locale, QJSIntlBytes *);
QJSIntlStatus qjs_intl_duration_system_supported(const QJSIntlDurationService *, size_t locale,
                                               QJSIntlBytes system, int *supported);
/* Effective DigitalFormat flag, distinct from the raw native pattern width.
   The initial native policy is false: do not override explicit text hours.
   This preserves the normative algorithm despite its numeric-only slot
   description; see tools/intl/README.md for the specification discrepancy. */
QJSIntlStatus qjs_intl_duration_two_digit_hours(const QJSIntlDurationService *, size_t locale, int *);
/* Copy a fully resolved option record; retain no caller strings. An empty
   system chooses the locale default. Explicit unsupported systems return
   UNSUPPORTED without fallback. Formatter owns its Number/List handles and
   separators, and may outlive the service. Errors reset *out. */
QJSIntlStatus qjs_intl_duration_formatter_open(const QJSIntlDurationService *, size_t locale,
                                             QJSIntlBytes system, const QJSIntlDurationOptions *,
                                             QJSIntlDurationFormatter **);
void qjs_intl_duration_formatter_close(QJSIntlDurationFormatter *);
/* Borrow an integral, same-sign duration record, validating it before work.
   Combine fractional units exactly, format only the first displayed sign,
   and compose typed Number parts through digital separators and ListFormat.
   Results own UTF-16/parts and survive all handles. Fresh disjoint output;
   errors reset it. Clear with the creating allocator via format.h. */
QJSIntlStatus qjs_intl_duration_format(const QJSIntlDurationFormatter *, const QJSTemporalDuration *,
                                      QJSIntlDurationFormatted *);
#endif
