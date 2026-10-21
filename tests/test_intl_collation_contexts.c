/* Prefix/suffix splits and malformed references over borrowed root pools.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/collation/contexts.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t qjs_intl_collation_fixture_bytes[];
extern const size_t qjs_intl_collation_fixture_length;

static void put_u32(uint8_t *p, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        p[i] = value >> (i * 8);
}

static QJSIntlStatus open_contexts(const uint8_t *bytes, size_t length,
                                  QJSIntlCollationContexts *out)
{
    QJSIntlPack pack;
    assert(!qjs_intl_pack_open(bytes, length, &pack));
    return qjs_intl_collation_contexts_open(&pack, out);
}

static void bad_word(uint8_t *bytes, size_t offset, uint32_t value)
{
    QJSIntlCollationContexts view, zero;
    memset(&zero, 0, sizeof(zero));
    memset(&view, 0xff, sizeof(view));
    memcpy(bytes, qjs_intl_collation_fixture_bytes, qjs_intl_collation_fixture_length);
    put_u32(bytes + offset, value);
    assert(open_contexts(bytes, qjs_intl_collation_fixture_length, &view) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&view, &zero, sizeof(view)));
}

int main(void)
{
    QJSIntlCollationContexts view, zero_view;
    QJSIntlCollationContext context, zero_context;
    QJSIntlCollationElement element;
    QJSIntlPack pack;
    uint32_t code;
    size_t length = qjs_intl_collation_fixture_length, metadata, extent;
    uint8_t *storage = malloc(length + 1), *bytes;
    assert(storage);
    bytes = storage + 1; /* Exercise unaligned metadata reads too. */
    memset(&zero_view, 0, sizeof(zero_view));
    memset(&zero_context, 0, sizeof(zero_context));
    memcpy(bytes, qjs_intl_collation_fixture_bytes, length);
    assert(!open_contexts(bytes, length, &view));
    assert(view.section.count == 2 && view.section.length == 32);
    metadata = view.section.bytes - bytes;
    assert(!qjs_intl_collation_context_at(&view, 0, &context));
    assert(context.source_index == 1 && context.before.key_count == 1);
    assert(context.before.element_count == 1 && context.mapping.key_count == 1);
    assert(!qjs_intl_collation_key_at(&context.before, 0, &code) && code == 0x61);
    assert(!qjs_intl_collation_key_at(&context.mapping, 0, &code) && code == 0x62);
    assert(!qjs_intl_collation_element_at(&context.mapping, 0, &element));
    assert(!element.primary && element.secondary == 0x21);
    assert(!qjs_intl_collation_context_at(&view, 1, &context));
    assert(context.source_index == 2 && context.before.key_count == 2);
    assert(context.before.element_count == 2 && context.mapping.element_count == 1);
    assert(!qjs_intl_collation_key_at(&context.mapping, 0, &code) && code == 0x63);
    assert(!qjs_intl_collation_element_at(&context.mapping, 0, &element));
    assert(!element.primary && element.secondary == 0x22);
    assert(qjs_intl_collation_context_at(&view, 2, &context) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&context, &zero_context, sizeof(context)));
    assert(qjs_intl_collation_context_at(NULL, 0, &context) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_context_at(&view, 0, NULL) == QJS_INTL_INVALID_ARGUMENT);

    bad_word(bytes, 92, UINT32_MAX); /* Directory count disagrees with extent. */
    for (extent = 0; extent < 32; extent++) {
        memcpy(bytes, qjs_intl_collation_fixture_bytes, length);
        put_u32(bytes + 8, metadata + extent);
        put_u32(bytes + 88, extent);
        assert(open_contexts(bytes, metadata + extent, &view) == QJS_INTL_DATA_ERROR);
    }
    /* A present empty section differs from a missing one. */
    memcpy(bytes, qjs_intl_collation_fixture_bytes, length);
    put_u32(bytes + 8, metadata + 8);
    put_u32(bytes + 88, 8);
    put_u32(bytes + 92, 0);
    assert(!open_contexts(bytes, metadata + 8, &view));
    assert(!view.section.count);
    assert(qjs_intl_collation_context_at(&view, 0, &context) == QJS_INTL_INVALID_ARGUMENT);

    bad_word(bytes, metadata, 2);
    bad_word(bytes, metadata + 4, 17U << 16);
    bad_word(bytes, metadata + 8, 3); /* Missing source mapping. */
    bad_word(bytes, metadata + 8, 0); /* Empty remainder. */
    bad_word(bytes, metadata + 12, 0);
    bad_word(bytes, metadata + 12, UINT32_MAX);
    bad_word(bytes, metadata + 16, 0);
    bad_word(bytes, metadata + 16, UINT32_MAX);
    bad_word(bytes, metadata + 20, 1); /* Repeated source mapping. */
    bad_word(bytes, metadata + 28, 1); /* Prefix CE count disagrees with root. */
    /* Alter one valid CE to violate only prefix preservation, not root schema. */
    memcpy(bytes, qjs_intl_collation_fixture_bytes, length);
    assert(!open_contexts(bytes, length, &view));
    assert(!qjs_intl_collation_context_at(&view, 0, &context));
    bad_word(bytes, context.before.elements - bytes, 0x00201001);
    /* Remove/rename each optional section while retaining a valid envelope. */
    memcpy(bytes, qjs_intl_collation_fixture_bytes, length);
    put_u32(bytes + 64, QJS_INTL_SECTION_CALENDAR_ERAS);
    assert(open_contexts(bytes, length, &view) == QJS_INTL_DATA_ERROR);
    memcpy(bytes, qjs_intl_collation_fixture_bytes, length);
    put_u32(bytes + 80, QJS_INTL_SECTION_COLLATION_CONTEXTS + 1);
    assert(!qjs_intl_pack_open(bytes, length, &pack));
    assert(qjs_intl_collation_contexts_open(&pack, &view) == QJS_INTL_UNSUPPORTED);
    assert(!memcmp(&view, &zero_view, sizeof(view)));
    assert(qjs_intl_collation_contexts_open(NULL, &view) == QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_collation_contexts_open(&pack, NULL) == QJS_INTL_INVALID_ARGUMENT);
    free(storage);
    return 0;
}
