/* Native clock rules; name selection and rendering remain separate.
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
#ifndef QJS_INTL_DATETIME_DAYPERIOD_H
#define QJS_INTL_DATETIME_DAYPERIOD_H
#include "dayperiod-data.h"

typedef struct QJSIntlDayPeriodRuleList {
    uint32_t start, count;
} QJSIntlDayPeriodRuleList;

/* Select one whole list by simple locale truncation, then root, independently
   of the exceptional main-data parent graph. Require an already validated
   canonical source locale without extensions. Do not merge parent codes or
   substitute another kind; explicit empty/point-only lists stop inheritance.
   Borrow unchanged validated data, allocate nothing and reset failed output.
   UNSUPPORTED means no list, distinct from successful count zero. */
QJSIntlStatus qjs_intl_dayperiod_resolve(const QJSIntlDayPeriodData *data,
                                       QJSIntlDayPeriodRuleSetKind kind, QJSIntlBytes locale,
                                       QJSIntlDayPeriodRuleList *out);
/* Select a semantic code from local clock seconds 0..86399. fraction is a
   boolean saying the supplied local-time record has a nonzero subsecond part.
   The caller applies its specified precision conversion first; omitted display
   fields do not round the clock. a always chooses AM/PM; b proposes exact
   midnight/noon then AM/PM. Neither reads data/rules. B uses a resolved list
   from this unchanged owner: exact points, variable ranges, then AM/PM.
   use_midnight is a boolean caller policy: when false, b at 00:00 uses AM
   and B ignores the exact midnight point in favor of the variable range.
   Noon remains available. TR35 treats midnight support as a caller choice
   because start-of-day and end-of-day usage can be ambiguous.
   A NULL B list denotes absent rules; empty lists also fall back to AM/PM.
   Keep fixed/variable label availability and localized-name fallback with the
   renderer. Allocate nothing, change no inputs and reset output on failure. */
QJSIntlStatus qjs_intl_dayperiod_select(const QJSIntlDayPeriodData *data,
                                      const QJSIntlDayPeriodRuleList *rules, unsigned int symbol,
                                      uint32_t second, unsigned int fraction, unsigned int use_midnight,
                                      QJSIntlDayPeriodCode *out);
#endif
