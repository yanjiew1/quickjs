/* Canonical runtime bytes own their data independently of compiler staging.
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
#include "../tools/intl/collation-archive.h"
#include "../src/intl/native/wire.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t intl_root_fixture[];
extern const size_t intl_root_fixture_size;
extern const uint8_t intl_program_success[];
extern const size_t intl_program_success_size;

typedef struct State {
    size_t calls, fail_at, live, resizes;
    unsigned int ends;
    QJSIntlCollationArchive archive;
} State;

static void *allocate(void *opaque, size_t size)
{
    State *state = opaque;
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
    State *state = opaque;
    void *next;
    assert(p && size);
    state->resizes++;
    if (++state->calls == state->fail_at)
        return NULL;
    next = realloc(p, size);
    assert(next);
    return next;
}

static void release(void *opaque, void *p)
{
    State *state = opaque;
    assert(p && state->live);
    state->live--;
    free(p);
}

static QJSIntlAllocator allocator(State *state)
{
    return (QJSIntlAllocator){state, allocate, resize, release};
}

static QJSIntlStatus add(void *opaque, const QJSIntlIRProgram *identity, QJSIntlCollationProgram *program)
{
    State *state = opaque;
    QJSIntlCollationSnapshot snapshot = {0};
    QJSIntlCompiledTrie trie = {0};
    QJSIntlStatus status = qjs_intl_collation_snapshot_create(program, identity, &snapshot);
    if (!status)
        status = qjs_intl_compiled_trie_create(&snapshot, &trie);
    if (!status)
        status = qjs_intl_collation_archive_add(&state->archive, &snapshot, &trie);
    qjs_intl_compiled_trie_clear(&trie);
    qjs_intl_collation_snapshot_clear(&snapshot);
    if (!status)
        state->ends++;
    return status;
}

static QJSIntlStatus create(const QJSIntlRootInput *root, State *state, QJSIntlCompiledCollation *out)
{
    QJSIntlIRView view;
    QJSIntlAllocator a = allocator(state);
    QJSIntlStatus status;
    *out = (QJSIntlCompiledCollation){0};
    assert(!qjs_intl_ir_open(intl_program_success, intl_program_success_size, root->fingerprint, &view, NULL));
    status = qjs_intl_collation_archive_init(&state->archive, root, &a);
    if (!status)
        status = qjs_intl_collation_program_execute(root, &view, &a, add, state, NULL);
    if (!status)
        status = qjs_intl_collation_archive_finish(&state->archive, out);
    qjs_intl_collation_archive_clear(&state->archive);
    return status;
}

static QJSIntlSection section(const QJSIntlCompiledCollation *image, unsigned int id)
{
    const uint8_t *row = image->bytes + 64 + (id - 1) * 16;
    uint32_t at = qjs_intl_read_u32(row + 4), size = qjs_intl_read_u32(row + 8);
    assert(qjs_intl_read_u32(row) == id && at <= image->length && size <= image->length - at);
    return (QJSIntlSection){image->bytes + at, size, qjs_intl_read_u32(row + 12)};
}

static void check_image(const QJSIntlRootInput *root, const QJSIntlCompiledCollation *image)
{
    static const unsigned int sources[] = {QJS_INTL_ROOT_MAPPINGS, QJS_INTL_ROOT_KEYS, QJS_INTL_ROOT_CONTEXTS,
        QJS_INTL_ROOT_CES, QJS_INTL_ROOT_HAN_RANGES, QJS_INTL_ROOT_HAN_RANKS};
    QJSIntlSection programs = section(image, 9), labels = section(image, 10), blocks = section(image, 11);
    uint32_t i, at = 256;
    assert(image->length >= 336 && !memcmp(image->bytes, "QJCL", 4));
    assert(qjs_intl_read_u32(image->bytes + 4) == 2);
    assert(qjs_intl_read_u32(image->bytes + 8) == (18u << 16));
    assert(qjs_intl_read_u32(image->bytes + 12) == (18u << 16));
    assert(qjs_intl_read_u32(image->bytes + 16) == image->length);
    assert(qjs_intl_read_u32(image->bytes + 20) == 12 && qjs_intl_read_u32(image->bytes + 24) == 2);
    assert(!qjs_intl_read_u32(image->bytes + 28) && !memcmp(image->bytes + 32, root->fingerprint, 32));
    for (i = 1; i <= 12; i++) {
        const uint8_t *row = image->bytes + 64 + (i - 1) * 16;
        QJSIntlSection data = section(image, i);
        assert(qjs_intl_read_u32(row + 4) == at && !(data.length & 3));
        at += data.length;
    }
    assert(at == image->length);
    assert(section(image, 1).length == 80 && section(image, 1).count == 1);
    assert(qjs_intl_read_u32(section(image, 1).bytes + 28) == root->metadata.han_base);
    assert(qjs_intl_read_u32(section(image, 1).bytes + 52) == root->metadata.numeric);
    for (i = 0; i < 6; i++) {
        QJSIntlSection data = section(image, i + 2);
        const QJSIntlRootInputSection *source = root->sections + sources[i];
        assert(data.length == source->length && data.count == source->count);
        if (data.length)
            assert(!memcmp(data.bytes, source->bytes, data.length));
    }
    for (i = 0; i < section(image, 8).count; i++)
        assert(!memcmp(section(image, 8).bytes + (size_t)i * 8,
                       root->sections[QJS_INTL_ROOT_GROUPS].bytes + (size_t)i * 16, 8));
    assert(programs.count == 2 && programs.length == 64 && blocks.count == 2);
    assert(labels.count <= labels.length && labels.length - labels.count < 4);
    for (i = labels.count; i < labels.length; i++)
        assert(!labels.bytes[i]);
    at = 0;
    for (i = 0; i < programs.count; i++) {
        static const uint32_t strides[] = {16,4,12,24,4,4,8,8,8,8,16,8};
        const uint8_t *record = programs.bytes + (size_t)i * 32, *block;
        uint32_t start = qjs_intl_read_u32(record + 24), size = qjs_intl_read_u32(record + 28), words = 64, j;
        uint32_t locale = qjs_intl_read_u32(record);
        assert(start == at && size <= blocks.length - at);
        block = blocks.bytes + start;
        assert(qjs_intl_read_u32(record + 4) == 2 && !memcmp(labels.bytes + locale, i ? "fr" : "en", 2));
        assert(qjs_intl_read_u32(block) == (i ? 3u : 2u));
        assert(qjs_intl_read_u32(block + 4) == (i ? QJS_INTL_COLLATION_CASE_OFF : QJS_INTL_COLLATION_UPPER_FIRST));
        assert(qjs_intl_read_u32(block + 8) == QJS_INTL_COLLATION_VARIABLE_PUNCT);
        assert(qjs_intl_read_u32(block + 12) == (i ? 16u : 0u));
        for (j = 0; j < 12; j++)
            words += qjs_intl_read_u32(block + 16 + j * 4) * strides[j];
        assert(words == size && qjs_intl_read_u32(block + 56)); /* At least the trie root. */
        at += size;
    }
    assert(at == blocks.length && !section(image, 12).count && !section(image, 12).length);
}

static void test_image_and_oom(const QJSIntlRootInput *root)
{
    State state = {0};
    QJSIntlCompiledCollation image, repeat;
    size_t count, i;
    assert(!create(root, &state, &image));
    assert(state.ends == 2 && state.live == 1); /* Only the finished image remains. */
    count = state.calls;
    check_image(root, &image);
    assert(!create(root, &state, &repeat));
    assert(repeat.length == image.length && !memcmp(image.bytes, repeat.bytes, image.length));
    qjs_intl_compiled_collation_clear(&image);
    qjs_intl_compiled_collation_clear(&repeat);
    assert(!state.live);
    for (i = 1; i <= count; i++) {
        state = (State){.fail_at = i};
        assert(create(root, &state, &image) == QJS_INTL_NO_MEMORY);
        assert(!image.bytes && !image.length && !state.live);
    }
}

static void test_buffer_growth_and_poison(const QJSIntlRootInput *root)
{
    State state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlCollationProgram program = {0};
    QJSIntlCompiledCollation image;
    unsigned int i;
    assert(!qjs_intl_collation_builder_init(&program.builder, root, &a));
    assert(!qjs_intl_collation_reorder_init(&program.reorder, root, &a));
    qjs_intl_collation_settings_init(&program.policy);
    assert(!qjs_intl_collation_archive_init(&state.archive, root, &a));
    assert(!qjs_intl_collation_archive_finish(&state.archive, &image));
    assert(!section(&image, 9).count && !section(&image, 10).length && !section(&image, 11).length);
    qjs_intl_compiled_collation_clear(&image);
    for (i = 0; i < 40; i++) {
        char locale[16];
        QJSIntlIRProgram identity = {{(const uint8_t *)locale, 0}, {(const uint8_t *)"standard", 8}, {0}, 0};
        identity.locale.length = snprintf(locale, sizeof(locale), "en-x-%04u", i);
        assert(!add(&state, &identity, &program));
    }
    for (i = 0; i < 3; i++) {
        const QJSIntlIRProgram identity = {{(const uint8_t *)"en-x-0039", 9},
            {(const uint8_t *)"traditional", 11},
            {(const uint8_t *)(i == 2 ? "b" : "a"), i ? 1u : 0u}, 0};
        assert(!add(&state, &identity, &program));
    }
    assert(state.resizes >= 3);
    assert(!qjs_intl_collation_archive_finish(&state.archive, &image));
    assert(section(&image, 9).count == 43);
    qjs_intl_compiled_collation_clear(&image);
    /* Repeating the previous identity is rejected before a new publication. */
    {
        const QJSIntlIRProgram identity = {{(const uint8_t *)"en-x-0039", 9},
            {(const uint8_t *)"traditional", 11}, {(const uint8_t *)"b", 1}, 0};
        assert(add(&state, &identity, &program) == QJS_INTL_DATA_ERROR && state.archive.failed);
        assert(qjs_intl_collation_archive_finish(&state.archive, &image) == QJS_INTL_INVALID_ARGUMENT && !image.bytes);
    }
    qjs_intl_collation_archive_clear(&state.archive);
    qjs_intl_collation_builder_clear(&program.builder);
    qjs_intl_collation_reorder_clear(&program.reorder);
    assert(!state.live);
}

int main(void)
{
    State state = {0};
    QJSIntlAllocator a = allocator(&state);
    QJSIntlRootInput root;
    assert(!qjs_intl_root_input_open(intl_root_fixture, intl_root_fixture_size, &a, &root));
    assert(!state.live);
    test_image_and_oom(&root);
    test_buffer_growth_and_poison(&root);
    return 0;
}
