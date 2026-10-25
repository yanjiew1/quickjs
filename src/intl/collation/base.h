/* Logical root semantics shared by compiler input and runtime wire readers.
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
#ifndef QJS_INTL_COLLATION_BASE_H
#define QJS_INTL_COLLATION_BASE_H
#include "coordinates.h"
#include "keys.h"
#include "../native/pack.h"
#include "../native/wire.h"

typedef struct QJSIntlCollationBaseMetadata {
    QJSIntlCollationRootLimits limits;
    uint32_t han_base, han_count, generic_base, generic_count, generic_limit;
    uint32_t variable_top, numeric, field_separator;
} QJSIntlCollationBaseMetadata;

typedef struct QJSIntlCollationBaseSource {
    QJSIntlCollationBaseMetadata metadata;
    QJSIntlSection mappings, keys, contexts, elements, han_ranges, han_ranks;
} QJSIntlCollationBaseSource;

typedef struct QJSIntlCollationBase {
    QJSIntlCollationBaseMetadata metadata;
    QJSIntlCollationKeyIndex keys;
    QJSIntlSection elements, han_ranges, han_ranks;
    int ready;
} QJSIntlCollationBase;

/* The enclosing reader proves readable span extents and source/version identity.
   Check exact strides/counts and all common metadata/key/CE/Han semantics here.
   Output copies the descriptor and borrows unchanged bytes; no retained memory
   or teardown. Temporary audit allocation is released on success/error. Output
   is fresh/disjoint, resets on error, and distinguishes OOM from malformed data. */
QJSIntlStatus qjs_intl_collation_base_open(const QJSIntlCollationBaseSource *, const QJSIntlAllocator *,
                                           QJSIntlCollationBase *);
/* Also used for compiler reset-source facts. Metadata is from an opened base;
   record has 24 readable bytes. Checking does not allocate or retain a pointer. */
int qjs_intl_collation_base_check_element(const QJSIntlCollationBaseMetadata *, const uint8_t *record);
/* Decode only after validation. Fact and pool consumers share the same flags. */
static inline void qjs_intl_collation_base_read_element(const uint8_t *p, QJSIntlCollationCoordinate *out)
{
    unsigned int i;
    uint32_t flags = qjs_intl_read_u32(p + 20);
    for (i = 0; i < 4; i++)
        out->weights[i] = qjs_intl_read_u32(p + i * 4);
    out->root_primary = qjs_intl_read_u32(p + 16);
    out->case_value = flags & 3;
    out->variable = !!(flags & 4);
    out->implicit = !!(flags & 8);
}

/* Queries require an unchanged successfully opened base. Outputs reset on error,
   never allocate and are disjoint from input and each other. Implicit accepts isolated UTF-16
   surrogates too; stored keys are strictly Unicode scalar sequences. */
QJSIntlStatus qjs_intl_collation_base_mapping(const QJSIntlCollationBase *, uint32_t id,
                                              uint32_t *ce_start, uint32_t *ce_count);
QJSIntlStatus qjs_intl_collation_base_element(const QJSIntlCollationBase *, uint32_t index,
                                              QJSIntlCollationCoordinate *);
QJSIntlStatus qjs_intl_collation_base_implicit(const QJSIntlCollationBase *, uint32_t code,
                                               QJSIntlCollationCoordinate *);
#endif
