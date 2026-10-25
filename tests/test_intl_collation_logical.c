/* Logical implicit pairs and the complete virtual root-primary ordering.
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
#include "../src/intl/collation/logical.h"
#include <assert.h>

static uint32_t ordinal(uint32_t key)
{
    uint32_t result;
    assert(!qjs_intl_collation_primary_ordinal(key, &result));
    return result;
}

static void test_ordinal_domain(void)
{
    uint32_t primary, tail, expected = 0, out;
    assert(ordinal(0) == 0);
    /* An independent enumeration proves adjacency, not just selected examples
       of the arithmetic formula. No virtual root array is allocated. */
    for (primary = 1; primary <= 0xffff; primary++) {
        assert(ordinal(primary << 16) == ++expected);
        if (primary >= 0xfb00 && primary <= 0xfbe1) {
            for (tail = 0x8000; tail <= 0xffff; tail++)
                assert(ordinal((primary << 16) | tail) == ++expected);
        }
    }
    assert(expected == QJS_INTL_COLLATION_PRIMARY_ROOT_MAX);
    assert(qjs_intl_collation_primary_ordinal(1, &out) == QJS_INTL_INVALID_ARGUMENT && !out);
    assert(qjs_intl_collation_primary_ordinal(0xfb000001, &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_primary_ordinal(0xfaff8000, &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_primary_ordinal(0xfbe28000, &out) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_primary_ordinal(0, NULL) == QJS_INTL_INVALID_ARGUMENT);
}

static void test_pairs_and_metadata(void)
{
    const QJSIntlCollationElement ordinary[] = {
        {0, 0, 0, 0}, {0, 0x21, 2, 0}, {0, 0, 2, 0},
        {0x200, 0x20, 2, 0}, {0x201, 0x20, 2, 1}, {0xffff, 0x20, 2, 0},
    };
    const uint16_t heads[] = {0xfb00, 0xfb01, 0xfb40, 0xfbe1};
    const uint16_t tails[] = {0x8000, 0xffff, 0xfbc0};
    QJSIntlCollationLogical logical;
    unsigned int consumed, i, j;
    for (i = 0; i < sizeof(ordinary) / sizeof(*ordinary); i++) {
        assert(!qjs_intl_collation_logical_root(ordinary + i, NULL, &logical, &consumed));
        assert(consumed == 1 && !logical.implicit);
        assert(logical.primary == ordinal((uint32_t)ordinary[i].primary << 16));
        assert(logical.secondary == ordinary[i].secondary && logical.tertiary == ordinary[i].tertiary);
        assert(logical.variable == ordinary[i].variable);
    }
    for (i = 0; i < sizeof(heads) / sizeof(*heads); i++) {
        for (j = 0; j < sizeof(tails) / sizeof(*tails); j++) {
            QJSIntlCollationElement first = {heads[i], 0x20, 0x14, 0};
            QJSIntlCollationElement second = {tails[j], 0, 0, 0};
            assert(!qjs_intl_collation_logical_root(&first, &second, &logical, &consumed));
            assert(consumed == 2 && logical.implicit && !logical.variable);
            assert(logical.primary == ordinal(((uint32_t)heads[i] << 16) | tails[j]));
            assert(logical.secondary == 0x20 && logical.tertiary == 0x14);
        }
    }
}

static void test_malformed_root_pairs(void)
{
    QJSIntlCollationElement first = {0xfb40, 0x20, 2, 0};
    const QJSIntlCollationElement tails[] = {
        {0x7fff, 0, 0, 0}, {0x8000, 1, 0, 0},
        {0x8000, 0, 1, 0}, {0x8000, 0, 0, 1},
    };
    const QJSIntlCollationElement invalid[] = {
        {0x8000, 0, 0, 0}, {0x1000, 0, 2, 0}, {0x1000, 0x20, 0, 0},
        {0, 0x20, 0, 0}, {0, 0, 0, 1}, {0x1000, 0x20, 2, 2},
    };
    QJSIntlCollationLogical logical;
    unsigned int consumed, i;
    assert(qjs_intl_collation_logical_root(&first, NULL, &logical, &consumed) == QJS_INTL_DATA_ERROR);
    for (i = 0; i < sizeof(tails) / sizeof(*tails); i++) {
        assert(qjs_intl_collation_logical_root(&first, tails + i, &logical, &consumed) ==
               QJS_INTL_DATA_ERROR);
        assert(!consumed && !logical.primary && !logical.secondary && !logical.tertiary &&
               !logical.variable && !logical.implicit);
    }
    for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
        assert(qjs_intl_collation_logical_root(invalid + i, NULL, &logical, &consumed) ==
               QJS_INTL_DATA_ERROR);
    assert(qjs_intl_collation_logical_root(NULL, NULL, &logical, &consumed) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_logical_root(&first, NULL, NULL, &consumed) == QJS_INTL_INVALID_ARGUMENT);
    assert(!consumed);
}

int main(void)
{
    test_ordinal_domain();
    test_pairs_and_metadata();
    test_malformed_root_pairs();
    return 0;
}
