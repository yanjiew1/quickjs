/* Checked immutable prefix/suffix queries for compiled tailoring overlays.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_TRIE_H
#define QJS_INTL_COLLATION_TRIE_H
#include "overlay.h"
#include "keys.h"
#include "../native/pack.h"

#define QJS_INTL_COLLATION_TRIE_MAX_SIZE (64u * 1024 * 1024)

typedef struct QJSIntlCollationTrie {
    const uint8_t *nodes, *edges, *suppressed;
    uint32_t node_count, edge_count, suppressed_count;
} QJSIntlCollationTrie;

/* Node16: edge start/count, suffix-root ID, terminal mapping ID. Edge8:
   scalar/child ID. NONE is UINT32_MAX. IDs increase from each parent to child
   or suffix root. Root groups by first mapped scalar, then reversed prefix;
   suffix roots start a forward suffix trie including the first mapped scalar.
   Suppression is a separate sorted scalar set. All words are explicit LE32.

   key index is already validated and unchanged. Open validates the whole trie,
   including exact agreement with every key/split and unique terminal IDs.
   Temporary allocator state is released before return; no retained allocation.
   Bytes outlive the borrowed output/overlay and remain immutable; the view
   descriptor itself stays alive at the same address for every overlay/matcher
   created from it. Key-index storage is needed only during open. Output is
   fresh/disjoint and resets on failure. Opening distinguishes OOM from bad data. */
QJSIntlStatus qjs_intl_collation_trie_open(const QJSIntlSection *nodes, const QJSIntlSection *edges,
                                           const QJSIntlSection *suppressed,
                                           const QJSIntlCollationKeyIndex *, const QJSIntlAllocator *,
                                           QJSIntlCollationTrie *);
QJSIntlStatus qjs_intl_collation_trie_overlay(const QJSIntlCollationTrie *, QJSIntlCollationOverlay *);
#endif
