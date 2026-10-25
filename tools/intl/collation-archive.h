/* Staged canonical runtime archive output, separate from compiler input ABI.
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
#ifndef QJS_INTL_COMPILER_ARCHIVE_H
#define QJS_INTL_COMPILER_ARCHIVE_H
#include "collation-trie.h"

typedef struct QJSIntlArchiveBuffer {
    uint8_t *bytes;
    uint32_t length, capacity;
} QJSIntlArchiveBuffer;

typedef struct QJSIntlCollationArchive {
    QJSIntlAllocator allocator;
    QJSIntlRootInput root;
    QJSIntlArchiveBuffer programs, labels, blocks;
    uint32_t count;
    int ready, failed;
} QJSIntlCollationArchive;

typedef struct QJSIntlCompiledCollation {
    QJSIntlAllocator allocator;
    uint8_t *bytes;
    size_t length;
} QJSIntlCompiledCollation;

/* Root is validated, unchanged and alive until archive clear. Initialize fresh
   storage, clear before reuse. Failed commands poison staging: abort/clear it.
   No output becomes externally visible until successful finish. */
QJSIntlStatus qjs_intl_collation_archive_init(QJSIntlCollationArchive *, const QJSIntlRootInput *,
                                              const QJSIntlAllocator *);
void qjs_intl_collation_archive_clear(QJSIntlCollationArchive *);
/* Copy one program, in strict source-identity order, from the same root owner.
   Snapshot and trie were successfully created and remain unchanged.
   Validate trie/key agreement before copying. Inputs may be cleared on return;
   no ownership transfer, callback or pointer into them is retained. */
QJSIntlStatus qjs_intl_collation_archive_add(QJSIntlCollationArchive *, const QJSIntlCollationSnapshot *,
                                             const QJSIntlCompiledTrie *);
/* Fresh/disjoint output, empty on failure. Image owns canonical LE bytes and
   outlives all input/staging owners. Locale/type bindings are initially empty;
   Python source selection adds those before publishing a prepared QJIN pack. */
QJSIntlStatus qjs_intl_collation_archive_finish(QJSIntlCollationArchive *, QJSIntlCompiledCollation *);
void qjs_intl_compiled_collation_clear(QJSIntlCompiledCollation *);
#endif
