/* Service identity, resolved policies, lifetime and bounded allocation failures.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/collation/collation.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Live {
    void *ptr;
    size_t call;
} Live;

typedef struct Allocation {
    size_t calls, fail_at, live;
    Live slots[64];
} Allocation;

static size_t slot(Allocation *state, void *ptr)
{
    size_t i;

    for (i = 0; i < sizeof(state->slots) / sizeof(*state->slots); i++) {
        if (state->slots[i].ptr == ptr)
            return i;
    }
    abort();
}

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;

    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr) {
        state->slots[slot(state, NULL)] = (Live){ptr, state->calls};
        state->live++;
    }
    return ptr;
}

static void *resize(void *opaque, void *ptr, size_t size)
{
    Allocation *state = opaque;
    size_t index = slot(state, ptr);
    int had_ptr = ptr != NULL;
    void *next;

    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    next = realloc(ptr, size);
    if (next) {
        state->slots[index] = (Live){next, state->calls};
        state->live += !had_ptr;
    }
    return next;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;

    assert(ptr && state->live);
    state->slots[slot(state, ptr)] = (Live){0};
    state->live--;
    free(ptr);
}

static QJSIntlBytes bytes(const char *text)
{
    return (QJSIntlBytes){text, strlen(text)};
}

static int same(QJSIntlBytes value, const char *text)
{
    return value.length == strlen(text) && !memcmp(value.data, text, value.length);
}

static size_t find(const QJSIntlBytes *available, size_t count, const char *name)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (same(available[i], name))
            return i;
    }
    abort();
}

static void inventory(QJSIntlCollationService *service, Allocation *state, size_t en)
{
    QJSIntlBytes name, previous = {0};
    uint32_t cursor = 0;
    size_t calls = state->calls, count = 0;
    int supported, emoji = 0, eor = 0;
    QJSIntlStatus status;

    state->fail_at = state->calls + 1;
    while (!(status = qjs_intl_collation_available_type_next(service, &cursor, &name))) {
        if (previous.data) {
            size_t length = previous.length < name.length ? previous.length : name.length;
            int order = memcmp(previous.data, name.data, length);

            assert(order < 0 || (!order && previous.length < name.length));
        }
        assert(!same(name, "standard") && !same(name, "search") && !same(name, "default"));
        emoji |= same(name, "emoji");
        eor |= same(name, "eor");
        previous = name;
        count++;
    }
    assert(status == QJS_INTL_UNSUPPORTED && !name.data && !name.length);
    assert(emoji && eor && (TEST_INTL_PROFILE ? count > 2 : count == 2));
    assert(qjs_intl_collation_available_type_next(service, &cursor, &name) == QJS_INTL_UNSUPPORTED);

    for (cursor = 0, count = 0; !(status = qjs_intl_collation_type_next(service, en, QJS_INTL_COLLATION_SORT,
                                                                     &cursor, &name)); count++) {
        assert(!qjs_intl_collation_type_supported(service, en, QJS_INTL_COLLATION_SORT, name, &supported));
        assert(supported);
    }
    assert(status == QJS_INTL_UNSUPPORTED && count == 2 && !name.data && !name.length);
    cursor = 0;
    assert(qjs_intl_collation_type_next(service, en, QJS_INTL_COLLATION_SEARCH, &cursor, &name) == QJS_INTL_UNSUPPORTED);
    assert(!qjs_intl_collation_type_supported(service, en, QJS_INTL_COLLATION_SEARCH, bytes("emoji"), &supported));
    assert(!supported);
    {
        const char *unknown[] = {"", "default", "standard", "foobar", "EMOJI"};
        size_t i;

        for (i = 0; i < sizeof(unknown) / sizeof(*unknown); i++) {
            assert(!qjs_intl_collation_type_supported(service, en, QJS_INTL_COLLATION_SORT, bytes(unknown[i]), &supported));
            assert(!supported);
        }
    }
    assert(state->calls == calls);
    state->fail_at = 0;
}

static void invalid_arguments(QJSIntlCollationService *service, size_t count, size_t en)
{
    QJSIntlCollationOptions options, zero = {0};
    QJSIntlCollator *collator;
    QJSIntlBytes name;
    const QJSIntlBytes *available;
    uint32_t cursor = UINT32_MAX;
    size_t length = 123;
    int supported = 123, order = 123;

    assert(qjs_intl_collation_available(NULL, &available, &length) == QJS_INTL_INVALID_ARGUMENT);
    assert(!available && !length);
    memset(&options, 0xff, sizeof(options));
    assert(qjs_intl_collation_defaults(service, count, QJS_INTL_COLLATION_SORT, &options) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&options, &zero, sizeof(options)));
    assert(qjs_intl_collation_type_supported(service, en, QJS_INTL_COLLATION_SORT,
                                             (QJSIntlBytes){NULL, 1}, &supported) == QJS_INTL_INVALID_ARGUMENT);
    assert(!supported);
    assert(qjs_intl_collation_type_supported(service, en, QJS_INTL_COLLATION_SORT,
                                             (QJSIntlBytes){"emoji\0x", 7}, &supported) == QJS_INTL_OK && !supported);
    assert(qjs_intl_collation_type_next(service, en, QJS_INTL_COLLATION_SORT,
                                       &cursor, &name) == QJS_INTL_INVALID_ARGUMENT);
    assert(cursor == UINT32_MAX && !name.data && !name.length);
    assert(qjs_intl_collation_available_type_next(service, &cursor, &name) == QJS_INTL_INVALID_ARGUMENT);
    assert(cursor == UINT32_MAX && !name.data && !name.length);

    assert(!qjs_intl_collation_defaults(service, en, QJS_INTL_COLLATION_SORT, &options));
    options.numeric = 2;
    assert(qjs_intl_collator_open(service, en, &options, &collator) == QJS_INTL_INVALID_ARGUMENT && !collator);
    assert(qjs_intl_collator_compare(NULL, NULL, 0, NULL, 0, &order) == QJS_INTL_INVALID_ARGUMENT && !order);
}

static void check(QJSIntlCollationService *service, size_t locale, QJSIntlCollationOptions *options,
                   const uint16_t *a, size_t an, const uint16_t *b, size_t bn, int expected)
{
    QJSIntlCollator *collator;
    int order;

    assert(!qjs_intl_collator_open(service, locale, options, &collator));
    assert(!qjs_intl_collator_compare(collator, a, an, b, bn, &order) && order == expected);
    assert(!qjs_intl_collator_compare(collator, b, bn, a, an, &order) && order == -expected);
    qjs_intl_collator_close(collator);
}

static void policies(QJSIntlCollationService *service, size_t en)
{
    QJSIntlCollationOptions options;
    static const uint16_t e[] = {'e'}, upper[] = {'E'}, accent[] = {0xe9}, decomposed[] = {'e', 0x301};
    static const uint16_t ab[] = {'a', 'b'}, dash[] = {'a', '-', 'b'}, space[] = {'a', ' ', 'b'};
    static const uint16_t currency[] = {'a', '$', 'b'}, middle_dot[] = {'L', 0xb7}, l[] = {'L'};
    static const uint16_t two[] = {'2'}, ten[] = {'1', '0'}, zero_two[] = {'0', '2'};
    static const uint16_t arab_ten[] = {0x661, 0x660}, nul[] = {'a', 0, 'b'}, a[] = {'a'};
    static const uint16_t surrogate[] = {0xd800}, next_surrogate[] = {0xd801};

    assert(!qjs_intl_collation_defaults(service, en, QJS_INTL_COLLATION_SORT, &options));
    assert(options.sensitivity == QJS_INTL_COLLATION_VARIANT && !options.collation.data);
    check(service, en, &options, accent, 1, decomposed, 2, 0);
    check(service, en, &options, nul, 3, a, 1, 1);
    check(service, en, &options, surrogate, 1, next_surrogate, 1, -1);
    options.case_first = QJS_INTL_COLLATION_LOWER_FIRST;
    check(service, en, &options, e, 1, upper, 1, -1);
    options.case_first = QJS_INTL_COLLATION_UPPER_FIRST;
    check(service, en, &options, e, 1, upper, 1, 1);

    options.sensitivity = QJS_INTL_COLLATION_BASE;
    check(service, en, &options, e, 1, upper, 1, 0);
    check(service, en, &options, e, 1, accent, 1, 0);
    options.sensitivity = QJS_INTL_COLLATION_ACCENT;
    check(service, en, &options, e, 1, upper, 1, 0);
    check(service, en, &options, e, 1, accent, 1, -1);
    options.sensitivity = QJS_INTL_COLLATION_CASE;
    check(service, en, &options, e, 1, accent, 1, 0);
    check(service, en, &options, e, 1, upper, 1, 1);

    options.sensitivity = QJS_INTL_COLLATION_VARIANT;
    check(service, en, &options, dash, 3, ab, 2, -1);
    options.ignore_punctuation = 1;
    check(service, en, &options, dash, 3, ab, 2, 0);
    check(service, en, &options, space, 3, ab, 2, 0);
    check(service, en, &options, currency, 3, ab, 2, -1);
    options.ignore_punctuation = 0;
    check(service, en, &options, two, 1, ten, 2, 1);
    options.numeric = 1;
    check(service, en, &options, two, 1, ten, 2, -1);
    check(service, en, &options, zero_two, 2, two, 1, 0);
    check(service, en, &options, arab_ten, 2, two, 1, 1);
    check(service, en, &options, NULL, 0, NULL, 0, 0);

    /* Root search suppresses the L|middle-dot prefix so the dot is findable
       in every context. This distinguishes real usage targets, not just lists. */
    options.sensitivity = QJS_INTL_COLLATION_BASE;
    options.numeric = 0;
    check(service, en, &options, middle_dot, 2, l, 1, 0);
    assert(!qjs_intl_collation_defaults(service, en, QJS_INTL_COLLATION_SEARCH, &options));
    options.sensitivity = QJS_INTL_COLLATION_BASE;
    options.ignore_punctuation = 0;
    check(service, en, &options, middle_dot, 2, l, 1, 1);
}

static void lifetime_and_failures(QJSIntlCollationService *service, size_t en, Allocation *state)
{
    QJSIntlCollationOptions options;
    QJSIntlCollator *collator;
    static const uint16_t a[] = {'a'}, b[] = {'b'};
    char type[] = "emoji";
    size_t calls, i, live;
    int order;

    assert(!qjs_intl_collation_defaults(service, en, QJS_INTL_COLLATION_SORT, &options));
    options.collation = bytes("foobar");
    assert(qjs_intl_collator_open(service, en, &options, &collator) == QJS_INTL_UNSUPPORTED && !collator);
    options.collation = bytes(type);
    state->fail_at = state->calls + 1;
    assert(qjs_intl_collator_open(service, en, &options, &collator) == QJS_INTL_NO_MEMORY && !collator);
    state->fail_at = 0;
    assert(!qjs_intl_collator_open(service, en, &options, &collator));
    memset(type, 'x', sizeof(type) - 1);
    memset(&options, 0xff, sizeof(options));
    qjs_intl_collation_close(service);
    assert(state->live == 1);

    calls = state->calls;
    live = state->live;
    assert(!qjs_intl_collator_compare(collator, a, 1, b, 1, &order) && order == -1);
    calls = state->calls - calls;
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        order = 123;
        assert(qjs_intl_collator_compare(collator, a, 1, b, 1, &order) == QJS_INTL_NO_MEMORY);
        assert(!order && state->live == live);
    }
    state->fail_at = 0;
    assert(qjs_intl_collator_compare(collator, NULL, 1, b, 1, &order) == QJS_INTL_INVALID_ARGUMENT && !order);
    qjs_intl_collator_close(collator);
    assert(!state->live);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlCollationService *service;
    const QJSIntlBytes *available;
    size_t count, en, i, failure_ids[66], failures = 2;

    qjs_intl_collation_close(NULL);
    qjs_intl_collator_close(NULL);
    assert(!qjs_intl_collation_open(&allocator, NULL, &service));
    assert(!qjs_intl_collation_available(service, &available, &count));
    assert(TEST_INTL_PROFILE ? count > 2 : count == 2);
    en = find(available, count, "en");

    /* Sweep persistent service owners and an early validation allocation.
       Deep reader OOM coverage uses small authored fixtures elsewhere, avoiding
       thousands of complete native-profile rescans in quick repository tests. */
    failure_ids[0] = 1;
    failure_ids[1] = 2;
    for (i = 0; i < sizeof(state.slots) / sizeof(*state.slots); i++) {
        if (state.slots[i].ptr) {
            size_t j;

            for (j = 0; j < failures && failure_ids[j] != state.slots[i].call; j++)
                ;
            if (j == failures)
                failure_ids[failures++] = state.slots[i].call;
        }
    }
    inventory(service, &state, en);
    invalid_arguments(service, count, en);
    policies(service, en);
    if (TEST_INTL_PROFILE) {
        QJSIntlCollationOptions options;
        size_t de = find(available, count, "de"), sv = find(available, count, "sv");
        static const uint16_t umlaut[] = {0xe4}, ae[] = {'a', 'e'}, z[] = {'z'};

        assert(!qjs_intl_collation_defaults(service, de, QJS_INTL_COLLATION_SORT, &options));
        options.sensitivity = QJS_INTL_COLLATION_BASE;
        check(service, de, &options, umlaut, 1, ae, 2, -1);
        options.collation = bytes("phonebk");
        check(service, de, &options, umlaut, 1, ae, 2, 0);
        assert(!qjs_intl_collation_defaults(service, sv, QJS_INTL_COLLATION_SORT, &options));
        options.sensitivity = QJS_INTL_COLLATION_BASE;
        check(service, sv, &options, umlaut, 1, z, 1, 1);
        assert(!qjs_intl_collation_defaults(service, find(available, count, "th"), QJS_INTL_COLLATION_SORT, &options));
        assert(options.ignore_punctuation);
    }
    lifetime_and_failures(service, en, &state);

    for (i = 0; i < failures; i++) {
        state.calls = 0;
        state.fail_at = failure_ids[i];
        assert(qjs_intl_collation_open(&allocator, NULL, &service) == QJS_INTL_NO_MEMORY && !service);
        assert(!state.live);
    }
    state.fail_at = 0;
    assert(qjs_intl_collation_open(NULL, NULL, &service) == QJS_INTL_INVALID_ARGUMENT && !service);
    return 0;
}
