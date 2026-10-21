/* Shared source-file ordering audit for raw and ranked collation readers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_TEST_INTL_COLLATION_ORDERED_H
#define QJS_TEST_INTL_COLLATION_ORDERED_H
#include "../src/intl/common/types.h"

typedef QJSIntlStatus (*QJSIntlOrderedCompare)(void *, const uint16_t *, size_t,
                                              const uint16_t *, size_t, int *);

/* Optional complete CLDR source-file audit, separate from bounded repo tests.
   Compare neighboring counted UTF-16 strings; comments/sort keys are diagnostics
   only. Reject future input that exceeds explicit parser bounds. */
void qjs_test_intl_collation_ordered(const char *, QJSIntlOrderedCompare, void *);
#endif
