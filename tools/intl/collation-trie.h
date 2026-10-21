/* Host producer for the runtime's checked immutable overlay trie.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMPILER_TRIE_H
#define QJS_INTL_COMPILER_TRIE_H
#include "collation-snapshot.h"
#include "../../src/intl/collation/trie.h"

typedef struct QJSIntlCompiledTrie {
    QJSIntlAllocator allocator;
    uint8_t *node_bytes, *edge_bytes, *suppressed_bytes;
    QJSIntlSection nodes, edges, suppressed;
} QJSIntlCompiledTrie;

/* Snapshot is unchanged throughout creation. Copy only live keys/suppression;
   output owns LE arrays independently of the snapshot. Fresh/disjoint output,
   empty on error. Allocator state outlives clear; do not copy a live owner. */
QJSIntlStatus qjs_intl_compiled_trie_create(const QJSIntlCollationSnapshot *, QJSIntlCompiledTrie *);
void qjs_intl_compiled_trie_clear(QJSIntlCompiledTrie *);
#endif
