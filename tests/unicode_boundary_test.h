/* Shared official Unicode boundary conformance-file runner.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef TEST_UNICODE_BOUNDARY_H
#define TEST_UNICODE_BOUNDARY_H
#include <stddef.h>
#include <stdint.h>
typedef size_t UnicodeBoundaryNext(const uint16_t *, size_t, size_t);
unsigned int test_unicode_boundary_file(const char *, UnicodeBoundaryNext *);
#endif
