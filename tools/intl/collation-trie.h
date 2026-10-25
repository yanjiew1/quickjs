/* Host producer for the runtime's checked immutable overlay trie.
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
