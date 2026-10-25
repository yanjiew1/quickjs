/* Whole-group order, aliases, atomic replacement and allocation failure.
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
#include "../tools/intl/collation-reorder.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t intl_root_fixture[], intl_root_shared_group[];
extern const size_t intl_root_fixture_size, intl_root_shared_group_size;

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *p;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    p = malloc(size);
    assert(p);
    state->live++;
    return p;
}

static void *resize(void *opaque, void *p, size_t size)
{
    Allocation *state = opaque;
    assert(p && size);
    if (++state->calls == state->fail_at)
        return NULL;
    p = realloc(p, size);
    assert(p);
    return p;
}

static void release(void *opaque, void *p)
{
    Allocation *state = opaque;
    assert(p && state->live);
    state->live--;
    free(p);
}

static QJSIntlAllocator allocator(Allocation *state)
{
    return (QJSIntlAllocator){state, allocate, resize, release};
}

static void put32(uint8_t *p, uint32_t code)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        p[i] = code >> (i * 8);
}

static QJSIntlStatus apply(QJSIntlCollationReorder *owner, const char *const *tokens, size_t count)
{
    uint8_t name[28], values[256];
    QJSIntlIRInstruction node = {.kind = QJS_INTL_IR_SETTING};
    size_t i, j, at = 0;
    for (i = 0; i < 7; i++)
        put32(name + i * 4, (uint8_t)"reorder"[i]);
    for (i = 0; i < count; i++) {
        size_t length = strlen(tokens[i]);
        assert(length + 1 <= sizeof(values) / 4 - at);
        for (j = 0; j <= length; j++)
            put32(values + at++ * 4, (uint8_t)tokens[i][j]);
    }
    if (!count)
        put32(values + at++ * 4, 0);
    node.fields[0] = (QJSIntlIRScalars){name, 7};
    node.fields[1] = (QJSIntlIRScalars){values, (uint32_t)at};
    return qjs_intl_collation_reorder_apply(owner, &node);
}

static void ranks(const QJSIntlCollationReorder *owner, const uint32_t expected[8])
{
    uint32_t i, rank;
    assert(owner->count == 8);
    for (i = 0; i < 8; i++)
        assert(!qjs_intl_collation_reorder_rank(owner, i, &rank) && rank == expected[i]);
    assert(qjs_intl_collation_reorder_rank(owner, 8, &rank) == QJS_INTL_INVALID_ARGUMENT && !rank);
}

static void test_order_and_atomicity(const QJSIntlRootInput *root)
{
    static const uint32_t original[] = {0, 1, 2, 3, 4, 5, 6, 7};
    static const uint32_t hani_latin[] = {0, 1, 2, 3, 4, 7, 5, 6};
    static const uint32_t digit_end[] = {0, 1, 2, 3, 7, 4, 5, 6};
    static const uint32_t hani_first[] = {0, 1, 2, 3, 4, 6, 5, 7};
    const char *const first[] = {"Hani", "others", "Latn"}, *const digit[] = {"others", "digit"};
    const char *const hani[] = {"Hani"}, *const others[] = {"others"}, *const unknown[] = {"Zzzz"};
    const char *const duplicate[] = {"Hani", "Hani"}, *const duplicate_others[] = {"others", "Zzzz"};
    const char *const empty[] = {"Hani", ""}, *const common[] = {"Zyyy"}, *const unavailable[] = {"Fake"};
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationReorder owner;
    assert(!qjs_intl_collation_reorder_init(&owner, root, &a));
    ranks(&owner, original);
    assert(!apply(&owner, first, 3));
    ranks(&owner, hani_latin);
    assert(!apply(&owner, digit, 2));
    ranks(&owner, digit_end); /* A later list replaces the complete prior order. */
    assert(!apply(&owner, hani, 1));
    ranks(&owner, hani_first);
    assert(apply(&owner, duplicate, 2) == QJS_INTL_DATA_ERROR);
    assert(apply(&owner, duplicate_others, 2) == QJS_INTL_DATA_ERROR);
    assert(apply(&owner, empty, 2) == QJS_INTL_DATA_ERROR);
    assert(apply(&owner, common, 1) == QJS_INTL_DATA_ERROR);
    assert(apply(&owner, NULL, 0) == QJS_INTL_DATA_ERROR);
    assert(apply(&owner, unavailable, 1) == QJS_INTL_UNSUPPORTED);
    ranks(&owner, hani_first);
    state.fail_at = state.calls + 1;
    assert(apply(&owner, others, 1) == QJS_INTL_NO_MEMORY);
    ranks(&owner, hani_first);
    state.fail_at = 0;
    assert(!apply(&owner, others, 1));
    ranks(&owner, original);
    assert(!apply(&owner, unknown, 1));
    ranks(&owner, original);
    qjs_intl_collation_reorder_clear(&owner);
    qjs_intl_collation_reorder_clear(&owner);
    assert(!state.live);
    state.fail_at = state.calls + 1;
    assert(qjs_intl_collation_reorder_init(&owner, root, &a) == QJS_INTL_NO_MEMORY);
    qjs_intl_collation_reorder_clear(&owner);
    assert(!state.live);
}

static void test_aliases_and_tokens(const QJSIntlRootInput *root)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationReorder owner;
    uint8_t name[28], values[24];
    QJSIntlIRInstruction node = {.kind = QJS_INTL_IR_SETTING};
    const char *const duplicate[] = {"Hira", "Kana"}, *const alias[] = {"Hani", "Hrkt"};
    const uint32_t expected[] = {0, 1, 2, 3, 4, 6, 5, 7};
    uint32_t i, hira, kana;
    assert(!qjs_intl_root_input_group(root, (QJSIntlBytes){"Hira", 4}, &hira));
    assert(!qjs_intl_root_input_group(root, (QJSIntlBytes){"Kana", 4}, &kana) && hira == kana);
    assert(!qjs_intl_collation_reorder_init(&owner, root, &a));
    assert(!apply(&owner, alias, 2));
    ranks(&owner, expected);
    assert(apply(&owner, duplicate, 2) == QJS_INTL_DATA_ERROR);
    for (i = 0; i < 7; i++)
        put32(name + i * 4, (uint8_t)"reorder"[i]);
    node.fields[0] = (QJSIntlIRScalars){name, 7};
    node.fields[1] = (QJSIntlIRScalars){values, 2};
    put32(values, 0x100);
    put32(values + 4, 0);
    assert(qjs_intl_collation_reorder_apply(&owner, &node) == QJS_INTL_DATA_ERROR);
    for (i = 0; i < 4; i++)
        put32(values + i * 4, (uint8_t)"Hani"[i]);
    node.fields[1].count = 4; /* Valid scalars but missing the value terminator. */
    assert(qjs_intl_collation_reorder_apply(&owner, &node) == QJS_INTL_DATA_ERROR);
    ranks(&owner, expected);
    qjs_intl_collation_reorder_clear(&owner);
    assert(!state.live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRootInput root;
    assert(!qjs_intl_root_input_open(intl_root_fixture, intl_root_fixture_size, &a, &root));
    test_order_and_atomicity(&root);
    assert(!qjs_intl_root_input_open(intl_root_shared_group, intl_root_shared_group_size, &a, &root));
    test_aliases_and_tokens(&root);
    assert(!state.live);
    return 0;
}
