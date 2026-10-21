/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-root-input.h"
#include "../../src/intl/native/wire.h"
#include "../../src/cutils/cutils.h"
#include "../../src/unicode/libunicode.h"
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

static int scalar(uint32_t code)
{
    return code <= 0x10ffff && (code < 0xd800 || code > 0xdfff);
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
    return r->secondary_common && r->secondary_common < r->secondary_boundary &&
        r->secondary_boundary <= r->secondary_max &&
        r->tertiary_common && r->tertiary_common < r->tertiary_boundary &&
        r->tertiary_boundary <= r->tertiary_max &&
        m->han_count && m->han_count <= 0x110000 && m->generic_count == 0x110000 &&
        m->field_separator && m->field_separator < m->variable_top &&
        m->variable_top < m->numeric && m->numeric < m->han_base &&
        (uint64_t)m->han_base + m->han_count < m->implicit_first_marker &&
        m->implicit_first_marker < m->implicit_last_marker &&
        m->implicit_last_marker == m->generic_base &&
        (uint64_t)m->generic_base + m->generic_count + 1 == m->generic_limit &&
        m->generic_limit <= r->primary_max && m->last_regular == m->han_base;
}

static void element(const uint8_t *p, QJSIntlCollationCoordinate *out)
{
    unsigned int i;
    uint32_t flags = word(p, 5);
    for (i = 0; i < 4; i++)
        out->weights[i] = word(p, i);
    out->root_primary = word(p, 4);
    out->case_value = flags & 3;
    out->variable = !!(flags & 4);
    out->implicit = !!(flags & 8);
}

static int valid_element(const uint8_t *bytes, const QJSIntlRootInputMetadata *m)
{
    const QJSIntlCollationRootLimits *r = &m->limits;
    uint32_t p = word(bytes, 0), s = word(bytes, 1), t = word(bytes, 2);
    uint32_t flags = word(bytes, 5), case_value = flags & 3;
    int implicit = (m->han_base < p && (uint64_t)p <= (uint64_t)m->han_base + m->han_count) ||
        (m->generic_base < p && p < m->generic_limit);
    if (p > r->primary_max || s > r->secondary_max || t > r->tertiary_max ||
        word(bytes, 3) || word(bytes, 4) != p || flags & ~15u || case_value == 3 ||
        p == m->numeric || !!(flags & 8) != implicit ||
        !!(flags & 4) != (m->field_separator < p && p <= m->variable_top))
        return 0;
    if (p)
        return s && s < r->secondary_boundary && t && t < r->tertiary_boundary;
    if (s)
        return s > r->secondary_boundary && t && t < r->tertiary_boundary;
    if (t)
        return t > r->tertiary_boundary && case_value == QJS_INTL_COLLATION_UPPER;
    return case_value == QJS_INTL_COLLATION_LOWER;
}

static int mappings(const QJSIntlRootInput *view)
{
    const QJSIntlCollationKeyIndex *keys = &view->keys;
    uint32_t i, end = 0, context = 0, previous_prefix = 0, previous_count = 0;
    const uint8_t *previous = NULL;
    for (i = 0; i < keys->key_count; i++) {
        if (!scalar(word(keys->keys, i)))
            return 0;
    }
    for (i = 0; i < keys->context_count; i++) {
        const uint8_t *row = keys->contexts + (size_t)i * 12;
        uint32_t id = word(row, 0), prefix = word(row, 1);
        if (id >= keys->row_count || (i && id <= word(row - 12, 0)) || !prefix ||
            prefix >= word(keys->rows + (size_t)id * 16, 1) || word(row, 2))
            return 0;
    }
    for (i = 0; i < keys->row_count; i++) {
        const uint8_t *row = keys->rows + (size_t)i * 16, *key;
        uint32_t start = word(row, 0), count = word(row, 1), prefix = 0;
        uint32_t ce_start = word(row, 2), ce_count = word(row, 3), j;
        uint32_t pool_count = view->sections[QJS_INTL_ROOT_CES].count;
        if (start != end || !count || count > keys->key_count - end || !ce_count ||
            ce_start > pool_count || ce_count > pool_count - ce_start)
            return 0;
        key = keys->keys + (size_t)start * 4;
        end += count;
        if (context < keys->context_count && word(keys->contexts + (size_t)context * 12, 0) == i) {
            prefix = word(keys->contexts + (size_t)context++ * 12, 1);
            /* Source keys remain original. Check both starts with the same
               pinned boundary predicate used for new tailoring contexts. */
            if (!unicode_nfc_boundary_before(word(key, 0)) ||
                !unicode_nfc_boundary_before(word(key, prefix)))
                return 0;
        }
        if (word(key, prefix) == 0xfdd0)
            return 0;
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
    return keys->row_count && end == keys->key_count;
}

static int han(const QJSIntlRootInput *view, uint8_t *seen)
{
    const QJSIntlRootInputSection *ranges = &view->sections[QJS_INTL_ROOT_HAN_RANGES];
    const QJSIntlRootInputSection *ranks = &view->sections[QJS_INTL_ROOT_HAN_RANKS];
    uint32_t i, end = 0, previous = 0;
    memset(seen, 0, (view->metadata.han_count + 7u) / 8u);
    for (i = 0; i < ranges->count; i++) {
        const uint8_t *row = ranges->bytes + (size_t)i * 12;
        uint32_t first = word(row, 0), last = word(row, 1), start = word(row, 2), count;
        if (first > last || !scalar(first) || !scalar(last) ||
            (first <= 0xdfff && last >= 0xd800) || (i && first <= previous) || start != end)
            return 0;
        count = last - first + 1;
        if (count > ranks->count - end)
            return 0;
        end += count;
        previous = last;
    }
    if (end != ranks->count || end != view->metadata.han_count)
        return 0;
    for (i = 0; i < ranks->count; i++) {
        uint32_t rank = word(ranks->bytes, i);
        if (!rank || rank > end || seen[(rank - 1) / 8] & (1u << ((rank - 1) % 8)))
            return 0;
        seen[(rank - 1) / 8] |= 1u << ((rank - 1) % 8);
    }
    return 1;
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
        if (word(row, 0) != i + 1 || word(row, 1) != role || !valid_element(ce, m))
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

QJSIntlStatus qjs_intl_root_input_open(const uint8_t *bytes, size_t length,
                                      const QJSIntlAllocator *allocator, QJSIntlRootInput *out)
{
    QJSIntlRootInput view = {0};
    uint32_t i, at = ROOT_HEADER_SIZE + ROOT_DIRECTORY_SIZE;
    uint8_t *scratch;
    size_t scratch_size;
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
    view.keys = (QJSIntlCollationKeyIndex){view.sections[QJS_INTL_ROOT_MAPPINGS].bytes,
        view.sections[QJS_INTL_ROOT_KEYS].bytes, view.sections[QJS_INTL_ROOT_CONTEXTS].bytes,
        view.sections[QJS_INTL_ROOT_MAPPINGS].count, view.sections[QJS_INTL_ROOT_KEYS].count,
        view.sections[QJS_INTL_ROOT_CONTEXTS].count};
    /* Reuse one bounded bitmap for exact Han permutation and token uniqueness.
       Unlike CE expansion per mapping, this scratch is independent of sharing. */
    scratch_size = (view.metadata.han_count + 7u) / 8u;
    if (scratch_size < (TOKEN_COUNT + 7u) / 8u)
        scratch_size = (TOKEN_COUNT + 7u) / 8u;
    scratch = allocator->malloc(allocator->opaque, scratch_size);
    if (!scratch)
        return QJS_INTL_NO_MEMORY;
    valid = mappings(&view) && han(&view, scratch) && facts(&view) && groups(&view, scratch);
    for (i = 0; valid && i < view.sections[QJS_INTL_ROOT_CES].count; i++)
        valid = valid_element(view.sections[QJS_INTL_ROOT_CES].bytes + (size_t)i * 24, &view.metadata);
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
    const uint8_t *row;
    if (start)
        *start = 0;
    if (count)
        *count = 0;
    if (!view || !view->fingerprint || !start || !count || id >= view->keys.row_count)
        return QJS_INTL_INVALID_ARGUMENT;
    row = view->keys.rows + (size_t)id * 16;
    *start = word(row, 2);
    *count = word(row, 3);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_root_input_element_at(const QJSIntlRootInput *view, uint32_t index,
                                            QJSIntlCollationCoordinate *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    if (!view || !view->fingerprint || !out || index >= view->sections[QJS_INTL_ROOT_CES].count)
        return QJS_INTL_INVALID_ARGUMENT;
    element(view->sections[QJS_INTL_ROOT_CES].bytes + (size_t)index * 24, out);
    return QJS_INTL_OK;
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
    element(row + 8, out);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_root_input_implicit(const QJSIntlRootInput *view, uint32_t code,
                                          QJSIntlCollationCoordinate *out)
{
    uint32_t lo = 0, hi, primary;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!view || !view->fingerprint || !out || code > 0x10ffff)
        return QJS_INTL_INVALID_ARGUMENT;
    hi = view->sections[QJS_INTL_ROOT_HAN_RANGES].count;
    primary = view->metadata.generic_base + code + 1;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        const uint8_t *row = view->sections[QJS_INTL_ROOT_HAN_RANGES].bytes + (size_t)mid * 12;
        uint32_t first = word(row, 0), last = word(row, 1);
        if (code < first)
            hi = mid;
        else if (code > last)
            lo = mid + 1;
        else {
            primary = view->metadata.han_base + word(view->sections[QJS_INTL_ROOT_HAN_RANKS].bytes,
                                                    word(row, 2) + code - first);
            break;
        }
    }
    *out = (QJSIntlCollationCoordinate){{primary, view->metadata.limits.secondary_common,
        view->metadata.limits.tertiary_common, 0}, primary, QJS_INTL_COLLATION_LOWER, 0, 1};
    return QJS_INTL_OK;
}
