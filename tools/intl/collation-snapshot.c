/* Discard construction history while preserving virtual-root rank translation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-snapshot.h"
#include <stdlib.h>
#include <string.h>

typedef struct Entry {
    uint32_t terminal, sequence, prefix_count, suffix_count;
    uint32_t *key;
} Entry;

typedef struct Collect {
    const QJSIntlCollationMap *map;
    Entry *entries;
    uint32_t *keys;
    uint32_t count, key_count, context_count, element_count;
} Collect;

typedef struct Suppressed {
    uint32_t *codes, count;
} Suppressed;

static void put32(uint8_t *out, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4; i++)
        out[i] = value >> (8 * i);
}

static void *array(const QJSIntlAllocator *a, uint64_t count, size_t size, QJSIntlStatus *status)
{
    void *result;
    if (*status || !count)
        return NULL;
    if (count > SIZE_MAX / size) {
        *status = QJS_INTL_OVERFLOW;
        return NULL;
    }
    result = a->malloc(a->opaque, (size_t)count * size);
    if (!result)
        *status = QJS_INTL_NO_MEMORY;
    return result;
}

void qjs_intl_collation_snapshot_clear(QJSIntlCollationSnapshot *snapshot)
{
    unsigned int i;
    void *owned[] = {snapshot ? snapshot->rows : NULL, snapshot ? snapshot->key_bytes : NULL,
        snapshot ? snapshot->contexts : NULL, snapshot ? snapshot->labels : NULL,
        snapshot ? snapshot->elements : NULL, snapshot ? snapshot->suppressed : NULL,
        snapshot ? snapshot->group_ranks : NULL};
    if (!snapshot)
        return;
    for (i = 0; i < sizeof(owned) / sizeof(*owned); i++) {
        if (owned[i])
            snapshot->allocator.free(snapshot->allocator.opaque, owned[i]);
    }
    for (i = 0; i < 4; i++) {
        if (snapshot->offsets[i])
            snapshot->allocator.free(snapshot->allocator.opaque, snapshot->offsets[i]);
    }
    memset(snapshot, 0, sizeof(*snapshot));
}

static QJSIntlStatus collect(void *opaque, uint32_t terminal, uint32_t sequence,
                             const QJSIntlRelationMapping *mapping)
{
    Collect *state = opaque;
    uint32_t prefix, suffix;
    QJSIntlStatus status = qjs_intl_collation_map_key_size(state->map, terminal, &prefix, &suffix);
    uint64_t key_count = (uint64_t)state->key_count + prefix + suffix;
    if (status)
        return status;
    if (state->count == UINT32_MAX - 1 || key_count > UINT32_MAX ||
        mapping->count > UINT32_MAX - state->element_count)
        return QJS_INTL_OVERFLOW;
    if (state->entries) {
        Entry *entry = state->entries + state->count;
        *entry = (Entry){terminal, sequence, prefix, suffix, state->keys + state->key_count};
        status = qjs_intl_collation_map_key_copy(state->map, terminal,
                                                 entry->key, prefix, entry->key + prefix, suffix);
        if (status)
            return status;
    }
    state->count++;
    state->key_count = key_count;
    state->context_count += prefix != 0;
    state->element_count += mapping->count;
    return QJS_INTL_OK;
}

static QJSIntlStatus suppressed(void *opaque, uint32_t code)
{
    Suppressed *state = opaque;
    if (state->count == UINT32_MAX)
        return QJS_INTL_OVERFLOW;
    if (state->codes)
        state->codes[state->count] = code;
    state->count++;
    return QJS_INTL_OK;
}

static int entry_order(const void *a, const void *b)
{
    const Entry *left = a, *right = b;
    uint32_t ln = left->prefix_count + left->suffix_count, rn = right->prefix_count + right->suffix_count;
    uint32_t i, count = ln < rn ? ln : rn;
    for (i = 0; i < count; i++) {
        if (left->key[i] != right->key[i])
            return left->key[i] < right->key[i] ? -1 : 1;
    }
    if (ln != rn)
        return ln < rn ? -1 : 1;
    return left->prefix_count < right->prefix_count ? -1 : left->prefix_count != right->prefix_count;
}

static int scalar_order(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x != y;
}

static QJSIntlStatus copy_entries(QJSIntlCollationProgram *program, const Collect *state,
                                  QJSIntlCollationSnapshot *out)
{
    uint32_t i, key_at = 0, context_at = 0, ce_at = 0;
    for (i = 0; i < state->count; i++) {
        const Entry *entry = state->entries + i;
        QJSIntlRelationMapping mapping;
        uint32_t n = entry->prefix_count + entry->suffix_count, j;
        QJSIntlStatus status = qjs_intl_collation_map_sequence(state->map, entry->sequence, &mapping);
        if (status)
            return status;
        put32(out->rows + (size_t)i * 16, key_at);
        put32(out->rows + (size_t)i * 16 + 4, n);
        put32(out->rows + (size_t)i * 16 + 8, ce_at);
        put32(out->rows + (size_t)i * 16 + 12, (uint32_t)mapping.count);
        for (j = 0; j < n; j++)
            put32(out->key_bytes + (size_t)key_at++ * 4, entry->key[j]);
        if (entry->prefix_count) {
            uint8_t *row = out->contexts + (size_t)context_at++ * 12;
            put32(row, i);
            put32(row + 4, entry->prefix_count);
            put32(row + 8, 0);
        }
        for (j = 0; j < mapping.count; j++) {
            const QJSIntlRelationElement *ce = mapping.elements + j;
            QJSIntlCollationRankedElement *ranked = out->elements + ce_at++;
            unsigned int axis;
            /* Arrays own copies, never overwrite immutable historical CEs. */
            memset(ranked, 0, sizeof(*ranked));
            ranked->root_primary = ce->root_primary;
            ranked->case_value = ce->case_value;
            ranked->variable = ce->variable;
            ranked->implicit = ce->implicit;
            for (axis = 0; axis < 4; axis++) {
                status = qjs_intl_weight_resolve(program->builder.relations.axes + axis,
                                                  ce->weights[axis], ranked->ranks + axis);
                if (status)
                    return status;
            }
        }
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus copy_metadata(QJSIntlCollationProgram *program, const QJSIntlIRProgram *identity,
                                   QJSIntlCollationSnapshot *out)
{
    const QJSIntlIRLabel source[] = {identity->locale, identity->collation, identity->alternate};
    QJSIntlIRLabel *target[] = {&out->identity.locale, &out->identity.collation, &out->identity.alternate};
    QJSIntlStatus status = QJS_INTL_OK;
    uint64_t size = (uint64_t)source[0].length + source[1].length + source[2].length;
    size_t at = 0;
    unsigned int i;
    out->labels = array(&out->allocator, size, 1, &status);
    if (status)
        return status;
    for (i = 0; i < 3; i++) {
        *target[i] = (QJSIntlIRLabel){source[i].length ? out->labels + at : NULL, source[i].length};
        if (source[i].length)
            memcpy(out->labels + at, source[i].bytes, source[i].length);
        at += source[i].length;
    }
    out->identity.instruction_count = identity->instruction_count;
    out->policy = program->policy;
    out->group_count = program->reorder.count;
    out->group_ranks = array(&out->allocator, out->group_count, sizeof(*out->group_ranks), &status);
    if (status)
        return status;
    for (i = 0; i < out->group_count; i++) {
        status = qjs_intl_collation_reorder_rank(&program->reorder, i, out->group_ranks + i);
        if (status)
            return status;
    }
    for (i = 0; i < 4; i++) {
        const QJSIntlWeightGraph *graph = program->builder.relations.axes + i;
        uint32_t j;
        out->root_maxima[i] = graph->root_max;
        out->offset_counts[i] = graph->gap_count;
        out->offsets[i] = array(&out->allocator, graph->gap_count, sizeof(*out->offsets[i]), &status);
        if (status)
            return status;
        for (j = 0; j < graph->gap_count; j++) {
            QJSIntlCollationRootOffset *offset = out->offsets[i] + j;
            status = qjs_intl_weight_offset_at(graph, j, &offset->root_gap, &offset->added);
            if (status)
                return status;
        }
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_snapshot_create(QJSIntlCollationProgram *program,
                                                 const QJSIntlIRProgram *identity,
                                                 QJSIntlCollationSnapshot *out)
{
    QJSIntlCollationSnapshot temp = {0};
    Collect state = {0};
    Suppressed suppression = {0};
    QJSIntlStatus status;
    unsigned int i;
    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    *out = (QJSIntlCollationSnapshot){0};
    if (!program || !identity || !identity->locale.bytes || !identity->locale.length ||
        !identity->collation.bytes || !identity->collation.length ||
        (identity->alternate.length && !identity->alternate.bytes) ||
        !program->builder.ready || program->builder.failed || !program->reorder.ready)
        return QJS_INTL_INVALID_ARGUMENT;
    temp.allocator = program->builder.allocator;
    temp.root = program->builder.root;
    for (i = 0; i < 4; i++) {
        status = qjs_intl_weight_freeze(program->builder.relations.axes + i);
        if (status)
            goto done;
    }
    state.map = &program->builder.map;
    status = qjs_intl_collation_map_visit(state.map, collect, &state);
    if (!status)
        status = qjs_intl_collation_map_visit_suppressed(state.map, suppressed, &suppression);
    if (status)
        goto done;
    temp.keys.row_count = state.count;
    temp.keys.key_count = state.key_count;
    temp.keys.context_count = state.context_count;
    temp.element_count = state.element_count;
    temp.suppressed_count = suppression.count;
    state.entries = array(&temp.allocator, state.count, sizeof(*state.entries), &status);
    state.keys = array(&temp.allocator, state.key_count, sizeof(*state.keys), &status);
    temp.rows = array(&temp.allocator, state.count, 16, &status);
    temp.key_bytes = array(&temp.allocator, state.key_count, 4, &status);
    temp.contexts = array(&temp.allocator, state.context_count, 12, &status);
    temp.elements = array(&temp.allocator, state.element_count, sizeof(*temp.elements), &status);
    temp.suppressed = array(&temp.allocator, suppression.count, sizeof(*temp.suppressed), &status);
    if (status)
        goto done;
    state.count = state.key_count = state.context_count = state.element_count = 0;
    status = qjs_intl_collation_map_visit(state.map, collect, &state);
    suppression = (Suppressed){temp.suppressed, 0};
    if (!status)
        status = qjs_intl_collation_map_visit_suppressed(state.map, suppressed, &suppression);
    if (status)
        goto done;
    if (state.count > 1)
        qsort(state.entries, state.count, sizeof(*state.entries), entry_order);
    if (suppression.count > 1)
        qsort(temp.suppressed, suppression.count, sizeof(*temp.suppressed), scalar_order);
    status = copy_entries(program, &state, &temp);
    if (!status)
        status = copy_metadata(program, identity, &temp);
    if (status)
        goto done;
    temp.keys.rows = temp.rows;
    temp.keys.keys = temp.key_bytes;
    temp.keys.contexts = temp.contexts;
    temp.ready = 1;
    *out = temp;
    temp = (QJSIntlCollationSnapshot){0};
done:
    if (state.entries)
        program->builder.allocator.free(program->builder.allocator.opaque, state.entries);
    if (state.keys)
        program->builder.allocator.free(program->builder.allocator.opaque, state.keys);
    qjs_intl_collation_snapshot_clear(&temp);
    return status;
}

QJSIntlStatus qjs_intl_collation_snapshot_root_rank(const QJSIntlCollationSnapshot *snapshot,
                                                    unsigned int axis, uint32_t root, uint32_t *rank)
{
    uint32_t low = 0, high;
    if (rank)
        *rank = 0;
    if (!snapshot || !snapshot->ready || !rank || axis >= 4 || root > snapshot->root_maxima[axis])
        return QJS_INTL_INVALID_ARGUMENT;
    high = snapshot->offset_counts[axis];
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        if (snapshot->offsets[axis][middle].root_gap < root)
            low = middle + 1;
        else
            high = middle;
    }
    *rank = root + (low ? snapshot->offsets[axis][low - 1].added : 0);
    return QJS_INTL_OK;
}
