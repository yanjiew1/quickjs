/* Exercise storage lookup over every mapping in each prepared root profile.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/collation/elements.h"
#include "../src/intl/collation/compare.h"
#include "../src/intl/collation/logical.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

extern const uint8_t qjs_intl_native_bytes[];
extern const size_t qjs_intl_native_length;

static void *allocate(void *opaque, size_t size)
{
    (void)opaque;
    assert(size);
    return malloc(size);
}

static void *resize(void *opaque, void *ptr, size_t size)
{
    (void)opaque;
    assert(size);
    return realloc(ptr, size);
}

static void release(void *opaque, void *ptr)
{
    (void)opaque;
    free(ptr);
}

static unsigned int weight(const QJSIntlCollationElement *element, int level)
{
    return level == 0 ? element->primary : level == 1 ? element->secondary : element->tertiary;
}

/* Compare each level's nonzero weights with the independent allkeys source
   mapping. NFD and contractions may regroup CEs without changing those levels. */
static void check_elements(const QJSIntlCollationContexts *data,
                           const QJSIntlCollationEntry *expected,
                           const uint32_t *key, uint32_t source_index)
{
    QJSIntlAllocator a = {NULL, allocate, resize, release};
    QJSIntlCollationElements it;
    QJSIntlCollationElement *actual = NULL, element;
    size_t count = 0, capacity = 0, units = 0, i;
    uint16_t *input;
    uint64_t bytes = (uint64_t)expected->key_count * 2 * sizeof(*input);
    int present, level;
    assert(bytes <= SIZE_MAX);
    input = malloc((size_t)bytes);
    assert(input);
    for (i = 0; i < expected->key_count; i++) {
        uint32_t c = key[i];
        if (c <= 0xffff) {
            input[units++] = c;
        } else {
            input[units++] = 0xd800 + ((c - 0x10000) >> 10);
            input[units++] = 0xdc00 + ((c - 0x10000) & 0x3ff);
        }
    }
    assert(!qjs_intl_collation_elements_init(&it, data, input, units, &a));
    for (;;) {
        assert(!qjs_intl_collation_elements_next(&it, &element, &present));
        if (!present)
            break;
        if (count == capacity) {
            QJSIntlCollationElement *next;
            assert(capacity <= SIZE_MAX / sizeof(*actual) / 2);
            capacity = capacity ? capacity * 2 : 8;
            assert(capacity <= SIZE_MAX / sizeof(*actual));
            next = realloc(actual, capacity * sizeof(*actual));
            assert(next);
            actual = next;
        }
        actual[count++] = element;
    }
    qjs_intl_collation_elements_clear(&it);
    /* A stored implicit pair is one logical primary, even when its tail looks
       like another head. Exercise this invariant after real NFD lookup too. */
    for (i = 0; i < count;) {
        QJSIntlCollationLogical logical;
        unsigned int consumed;
        assert(!qjs_intl_collation_logical_root(actual + i, i + 1 < count ? actual + i + 1 : NULL,
                                               &logical, &consumed));
        assert(consumed == 1 || consumed == 2);
        i += consumed;
    }
    for (level = 0; level < 3; level++) {
        size_t left = 0, right = 0;
        for (;;) {
            unsigned int x = 0, y = 0;
            while (left < expected->element_count && !x) {
                assert(!qjs_intl_collation_element_at(expected, left++, &element));
                x = weight(&element, level);
            }
            while (right < count && !y)
                y = weight(&actual[right++], level);
            if (x != y) {
                fprintf(stderr, "root mapping %u, level %d: expected %x, got %x\n",
                        source_index, level + 1, x, y);
                abort();
            }
            if (!x)
                break;
        }
    }
    free(actual);
    free(input);
}

static void check_settings(const QJSIntlCollationContexts *data)
{
    QJSIntlAllocator a = {NULL, allocate, resize, release};
    QJSIntlRootCollationOptions options = {0};
    int order;
#define V(...) {__VA_ARGS__}
#define CHECK(left, right, expected) do { \
    const uint16_t x[] = left, y[] = right; \
    assert(!qjs_intl_collation_compare_root(data, x, sizeof(x) / sizeof(*x), \
                                           y, sizeof(y) / sizeof(*y), &a, &options, &order)); \
    assert(order == expected); \
    assert(!qjs_intl_collation_compare_root(data, y, sizeof(y) / sizeof(*y), \
                                           x, sizeof(x) / sizeof(*x), &a, &options, &order)); \
    assert(order == -(expected)); \
} while (0)
    CHECK(V('a'), V('A'), -1);
    options.case_first = QJS_INTL_COLLATION_UPPER_FIRST;
    CHECK(V('a'), V('A'), 1);
    CHECK(V(0x3041), V(0x3042), 1); /* Kana size, not Unicode letter case. */
    options.case_first = QJS_INTL_COLLATION_LOWER_FIRST;
    CHECK(V(0x3041), V(0x3042), -1);
    CHECK(V(0x1c5), V(0x1c4), -1); /* Titlecase expansion: UPPER, lower. */
    options.sensitivity = QJS_INTL_COLLATION_CASE;
    CHECK(V('a'), V(0xe4), 0);
    CHECK(V('a'), V(0xff41), 0);
    CHECK(V(0xfb00), V('f','f'), 0);
    CHECK(V(0xdf), V('s','s'), 0);
    CHECK(V(0x1c5), V(0x1c4), -1);
    CHECK(V(0x1c5), V(0x1c6), 1);
    options.case_first = QJS_INTL_COLLATION_UPPER_FIRST;
    CHECK(V(0x1c5), V(0x1c4), 1);
    CHECK(V(0x1c5), V(0x1c6), -1);
    options.sensitivity = QJS_INTL_COLLATION_BASE;
    CHECK(V('A'), V(0xe4), 0);
    options.sensitivity = QJS_INTL_COLLATION_ACCENT;
    CHECK(V('a'), V(0xe4), -1);
    CHECK(V('a'), V('A'), 0);
    options.numeric = 1;
    CHECK(V('0','0','2'), V(0x662), 0);
    CHECK(V(0x20c4), V('0'), -1);
    CHECK(V('9','9'), V(0x24ea), -1);
    options.ignore_punctuation = 1;
    options.sensitivity = QJS_INTL_COLLATION_VARIANT;
    CHECK(V('a','-',0x301), V('a'), 0);
    CHECK(V('a',0x301), V('a'), 1);
    CHECK(V('$'), V(' '), 1);
    CHECK(V('+'), V(' '), 1);
    CHECK(V(0xfffe), V(' '), 1);
    CHECK(V('1','-','2'), V('1','2'), -1);
#undef CHECK
#undef V
}

/* Optional complete source-file audit belongs in CI, not the quick repository
   suite. Parse code points before ';'; comments contain diagnostic sort keys only.
   Bounds are explicit: reject an unexpectedly larger future fixture. */
static void ordered_file(const QJSIntlCollationContexts *data, const char *path, int shifted)
{
    QJSIntlAllocator a = {NULL, allocate, resize, release};
    QJSIntlRootCollationOptions options = {.ignore_punctuation = shifted};
    uint16_t previous[1024], current[1024];
    size_t previous_length = 0, previous_line = 0, line_number = 0, count = 0;
    char line[4096];
    FILE *file = fopen(path, "r");
    assert(file);
    while (fgets(line, sizeof(line), file)) {
        char *at = line, *end;
        size_t length = 0;
        int order;
        line_number++;
        assert(strchr(line, '\n') || feof(file));
        while (*at == ' ' || *at == '\t')
            at++;
        if (*at == '#' || *at == '\n' || *at == '\r' || !*at)
            continue;
        while (*at != ';') {
            unsigned long code;
            errno = 0;
            code = strtoul(at, &end, 16);
            assert(end != at && !errno && code <= 0x10ffff);
            /* The official file also probes lone surrogate code units. */
            assert(length + 2 <= sizeof(current) / sizeof(*current));
            if (code <= 0xffff) {
                current[length++] = code;
            } else {
                current[length++] = 0xd800 + ((code - 0x10000) >> 10);
                current[length++] = 0xdc00 + ((code - 0x10000) & 0x3ff);
            }
            at = end;
            while (*at == ' ' || *at == '\t')
                at++;
        }
        assert(length);
        if (count) {
            QJSIntlStatus status = qjs_intl_collation_compare_root(data, previous, previous_length,
                                                                   current, length, &a, &options, &order);
            if (status || order > 0) {
                fprintf(stderr, "%s: lines %zu/%zu out of root order (status %d)\n",
                        path, previous_line, line_number, status);
                abort();
            }
        }
        memcpy(previous, current, length * sizeof(*current));
        previous_length = length;
        previous_line = line_number;
        count++;
    }
    assert(!ferror(file) && count);
    assert(!fclose(file));
    printf("checked %zu ordered root collation strings\n", count);
}

int main(int argc, char **argv)
{
    QJSIntlPack pack;
    QJSIntlCollationRoot root;
    QJSIntlCollationContexts contexts;
    uint32_t *key = NULL, index;
    size_t capacity = 0;
    assert(!qjs_intl_pack_open(qjs_intl_native_bytes, qjs_intl_native_length, &pack));
    assert(!qjs_intl_collation_root_open(&pack, &root));
    assert(!qjs_intl_collation_contexts_open(&pack, &contexts));
    check_settings(&contexts);
    if (argc != 1) {
        assert(argc == 3 && (!strcmp(argv[1], "--ordered") || !strcmp(argv[1], "--ordered-shifted")));
        ordered_file(&contexts, argv[2], !strcmp(argv[1], "--ordered-shifted"));
        return 0;
    }
    for (index = 0; index < root.section.count; index++) {
        QJSIntlCollationEntry entry, found;
        size_t i, needed;
        assert(!qjs_intl_collation_root_entry_at(&root, index, &entry));
        needed = entry.key_count;
        assert(needed < SIZE_MAX / sizeof(*key));
        needed++;
        if (needed > capacity) {
            uint32_t *next = realloc(key, needed * sizeof(*key));
            assert(next);
            key = next;
            capacity = needed;
        }
        for (i = 0; i < entry.key_count; i++)
            assert(!qjs_intl_collation_key_at(&entry, i, &key[i]));
        assert(!qjs_intl_collation_root_find(&root, key, entry.key_count, &found));
        assert(found.keys == entry.keys && found.elements == entry.elements &&
               found.element_count == entry.element_count);
        check_elements(&contexts, &entry, key, index);
        /* No stored scalar can extend this suffix: preserve this complete
           mapping even if nearby longer contraction keys share its prefix. */
        key[entry.key_count] = 0x110000;
        assert(!qjs_intl_collation_root_prefix(&root, key, needed, &found));
        assert(found.keys == entry.keys && found.key_count == entry.key_count);
    }
    assert(contexts.section.count == 4);
    for (index = 0; index < contexts.section.count; index++) {
        QJSIntlCollationContext context;
        QJSIntlCollationElement element;
        uint32_t before, suffix;
        assert(!qjs_intl_collation_context_at(&contexts, index, &context));
        assert(context.before.key_count == 1 && context.mapping.key_count == 1);
        assert(!qjs_intl_collation_key_at(&context.before, 0, &before));
        assert(!qjs_intl_collation_key_at(&context.mapping, 0, &suffix));
        assert((before == 0x4c || before == 0x6c) && (suffix == 0xb7 || suffix == 0x387));
        assert(context.mapping.element_count == 1);
        assert(!qjs_intl_collation_element_at(&context.mapping, 0, &element));
        assert(!element.primary && element.secondary == 0x11f && element.tertiary == 2);
    }
    free(key);
    printf("checked %u prepared root collation mappings\n", root.section.count);
    return 0;
}
