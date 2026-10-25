/* Prescribed ECMA-402 format matching, independent of backend candidates.
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
