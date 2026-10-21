/* Pinned Unicode text operations shared by host collation compilation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMPILER_TEXT_H
#define QJS_INTL_COMPILER_TEXT_H
#include "../../src/intl/common/types.h"

typedef struct QJSIntlCompilerText {
    uint32_t *codes;
    uint32_t count;
} QJSIntlCompilerText;

/* Output is fresh/cleared and disjoint from input; it owns its scalar array.
   The allocator remains alive through clear. No NUL-termination is implied. */
QJSIntlStatus qjs_intl_compiler_text_nfd(const QJSIntlAllocator *, const uint32_t *, uint32_t,
                                         QJSIntlCompilerText *);
void qjs_intl_compiler_text_clear(const QJSIntlAllocator *, QJSIntlCompilerText *);
/* Starred rules require both unchanged NFD and zero combining class. Check the
   original scalar: normalizing first would hide a forbidden decomposition. */
QJSIntlStatus qjs_intl_compiler_text_inert(const QJSIntlAllocator *, uint32_t, int *);
#endif
