/* Borrowed immutable mapping identity and keys, independent of CE encoding.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
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
