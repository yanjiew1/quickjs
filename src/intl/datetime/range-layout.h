/* Proven date/clock operands for same-day interval composition.
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
#ifndef QJS_INTL_DATETIME_RANGE_LAYOUT_H
#define QJS_INTL_DATETIME_RANGE_LAYOUT_H
#include "pattern.h"
#include "../common/format.h"

typedef struct QJSIntlDateRangeLayout {
    QJSIntlAllocator allocator;
    QJSIntlDatePattern date, clock, connector;
} QJSIntlDateRangeLayout;

/* Retain actual, admitted date/clock operands, not substrings guessed from a
   final pattern. Decode the LDML connector and require its recomposition to
   equal the selected ordinary program. UNSUPPORTED leaves no layout when
   this factorization is not proved; operational errors remain errors.
   Own all patterns independently of the inputs. Require malloc/free and a
   fresh disjoint output; reset/unwind errors and clear with the saved allocator. */
QJSIntlStatus qjs_intl_date_range_layout_open(const QJSIntlAllocator *,
                                              const QJSIntlDatePattern *date,
                                              const QJSIntlDatePattern *clock,
                                              QJSIntlBytes connector,
                                              const QJSIntlDatePattern *ordinary,
                                              QJSIntlDateRangeLayout *);
void qjs_intl_date_range_layout_clear(QJSIntlDateRangeLayout *);

/* Require an unchanged successful layout_open result. The caller has
   established equal calendar dates and no selected whole interval. Combine one date with two complete clocks through the real range
   fallback, then the retained date/time connector. Borrow formatted operands;
   preserve their typed parts, assigning only date/glue as shared. Keep the
   fallback's endpoint order and return independent owned output. */
QJSIntlStatus qjs_intl_date_range_layout_compose(const QJSIntlDateRangeLayout *,
                                                 const QJSIntlPairPattern *fallback,
                                                 const QJSIntlFormatted *date,
                                                 const QJSIntlFormatted clocks[2],
                                                 size_t maximum_output_length,
                                                 QJSIntlFormattedRange *);
#endif
