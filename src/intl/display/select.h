/* Native label-selection policy over immutable source layers.
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
