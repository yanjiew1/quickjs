/* Staged canonical runtime archive output, separate from compiler input ABI.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
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
