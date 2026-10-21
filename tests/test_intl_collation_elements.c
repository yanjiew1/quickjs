/* Root matching, immutable preceding context and initialization ownership.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/collation/elements.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

static QJSIntlAllocator allocator(Allocation *state)
{
    return (QJSIntlAllocator){state, allocate, resize, release};
}

static void check(const QJSIntlCollationContexts *data, const uint16_t *input, size_t length,
                  const uint16_t *primary, const uint16_t *secondary, size_t count)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationElements it;
    QJSIntlCollationElement element;
    size_t i;
    int present;
    assert(!qjs_intl_collation_elements_init(&it, data, input, length, &a));
    /* The callbacks themselves are copied; this descriptor need not survive. */
    memset(&a, 0, sizeof(a));
    for (i = 0; i < count; i++) {
        assert(!qjs_intl_collation_elements_next(&it, &element, &present) && present);
        assert(element.primary == primary[i] && element.secondary == secondary[i]);
        /* These authored CEs use tertiary 2 except the zero-CE and implicit
           trailing weights. Space alone carries this fixture's variable flag. */
        assert(element.tertiary == (secondary[i] ? 2 : 0));
        assert(element.variable == (primary[i] == 0x500));
    }
    assert(!qjs_intl_collation_elements_next(&it, &element, &present) && !present);
    assert(!qjs_intl_collation_elements_next(&it, &element, &present) && !present);
    qjs_intl_collation_elements_clear(&it);
    qjs_intl_collation_elements_clear(&it);
    assert(!state.live);
}

#define VALUES(...) __VA_ARGS__
#define CHECK(text, p, s) do { \
    const uint16_t text_values[] = VALUES text; \
    const uint16_t primary_values[] = VALUES p; \
    const uint16_t secondary_values[] = VALUES s; \
    assert(sizeof(primary_values) == sizeof(secondary_values)); \
    check(data, text_values, sizeof(text_values) / sizeof(*text_values), \
          primary_values, secondary_values, sizeof(primary_values) / sizeof(*primary_values)); \
} while (0)

static void matching(const QJSIntlCollationContexts *data)
{
    /* Longest contiguous matching may pass a missing intermediate full key. */
    CHECK(({'a','b','c'}), ({0x2000}), ({0x20}));
    CHECK(({'a',0x323,0x301}), ({0x3800}), ({0x20}));
    CHECK(({'a',0x323,0x331,0x301}), ({0x1000,0,0,0}), ({0x20,0x35,0x37,0x31}));
    CHECK(({'a','b','x'}), ({0x1000,0x1300,0x1200}), ({0x20,0x20,0x20}));
    CHECK(({0xe5}), ({0x3000}), ({0x20})); /* Complete canonical decomposition. */
    CHECK(({'a',0x30a}), ({0x3000}), ({0x20}));
    /* A skipped lower-class mark does not block later successful equal-class
       extensions; a failed equal-class mark does block its remaining group. */
    CHECK(({'a',0x323,0x30a,0x301}), ({0x3100,0}), ({0x20,0x35}));
    CHECK(({'a',0x304,0x30a}), ({0x1000,0,0}), ({0x20,0x32,0x34}));
    CHECK(({'a',0x34f,0x30a}), ({0x1000,0,0}), ({0x20,0,0x34}));
    CHECK(({'a',0x323,0x30a,0x304}), ({0x3000,0x3200}), ({0x20,0x20}));
    /* Before-context observes original text even after an overlapping
       contraction; a failed longer prefix/suffix trial consumes nothing. */
    CHECK(({'x','l',0xb7}), ({0x3400,0}), ({0x20,0x41}));
    CHECK(({'o','l',0xb7,'z'}), ({0x3300,0}), ({0x20,0x43}));
    CHECK(({'o','l',0xb7,'x'}), ({0x3300,0}), ({0x20,0x46}));
    CHECK(({'b','l',0xb7,'x'}), ({0x3500,0,0x1200}), ({0x20,0x42,0x20}));
    CHECK(({'l',0xb7,'z'}), ({0x3600}), ({0x20}));
    CHECK(({'x','l',0xb7,'z'}), ({0x3400,0,0x1900}), ({0x20,0x41,0x20}));
    CHECK(({' '}), ({0x500}), ({0x20}));
    CHECK(({'l',':','x'}), ({0x1500,0x1800,0x1200}), ({0x20,0x20,0x20}));
    CHECK(({'a',0x323,0x30a,0xb7}), ({0x3000,0,0}), ({0x20,0x35,0x45}));
    CHECK(({0xd83d,0xde00}), ({0x4500}), ({0x20}));
    CHECK(({0xd800}), ({0xfbc1,0xd800}), ({0x20,0}));
    CHECK(({0x378}), ({0xfbc0,0x8378}), ({0x20,0}));
    CHECK(({0xac00}), ({0x4600,0x4700}), ({0x20,0x20}));
    CHECK(({0xfffe,0xffff}), ({0x200,0xffff}), ({0x20,0x20}));
    check(data, NULL, 0, NULL, NULL, 0);
}
#undef CHECK
#undef VALUES

static void long_runs(const QJSIntlCollationContexts *data)
{
    enum { N = 4096 };
    uint16_t *input = malloc((N + 2) * sizeof(*input));
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationElements it;
    QJSIntlCollationElement element;
    int i, present;
    assert(input);
    for (i = 0; i < N; i++)
        input[i] = 'v';
    assert(!qjs_intl_collation_elements_init(&it, data, input, N, &a));
    assert(!qjs_intl_collation_elements_next(&it, &element, &present) && present);
    assert(element.primary == 0x4900);
    /* Following a long left-to-right deletion chain must not recurse. */
    assert(!qjs_intl_collation_elements_next(&it, &element, &present) && !present);
    qjs_intl_collation_elements_clear(&it);
    input[0] = 'a';
    for (i = 1; i <= N; i++)
        input[i] = 0x323;
    input[N + 1] = 0x30a;
    assert(!qjs_intl_collation_elements_init(&it, data, input, N + 2, &a));
    assert(!qjs_intl_collation_elements_next(&it, &element, &present) && present);
    assert(element.primary == 0x3000);
    for (i = 0; i < N; i++) {
        assert(!qjs_intl_collation_elements_next(&it, &element, &present) && present);
        assert(!element.primary && element.secondary == 0x35);
    }
    assert(!qjs_intl_collation_elements_next(&it, &element, &present) && !present);
    qjs_intl_collation_elements_clear(&it);
    assert(!state.live);
    free(input);
}

static void input_lifetime(const QJSIntlCollationContexts *data)
{
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationContexts borrowed_view = *data;
    QJSIntlCollationElements it;
    QJSIntlCollationElement element;
    uint16_t *input = malloc(2 * sizeof(*input));
    int present;
    assert(input);
    input[0] = 'a';
    input[1] = 0x30a;
    assert(!qjs_intl_collation_elements_init(&it, &borrowed_view, input, 2, &a));
    /* Only pack bytes and the allocator's opaque owner remain borrowed. */
    memset(input, 0, 2 * sizeof(*input));
    free(input);
    memset(&borrowed_view, 0, sizeof(borrowed_view));
    assert(!qjs_intl_collation_elements_next(&it, &element, &present) && present);
    assert(element.primary == 0x3000);
    assert(!qjs_intl_collation_elements_next(&it, &element, &present) && !present);
    qjs_intl_collation_elements_clear(&it);
    assert(!state.live);
}

static void failures(const QJSIntlCollationContexts *data)
{
    uint16_t input[129];
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationContexts missing = {0};
    QJSIntlCollationElements it;
    QJSIntlCollationElement element;
    size_t n;
    int present;
    /* Recursive decomposition forces NFD growth before long-run sort scratch. */
    input[0] = 0x1e69;
    for (n = 1; n < sizeof(input) / sizeof(*input); n++)
        input[n] = n & 1 ? 0x301 : 0x323;
    assert(qjs_intl_collation_elements_init(&it, &missing, input, 129, &a) == QJS_INTL_UNSUPPORTED);
    qjs_intl_collation_elements_clear(&it);
    assert(qjs_intl_collation_elements_init(&it, data, input, SIZE_MAX, &a) == QJS_INTL_OVERFLOW);
    qjs_intl_collation_elements_clear(&it);
    assert(!state.calls && !state.live);
    for (n = 1;; n++) {
        QJSIntlStatus status;
        assert(n < 128);
        state.calls = 0;
        state.fail_at = n;
        status = qjs_intl_collation_elements_init(&it, data, input, 129, &a);
        if (!status) {
            qjs_intl_collation_elements_clear(&it);
            assert(!state.live);
            break;
        }
        assert(status == QJS_INTL_NO_MEMORY);
        assert(qjs_intl_collation_elements_next(&it, &element, &present) == QJS_INTL_INVALID_ARGUMENT);
        qjs_intl_collation_elements_clear(&it);
        qjs_intl_collation_elements_clear(&it);
        assert(!state.live);
    }
}

static void mapping_interface(const QJSIntlCollationContexts *data)
{
    static const uint16_t input[] = {'a', 'b', 'c', 0x378, '0', '0', '2', 0x34f, '0'};
    Allocation state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationMatch match;
    QJSIntlCollationMatchResult result;
    QJSIntlCollationNumber number;
    QJSIntlCollationElement element;
    int present;
    assert(!qjs_intl_collation_match_init(&match, data, input,
                                         sizeof(input) / sizeof(*input), &a));
    /* The shared matcher identifies storage or an unmatched scalar. It does
       not invent implicit CEs or expose an expansion one element at a time. */
    assert(!qjs_intl_collation_match_next(&match, &result, &number, &present) && present);
    assert(result.kind == QJS_INTL_COLLATION_MATCH_ROOT && !number.length);
    assert(result.entry.key_count == 3 && result.entry.element_count == 1);
    assert(!qjs_intl_collation_element_at(&result.entry, 0, &element));
    assert(element.primary == 0x2000);
    assert(!qjs_intl_collation_match_next(&match, &result, &number, &present) && present);
    assert(result.kind == QJS_INTL_COLLATION_MATCH_SCALAR && result.scalar == 0x378);
    assert(!qjs_intl_collation_match_next(&match, &result, &number, &present) && present);
    assert(result.kind == QJS_INTL_COLLATION_MATCH_NUMBER && number.length == 1);
    assert(number.digits[0] == '2');
    assert(!qjs_intl_collation_match_next(&match, &result, &number, &present) && present);
    assert(result.kind == QJS_INTL_COLLATION_MATCH_ROOT && !number.length);
    assert(!qjs_intl_collation_match_next(&match, &result, &number, &present) && present);
    assert(result.kind == QJS_INTL_COLLATION_MATCH_NUMBER && number.length == 1);
    assert(number.digits[0] == '0');
    assert(!qjs_intl_collation_match_next(&match, &result, &number, &present) && !present);
    assert(result.kind == QJS_INTL_COLLATION_MATCH_NONE && !number.length);
    assert(!qjs_intl_collation_match_next(&match, &result, &number, &present) && !present);
    qjs_intl_collation_match_clear(&match);
    qjs_intl_collation_match_clear(&match);
    assert(!state.live);
}

int main(void)
{
    QJSIntlPack pack;
    QJSIntlCollationContexts data;
    assert(!qjs_intl_pack_open(qjs_intl_collation_fixture_bytes,
                              qjs_intl_collation_fixture_length, &pack));
    assert(!qjs_intl_collation_contexts_open(&pack, &data));
    matching(&data);
    long_runs(&data);
    input_lifetime(&data);
    failures(&data);
    mapping_interface(&data);
    return 0;
}
