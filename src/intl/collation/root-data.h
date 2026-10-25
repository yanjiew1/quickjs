/* Root UCA mappings; storage lookup only, without comparison policy.
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
#ifndef QJS_INTL_COLLATION_ROOT_DATA_H
#define QJS_INTL_COLLATION_ROOT_DATA_H
#include "../native/pack.h"
#include "weights.h"

typedef struct QJSIntlCollationRoot {
    QJSIntlSection section;
    const uint8_t *mappings, *keys, *elements;
    uint32_t key_count, element_count;
} QJSIntlCollationRoot;

typedef struct QJSIntlCollationEntry {
    const uint8_t *keys, *elements;
    uint32_t key_count, element_count;
} QJSIntlCollationEntry;

/* Borrow an unchanged validated pack, independent of locale/string tables.
   Validate the optional section without allocation; missing is UNSUPPORTED.
   Views/entries borrow immutable pack bytes. Follow pack.h's disjoint-output
   contract; non-NULL outputs are reset on failure. No teardown is needed. */
QJSIntlStatus qjs_intl_collation_root_open(const QJSIntlPack *, QJSIntlCollationRoot *);
QJSIntlStatus qjs_intl_collation_root_entry_at(const QJSIntlCollationRoot *, uint32_t,
                                              QJSIntlCollationEntry *);
QJSIntlStatus qjs_intl_collation_key_at(const QJSIntlCollationEntry *, uint32_t, uint32_t *);
QJSIntlStatus qjs_intl_collation_element_at(const QJSIntlCollationEntry *, uint32_t,
                                            QJSIntlCollationElement *);
/* Exact or longest contiguous scalar-prefix lookup. Empty/missing keys return
   UNSUPPORTED. Values outside the stored scalar domain simply do not match;
   do not scan an entire remaining string on every prefix lookup. The result's
   key_count is the consumed prefix length. Normalization, implicit weights and
   discontiguous contractions belong to element generation, not this reader. */
QJSIntlStatus qjs_intl_collation_root_find(const QJSIntlCollationRoot *, const uint32_t *, size_t,
                                          QJSIntlCollationEntry *);
QJSIntlStatus qjs_intl_collation_root_prefix(const QJSIntlCollationRoot *, const uint32_t *, size_t,
                                            QJSIntlCollationEntry *);
#endif
