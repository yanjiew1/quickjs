/* Default root level priority and comparison allocation failure.
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
#include "../src/intl/collation/compare.h"
#include <assert.h>
#include <stdlib.h>

extern const uint8_t qjs_intl_collation_fixture_bytes[];
extern const size_t qjs_intl_collation_fixture_length;

typedef struct Allocation {
    size_t calls, fail_at, live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    assert(ptr);
    state->live++;
    return ptr;
}

static void *resize(void *opaque, void *ptr, size_t size)
{
    Allocation *state = opaque;
    void *next;
    assert(ptr && size);
    if (++state->calls == state->fail_at)
        return NULL;
    next = realloc(ptr, size);
    assert(next);
    return next;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static void check_options(const QJSIntlCollationContexts *data,
                  const uint16_t *left, size_t left_length,
                  const uint16_t *right, size_t right_length,
                          const QJSIntlRootCollationOptions *options, int expected)
{
    Allocation state = {0};
    QJSIntlAllocator a = {&state, allocate, resize, release};
    int order = 9;
    assert(!qjs_intl_collation_compare_root(data, left, left_length, right, right_length,
                                           &a, options, &order));
    assert(order == expected && !state.live);
    assert(!qjs_intl_collation_compare_root(data, right, right_length, left, left_length,
                                           &a, options, &order));
    assert(order == -expected && !state.live);
}

static void check(const QJSIntlCollationContexts *data,
                  const uint16_t *left, size_t left_length,
                  const uint16_t *right, size_t right_length, int expected)
{
    check_options(data, left, left_length, right, right_length, NULL, expected);
}

#define CHECK(left, right, expected) do { \
    const uint16_t x[] = left, y[] = right; \
    check(data, x, sizeof(x) / sizeof(*x), y, sizeof(y) / sizeof(*y), expected); \
} while (0)
/* Compound array initializers need a wrapper to protect macro commas. */
#define V(...) {__VA_ARGS__}
static void comparisons(const QJSIntlCollationContexts *data)
{
    CHECK(V('a'), V('a'), 0);
    CHECK(V('x'), V('b'), -1); /* Deliberately differs from code-point order. */
    CHECK(V('A'), V('a'), 1);
    CHECK(V('A','x'), V('a','b'), -1); /* Primary precedes earlier case. */
    CHECK(V('u','x'), V('a','b'), -1); /* Primary precedes earlier accent. */
    CHECK(V('A'), V('u'), -1); /* Secondary precedes tertiary. */
    CHECK(V('a'), V('a','x'), -1);
    CHECK(V('a',0x301), V('a'), 1);
    CHECK(V('a',0x34f), V('a'), 0);
    CHECK(V(0xe5), V('a',0x30a), 0);
    CHECK(V(' '), V('a'), -1); /* Non-ignorable variable weight. */
    CHECK(V(0xd800), V(0xd801), -1);
    CHECK(V(0xd83d,0xde00), V('v'), -1);
    CHECK(V('a',0x323,0x30a), V(0xe5), 1);
    check(data, NULL, 0, NULL, 0, 0);
    check(data, NULL, 0, (const uint16_t[]){0x34f}, 1, 0);
    check(data, NULL, 0, (const uint16_t[]){0x301}, 1, -1);
}
#undef V
#undef CHECK

static void numeric_comparisons(const QJSIntlCollationContexts *data)
{
    const QJSIntlRootCollationOptions options = {.numeric = 1};
#define V(...) {__VA_ARGS__}
#define N(left, right, expected) do { \
    const uint16_t x[] = left, y[] = right; \
    check_options(data, x, sizeof(x) / sizeof(*x), y, sizeof(y) / sizeof(*y), \
                  &options, expected); \
} while (0)
    N(V('2'), V('1','2'), -1);
    N(V('0','0','2'), V('2'), 0);
    N(V('0','0'), V('0'), 0);
    N(V(0x662,0xff11,0xd835,0xdfda), V('2','1','2'), 0);
    N(V(0x2460), V('2'), 1); /* Circled one is not a decimal digit. */
    N(V(0xb2), V('2'), 1); /* Nor is superscript two; no NFKD. */
    N(V(0x20c4), V('0'), -1); /* Reserved numeric band follows currency. */
    N(V('9','9'), V(0x24ea), -1); /* Entire numeric band precedes ordinary zero. */
    N(V('1',0x34f,'2'), V('1','2'), -1);
    N(V('1',0,'2'), V('1','2'), -1);
    N(V('1',0x301,'2'), V('1','2'), -1);
    N(V('1','.','2'), V('1','2'), -1);
    N(V('0','2','x'), V('2','b'), -1);
    N(V('0','2','A'), V('2','a'), 1);
    N(V('0','2','A'), V('2','u'), -1);
    N(V('1','x','2'), V('1','x','1','2'), -1);
#undef N
#undef V
    check(data, (const uint16_t[]){'2'}, 1, (const uint16_t[]){'1','2'}, 2, 1);
    check(data, (const uint16_t[]){0x2460}, 1, (const uint16_t[]){'2'}, 1, -1);
    {
        const size_t lengths[] = {253,254,255,508,509};
        uint16_t left[1024], right[1024];
        size_t n, i;
        for (n = 0; n < sizeof(lengths) / sizeof(*lengths); n++) {
            size_t length = lengths[n];
            left[0] = '1';
            for (i = 1; i < length; i++)
                left[i] = '0';
            for (i = 0; i < length - 1; i++)
                right[i] = '9';
            check_options(data, left, length, right, length - 1, &options, 1);
            for (i = 0; i < length; i++)
                right[i] = left[i];
            right[length - 1] = '1';
            check_options(data, left, length, right, length, &options, -1);
        }
        for (i = 0; i < 509; i++)
            left[i] = right[i] = '1';
        left[254] = '0'; /* Never strip zeros again after a 254-digit chunk. */
        check_options(data, left, 509, right, 509, &options, -1);
        for (i = 0; i < 1024; i++)
            left[i] = '0';
        check_options(data, left, 1024, (const uint16_t[]){'0'}, 1, &options, 0);
        left[1023] = '1';
        check_options(data, left, 1024, (const uint16_t[]){'1'}, 1, &options, 0);
    }
    {
        const uint16_t inputs[][3] = {{'0'},{'0','0'},{'1'},{'0','1'},{'2'},
                                      {'9'},{'1','0'},{'1','2'},{'9','9'},{'1','0','0'}};
        const size_t lengths[] = {1,2,1,2,1,1,2,2,2,3};
        const int values[] = {0,0,1,1,2,9,10,12,99,100};
        int i, j;
        for (i = 0; i < 10; i++)
            for (j = 0; j < 10; j++)
                check_options(data, inputs[i], lengths[i], inputs[j], lengths[j], &options,
                              (values[i] > values[j]) - (values[i] < values[j]));
    }
}

static void allocation_failures(const QJSIntlCollationContexts *data)
{
    uint16_t left[100], right[200];
    Allocation state = {0};
    QJSIntlAllocator a = {&state, allocate, resize, release};
    size_t i, calls;
    int order;
    for (i = 0; i < 100; i++) {
        left[i] = right[2 * i] = 'a';
        right[2 * i + 1] = 0x34f;
    }
    /* Equal weights with different source text exhaust both streams and spill
       the small caches, exercising malloc and realloc without an identity hit. */
    assert(!qjs_intl_collation_compare_root(data, left, 100, right, 200, &a, NULL, &order));
    assert(!order && !state.live);
    calls = state.calls;
    assert(calls > 8);
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        order = 9;
        assert(qjs_intl_collation_compare_root(data, left, 100, right, 200, &a, NULL, &order) ==
               QJS_INTL_NO_MEMORY);
        assert(!order && !state.live);
    }
    /* Identical input needs no normalization or allocation at all. */
    state.calls = 0;
    state.fail_at = 1;
    assert(!qjs_intl_collation_compare_root(data, left, 100, left, 100, &a, NULL, &order));
    assert(!order && !state.calls);
}

static void case_settings(const QJSIntlCollationContexts *data)
{
    QJSIntlRootCollationOptions options = {0};
#define V(...) {__VA_ARGS__}
#define C(left, right, expected) do { \
    const uint16_t x[] = left, y[] = right; \
    check_options(data, x, sizeof(x) / sizeof(*x), y, sizeof(y) / sizeof(*y), \
                  &options, expected); \
} while (0)
    options.sensitivity = QJS_INTL_COLLATION_BASE;
    C(V('A'), V('u'), 0);
    C(V('a',0x301), V('a'), 0);
    options.sensitivity = QJS_INTL_COLLATION_ACCENT;
    C(V('A'), V('a'), 0);
    C(V('A'), V('u'), -1);
    C(V('a',0x301), V('a'), 1);
    options.sensitivity = QJS_INTL_COLLATION_CASE;
    C(V('A'), V('a'), 1);
    C(V('A'), V('u'), 1);
    C(V('a'), V('w'), 0); /* Other tertiary variants are ignored. */
    C(V('a',0x30b), V('a'), 0); /* Even a synthetic UPPER accent is ignored. */
    C(V('a',0x345), V('a',0x346), 0);
    options.case_first = QJS_INTL_COLLATION_UPPER_FIRST;
    C(V('A'), V('a'), -1);
    options.sensitivity = QJS_INTL_COLLATION_VARIANT;
    C(V('A'), V('a'), -1);
    C(V('a',0x345), V('a',0x346), -1); /* T-only case rank never inverts. */
    options.case_first = QJS_INTL_COLLATION_LOWER_FIRST;
    C(V('n'), V('p'), -1); /* Promote case within each CE, not a whole case pass. */
    options.sensitivity = QJS_INTL_COLLATION_CASE;
    C(V('n'), V('p'), 1);
    options.sensitivity = QJS_INTL_COLLATION_VARIANT;
    C(V('H'), V('h'), 1);
    options.case_first = QJS_INTL_COLLATION_CASE_OFF;
    C(V('H'), V('h'), -1); /* Off preserves raw order; it is not lower-first. */
    {
        int sensitivity, case_first;
        options.numeric = 1;
        for (sensitivity = QJS_INTL_COLLATION_VARIANT; sensitivity <= QJS_INTL_COLLATION_CASE;
             sensitivity++) {
            options.sensitivity = sensitivity;
            for (case_first = QJS_INTL_COLLATION_CASE_OFF; case_first <= QJS_INTL_COLLATION_LOWER_FIRST;
                 case_first++) {
                options.case_first = case_first;
                C(V('0','0','2'), V('2'), 0);
                C(V('0','0'), V('0'), 0);
            }
        }
    }
#undef C
#undef V
}

static void shifted_comparisons(const QJSIntlCollationContexts *data)
{
    QJSIntlRootCollationOptions options = {.ignore_punctuation = 1};
#define V(...) {__VA_ARGS__}
#define S(left, right, expected) do { \
    const uint16_t x[] = left, y[] = right; \
    check_options(data, x, sizeof(x) / sizeof(*x), y, sizeof(y) / sizeof(*y), \
                  &options, expected); \
} while (0)
    S(V(' ','a'), V('a'), 0);
    S(V('a','-',0x301), V('a'), 0); /* Accents after a variable are shifted too. */
    S(V('a',' ',0x34f,0x301), V('a'), 0);
    S(V('a',0x301), V('a'), 1); /* Ordinary accents remain significant. */
    S(V('-',0x301,'a',0x301), V('a'), 1); /* New primary restores accent weights. */
    S(V('g'), V('a'), 0); /* Expansion members retain their own variable flags. */
    S(V(0xfffe), V(' '), 1); /* Reserved low primary is not a variable. */
    S(V(0xffff), V(' '), 1);
    S(V(0xd800), V(' '), 1);
    S(V(0x20c4), V(' '), 1); /* Currency/symbol bands remain significant. */
    options.numeric = 1;
    S(V('-','0','0','2'), V('2'), 0);
    S(V('-','2',0x301), V('2'), 1); /* Numeric primary ends a variable sequence. */
    S(V('1','-','2'), V('1','2'), -1); /* Removed punctuation still delimits numbers. */
    {
        int sensitivity;
        for (sensitivity = QJS_INTL_COLLATION_VARIANT; sensitivity <= QJS_INTL_COLLATION_CASE;
             sensitivity++) {
            options.sensitivity = sensitivity;
            S(V('-','a',' '), V('a'), 0);
        }
    }
#undef S
#undef V
}

static void numeric_allocation_failures(const QJSIntlCollationContexts *data)
{
    const QJSIntlRootCollationOptions options = {.numeric = 1};
    uint16_t left[200], right[300];
    Allocation state = {0};
    QJSIntlAllocator a = {&state, allocate, resize, release};
    size_t i, calls;
    int order;
    for (i = 0; i < 100; i++) {
        left[2 * i] = right[3 * i + 1] = '1';
        left[2 * i + 1] = right[3 * i + 2] = 'x';
        right[3 * i] = '0';
    }
    assert(!qjs_intl_collation_compare_root(data, left, 200, right, 300, &a, &options, &order));
    assert(!order && !state.live);
    calls = state.calls;
    assert(calls > 8);
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        order = 9;
        assert(qjs_intl_collation_compare_root(data, left, 200, right, 300, &a, &options, &order) ==
               QJS_INTL_NO_MEMORY);
        assert(!order && !state.live);
    }
}

int main(void)
{
    QJSIntlPack pack;
    QJSIntlCollationContexts data;
    Allocation state = {0};
    QJSIntlAllocator a = {&state, allocate, resize, release};
    int order = 9;
    QJSIntlRootCollationOptions bad = {.sensitivity = (QJSIntlCollationSensitivity)-1};
    assert(!qjs_intl_pack_open(qjs_intl_collation_fixture_bytes,
                              qjs_intl_collation_fixture_length, &pack));
    assert(!qjs_intl_collation_contexts_open(&pack, &data));
    comparisons(&data);
    numeric_comparisons(&data);
    case_settings(&data);
    shifted_comparisons(&data);
    allocation_failures(&data);
    numeric_allocation_failures(&data);
    assert(qjs_intl_collation_compare_root(&data, NULL, 1, NULL, 0, &a, NULL, &order) ==
           QJS_INTL_INVALID_ARGUMENT && !order);
    order = 9;
    assert(qjs_intl_collation_compare_root(NULL, NULL, 0, NULL, 0, &a, NULL, &order) ==
           QJS_INTL_INVALID_ARGUMENT && !order);
    assert(qjs_intl_collation_compare_root(&data, NULL, 0, NULL, 0, &a, NULL, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_compare_root(&data, NULL, 0, NULL, 0, &a, &bad, &order) ==
           QJS_INTL_INVALID_ARGUMENT && !order);
    bad.sensitivity = QJS_INTL_COLLATION_VARIANT;
    bad.case_first = (QJSIntlCollationCaseFirst)3;
    assert(qjs_intl_collation_compare_root(&data, NULL, 0, NULL, 0, &a, &bad, &order) ==
           QJS_INTL_INVALID_ARGUMENT && !order);
    return 0;
}
