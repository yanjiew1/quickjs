/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-root-input.h"
#include "../../src/intl/native/wire.h"
#include <string.h>

#define ROOT_MAX_SIZE (64u * 1024 * 1024)
#define ROOT_HEADER_SIZE 64u
#define ROOT_DIRECTORY_SIZE (QJS_INTL_ROOT_SECTION_COUNT * 16u)
#define ROOT_VERSION (18u << 16)
#define SCRIPT_COUNT (26u * 26 * 26 * 26)
#define TOKEN_COUNT (SCRIPT_COUNT + 5u)

static uint32_t word(const uint8_t *p, size_t index)
{
    return qjs_intl_read_u32(p + index * 4);
}

static int metadata(QJSIntlRootInput *view)
{
    const uint8_t *p = view->sections[QJS_INTL_ROOT_META].bytes;
    QJSIntlRootInputMetadata *m = &view->metadata;
    QJSIntlCollationRootLimits *r = &m->limits;
    unsigned int i;
    if (view->sections[QJS_INTL_ROOT_META].length != 18 * 4 ||
        view->sections[QJS_INTL_ROOT_META].count != 1)
        return 0;
    for (i = 0; i < 18; i++) {
        if (word(p, i) >= UINT32_C(0x80000000))
            return 0;
    }
    *r = (QJSIntlCollationRootLimits){word(p, 0), word(p, 1), word(p, 2),
        word(p, 3), word(p, 4), word(p, 5), word(p, 6)};
    m->han_base = word(p, 7);
    m->han_count = word(p, 8);
    m->generic_base = word(p, 9);
    m->generic_count = word(p, 10);
    m->generic_limit = word(p, 11);
    m->implicit_first_marker = word(p, 12);
    m->implicit_last_marker = word(p, 13);
    m->variable_top = word(p, 14);
    m->numeric = word(p, 15);
    m->field_separator = word(p, 16);
    m->last_regular = word(p, 17);
    /* Common coordinate/CE/Han invariants belong to the base reader. These
       constraints describe compiler-only source markers and reset seeds. */
    return (uint64_t)m->han_base + m->han_count < m->implicit_first_marker &&
        m->implicit_first_marker < m->implicit_last_marker &&
        m->implicit_last_marker == m->generic_base && m->last_regular == m->han_base;
}

static int compare_ce(const uint8_t *a, const uint8_t *b)
{
    unsigned int i;
    for (i = 0; i < 3; i++) {
        if (word(a, i) != word(b, i))
            return word(a, i) < word(b, i) ? -1 : 1;
    }
    return 0;
}

static int facts(const QJSIntlRootInput *view)
{
    const QJSIntlRootInputMetadata *m = &view->metadata;
    const QJSIntlCollationRootLimits *r = &m->limits;
    const uint8_t *resets = view->sections[QJS_INTL_ROOT_RESETS].bytes;
    const uint8_t *weak = view->sections[QJS_INTL_ROOT_WEAK_LIMITS].bytes;
    unsigned int i;
    for (i = 0; i < 14; i++) {
        const uint8_t *row = resets + i * 32, *ce = row + 8;
        unsigned int kind = i / 2, role = i == 9 ? 3 : kind == 0 || kind == 1 || kind == 5 ? 2 : 1;
        uint32_t p = word(ce, 0), s = word(ce, 1), t = word(ce, 2);
        int valid;
        if (word(row, 0) != i + 1 || word(row, 1) != role || !qjs_intl_collation_base_check_element(&view->base.metadata, ce))
            return 0;
        switch (kind) {
        case 0: valid = !p && !s && !t; break;
        case 1: valid = !p && !s && t > r->tertiary_boundary; break;
        case 2: valid = !p && s > r->secondary_boundary; break;
        case 3: valid = m->field_separator < p && p <= m->variable_top; break;
        case 4: valid = m->variable_top < p && p <= m->han_base; break;
        case 5: valid = p == (i == 10 ? m->implicit_first_marker : m->implicit_last_marker); break;
        default: valid = m->generic_limit <= p && p <= r->primary_max; break;
        }
        if (i == 9)
            valid = valid && p == m->last_regular;
        if (kind >= 5 || i == 9)
            valid = valid && s == r->secondary_common && t == r->tertiary_common && !word(ce, 5);
        if (!valid || (i % 2 && compare_ce(ce - 32, ce) > 0))
            return 0;
    }
    for (i = 0; i < 4; i++) {
        const uint8_t *row = weak + i * 16;
        uint32_t axis = i < 2 ? 1 : 2, value = word(row, 2);
        uint32_t boundary = axis == 1 ? r->secondary_boundary : r->tertiary_boundary;
        if (word(row, 0) != i + 1 || word(row, 1) != axis || !value || value >= boundary ||
            word(row, 3) || (i % 2 && word(row - 16, 2) > value))
            return 0;
    }
    return 1;
}

static const char *const core_names[] = {"space", "punct", "symbol", "currency", "digit"};

static uint32_t token_id(const uint8_t *bytes, uint32_t length)
{
    uint32_t i, id;
    for (i = 0; i < 5; i++) {
        if (length == strlen(core_names[i]) && !memcmp(bytes, core_names[i], length))
            return SCRIPT_COUNT + i;
    }
    if (length != 4 || bytes[0] < 'A' || bytes[0] > 'Z')
        return UINT32_MAX;
    id = bytes[0] - 'A';
    for (i = 1; i < 4; i++) {
        if (bytes[i] < 'a' || bytes[i] > 'z')
            return UINT32_MAX;
        id = id * 26 + bytes[i] - 'a';
    }
    return id;
}

static int groups(const QJSIntlRootInput *view, uint8_t *seen)
{
    const QJSIntlRootInputSection *tokens = &view->sections[QJS_INTL_ROOT_TOKENS];
    const QJSIntlRootInputSection *strings = &view->sections[QJS_INTL_ROOT_TOKEN_BYTES];
    const QJSIntlRootInputSection *rows = &view->sections[QJS_INTL_ROOT_GROUPS];
    const QJSIntlRootInputMetadata *m = &view->metadata;
    uint32_t i, end = 0, previous = 0, core[5] = {0}, digit = 0;
    unsigned int unassigned = 0, has_han = 0;
    if (strings->count > strings->length || strings->length - strings->count > 3)
        return 0;
    for (i = strings->count; i < strings->length; i++) {
        if (strings->bytes[i])
            return 0;
    }
    memset(seen, 0, (TOKEN_COUNT + 7u) / 8u);
    for (i = 0; i < tokens->count; i++) {
        const uint8_t *row = tokens->bytes + (size_t)i * 8;
        uint32_t start = word(row, 0), count = word(row, 1), id;
        if (start != end || !count || count > strings->count - end)
            return 0;
        id = token_id(strings->bytes + start, count);
        if (id == UINT32_MAX || seen[id / 8] & (1u << (id % 8)))
            return 0;
        seen[id / 8] |= 1u << (id % 8);
        end += count;
    }
    if (end != strings->count)
        return 0;
    end = 0;
    for (i = 0; i < rows->count; i++) {
        const uint8_t *row = rows->bytes + (size_t)i * 16;
        uint32_t p = word(row, 0), kind = word(row, 1), start = word(row, 2), count = word(row, 3), j;
        if (p <= previous || p > m->generic_base || (kind != 1 && kind != 2) || start != end ||
            count > tokens->count - end || (kind == 1) != !!count)
            return 0;
        if (kind == 2) {
            if (p != m->generic_base)
                return 0;
            unassigned++;
        }
        for (j = 0; j < count; j++) {
            const uint8_t *token = tokens->bytes + (size_t)(start + j) * 8;
            const uint8_t *text = strings->bytes + word(token, 0);
            uint32_t id = token_id(text, word(token, 1));
            if (id >= SCRIPT_COUNT)
                core[id - SCRIPT_COUNT] = i + 1;
            if (id == token_id((const uint8_t *)"Hani", 4)) {
                if (p != m->han_base)
                    return 0;
                has_han = 1;
            }
        }
        end += count;
        previous = p;
    }
    if (end != tokens->count || unassigned != 1 || !has_han)
        return 0;
    for (i = 0; i < 5; i++) {
        if (!core[i] || (i && core[i - 1] >= core[i]))
            return 0;
    }
    digit = core[4] - 1;
    return digit + 1 < rows->count && word(rows->bytes + (size_t)digit * 16, 0) <= m->numeric &&
        m->numeric < word(rows->bytes + (size_t)(digit + 1) * 16, 0);
}

static QJSIntlSection base_span(const QJSIntlRootInputSection *s)
{
    return (QJSIntlSection){s->bytes, s->length, s->count};
}

static QJSIntlCollationBaseSource base_source(const QJSIntlRootInput *view)
{
    const QJSIntlRootInputMetadata *m = &view->metadata;
    const QJSIntlRootInputSection *s = view->sections;
    QJSIntlCollationBaseSource source = {0};
    source.metadata = (QJSIntlCollationBaseMetadata){m->limits, m->han_base, m->han_count,
        m->generic_base, m->generic_count, m->generic_limit, m->variable_top, m->numeric, m->field_separator};
    source.mappings = base_span(s + QJS_INTL_ROOT_MAPPINGS);
    source.keys = base_span(s + QJS_INTL_ROOT_KEYS);
    source.contexts = base_span(s + QJS_INTL_ROOT_CONTEXTS);
    source.elements = base_span(s + QJS_INTL_ROOT_CES);
    source.han_ranges = base_span(s + QJS_INTL_ROOT_HAN_RANGES);
    source.han_ranks = base_span(s + QJS_INTL_ROOT_HAN_RANKS);
    return source;
}

QJSIntlStatus qjs_intl_root_input_open(const uint8_t *bytes, size_t length,
                                      const QJSIntlAllocator *allocator, QJSIntlRootInput *out)
{
    QJSIntlRootInput view = {0};
    uint32_t i, at = ROOT_HEADER_SIZE + ROOT_DIRECTORY_SIZE;
    uint8_t *scratch;
    QJSIntlCollationBaseSource source;
    QJSIntlStatus status;
    int valid;
    /* Metadata and token byte pools have their own count semantics. */
    static const unsigned int strides[] = {0, 16, 4, 12, 24, 12, 4, 32, 16, 16, 8, 0};
    if (out)
        memset(out, 0, sizeof(*out));
    if (!bytes || !out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    if (length < at || length > ROOT_MAX_SIZE || memcmp(bytes, "QJCR", 4) || word(bytes, 1) != 1 ||
        word(bytes, 2) != ROOT_VERSION || word(bytes, 3) != ROOT_VERSION || word(bytes, 4) != length ||
        word(bytes, 5) != QJS_INTL_ROOT_SECTION_COUNT || word(bytes, 6) || word(bytes, 7))
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < QJS_INTL_ROOT_SECTION_COUNT; i++) {
        const uint8_t *row = bytes + ROOT_HEADER_SIZE + i * 16;
        uint32_t offset = word(row, 1), size = word(row, 2), count = word(row, 3), stride = strides[i];
        if (word(row, 0) != i + 1 || offset != at || size % 4 || size > length - at ||
            (stride && (size % stride || count != size / stride)))
            return QJS_INTL_DATA_ERROR;
        view.sections[i] = (QJSIntlRootInputSection){bytes + at, size, count};
        at += size;
    }
    if (at != length || !metadata(&view) || view.sections[QJS_INTL_ROOT_RESETS].count != 14 ||
        view.sections[QJS_INTL_ROOT_WEAK_LIMITS].count != 4)
        return QJS_INTL_DATA_ERROR;
    source = base_source(&view);
    status = qjs_intl_collation_base_open(&source, allocator, &view.base);
    if (status)
        return status;
    view.keys = view.base.keys;
    /* Compiler token uniqueness is independent of the shared Han bitmap. */
    scratch = allocator->malloc(allocator->opaque, (TOKEN_COUNT + 7u) / 8u);
    if (!scratch)
        return QJS_INTL_NO_MEMORY;
    valid = facts(&view) && groups(&view, scratch);
    allocator->free(allocator->opaque, scratch);
    if (!valid)
        return QJS_INTL_DATA_ERROR;
    view.fingerprint = bytes + 32;
    *out = view;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_root_input_mapping(const QJSIntlRootInput *view, uint32_t id,
                                         uint32_t *start, uint32_t *count)
{
    return qjs_intl_collation_base_mapping(view && view->fingerprint ? &view->base : NULL, id, start, count);
}

QJSIntlStatus qjs_intl_root_input_element_at(const QJSIntlRootInput *view, uint32_t index,
                                            QJSIntlCollationCoordinate *out)
{
    return qjs_intl_collation_base_element(view && view->fingerprint ? &view->base : NULL, index, out);
}

QJSIntlStatus qjs_intl_root_input_group(const QJSIntlRootInput *view, QJSIntlBytes token,
                                        uint32_t *group_id)
{
    uint32_t i;
    if (group_id)
        *group_id = 0;
    if (!view || !view->fingerprint || !group_id || (!token.data && token.length))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!token.length || token.length > 8)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < view->sections[QJS_INTL_ROOT_GROUPS].count; i++) {
        const uint8_t *group = view->sections[QJS_INTL_ROOT_GROUPS].bytes + (size_t)i * 16;
        uint32_t first = word(group, 2), count = word(group, 3), j;
        for (j = 0; j < count; j++) {
            const uint8_t *row = view->sections[QJS_INTL_ROOT_TOKENS].bytes + (size_t)(first + j) * 8;
            if (word(row, 1) == token.length &&
                !memcmp(view->sections[QJS_INTL_ROOT_TOKEN_BYTES].bytes + word(row, 0), token.data, token.length)) {
                *group_id = i;
                return QJS_INTL_OK;
            }
        }
    }
    return QJS_INTL_UNSUPPORTED;
}

QJSIntlStatus qjs_intl_root_input_reset(const QJSIntlRootInput *view, QJSIntlRootReset kind,
                                        unsigned int *role, QJSIntlCollationCoordinate *out)
{
    const uint8_t *row;
    if (out)
        memset(out, 0, sizeof(*out));
    if (role)
        *role = 0;
    if (!view || !view->fingerprint || !role || !out ||
        kind < QJS_INTL_ROOT_FIRST_TERTIARY_IGNORABLE || kind > QJS_INTL_ROOT_LAST_TRAILING)
        return QJS_INTL_INVALID_ARGUMENT;
    row = view->sections[QJS_INTL_ROOT_RESETS].bytes + (size_t)(kind - 1) * 32;
    *role = word(row, 1);
    qjs_intl_collation_base_read_element(row + 8, out);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_root_input_implicit(const QJSIntlRootInput *view, uint32_t code,
                                          QJSIntlCollationCoordinate *out)
{
    return qjs_intl_collation_base_implicit(view && view->fingerprint ? &view->base : NULL, code, out);
}
