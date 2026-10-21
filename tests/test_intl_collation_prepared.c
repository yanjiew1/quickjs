/* Prepared QJIN profiles carry checked, exactly bound executable programs.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/collation/compiled-data.h"
#include "../src/intl/native/data.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Allocation {
    size_t live;
} Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;

    assert(size);
    ptr = malloc(size);
    if (ptr)
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

static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && !memcmp(a.data, b.data, a.length);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, NULL, release};
    QJSIntlData data;
    QJSIntlSection section;
    QJSIntlCompiledCollationData compiled;
    QJSIntlCompiledCollationProgram program;
    QJSIntlCollationBinding binding;
    QJSIntlBytes tag;
    unsigned char *used;
    uint32_t i, locale = 0, defaults = 0, parent;

    assert(!qjs_intl_data_open(qjs_intl_native_bytes, qjs_intl_native_length, &allocator, &data));
    assert(data.pack.profile == TEST_INTL_PROFILE);
    assert(!qjs_intl_pack_section(&data.pack, QJS_INTL_SECTION_COMPILED_COLLATION, &section));

    /* The inner source fingerprint differs from the outer recipe generation.
       Source pairing is a preparation responsibility, not this runtime open. */
    assert(!qjs_intl_compiled_collation_open(section.bytes, section.length, NULL, &allocator, &compiled));
    assert(section.count == compiled.program_count && section.count);
    assert(memcmp(compiled.fingerprint, data.pack.generation, 32));
    assert(!state.live);
    used = calloc(compiled.program_count, 1);
    assert(used);

    for (i = 0; i < compiled.sections[QJS_INTL_COMPILED_BINDINGS].count; i++) {
        assert(!qjs_intl_compiled_collation_binding(&compiled, i, &binding));
        if (!binding.collation.length) {
            /* root is a source fallback, never a public service locale. */
            do {
                assert(locale < data.locales.count);
                assert(!qjs_intl_data_locale(&data, locale++, &tag, &parent));
            } while (tag.length == 4 && !memcmp(tag.data, "root", 4));
            assert(equal(tag, binding.locale));
            assert(binding.sort_program != UINT32_MAX && binding.search_program != UINT32_MAX);
            defaults++;
        }
        if (binding.sort_program != UINT32_MAX)
            used[binding.sort_program] = 1;
        if (binding.search_program != UINT32_MAX)
            used[binding.search_program] = 1;
    }
    while (locale < data.locales.count) {
        assert(!qjs_intl_data_locale(&data, locale++, &tag, &parent));
        assert(tag.length == 4 && !memcmp(tag.data, "root", 4));
    }
    assert(defaults + 1 == data.locales.count);
    assert(TEST_INTL_PROFILE ? defaults > 2 : defaults == 2);

    for (i = 0; i < compiled.program_count; i++) {
        assert(used[i]);
        assert(!qjs_intl_compiled_collation_program(&compiled, i, &program));
        assert(program.ready && !program.alternate.length);
    }
    free(used);
    assert(!state.live);
    return 0;
}
