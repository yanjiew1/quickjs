/* Independent storage-reader checks, not a collation comparison oracle.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/collation/root-data.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t qjs_intl_collation_fixture_bytes[];
extern const size_t qjs_intl_collation_fixture_length;

static void put_u32(uint8_t *p, uint32_t value)
{
    size_t i;
    for (i = 0; i < 4; i++)
        p[i] = value >> (i * 8);
}

static QJSIntlStatus open_root(const uint8_t *bytes, size_t length, QJSIntlCollationRoot *out)
{
    QJSIntlPack pack;
    assert(!qjs_intl_pack_open(bytes, length, &pack));
    return qjs_intl_collation_root_open(&pack, out);
}

static void bad_word(uint8_t *bytes, size_t offset, uint32_t value)
{
    QJSIntlCollationRoot root, zero = {0};
    memcpy(bytes, qjs_intl_collation_fixture_bytes, qjs_intl_collation_fixture_length);
    put_u32(bytes + offset, value);
    memset(&root, 0xff, sizeof(root));
    assert(open_root(bytes, qjs_intl_collation_fixture_length, &root) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&root, &zero, sizeof(root)));
}

int main(void)
{
    const uint32_t a[] = {0x61}, b[] = {0x62}, long_key[] = {0x61, 0x62, 0x79, 0x7a};
    const uint32_t fallback[] = {0x61, 0x62, 0x7a}, extended[] = {0x61, 0x62, 0x79, 0x7a, 0x62};
    const uint32_t deeper_fallback[] = {0x61, 0x62, 0x79, 0x78};
    const uint32_t nul[] = {0}, noncharacter[] = {0xfdd0}, supplementary[] = {0x1f600};
    const uint32_t absent[] = {0x63}, surrogate[] = {0xd800}, out_of_range[] = {0x110000};
    QJSIntlCollationRoot root, zero_root = {0};
    QJSIntlCollationEntry entry, zero_entry = {0};
    QJSIntlCollationElement element, zero_element = {0};
    QJSIntlPack pack;
    uint32_t scalar;
    size_t length = qjs_intl_collation_fixture_length, maps, keys, elements, i;
    uint8_t *storage = malloc(length + 9), *bytes;
    assert(storage);
    bytes = storage + 1; /* Exercise unaligned u16 and u32 wire reads. */
    memcpy(bytes, qjs_intl_collation_fixture_bytes, length);
    assert(!open_root(bytes, length, &root));
    assert(root.section.count == 7 && root.key_count == 12 && root.element_count == 26);
    assert(!qjs_intl_collation_root_find(&root, a, 1, &entry));
    assert(entry.key_count == 1 && entry.element_count == 1);
    assert(!qjs_intl_collation_key_at(&entry, 0, &scalar) && scalar == 0x61);
    assert(!qjs_intl_collation_element_at(&entry, 0, &element));
    assert(element.primary == 0x1000 && element.secondary == 0x20 &&
           element.tertiary == 2 && !element.variable);
    assert(!qjs_intl_collation_root_find(&root, long_key, 4, &entry) && entry.key_count == 4);
    assert(!qjs_intl_collation_root_prefix(&root, extended, 5, &entry) && entry.key_count == 4);
    assert(!qjs_intl_collation_root_prefix(&root, fallback, 3, &entry) && entry.key_count == 1);
    assert(!qjs_intl_collation_root_prefix(&root, deeper_fallback, 4, &entry) && entry.key_count == 3);
    assert(qjs_intl_collation_root_find(&root, fallback, 3, &entry) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&entry, &zero_entry, sizeof(entry)));
    assert(!qjs_intl_collation_root_find(&root, b, 1, &entry) && entry.element_count == 19);
    for (i = 0; i < entry.element_count; i++) {
        assert(!qjs_intl_collation_element_at(&entry, i, &element));
        assert(element.primary == 0x1100 + i && element.variable == (i == 0));
    }
    assert(!qjs_intl_collation_root_find(&root, nul, 1, &entry));
    assert(!qjs_intl_collation_element_at(&entry, 0, &element));
    assert(!memcmp(&element, &zero_element, sizeof(element)));
    assert(!qjs_intl_collation_root_find(&root, noncharacter, 1, &entry));
    assert(!qjs_intl_collation_root_prefix(&root, supplementary, 1, &entry));
    assert(qjs_intl_collation_root_prefix(&root, absent, 1, &entry) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&entry, &zero_entry, sizeof(entry)));
    assert(qjs_intl_collation_root_find(&root, surrogate, 1, &entry) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_collation_root_prefix(&root, out_of_range, 1, &entry) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_collation_root_find(&root, NULL, 0, &entry) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_collation_root_prefix(&root, NULL, 0, &entry) == QJS_INTL_UNSUPPORTED);
    assert(qjs_intl_collation_root_find(&root, NULL, 1, &entry) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_root_prefix(&root, NULL, 1, &entry) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_root_find(NULL, a, 1, &entry) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_root_prefix(&root, a, 1, NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_root_entry_at(&root, 7, &entry) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&entry, &zero_entry, sizeof(entry)));
    assert(!qjs_intl_collation_root_entry_at(&root, 2, &entry));
    assert(qjs_intl_collation_key_at(&entry, 3, &scalar) == QJS_INTL_INVALID_ARGUMENT && !scalar);
    assert(qjs_intl_collation_element_at(&entry, 2, &element) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&element, &zero_element, sizeof(element)));
    assert(!qjs_intl_collation_root_entry_at(&root, 6, &entry));
    assert(!qjs_intl_collation_key_at(&entry, 0, &scalar) && scalar == 0x1f600);
    maps = root.mappings - bytes;
    keys = root.keys - bytes;
    elements = root.elements - bytes;
    /* Counts/extents, partitions, canonical scalar order, and CE flags are
       independent requirements. Mutations retain a valid pack envelope. */
    bad_word(bytes, 76, 0);
    bad_word(bytes, 76, UINT32_MAX);
    bad_word(bytes, 80, 2);
    bad_word(bytes, 84, 17U << 16);
    bad_word(bytes, 88, UINT32_MAX);
    bad_word(bytes, 92, UINT32_MAX);
    bad_word(bytes, maps, 1);
    bad_word(bytes, maps + 4, 0);
    bad_word(bytes, maps + 4, UINT32_MAX);
    bad_word(bytes, maps + 8, 1);
    bad_word(bytes, maps + 12, 0);
    bad_word(bytes, maps + 12, UINT32_MAX);
    bad_word(bytes, maps + 16, 0);
    bad_word(bytes, maps + 16, 2);
    bad_word(bytes, maps + 24, 0);
    bad_word(bytes, maps + 24, 2);
    bad_word(bytes, maps + 6 * 16 + 4, 2);
    bad_word(bytes, maps + 6 * 16 + 12, 2);
    bad_word(bytes, keys, 0x62); /* Descending first/second key. */
    bad_word(bytes, keys + 4, 0); /* Duplicate first/second key. */
    bad_word(bytes, keys + 2 * 4, 0xd800);
    bad_word(bytes, keys + 2 * 4, 0x110000);
    bad_word(bytes, elements + 4, 2U << 16); /* Invalid u16 variable flag. */
    bad_word(bytes, elements + 4, 1U << 16); /* Variable element with zero primary. */
    for (i = 80; i < length; i++) {
        memcpy(bytes, qjs_intl_collation_fixture_bytes, length);
        put_u32(bytes + 8, i);
        put_u32(bytes + 72, i - 80);
        assert(open_root(bytes, i, &root) == QJS_INTL_DATA_ERROR);
        assert(!memcmp(&root, &zero_root, sizeof(root)));
    }
    /* Extra elements satisfy dimensions but violate complete pool ownership. */
    memcpy(bytes, qjs_intl_collation_fixture_bytes, length);
    memset(bytes + length, 0, 8);
    put_u32(bytes + 8, length + 8);
    put_u32(bytes + 72, length + 8 - 80);
    put_u32(bytes + 92, 27);
    assert(open_root(bytes, length + 8, &root) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&root, &zero_root, sizeof(root)));
    memcpy(bytes, qjs_intl_collation_fixture_bytes, length);
    memset(bytes + length, 0, 4);
    put_u32(bytes + 8, length + 4);
    put_u32(bytes + 72, length + 4 - 80);
    assert(open_root(bytes, length + 4, &root) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&root, &zero_root, sizeof(root)));
    /* A different valid optional section is not malformed root data. */
    memcpy(bytes, qjs_intl_collation_fixture_bytes, length);
    put_u32(bytes + 64, QJS_INTL_SECTION_CALENDAR_ERAS);
    assert(!qjs_intl_pack_open(bytes, length, &pack));
    assert(qjs_intl_collation_root_open(&pack, &root) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&root, &zero_root, sizeof(root)));
    assert(qjs_intl_collation_root_open(NULL, &root) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_root_open(&pack, NULL) == QJS_INTL_INVALID_ARGUMENT);
    free(storage);
    puts("Intl root collation storage tests passed");
    return 0;
}
