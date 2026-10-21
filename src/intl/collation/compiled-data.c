/* Complete semantic validation precedes every borrowed runtime program view.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "compiled-data.h"
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include <string.h>

static uint32_t word(const uint8_t *p, size_t index)
{
    return qjs_intl_read_u32(p + index * 4);
}

static uint32_t root_maximum(const QJSIntlCompiledCollationProgram *p, unsigned int axis)
{
    const QJSIntlCollationRootLimits *r = &p->root.metadata.limits;
    return axis == 0 ? r->primary_max : axis == 1 ? r->secondary_max : axis == 2 ? r->tertiary_max : 0;
}

static uint32_t root_rank(const QJSIntlCompiledCollationProgram *p, unsigned int axis, uint32_t root)
{
    const QJSIntlSection *s = p->offsets + axis;
    uint32_t low = 0, high = s->count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        if (word(s->bytes + (size_t)middle * 8, 0) < root)
            low = middle + 1;
        else
            high = middle;
    }
    return root + (low ? word(s->bytes + (size_t)(low - 1) * 8, 1) : 0);
}

static int label(const QJSIntlSection *pool, uint32_t start, uint32_t count, QJSIntlBytes *out, int audit)
{
    const uint8_t *at, *end, *next;
    if (start > pool->count || count > pool->count - start)
        return 0;
    at = pool->bytes + start;
    end = at + count;

    *out = (QJSIntlBytes){(const char *)at, count};

    if (!audit)
        return 1;

    for (; at < end; at = next) {
        int code = unicode_from_utf8(at, (int)(end - at), &next);
        if (code < 0x20 || code == 0x7f || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            return 0;
    }
    return 1;
}

static int label_order(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t count = a.length < b.length ? a.length : b.length;
    int order = count ? memcmp(a.data, b.data, count) : 0;
    return order ? order : a.length < b.length ? -1 : a.length != b.length;
}

static int identity_order(const QJSIntlCompiledCollationProgram *a, const QJSIntlCompiledCollationProgram *b)
{
    int order = label_order(a->locale, b->locale);
    if (!order)
        order = label_order(a->collation, b->collation);
    if (!order)
        order = label_order(a->alternate, b->alternate);
    return order;
}

static int take(const uint8_t **at, size_t *remaining, uint32_t count, unsigned int stride, QJSIntlSection *out)
{
    uint64_t size = (uint64_t)count * stride;
    if (size > *remaining)
        return 0;
    *out = (QJSIntlSection){*at, (size_t)size, count};
    *at += (size_t)size;
    *remaining -= (size_t)size;
    return 1;
}

static int program_view(const QJSIntlCompiledCollationData *data, uint32_t index,
                        QJSIntlCompiledCollationProgram *out, int audit_labels)
{
    const uint8_t *row = data->sections[QJS_INTL_COMPILED_PROGRAMS].bytes + (size_t)index * 32;
    const QJSIntlSection *blocks = data->sections + QJS_INTL_COMPILED_BLOCKS;
    uint32_t start = word(row, 6), size = word(row, 7), flags, i;
    const uint8_t *header, *at;
    size_t remaining;
    QJSIntlSection rows, keys, contexts, nodes, edges;
    *out = (QJSIntlCompiledCollationProgram){0};

    if (start > blocks->length || size > blocks->length - start || size < QJS_INTL_COMPILED_COLLATION_PROGRAM_HEADER)
        return 0;

    header = blocks->bytes + start;
    if (word(header, 0) < 1 || word(header, 0) > 5 || word(header, 1) > 2 ||
        word(header, 2) > 3 || (flags = word(header, 3)) & ~31u)
        return 0;

    out->policy = (QJSIntlCollationPolicy){(QJSIntlCollationStrength)word(header, 0),
        (QJSIntlCollationCaseFirst)word(header, 1), (QJSIntlCollationMaxVariable)word(header, 2),
        !!(flags & 1), !!(flags & 2), !!(flags & 4), !!(flags & 8), !!(flags & 16)};

    at = header + QJS_INTL_COMPILED_COLLATION_PROGRAM_HEADER;
    remaining = size - QJS_INTL_COMPILED_COLLATION_PROGRAM_HEADER;
    if (!take(&at, &remaining, word(header, 4), 16, &rows) ||
        !take(&at, &remaining, word(header, 5), 4, &keys) ||
        !take(&at, &remaining, word(header, 6), 12, &contexts) ||
        !take(&at, &remaining, word(header, 7), 24, &out->elements) ||
        !take(&at, &remaining, word(header, 8), 4, &out->suppressed) ||
        !take(&at, &remaining, word(header, 9), 4, &out->group_ranks))
        return 0;

    for (i = 0; i < 4; i++) {
        if (!take(&at, &remaining, word(header, 10 + i), 8, out->offsets + i))
            return 0;
    }

    if (!take(&at, &remaining, word(header, 14), 16, &nodes) ||
        !take(&at, &remaining, word(header, 15), 8, &edges) || remaining)
        return 0;

    out->keys = (QJSIntlCollationKeyIndex){rows.bytes, keys.bytes, contexts.bytes, rows.count, keys.count, contexts.count};
    out->trie = (QJSIntlCollationTrie){nodes.bytes, edges.bytes, out->suppressed.bytes,
        nodes.count, edges.count, out->suppressed.count};
    out->root = data->root;
    out->groups = data->sections[QJS_INTL_COMPILED_GROUPS];
    memcpy(out->core_groups, data->core_groups, sizeof(out->core_groups));
    out->hani_group = data->hani_group;

    return label(data->sections + QJS_INTL_COMPILED_LABELS, word(row, 0), word(row, 1), &out->locale, audit_labels) &&
        label(data->sections + QJS_INTL_COMPILED_LABELS, word(row, 2), word(row, 3), &out->collation, audit_labels) &&
        label(data->sections + QJS_INTL_COMPILED_LABELS, word(row, 4), word(row, 5), &out->alternate, audit_labels);
}

static int offsets(QJSIntlCompiledCollationProgram *p)
{
    unsigned int axis;
    for (axis = 0; axis < 4; axis++) {
        const QJSIntlSection *s = p->offsets + axis;
        uint32_t i, previous_gap = 0, previous_added = 0, maximum = root_maximum(p, axis);
        for (i = 0; i < s->count; i++) {
            uint32_t gap = word(s->bytes + (size_t)i * 8, 0), added = word(s->bytes + (size_t)i * 8, 1);
            if (gap > maximum || (i && gap <= previous_gap) || added <= previous_added ||
                (uint64_t)maximum + added >= UINT32_MAX)
                return 0;
            previous_gap = gap;
            previous_added = added;
        }
        /* Include last-root OWN-gap insertions. translation(root_max) alone
           omits them; the Q axis (root_max=0) is a useful boundary example. */
        p->final_maxima[axis] = maximum + previous_added;
    }
    return 1;
}

static int program_keys(const QJSIntlCompiledCollationProgram *p)
{
    const QJSIntlCollationKeyIndex *keys = &p->keys;
    uint32_t i, end = 0, context = 0, ce_end = 0, previous_count = 0, previous_prefix = 0;
    const uint8_t *previous = NULL;

    for (i = 0; i < keys->context_count; i++) {
        const uint8_t *row = keys->contexts + (size_t)i * 12;
        uint32_t id = word(row, 0), prefix = word(row, 1);
        if (id >= keys->row_count || (i && id <= word(row - 12, 0)) || !prefix ||
            prefix >= word(keys->rows + (size_t)id * 16, 1) || word(row, 2))
            return 0;
    }

    for (i = 0; i < keys->row_count; i++) {
        const uint8_t *row = keys->rows + (size_t)i * 16, *key;
        uint32_t start = word(row, 0), count = word(row, 1), ce_start = word(row, 2), ce_count = word(row, 3);
        uint32_t prefix = 0, j, previous_cc = 0;
        if (start != end || !count || count > keys->key_count - end || ce_start != ce_end ||
            ce_count > p->elements.count - ce_end)
            return 0;

        key = keys->keys + (size_t)start * 4;
        end += count;
        ce_end += ce_count;

        for (j = 0; j < count; j++) {
            uint32_t code = word(key, j), cc;
            if (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) || !unicode_nfd_quick_check(code))
                return 0;
            cc = unicode_get_cc(code);
            if (cc && previous_cc > cc)
                return 0;
            previous_cc = cc;
        }

        if (context < keys->context_count && word(keys->contexts + (size_t)context * 12, 0) == i) {
            prefix = word(keys->contexts + (size_t)context++ * 12, 1);
            if (!unicode_nfc_boundary_before(word(key, 0)) || !unicode_nfc_boundary_before(word(key, prefix)))
                return 0;
        }

        if (previous) {
            uint32_t limit = previous_count < count ? previous_count : count;
            int order = 0;
            for (j = 0; j < limit; j++) {
                uint32_t a = word(previous, j), b = word(key, j);
                if (a != b) {
                    order = a < b ? -1 : 1;
                    break;
                }
            }
            if (!order)
                order = previous_count < count ? -1 : previous_count > count ? 1 :
                    previous_prefix < prefix ? -1 : previous_prefix > prefix ? 1 : 0;
            if (order >= 0)
                return 0;
        }

        previous = key;
        previous_count = count;
        previous_prefix = prefix;
    }

    return end == keys->key_count && ce_end == p->elements.count;
}

static int program_elements(const QJSIntlCompiledCollationProgram *p)
{
    const QJSIntlCollationBaseMetadata *m = &p->root.metadata;
    uint32_t i, sb = root_rank(p, 1, m->limits.secondary_boundary), tb = root_rank(p, 2, m->limits.tertiary_boundary);
    uint32_t numeric = root_rank(p, 0, m->numeric);
    for (i = 0; i < p->elements.count; i++) {
        const uint8_t *record = p->elements.bytes + (size_t)i * 24;
        uint32_t primary = word(record, 0), secondary = word(record, 1), tertiary = word(record, 2);
        uint32_t origin = word(record, 4), flags = word(record, 5), case_value = flags & 3, axis;
        int implicit = (m->han_base < origin && (uint64_t)origin <= (uint64_t)m->han_base + m->han_count) ||
            (m->generic_base < origin && origin < m->generic_limit);

        for (axis = 0; axis < 4; axis++) {
            if (word(record, axis) > p->final_maxima[axis])
                return 0;
        }

        if ((!primary && origin) || primary == numeric || origin > m->limits.primary_max ||
            origin == m->numeric || flags & ~15u || case_value == 3 ||
            !!(flags & 8) != implicit || !!(flags & 4) != (m->field_separator < origin && origin <= m->variable_top))
            return 0;

        if (primary) {
            if (!secondary || secondary >= sb || !tertiary || tertiary >= tb)
                return 0;
        } else if (secondary) {
            if (secondary <= sb || !tertiary || tertiary >= tb)
                return 0;
        } else if (tertiary) {
            if (tertiary <= tb || case_value != QJS_INTL_COLLATION_UPPER)
                return 0;
        } else if (case_value != QJS_INTL_COLLATION_LOWER) {
            return 0;
        }
    }
    return 1;
}

static QJSIntlStatus program_validate(QJSIntlCompiledCollationProgram *p, const QJSIntlAllocator *allocator)
{
    QJSIntlSection nodes = {p->trie.nodes, (size_t)p->trie.node_count * 16, p->trie.node_count};
    QJSIntlSection edges = {p->trie.edges, (size_t)p->trie.edge_count * 8, p->trie.edge_count};
    QJSIntlCollationTrie checked;
    uint32_t i, count = p->groups.count;
    uint8_t *seen;

    if (!offsets(p) || p->group_ranks.count != count || !program_keys(p) || !program_elements(p))
        return QJS_INTL_DATA_ERROR;

    seen = allocator->malloc(allocator->opaque, (count + 7u) / 8u);
    if (!seen)
        return QJS_INTL_NO_MEMORY;
    memset(seen, 0, (count + 7u) / 8u);
    for (i = 0; i < count; i++) {
        uint32_t rank = word(p->group_ranks.bytes, i);
        if (rank >= count || seen[rank / 8] & (1u << (rank % 8))) {
            allocator->free(allocator->opaque, seen);
            return QJS_INTL_DATA_ERROR;
        }
        seen[rank / 8] |= 1u << (rank % 8);
    }
    allocator->free(allocator->opaque, seen);

    return qjs_intl_collation_trie_open(&nodes, &edges, &p->suppressed, &p->keys, allocator, &checked);
}

static int groups(QJSIntlCompiledCollationData *data)
{
    const QJSIntlSection *s = data->sections + QJS_INTL_COMPILED_GROUPS;
    const QJSIntlCollationBaseMetadata *m = &data->root.metadata;
    uint32_t previous = 0, i, hani = UINT32_MAX, digit;

    if (s->count < 7)
        return 0;

    for (i = 0; i < s->count; i++) {
        const uint8_t *row = s->bytes + (size_t)i * 8;
        uint32_t boundary = word(row, 0), kind = word(row, 1);
        if (boundary <= previous || (i + 1 == s->count ? kind != 2 || boundary != m->generic_base :
                                     kind != 1 || boundary >= m->generic_base))
            return 0;
        if (boundary == m->han_base)
            hani = i;
        previous = boundary;
    }

    if (hani == UINT32_MAX)
        return 0;

    for (i = 0; i < 5; i++) {
        if (data->core_groups[i] >= s->count - 1 || (i && data->core_groups[i - 1] >= data->core_groups[i]))
            return 0;
    }

    digit = data->core_groups[4];
    if (word(s->bytes + (size_t)digit * 8, 0) > m->numeric || m->numeric >= word(s->bytes + (size_t)(digit + 1) * 8, 0))
        return 0;
    data->hani_group = hani;
    return 1;
}

QJSIntlStatus qjs_intl_compiled_collation_open(const uint8_t *bytes, size_t length, const uint8_t *expected,
                                               const QJSIntlAllocator *allocator, QJSIntlCompiledCollationData *out)
{
    static const unsigned int strides[] = {80, 16, 4, 12, 24, 12, 4, 8, 32, 0, 0, 24};
    QJSIntlCompiledCollationData data = {0};
    QJSIntlCollationBaseSource source = {0};
    QJSIntlCompiledCollationProgram previous = {0};
    uint32_t at = QJS_INTL_COMPILED_COLLATION_HEADER + QJS_INTL_COMPILED_SECTION_COUNT * 16, i;
    uint32_t label_at = 0, block_at = 0;
    const uint8_t *meta;
    QJSIntlStatus status;

    if (out)
        memset(out, 0, sizeof(*out));

    if (!bytes || !allocator || !allocator->malloc || !allocator->free || !out)
        return QJS_INTL_INVALID_ARGUMENT;

    if (length < at || length > QJS_INTL_COMPILED_COLLATION_MAX_SIZE || memcmp(bytes, "QJCL", 4) ||
        word(bytes, 1) != QJS_INTL_COMPILED_COLLATION_VERSION ||
        word(bytes, 2) != QJS_INTL_COMPILED_COLLATION_UNICODE ||
        word(bytes, 3) != QJS_INTL_COMPILED_COLLATION_UNICODE || word(bytes, 4) != length ||
        word(bytes, 5) != QJS_INTL_COMPILED_SECTION_COUNT || word(bytes, 7) ||
        (expected && memcmp(bytes + 32, expected, 32)))
        return QJS_INTL_DATA_ERROR;

    if ((((uint32_t)LIBUNICODE_UNICODE_VERSION_MAJOR << 16) | (LIBUNICODE_UNICODE_VERSION_MINOR << 8) |
         LIBUNICODE_UNICODE_VERSION_PATCH) != QJS_INTL_COMPILED_COLLATION_UNICODE)
        return QJS_INTL_UNSUPPORTED;

    data.program_count = word(bytes, 6);
    for (i = 0; i < QJS_INTL_COMPILED_SECTION_COUNT; i++) {
        const uint8_t *row = bytes + QJS_INTL_COMPILED_COLLATION_HEADER + i * 16;
        uint32_t size = word(row, 2), count = word(row, 3), stride = strides[i];
        if (word(row, 0) != i + 1 || word(row, 1) != at || size % 4 || size > length - at ||
            (stride && (uint64_t)count * stride != size))
            return QJS_INTL_DATA_ERROR;
        data.sections[i] = (QJSIntlSection){bytes + at, size, count};
        at += size;
    }

    if (at != length || data.sections[QJS_INTL_COMPILED_META].count != 1 ||
        data.sections[QJS_INTL_COMPILED_PROGRAMS].count != data.program_count ||
        data.sections[QJS_INTL_COMPILED_BLOCKS].count != data.program_count ||
        data.sections[QJS_INTL_COMPILED_BINDINGS].count || data.sections[QJS_INTL_COMPILED_BINDINGS].length)
        return QJS_INTL_DATA_ERROR;

    {
        const QJSIntlSection *labels = data.sections + QJS_INTL_COMPILED_LABELS;
        if (labels->count > labels->length || labels->length - labels->count > 3)
            return QJS_INTL_DATA_ERROR;
        for (i = labels->count; i < labels->length; i++) {
            if (labels->bytes[i])
                return QJS_INTL_DATA_ERROR;
        }
    }

    meta = data.sections[QJS_INTL_COMPILED_META].bytes;
    source.metadata.limits = (QJSIntlCollationRootLimits){
        word(meta, 0), word(meta, 1), word(meta, 2), word(meta, 3),
        word(meta, 4), word(meta, 5), word(meta, 6)
    };

    source.metadata.han_base = word(meta, 7);
    source.metadata.han_count = word(meta, 8);

    source.metadata.generic_base = word(meta, 9);
    source.metadata.generic_count = word(meta, 10);
    source.metadata.generic_limit = word(meta, 11);

    source.metadata.variable_top = word(meta, 12);
    source.metadata.numeric = word(meta, 13);
    source.metadata.field_separator = word(meta, 14);

    source.mappings = data.sections[QJS_INTL_COMPILED_MAPPINGS];
    source.keys = data.sections[QJS_INTL_COMPILED_KEYS];
    source.contexts = data.sections[QJS_INTL_COMPILED_CONTEXTS];
    source.elements = data.sections[QJS_INTL_COMPILED_ELEMENTS];
    source.han_ranges = data.sections[QJS_INTL_COMPILED_HAN_RANGES];
    source.han_ranks = data.sections[QJS_INTL_COMPILED_HAN_RANKS];

    status = qjs_intl_collation_base_open(&source, allocator, &data.root);
    if (status)
        return status;

    for (i = 0; i < 5; i++)
        data.core_groups[i] = word(meta, 15 + i);

    if (!groups(&data))
        return QJS_INTL_DATA_ERROR;

    for (i = 0; i < data.program_count; i++) {
        QJSIntlCompiledCollationProgram program;
        const uint8_t *row = data.sections[QJS_INTL_COMPILED_PROGRAMS].bytes + (size_t)i * 32;
        unsigned int j;

        for (j = 0; j < 3; j++) {
            uint32_t start = word(row, j * 2), count = word(row, j * 2 + 1);
            const QJSIntlSection *labels = data.sections + QJS_INTL_COMPILED_LABELS;
            if (start != label_at || count > labels->count - label_at || (j < 2 && !count))
                return QJS_INTL_DATA_ERROR;
            label_at += count;
        }

        if (word(row, 6) != block_at || !program_view(&data, i, &program, 1) ||
            (i && identity_order(&previous, &program) >= 0))
            return QJS_INTL_DATA_ERROR;
        block_at += word(row, 7);
        status = program_validate(&program, allocator);
        if (status)
            return status;
        previous = program;
    }

    if (label_at != data.sections[QJS_INTL_COMPILED_LABELS].count || block_at != data.sections[QJS_INTL_COMPILED_BLOCKS].length)
        return QJS_INTL_DATA_ERROR;

    data.fingerprint = bytes + 32;
    data.ready = 1;
    *out = data;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_compiled_collation_program(const QJSIntlCompiledCollationData *data, uint32_t index,
                                                  QJSIntlCompiledCollationProgram *out)
{
    if (out)
        memset(out, 0, sizeof(*out));

    if (!data || !data->ready || !out || index >= data->program_count)
        return QJS_INTL_INVALID_ARGUMENT;

    /* The complete archive was audited at open. Recompute only fixed scalar
       descriptors, never repeat Unicode/property/permutation/trie validation. */
    if (!program_view(data, index, out, 0)) {
        memset(out, 0, sizeof(*out));
        return QJS_INTL_DATA_ERROR;
    }

    {
        unsigned int axis;
        for (axis = 0; axis < 4; axis++) {
            const QJSIntlSection *s = out->offsets + axis;
            out->final_maxima[axis] = root_maximum(out, axis) +
                (s->count ? word(s->bytes + (size_t)(s->count - 1) * 8, 1) : 0);
        }
    }

    out->ready = 1;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_compiled_collation_mapping(const QJSIntlCompiledCollationProgram *p, uint32_t index,
                                                uint32_t *ce_start, uint32_t *ce_count)
{
    const uint8_t *row;

    if (ce_start)
        *ce_start = 0;
    if (ce_count)
        *ce_count = 0;

    if (!p || !p->ready || !ce_start || !ce_count || index >= p->keys.row_count)
        return QJS_INTL_INVALID_ARGUMENT;

    row = p->keys.rows + (size_t)index * 16;
    *ce_start = word(row, 2);
    *ce_count = word(row, 3);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_compiled_collation_element(const QJSIntlCompiledCollationProgram *p, uint32_t index,
                                                  QJSIntlCollationRankedElement *out)
{
    const uint8_t *record;
    uint32_t flags;
    unsigned int axis;

    if (out)
        memset(out, 0, sizeof(*out));

    if (!p || !p->ready || !out || index >= p->elements.count)
        return QJS_INTL_INVALID_ARGUMENT;

    record = p->elements.bytes + (size_t)index * 24;
    for (axis = 0; axis < 4; axis++)
        out->ranks[axis] = word(record, axis);
    out->root_primary = word(record, 4);
    flags = word(record, 5);
    out->case_value = flags & 3;
    out->variable = !!(flags & 4);
    out->implicit = !!(flags & 8);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_compiled_collation_root_rank(const QJSIntlCompiledCollationProgram *p, unsigned int axis,
                                                    uint32_t ordinal, uint32_t *rank)
{
    if (rank)
        *rank = 0;
    if (!p || !p->ready || !rank || axis >= 4 || ordinal > root_maximum(p, axis))
        return QJS_INTL_INVALID_ARGUMENT;

    *rank = root_rank(p, axis, ordinal);
    return QJS_INTL_OK;
}

static void translate(const QJSIntlCompiledCollationProgram *p, const QJSIntlCollationCoordinate *ce,
                      QJSIntlCollationRankedElement *out)
{
    unsigned int axis;
    for (axis = 0; axis < 4; axis++)
        out->ranks[axis] = root_rank(p, axis, ce->weights[axis]);
    out->root_primary = ce->root_primary;
    out->case_value = ce->case_value;
    out->variable = ce->variable;
    out->implicit = ce->implicit;
}

QJSIntlStatus qjs_intl_compiled_collation_root_element(const QJSIntlCompiledCollationProgram *p, uint32_t index,
                                                       QJSIntlCollationRankedElement *out)
{
    QJSIntlCollationCoordinate ce;
    QJSIntlStatus status;

    if (out)
        memset(out, 0, sizeof(*out));

    if (!p || !p->ready || !out)
        return QJS_INTL_INVALID_ARGUMENT;

    status = qjs_intl_collation_base_element(&p->root, index, &ce);
    if (!status)
        translate(p, &ce, out);
    return status;
}

QJSIntlStatus qjs_intl_compiled_collation_implicit(const QJSIntlCompiledCollationProgram *p, uint32_t code,
                                                   QJSIntlCollationRankedElement *out)
{
    QJSIntlCollationCoordinate ce;
    QJSIntlStatus status;

    if (out)
        memset(out, 0, sizeof(*out));

    if (!p || !p->ready || !out)
        return QJS_INTL_INVALID_ARGUMENT;

    status = qjs_intl_collation_base_implicit(&p->root, code, &ce);
    if (!status)
        translate(p, &ce, out);
    return status;
}
