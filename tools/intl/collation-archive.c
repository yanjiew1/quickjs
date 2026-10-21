/* One shared logical root plus sparse per-program ranked state. No struct dumps.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-archive.h"
#include "../../src/intl/native/wire.h"
#include <string.h>

#define MAX_SIZE (256u * 1024 * 1024)
#define HEADER_SIZE 64u
#define SECTION_COUNT 12u
#define START (HEADER_SIZE + SECTION_COUNT * 16u)

static void put32(uint8_t *p, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        p[i] = value >> (i * 8);
}

static QJSIntlStatus finish(QJSIntlCollationArchive *out, QJSIntlStatus status)
{
    if (status)
        out->failed = 1;
    return status;
}

static QJSIntlStatus reserve(QJSIntlCollationArchive *owner, QJSIntlArchiveBuffer *buffer, uint64_t extra)
{
    uint32_t needed, capacity;
    uint8_t *next;
    if (extra > MAX_SIZE - buffer->length)
        return QJS_INTL_OVERFLOW;
    needed = buffer->length + (uint32_t)extra;
    if (needed <= buffer->capacity)
        return QJS_INTL_OK;
    capacity = buffer->capacity ? buffer->capacity : 256;
    while (capacity < needed)
        capacity = capacity > MAX_SIZE / 2 ? MAX_SIZE : capacity * 2;
    next = buffer->bytes ? owner->allocator.realloc(owner->allocator.opaque, buffer->bytes, capacity) :
        owner->allocator.malloc(owner->allocator.opaque, capacity);
    if (!next)
        return QJS_INTL_NO_MEMORY;
    buffer->bytes = next;
    buffer->capacity = capacity;
    return QJS_INTL_OK;
}

void qjs_intl_collation_archive_clear(QJSIntlCollationArchive *owner)
{
    if (!owner)
        return;
    if (owner->programs.bytes)
        owner->allocator.free(owner->allocator.opaque, owner->programs.bytes);
    if (owner->labels.bytes)
        owner->allocator.free(owner->allocator.opaque, owner->labels.bytes);
    if (owner->blocks.bytes)
        owner->allocator.free(owner->allocator.opaque, owner->blocks.bytes);
    memset(owner, 0, sizeof(*owner));
}

void qjs_intl_compiled_collation_clear(QJSIntlCompiledCollation *image)
{
    if (!image)
        return;
    if (image->bytes)
        image->allocator.free(image->allocator.opaque, image->bytes);
    *image = (QJSIntlCompiledCollation){0};
}

QJSIntlStatus qjs_intl_collation_archive_init(QJSIntlCollationArchive *owner, const QJSIntlRootInput *root,
                                              const QJSIntlAllocator *allocator)
{
    if (!owner)
        return QJS_INTL_INVALID_ARGUMENT;
    *owner = (QJSIntlCollationArchive){0};
    if (!root || !root->fingerprint || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    owner->root = *root;
    owner->allocator = *allocator;
    owner->ready = 1;
    return QJS_INTL_OK;
}

static int label_order(const uint8_t *a, uint32_t an, QJSIntlIRLabel b)
{
    uint32_t count = an < b.length ? an : b.length;
    int order = count ? memcmp(a, b.bytes, count) : 0;
    return order ? order : an < b.length ? -1 : an != b.length;
}

static int ordered(const QJSIntlCollationArchive *owner, const QJSIntlIRProgram *identity)
{
    const QJSIntlIRLabel labels[] = {identity->locale, identity->collation, identity->alternate};
    const uint8_t *last;
    unsigned int i;
    if (!owner->count)
        return 1;
    last = owner->programs.bytes + owner->programs.length - 32;
    for (i = 0; i < 3; i++) {
        uint32_t start = qjs_intl_read_u32(last + i * 8), length = qjs_intl_read_u32(last + i * 8 + 4);
        int order = label_order(owner->labels.bytes + start, length, labels[i]);
        if (order)
            return order < 0;
    }
    return 0;
}

static uint8_t *copy_bytes(uint8_t *p, const void *bytes, size_t count)
{
    if (count)
        memcpy(p, bytes, count);
    return p + count;
}

static uint8_t *copy_words(uint8_t *p, const uint32_t *words, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; i++, p += 4)
        put32(p, words[i]);
    return p;
}

QJSIntlStatus qjs_intl_collation_archive_add(QJSIntlCollationArchive *owner,
                                             const QJSIntlCollationSnapshot *snapshot,
                                             const QJSIntlCompiledTrie *trie)
{
    const QJSIntlIRProgram *identity;
    QJSIntlCollationTrie checked;
    QJSIntlStatus status;
    uint64_t size, labels_size;
    uint32_t counts[12], i, flags;
    uint8_t *p, *record;
    QJSIntlIRLabel labels[3];
    if (!owner || !owner->ready || owner->failed)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!snapshot || !snapshot->ready || !trie || snapshot->root.keys.rows != owner->root.keys.rows)
        return finish(owner, QJS_INTL_INVALID_ARGUMENT);
    identity = &snapshot->identity;
    if (!ordered(owner, identity))
        return finish(owner, QJS_INTL_DATA_ERROR);
    status = qjs_intl_collation_trie_open(&trie->nodes, &trie->edges, &trie->suppressed,
                                          &snapshot->keys, &owner->allocator, &checked);
    if (status)
        return finish(owner, status);
    if (trie->suppressed.count != snapshot->suppressed_count)
        return finish(owner, QJS_INTL_DATA_ERROR);
    for (i = 0; i < snapshot->suppressed_count; i++) {
        if (qjs_intl_read_u32(trie->suppressed.bytes + (size_t)i * 4) != snapshot->suppressed[i])
            return finish(owner, QJS_INTL_DATA_ERROR);
    }
    counts[0] = snapshot->keys.row_count;
    counts[1] = snapshot->keys.key_count;
    counts[2] = snapshot->keys.context_count;
    counts[3] = snapshot->element_count;
    counts[4] = snapshot->suppressed_count;
    counts[5] = snapshot->group_count;
    for (i = 0; i < 4; i++)
        counts[6 + i] = snapshot->offset_counts[i];
    counts[10] = trie->nodes.count;
    counts[11] = trie->edges.count;
    size = 64 + (uint64_t)counts[0] * 16 + (uint64_t)counts[1] * 4 + (uint64_t)counts[2] * 12 +
        (uint64_t)counts[3] * 24 + (uint64_t)counts[4] * 4 + (uint64_t)counts[5] * 4 +
        trie->nodes.length + trie->edges.length;
    for (i = 0; i < 4; i++)
        size += (uint64_t)counts[6 + i] * 8;
    labels[0] = identity->locale;
    labels[1] = identity->collation;
    labels[2] = identity->alternate;
    labels_size = (uint64_t)labels[0].length + labels[1].length + labels[2].length;
    if (owner->count == UINT32_MAX)
        return finish(owner, QJS_INTL_OVERFLOW);
    status = reserve(owner, &owner->programs, 32);
    if (!status)
        status = reserve(owner, &owner->labels, labels_size);
    if (!status)
        status = reserve(owner, &owner->blocks, size);
    if (status)
        return finish(owner, status);
    record = owner->programs.bytes + owner->programs.length;
    for (i = 0; i < 3; i++) {
        put32(record + i * 8, owner->labels.length);
        put32(record + i * 8 + 4, labels[i].length);
        if (labels[i].length)
            memcpy(owner->labels.bytes + owner->labels.length, labels[i].bytes, labels[i].length);
        owner->labels.length += labels[i].length;
    }
    put32(record + 24, owner->blocks.length);
    put32(record + 28, (uint32_t)size);
    p = owner->blocks.bytes + owner->blocks.length;
    put32(p, snapshot->policy.strength);
    put32(p + 4, snapshot->policy.case_first);
    put32(p + 8, snapshot->policy.max_variable);
    flags = (uint32_t)snapshot->policy.shifted | ((uint32_t)snapshot->policy.backwards_secondary << 1) |
        ((uint32_t)snapshot->policy.normalization << 2) | ((uint32_t)snapshot->policy.case_level << 3) |
        ((uint32_t)snapshot->policy.numeric << 4);
    put32(p + 12, flags);
    p = copy_words(p + 16, counts, 12);
    p = copy_bytes(p, snapshot->rows, (size_t)counts[0] * 16);
    p = copy_bytes(p, snapshot->key_bytes, (size_t)counts[1] * 4);
    p = copy_bytes(p, snapshot->contexts, (size_t)counts[2] * 12);
    for (i = 0; i < snapshot->element_count; i++) {
        const QJSIntlCollationRankedElement *ce = snapshot->elements + i;
        p = copy_words(p, ce->ranks, 4);
        put32(p, ce->root_primary);
        put32(p + 4, ce->case_value | (ce->variable << 2) | (ce->implicit << 3));
        p += 8;
    }
    p = copy_words(p, snapshot->suppressed, snapshot->suppressed_count);
    p = copy_words(p, snapshot->group_ranks, snapshot->group_count);
    for (i = 0; i < 4; i++) {
        uint32_t j;
        for (j = 0; j < snapshot->offset_counts[i]; j++) {
            put32(p, snapshot->offsets[i][j].root_gap);
            put32(p + 4, snapshot->offsets[i][j].added);
            p += 8;
        }
    }
    p = copy_bytes(p, trie->nodes.bytes, trie->nodes.length);
    copy_bytes(p, trie->edges.bytes, trie->edges.length);
    owner->programs.length += 32;
    owner->blocks.length += (uint32_t)size;
    owner->count++;
    return QJS_INTL_OK;
}

static void directory(uint8_t *bytes, unsigned int id, uint32_t *at, uint32_t size, uint32_t count)
{
    uint8_t *row = bytes + HEADER_SIZE + (id - 1) * 16;
    put32(row, id);
    put32(row + 4, *at);
    put32(row + 8, size);
    put32(row + 12, count);
    *at += size;
}

QJSIntlStatus qjs_intl_collation_archive_finish(QJSIntlCollationArchive *owner, QJSIntlCompiledCollation *out)
{
    static const unsigned int source_sections[] = {QJS_INTL_ROOT_MAPPINGS, QJS_INTL_ROOT_KEYS,
        QJS_INTL_ROOT_CONTEXTS, QJS_INTL_ROOT_CES, QJS_INTL_ROOT_HAN_RANGES, QJS_INTL_ROOT_HAN_RANKS};
    const QJSIntlRootInputMetadata *m;
    const QJSIntlCollationRootLimits *r;
    QJSIntlCompiledCollation image = {0};
    uint32_t metadata[20], groups, i, at = START, label_size;
    uint64_t size = START + 80;
    if (out)
        *out = (QJSIntlCompiledCollation){0};
    if (!owner || !owner->ready || owner->failed || !out)
        return QJS_INTL_INVALID_ARGUMENT;
    m = &owner->root.metadata;
    r = &m->limits;
    groups = owner->root.sections[QJS_INTL_ROOT_GROUPS].count;
    label_size = (owner->labels.length + 3u) & ~3u;
    for (i = 0; i < 6; i++)
        size += owner->root.sections[source_sections[i]].length;
    size += (uint64_t)groups * 8 + owner->programs.length + label_size + owner->blocks.length;
    if (size > MAX_SIZE)
        return finish(owner, QJS_INTL_OVERFLOW);
    metadata[0] = r->primary_max;
    metadata[1] = r->secondary_max;
    metadata[2] = r->tertiary_max;
    metadata[3] = r->secondary_common;
    metadata[4] = r->tertiary_common;
    metadata[5] = r->secondary_boundary;
    metadata[6] = r->tertiary_boundary;
    metadata[7] = m->han_base;
    metadata[8] = m->han_count;
    metadata[9] = m->generic_base;
    metadata[10] = m->generic_count;
    metadata[11] = m->generic_limit;
    metadata[12] = m->variable_top;
    metadata[13] = m->numeric;
    metadata[14] = m->field_separator;
    for (i = 0; i < 5; i++) {
        static const char *const names[] = {"space", "punct", "symbol", "currency", "digit"};
        QJSIntlStatus status = qjs_intl_root_input_group(&owner->root, (QJSIntlBytes){names[i], strlen(names[i])}, metadata + 15 + i);
        if (status)
            return finish(owner, status);
    }
    image.allocator = owner->allocator;
    image.bytes = owner->allocator.malloc(owner->allocator.opaque, (size_t)size);
    if (!image.bytes)
        return finish(owner, QJS_INTL_NO_MEMORY);
    image.length = (size_t)size;
    memset(image.bytes, 0, (size_t)size);
    memcpy(image.bytes, "QJCL", 4);
    put32(image.bytes + 4, 1);
    put32(image.bytes + 8, QJS_INTL_IR_UNICODE_VERSION);
    put32(image.bytes + 12, QJS_INTL_IR_UNICODE_VERSION);
    put32(image.bytes + 16, (uint32_t)size);
    put32(image.bytes + 20, SECTION_COUNT);
    put32(image.bytes + 24, owner->count);
    memcpy(image.bytes + 32, owner->root.fingerprint, 32);
    copy_words(image.bytes + at, metadata, 20);
    directory(image.bytes, 1, &at, 80, 1);
    for (i = 0; i < 6; i++) {
        const QJSIntlRootInputSection *section = owner->root.sections + source_sections[i];
        copy_bytes(image.bytes + at, section->bytes, section->length);
        directory(image.bytes, i + 2, &at, section->length, section->count);
    }
    for (i = 0; i < groups; i++) {
        const uint8_t *source = owner->root.sections[QJS_INTL_ROOT_GROUPS].bytes + (size_t)i * 16;
        memcpy(image.bytes + at + (size_t)i * 8, source, 8);
    }
    directory(image.bytes, 8, &at, groups * 8, groups);
    copy_bytes(image.bytes + at, owner->programs.bytes, owner->programs.length);
    directory(image.bytes, 9, &at, owner->programs.length, owner->count);
    copy_bytes(image.bytes + at, owner->labels.bytes, owner->labels.length);
    directory(image.bytes, 10, &at, label_size, owner->labels.length);
    copy_bytes(image.bytes + at, owner->blocks.bytes, owner->blocks.length);
    directory(image.bytes, 11, &at, owner->blocks.length, owner->count);
    directory(image.bytes, 12, &at, 0, 0);
    *out = image;
    return QJS_INTL_OK;
}
