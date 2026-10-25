/* Optional normalized mapping queries, shared by build-time and frozen storage.
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
#ifndef QJS_INTL_COLLATION_OVERLAY_H
#define QJS_INTL_COLLATION_OVERLAY_H
#include "../common/types.h"

#define QJS_INTL_COLLATION_NODE_NONE UINT32_MAX
#define QJS_INTL_COLLATION_SCALAR_MASK UINT32_C(0xffffff)

/* A narrow, non-allocating private storage interface. Queries cannot fail:
   mutable owners supply valid nodes; frozen readers must validate pack bytes
   first. Copyable cursors contain stable IDs, never reallocatable pointers.
   The owner is unchanged throughout a complete matcher scan; immutable sequence
   IDs remain valid across later mutations. All callbacks are required.

   Text is immutable NFD with CCC in its high byte: mask scalars before key
   comparison. Prefix queries inspect original contiguous text, even if consumed.
   longest_prefix returns the greatest matching length strictly below limit,
   at most start, or zero. Only prefixes with this first mapped scalar qualify.
   begin returns the suffix-trie node BEFORE that scalar (or NONE).
   terminal returns a sequence ID or NONE; zero is a valid, possibly ignorable
   mapping. has_children may overestimate, but cannot hide a live continuation.

   Suppression filters inherited prefixes and multi-scalar contractions starting
   at the mapped scalar, preserving ordinary root singletons. It never filters
   overlay entries, which can be introduced after suppression. */
typedef struct QJSIntlCollationOverlay {
    const void *opaque;
    uint32_t (*longest_prefix)(const void *, const uint32_t *text, uint32_t start,
                               uint32_t limit);
    uint32_t (*begin)(const void *, uint32_t first_scalar, const uint32_t *prefix,
                      uint32_t prefix_count);
    uint32_t (*step)(const void *, uint32_t node, uint32_t scalar);
    uint32_t (*terminal)(const void *, uint32_t node);
    int (*has_children)(const void *, uint32_t node);
    int (*root_context_suppressed)(const void *, uint32_t first_scalar);
} QJSIntlCollationOverlay;
#endif
