/* Native dialect-name matching over actual source candidates.
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
#ifndef QJS_INTL_DISPLAY_LANGUAGE_MATCH_H
#define QJS_INTL_DISPLAY_LANGUAGE_MATCH_H
#include "select.h"

typedef struct QJSIntlLanguageNameMatch {
    QJSIntlNameLabel label;
    QJSIntlBytes code; /* Normalized matched source key, not the API identity. */
    size_t subtags; /* Includes the primary language; zero when absent. */
} QJSIntlLanguageNameMatch;

/* Input is a fully validated, canonical Unicode Language ID, without extensions
   or private use. Caller handles aliases and duplicate variants before this
   operation. dialect=0 selects the primary only; dialect=1 chooses the present
   source candidate with most ordered included subtags, then earlier matching
   input positions and alphabetical key. Resolve each candidate's style and
   inheritance before ranking. Missing base yields OK with a zero result.

   Query only the same-primary-language inventory range, without enumerating
   variant subsets. Allocate nothing and borrow all results from the unchanged
   data owner. No qualifier composition, code fallback or locale admission.
   Follow pack.h's disjoint-output contract; reset output on failure. */
QJSIntlStatus qjs_intl_language_name_match(const QJSIntlNameData *, uint32_t locale,
                                         QJSIntlNameStyle, int dialect,
                                         QJSIntlBytes canonical, QJSIntlLanguageNameMatch *);
#endif
