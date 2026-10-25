/* Borrowed supplemental division-unit derivations, independent of unit text.
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
#ifndef QJS_INTL_UNIT_GRAMMAR_DATA_H
#define QJS_INTL_UNIT_GRAMMAR_DATA_H
#include "../native/data.h"

typedef enum QJSIntlUnitGrammarFeature {
    QJS_INTL_UNIT_GRAMMAR_PLURAL,
    QJS_INTL_UNIT_GRAMMAR_CASE,
    QJS_INTL_UNIT_GRAMMAR_FEATURE_COUNT
} QJSIntlUnitGrammarFeature;

typedef struct QJSIntlUnitGrammarRule {
    QJSIntlBytes locale, first, second;
} QJSIntlUnitGrammarRule;

typedef struct QJSIntlUnitGrammarData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *indices, *rows;
    uint32_t row_count;
} QJSIntlUnitGrammarData;

/* Require an unchanged validated data view. Validate section 22 and borrow its
   storage. Temporary validation allocations are released on every path. Follow
   pack.h's disjoint-output contract; non-NULL outputs are reset on failure.
   Rules contain source facts, not plural evaluation or unit-pattern fallback. */
QJSIntlStatus qjs_intl_unit_grammar_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                             QJSIntlUnitGrammarData *);

/* Allocate nothing and borrow the owner's unchanged storage. The origin may
   be outside the prepared locale registry. "compound" passes through the
   caller's feature; otherwise the value is a fixed plural category or case
   token. Plural and case may have different origins. */
QJSIntlStatus qjs_intl_unit_grammar_at(const QJSIntlUnitGrammarData *, uint32_t locale,
                                      QJSIntlUnitGrammarFeature, QJSIntlUnitGrammarRule *);
#endif
