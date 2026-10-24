/* Compiled list service: inventories, resolved CLDR contexts and ownership. */
#include "../src/intl/list/list.h"
#include "../src/intl/native/data.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (ptr)
        state->live++;
    return ptr;
}

static void *resize(void *opaque, void *old, size_t size)
{
    Allocation *state = opaque;
    int had_old = old != NULL;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = realloc(old, size);
    if (ptr && !had_old)
        state->live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static size_t find(const QJSIntlBytes *locales, size_t count, const char *tag)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if (locales[i].length == strlen(tag) &&
            !memcmp(locales[i].data, tag, locales[i].length))
            return i;
    }
    return SIZE_MAX;
}

static void check_pair(QJSIntlListFormatter *formatter,
                       const QJSIntlAllocator *allocator, const uint16_t *second,
                       size_t second_length, const uint16_t *expected,
                       size_t expected_length)
{
    static const uint16_t first[] = {'A'};
    QJSIntlUTF16 items[] = {{first, 1}, {second, second_length}};
    QJSIntlFormatted result = {0};
    assert(qjs_intl_list_format(formatter, items, 2, &result) == QJS_INTL_OK);
    assert(result.length == expected_length);
    assert(!memcmp(result.text, expected, expected_length * sizeof(*expected)));
    assert(result.part_count == 3);
    assert(result.parts[0].type == QJS_INTL_PART_ELEMENT);
    assert(result.parts[1].type == QJS_INTL_PART_LITERAL);
    assert(result.parts[2].type == QJS_INTL_PART_ELEMENT);
    qjs_intl_list_result_clear(allocator, &result);
    assert(!result.text && !result.parts);
    qjs_intl_list_result_clear(allocator, &result);
}

static void test_formatter_failures(QJSIntlListService *service, size_t locale,
                                    QJSIntlAllocator *allocator)
{
    Allocation *state = allocator->opaque;
    QJSIntlListFormatter *formatter;
    size_t live = state->live, start = state->calls, calls, i;
    assert(qjs_intl_list_formatter_open(service, locale, QJS_INTL_LIST_CONJUNCTION,
                                        QJS_INTL_LIST_LONG, &formatter) == QJS_INTL_OK);
    calls = state->calls - start;
    qjs_intl_list_formatter_close(formatter);
    assert(state->live == live);
    for (i = 1; i <= calls; i++) {
        state->fail_at = state->calls + i;
        assert(qjs_intl_list_formatter_open(service, locale, QJS_INTL_LIST_CONJUNCTION,
                                            QJS_INTL_LIST_LONG,
                                            &formatter) == QJS_INTL_NO_MEMORY);
        assert(!formatter && state->live == live);
        state->fail_at = 0;
    }
}

static void test_shared_data(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlData data = {0};
    QJSIntlListService *services[2] = {NULL, NULL};
    unsigned int order;

    assert(qjs_intl_list_open(&allocator, &data, &services[0]) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!services[0] && !state.calls);
    for (order = 0; order < 2; order++) {
        const QJSIntlBytes *locales;
        QJSIntlListFormatter *formatter;
        size_t count, english;
        static const uint16_t letter[] = {'i'};
        static const uint16_t expected[] = {'A', ' ', 'a', 'n', 'd', ' ', 'i'};

        assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length,
                                   &allocator, &data));
        assert(!qjs_intl_list_open(&allocator, &data, &services[0]));
        assert(!qjs_intl_list_open(&allocator, &data, &services[1]));
        /* Only compiled bytes are borrowed. Neither service retains the
           source view variable or a pointer into the other service. */
        memset(&data, 0, sizeof(data));
        qjs_intl_list_close(services[order]);
        assert(!qjs_intl_list_available(services[1 - order], &locales, &count));
        english = find(locales, count, "en");
        assert(english != SIZE_MAX);
        assert(!qjs_intl_list_formatter_open(services[1 - order], english,
                                             QJS_INTL_LIST_CONJUNCTION,
                                             QJS_INTL_LIST_LONG, &formatter));
        check_pair(formatter, &allocator, letter, 1, expected, 7);
        qjs_intl_list_formatter_close(formatter);
        qjs_intl_list_close(services[1 - order]);
        assert(!state.live);
    }
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlListService *service;
    QJSIntlListFormatter *formatter;
    const QJSIntlBytes *locales;
    size_t count, i, calls, english, spanish, hebrew;
    unsigned int type, style;
    static const uint16_t letter_i[] = {'i'}, letter_o[] = {'o'};
    static const uint16_t english_expected[] = {'A', ' ', 'a', 'n', 'd', ' ', 'i'};
    static const uint16_t spanish_and[] = {'A', ' ', 'e', ' ', 'i'};
    static const uint16_t spanish_or[] = {'A', ' ', 'u', ' ', 'o'};
    static const uint16_t aleph[] = {0x5d0}, non_hebrew[] = {0x590};
    static const uint16_t hebrew_expected[] = {'A', ' ', 0x5d5, 0x5d0};
    static const uint16_t hebrew_hyphen[] = {'A', ' ', 0x5d5, '-', 0x590};

    assert(qjs_intl_list_open(&allocator, NULL, &service) == QJS_INTL_OK);
    calls = state.calls;
    assert(qjs_intl_list_available(service, &locales, &count) == QJS_INTL_OK);
    assert(TEST_INTL_PROFILE ? count > 1000 : count == 2);
    for (i = 0; i < count; i++) {
        assert(locales[i].length &&
               !(locales[i].length == 4 && !memcmp(locales[i].data, "root", 4)));
        if (i) {
            size_t n = locales[i - 1].length < locales[i].length ? locales[i - 1].length
                                                                 : locales[i].length;
            int order = memcmp(locales[i - 1].data, locales[i].data, n);
            assert(order < 0 ||
                   (order == 0 && locales[i - 1].length < locales[i].length));
        }
    }
    english = find(locales, count, "en");
    spanish = find(locales, count, "es");
    hebrew = find(locales, count, "he");
    assert(english != SIZE_MAX);
    for (i = 0; i < count; i++) {
        static const uint16_t first[] = {'A'};
        QJSIntlUTF16 items[] = {{first, 1}, {letter_i, 1}};
        for (type = 0; type < 3; type++) {
            for (style = 0; style < 3; style++) {
                QJSIntlFormatted result = {0};
                assert(qjs_intl_list_formatter_open(service, i, type, style,
                                                    &formatter) == QJS_INTL_OK);
                assert(qjs_intl_list_format(formatter, items, 2, &result) ==
                       QJS_INTL_OK);
                assert(result.length >= 2 && result.part_count >= 2);
                qjs_intl_list_result_clear(&allocator, &result);
                qjs_intl_list_formatter_close(formatter);
            }
        }
    }
    for (type = 0; type < 3; type++) {
        for (style = 0; style < 3; style++) {
            QJSIntlFormatted result = {0};
            assert(qjs_intl_list_formatter_open(service, english, type, style,
                                                &formatter) == QJS_INTL_OK);
            assert(qjs_intl_list_format(formatter, NULL, 0, &result) == QJS_INTL_OK);
            assert(!result.length && !result.part_count);
            qjs_intl_list_result_clear(&allocator, &result);
            qjs_intl_list_formatter_close(formatter);
        }
    }
    test_formatter_failures(service, english, &allocator);
    if (TEST_INTL_PROFILE) {
        static const uint16_t russian_expected[] = {'A', ' ', 0x438, ' ', 'i'};
        size_t russian = find(locales, count, "ru");
        assert(spanish != SIZE_MAX && hebrew != SIZE_MAX);
        assert(russian != SIZE_MAX);
        assert(qjs_intl_list_formatter_open(service, russian, QJS_INTL_LIST_CONJUNCTION,
                                            QJS_INTL_LIST_LONG,
                                            &formatter) == QJS_INTL_OK);
        check_pair(formatter, &allocator, letter_i, 1, russian_expected, 5);
        qjs_intl_list_formatter_close(formatter);
        assert(qjs_intl_list_formatter_open(service, spanish, QJS_INTL_LIST_CONJUNCTION,
                                            QJS_INTL_LIST_LONG,
                                            &formatter) == QJS_INTL_OK);
        check_pair(formatter, &allocator, letter_i, 1, spanish_and, 5);
        qjs_intl_list_formatter_close(formatter);
        assert(qjs_intl_list_formatter_open(service, spanish, QJS_INTL_LIST_DISJUNCTION,
                                            QJS_INTL_LIST_LONG,
                                            &formatter) == QJS_INTL_OK);
        check_pair(formatter, &allocator, letter_o, 1, spanish_or, 5);
        qjs_intl_list_formatter_close(formatter);
        assert(qjs_intl_list_formatter_open(service, hebrew, QJS_INTL_LIST_CONJUNCTION,
                                            QJS_INTL_LIST_LONG,
                                            &formatter) == QJS_INTL_OK);
        check_pair(formatter, &allocator, aleph, 1, hebrew_expected, 4);
        check_pair(formatter, &allocator, non_hebrew, 1, hebrew_hyphen, 5);
        qjs_intl_list_formatter_close(formatter);
        test_formatter_failures(service, hebrew, &allocator);
    }
    assert(qjs_intl_list_formatter_open(service, count, 0, 0, &formatter) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!formatter);
    assert(qjs_intl_list_formatter_open(service, english, -1, 0, &formatter) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_formatter_open(service, english, 0, 3, &formatter) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_formatter_open(service, english, 0, 0, &formatter) ==
           QJS_INTL_OK);
    qjs_intl_list_close(service);
    check_pair(formatter, &allocator, letter_i, 1, english_expected, 7);
    qjs_intl_list_formatter_close(formatter);
    assert(!state.live);
    for (i = 1; i <= calls; i++) {
        state.fail_at = state.calls + i;
        assert(qjs_intl_list_open(&allocator, NULL, &service) == QJS_INTL_NO_MEMORY);
        assert(!service && !state.live);
        state.fail_at = 0;
    }
    assert(qjs_intl_list_open(NULL, NULL, &service) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_open(&allocator, NULL, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_list_available(NULL, &locales, &count) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!locales && !count);
    qjs_intl_list_formatter_close(NULL);
    qjs_intl_list_close(NULL);
    test_shared_data();
    puts("Intl compiled list service tests passed");
    return 0;
}
