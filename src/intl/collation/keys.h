/* Borrowed immutable mapping identity and keys, independent of CE encoding.
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
#ifndef QJS_INTL_COLLATION_KEYS_H
#define QJS_INTL_COLLATION_KEYS_H
#include "../common/types.h"

typedef struct QJSIntlCollationKeyIndex {
    const uint8_t *rows, *keys, *contexts;
    uint32_t row_count, key_count, context_count;
} QJSIntlCollationKeyIndex;

typedef struct QJSIntlCollationKeySpan {
    const uint8_t *keys;
    uint32_t count;
} QJSIntlCollationKeySpan;

/* Validated readers construct this view; these accessors do not validate pack
   bytes. All words are LE32. Mapping rows have 16 bytes: key_start/count followed
   by two payload fields owned solely by the CE reader. Context rows have 12
   bytes: mapping ID, prefix scalar count, reader-specific field. Context rows
   sort by mapping ID. Search keys concatenate prefix + suffix; the full order
   is (combined key, prefix count), permitting distinct splits of the same key.
   Existing raw allkeys readers may enforce stricter unique-key validation.
   All bytes outlive the unchanged view and callers; no allocation/teardown. */
QJSIntlStatus qjs_intl_collation_keys_at(const QJSIntlCollationKeyIndex *, uint32_t,
                                         QJSIntlCollationKeySpan *);
uint32_t qjs_intl_collation_keys_prefix(const QJSIntlCollationKeyIndex *, uint32_t mapping_id);
QJSIntlStatus qjs_intl_collation_keys_context(const QJSIntlCollationKeyIndex *, uint32_t,
                                              uint32_t *mapping_id, uint32_t *prefix_count);
#endif
