/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_COMPARE_H
#define QJS_INTL_COLLATION_COMPARE_H
#include "contexts.h"

/* Default root comparison: forward primary, secondary and tertiary levels,
   non-ignorable variable weights, no identical tie-breaker. This is a private
   foundation, not a locale-tailored Collator or its settings interface.
   data is an unchanged validated contexts view; inputs and output are disjoint.
   order is reset on failure and otherwise receives -1, 0 or 1. No input/view
   ownership is transferred; allocator callbacks may not reenter this operation. */
QJSIntlStatus qjs_intl_collation_compare_root(const QJSIntlCollationContexts *data,
                                             const uint16_t *left, size_t left_length,
                                             const uint16_t *right, size_t right_length,
                                             const QJSIntlAllocator *allocator, int *order);
#endif
