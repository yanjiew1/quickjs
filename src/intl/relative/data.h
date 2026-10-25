/* Resolved native relative-time source facts; no formatting fallback policy.
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
#ifndef QJS_INTL_RELATIVE_DATA_H
#define QJS_INTL_RELATIVE_DATA_H
#include "types.h"
#include "../native/data.h"
#include "../plural/rules.h"

typedef struct QJSIntlRelativeData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *keys, *locale_rows, *rows, *maps, *entries;
    uint32_t key_count, row_count, map_count, entry_count;
} QJSIntlRelativeData;

typedef struct QJSIntlRelativeMap {
    const uint8_t *entries;
    uint32_t count;
} QJSIntlRelativeMap;

typedef struct QJSIntlRelativeForm {
    QJSIntlBytes text, origin_locale;
    QJSIntlRelativeUnit origin_unit;
    QJSIntlRelativeWidth origin_width;
    unsigned int blocked;
} QJSIntlRelativeForm;

/* Borrow an unchanged validated shared owner. Check section shape, canonical
   sharing, source identities, literal keys, counted template syntax and required
   long other forms. No formatter availability or selection policy is inferred.
   malloc/free are required; all validation scratch is released before return.
   Follow pack.h's disjoint-output contract; non-NULL outputs reset on error. */
QJSIntlStatus qjs_intl_relative_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                          QJSIntlRelativeData *);
/* Exact shared-locale index and unit/width. Queries borrow storage and allocate
   nothing; no width, category or locale fallback. Require a map returned by
   this unchanged owner. Absent selectors return UNSUPPORTED; explicit blocked
   and empty values return OK and remain distinct. Literal query keys are
   arbitrary explicit-length Number strings, including non-integer misses. */
QJSIntlStatus qjs_intl_relative_map_at(const QJSIntlRelativeData *, uint32_t locale,
                                       QJSIntlRelativeUnit, QJSIntlRelativeWidth,
                                       QJSIntlRelativeMap *);
QJSIntlStatus qjs_intl_relative_pattern_find(const QJSIntlRelativeData *, const QJSIntlRelativeMap *,
                                             QJSIntlRelativeTense, QJSIntlPluralCategory,
                                             QJSIntlRelativeForm *);
QJSIntlStatus qjs_intl_relative_literal_find(const QJSIntlRelativeData *, const QJSIntlRelativeMap *,
                                             QJSIntlBytes key, QJSIntlRelativeForm *);
#endif
