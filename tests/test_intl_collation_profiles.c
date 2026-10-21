/* Exercise storage lookup over every mapping in each prepared root profile.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/collation/contexts.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern const uint8_t qjs_intl_native_bytes[];
extern const size_t qjs_intl_native_length;

int main(void)
{
    QJSIntlPack pack;
    QJSIntlCollationRoot root;
    QJSIntlCollationContexts contexts;
    uint32_t *key = NULL, index;
    size_t capacity = 0;
    assert(!qjs_intl_pack_open(qjs_intl_native_bytes, qjs_intl_native_length, &pack));
    assert(!qjs_intl_collation_root_open(&pack, &root));
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
        /* No stored scalar can extend this suffix: preserve this complete
           mapping even if nearby longer contraction keys share its prefix. */
        key[entry.key_count] = 0x110000;
        assert(!qjs_intl_collation_root_prefix(&root, key, needed, &found));
        assert(found.keys == entry.keys && found.key_count == entry.key_count);
    }
    assert(!qjs_intl_collation_contexts_open(&pack, &contexts));
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
