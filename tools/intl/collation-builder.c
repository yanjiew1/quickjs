/* Share canonical matching between host rule execution and runtime lookup.
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
#include "collation-builder.h"
#include "collation-text.h"
#include "../../src/intl/collation/match.h"
#include "../../src/cutils/cutils.h"
#include "../../src/unicode/libunicode.h"
#include <string.h>

static int active(const QJSIntlCollationBuilder *builder)
{
    return builder && builder->ready && !builder->failed;
}

static QJSIntlStatus finish(QJSIntlCollationBuilder *builder, QJSIntlStatus status)
{
    if (status && builder)
        builder->failed = 1;
    return status;
}

void qjs_intl_collation_builder_clear(QJSIntlCollationBuilder *builder)
{
    if (!builder)
        return;
    qjs_intl_collation_map_clear(&builder->map);
    qjs_intl_relations_clear(&builder->relations);
    memset(builder, 0, sizeof(*builder));
}

QJSIntlStatus qjs_intl_collation_builder_init(QJSIntlCollationBuilder *builder,
                                             const QJSIntlRootInput *root,
                                             const QJSIntlAllocator *allocator)
{
    QJSIntlStatus status;
    if (!builder)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(builder, 0, sizeof(*builder));
    if (!root || !root->fingerprint)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_relations_init(&builder->relations, allocator, &root->metadata.limits);
    if (status)
        return status;
    builder->allocator = *allocator;
    builder->root = *root;
    status = qjs_intl_collation_map_init(&builder->map, allocator);
    if (status)
        return status;
    builder->ready = 1;
    return QJS_INTL_OK;
}

static QJSIntlStatus append(QJSIntlCollationBuilder *builder, QJSIntlRelationMapping *out,
                             size_t *capacity, const QJSIntlRelationElement *elements, size_t count)
{
    if (count > SIZE_MAX - out->count)
        return QJS_INTL_OVERFLOW;
    if (out->count + count > *capacity) {
        size_t next_capacity = *capacity ? *capacity : 8;
        QJSIntlRelationElement *next;
        while (next_capacity < out->count + count) {
            if (next_capacity > SIZE_MAX / 2)
                return QJS_INTL_OVERFLOW;
            next_capacity *= 2;
        }
        if (next_capacity > SIZE_MAX / sizeof(*next))
            return QJS_INTL_OVERFLOW;
        next = out->elements ? builder->allocator.realloc(builder->allocator.opaque, out->elements,
                                                            next_capacity * sizeof(*next)) :
            builder->allocator.malloc(builder->allocator.opaque, next_capacity * sizeof(*next));
        if (!next)
            return QJS_INTL_NO_MEMORY;
        out->elements = next;
        *capacity = next_capacity;
    }
    if (count)
        memcpy(out->elements + out->count, elements, count * sizeof(*elements));
    out->count += count;
    return QJS_INTL_OK;
}

static QJSIntlStatus append_root(QJSIntlCollationBuilder *builder, QJSIntlRelationMapping *out,
                                  size_t *capacity, uint32_t mapping_id)
{
    uint32_t start, count, i;
    QJSIntlStatus status = qjs_intl_root_input_mapping(&builder->root, mapping_id, &start, &count);
    for (i = 0; !status && i < count; i++) {
        QJSIntlRelationElement ce;
        status = qjs_intl_root_input_element_at(&builder->root, start + i, &ce);
        if (!status)
            status = append(builder, out, capacity, &ce, 1);
    }
    return status;
}

static QJSIntlStatus lookup(QJSIntlCollationBuilder *builder, const QJSIntlCompilerText *text, int current,
                             QJSIntlRelationMapping *out)
{
    QJSIntlCollationOverlay overlay;
    QJSIntlCollationMatch match;
    QJSIntlCollationMatchResult result;
    QJSIntlStatus status;
    uint16_t *utf16;
    uint64_t units = 0;
    size_t capacity = 0, at = 0;
    uint32_t i;
    int present;
    *out = (QJSIntlRelationMapping){0};
    if (!text->count)
        return QJS_INTL_OK;
    for (i = 0; i < text->count; i++)
        units += text->codes[i] < 0x10000 ? 1 : 2;
    if (units > SIZE_MAX / sizeof(*utf16))
        return QJS_INTL_OVERFLOW;
    utf16 = builder->allocator.malloc(builder->allocator.opaque, (size_t)units * sizeof(*utf16));
    if (!utf16)
        return QJS_INTL_NO_MEMORY;
    for (i = 0; i < text->count; i++) {
        uint32_t code = text->codes[i];
        if (code < 0x10000) {
            utf16[at++] = code;
        } else {
            code -= 0x10000;
            utf16[at++] = 0xd800 + (code >> 10);
            utf16[at++] = 0xdc00 + (code & 0x3ff);
        }
    }
    if (current)
        qjs_intl_collation_map_overlay(&builder->map, &overlay);
    status = qjs_intl_collation_match_init_overlay(&match, &builder->root.keys, utf16, at,
                                                   current ? &overlay : NULL, &builder->allocator);
    builder->allocator.free(builder->allocator.opaque, utf16);
    if (status)
        goto done;
    for (;;) {
        status = qjs_intl_collation_match_next(&match, &result, NULL, &present);
        if (status || !present)
            break;
        if (result.kind == QJS_INTL_COLLATION_MATCH_OVERLAY) {
            QJSIntlRelationMapping mapping;
            status = qjs_intl_collation_map_sequence(&builder->map, result.sequence, &mapping);
            if (!status)
                status = append(builder, out, &capacity, mapping.elements, mapping.count);
        } else if (result.kind == QJS_INTL_COLLATION_MATCH_ROOT) {
            status = append_root(builder, out, &capacity, result.mapping_id);
        } else if (result.kind == QJS_INTL_COLLATION_MATCH_SCALAR) {
            QJSIntlRelationElement ce;
            status = qjs_intl_root_input_implicit(&builder->root, result.scalar, &ce);
            if (!status)
                status = append(builder, out, &capacity, &ce, 1);
        } else {
            status = QJS_INTL_DATA_ERROR;
        }
        if (status)
            break;
    }
done:
    qjs_intl_collation_match_clear(&match);
    if (status)
        qjs_intl_relation_mapping_clear(&builder->allocator, out);
    return status;
}

QJSIntlStatus qjs_intl_collation_builder_lookup(QJSIntlCollationBuilder *builder,
                                               const uint32_t *codes, uint32_t count,
                                               QJSIntlRelationMapping *out)
{
    QJSIntlCompilerText text = {0};
    QJSIntlStatus status;
    if (out)
        *out = (QJSIntlRelationMapping){0};
    if (!active(builder))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!out)
        return finish(builder, QJS_INTL_INVALID_ARGUMENT);
    status = qjs_intl_compiler_text_nfd(&builder->allocator, codes, count, &text);
    if (!status)
        status = lookup(builder, &text, 1, out);
    qjs_intl_compiler_text_clear(&builder->allocator, &text);
    return finish(builder, status);
}

QJSIntlStatus qjs_intl_collation_builder_position(QJSIntlCollationBuilder *builder,
                                                 const QJSIntlRelationElement *elements, size_t count,
                                                 unsigned int before)
{
    QJSIntlStatus status;
    if (!active(builder))
        return QJS_INTL_INVALID_ARGUMENT;
    status = before > 3 ? QJS_INTL_INVALID_ARGUMENT :
        qjs_intl_relations_reset(&builder->relations, elements, count);
    if (!status)
        builder->before = before;
    return finish(builder, status);
}

static QJSIntlStatus compare_position(const QJSIntlCollationBuilder *builder,
                                       const QJSIntlRelationElement *a,
                                       const QJSIntlRelationElement *b, int *order)
{
    unsigned int i;
    *order = 0;
    for (i = 0; i < 4; i++) {
        QJSIntlStatus status = qjs_intl_weight_compare(&builder->relations.axes[i],
                                                       a->weights[i], b->weights[i], order);
        if (status || *order)
            return status;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus position_class(const QJSIntlCollationBuilder *builder, QJSIntlRootReset kind,
                                     const QJSIntlRelationElement *ce, int *matches)
{
    uint32_t p = ce->weights[0], s = ce->weights[1], t = ce->weights[2];
    *matches = 0;
    if (kind == QJS_INTL_ROOT_LAST_REGULAR) {
        uint32_t base = builder->root.metadata.han_base;
        int left, right;
        QJSIntlStatus status = qjs_intl_weight_compare(&builder->relations.axes[0], p, base, &left);
        if (!status)
            status = qjs_intl_weight_compare(&builder->relations.axes[0], p, base + 1, &right);
        if (status)
            return status;
        /* Include primaries tailored before the first real Han root too.
           Their original provenance is Han, but their position is in this gap. */
        *matches = left >= 0 && right < 0;
    } else if (!p) {
        if (kind == QJS_INTL_ROOT_FIRST_PRIMARY_IGNORABLE || kind == QJS_INTL_ROOT_LAST_PRIMARY_IGNORABLE)
            *matches = s != 0;
        else if (kind == QJS_INTL_ROOT_FIRST_SECONDARY_IGNORABLE || kind == QJS_INTL_ROOT_LAST_SECONDARY_IGNORABLE)
            *matches = !s && t;
        else
            *matches = !s && !t;
    }
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_builder_logical(QJSIntlCollationBuilder *builder,
                                                QJSIntlRootReset kind, unsigned int before)
{
    QJSIntlRelationElement best;
    QJSIntlStatus status;
    unsigned int role;
    uint32_t sequence;
    if (!active(builder))
        return QJS_INTL_INVALID_ARGUMENT;
    if (before > 3)
        return finish(builder, QJS_INTL_INVALID_ARGUMENT);
    switch (kind) {
    case QJS_INTL_ROOT_FIRST_PRIMARY_IGNORABLE:
    case QJS_INTL_ROOT_LAST_PRIMARY_IGNORABLE:
    case QJS_INTL_ROOT_FIRST_SECONDARY_IGNORABLE:
    case QJS_INTL_ROOT_LAST_SECONDARY_IGNORABLE:
    case QJS_INTL_ROOT_LAST_TERTIARY_IGNORABLE:
    case QJS_INTL_ROOT_LAST_REGULAR:
        break;
    default:
        return finish(builder, QJS_INTL_UNSUPPORTED);
    }
    status = qjs_intl_root_input_reset(&builder->root, kind, &role, &best);
    if (status)
        return finish(builder, status);
    /* This arena intentionally retains established ordering positions. Removing
       a mapping does not undo its rule position. Live-terminal iteration serves
       packing instead; it would change reset-before ordering here. Keep source
       seeds too: an empty SI band has a persistent constructed baseline. */
    for (sequence = 0; sequence < builder->map.sequence_count; sequence++) {
        QJSIntlRelationMapping mapping;
        size_t i;
        status = qjs_intl_collation_map_sequence(&builder->map, sequence, &mapping);
        if (status)
            return finish(builder, status);
        for (i = 0; i < mapping.count; i++) {
            int matches, order;
            status = position_class(builder, kind, mapping.elements + i, &matches);
            if (status)
                return finish(builder, status);
            if (!matches)
                continue;
            /* Compare from the current extremum: append-ordered long chains
               then advance to their neighbour instead of walking their tail. */
            status = compare_position(builder, &best, mapping.elements + i, &order);
            if (status)
                return finish(builder, status);
            if ((kind % 2 == 0 && order < 0) || (kind % 2 != 0 && order > 0))
                best = mapping.elements[i];
        }
    }
    return qjs_intl_collation_builder_position(builder, &best, 1, before);
}

QJSIntlStatus qjs_intl_collation_builder_reset(QJSIntlCollationBuilder *builder,
                                              const uint32_t *codes, uint32_t count, unsigned int before)
{
    QJSIntlRelationMapping mapping = {0};
    QJSIntlStatus status;
    if (!active(builder))
        return QJS_INTL_INVALID_ARGUMENT;
    status = count ? qjs_intl_collation_builder_lookup(builder, codes, count, &mapping) :
        QJS_INTL_INVALID_ARGUMENT;
    if (!status)
        status = qjs_intl_collation_builder_position(builder, mapping.elements, mapping.count, before);
    qjs_intl_relation_mapping_clear(&builder->allocator, &mapping);
    return finish(builder, status);
}

QJSIntlStatus qjs_intl_collation_builder_relate(QJSIntlCollationBuilder *builder, unsigned int strength,
                                               const uint32_t *prefix, uint32_t prefix_count,
                                               const uint32_t *codes, uint32_t count,
                                               const uint32_t *extension, uint32_t extension_count)
{
    QJSIntlCompilerText before = {0}, text = {0}, after = {0};
    QJSIntlRelationMapping root = {0}, extra = {0}, mapping = {0};
    QJSIntlRelationCase *cases = NULL;
    QJSIntlStatus status;
    uint32_t sequence;
    size_t i;
    if (!active(builder))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!count || !codes || (prefix_count && !prefix) || strength > 4 ||
        (builder->before && strength != builder->before))
        return finish(builder, QJS_INTL_INVALID_ARGUMENT);
    if (prefix_count && (!unicode_nfc_boundary_before(prefix[0]) ||
                         !unicode_nfc_boundary_before(codes[0])))
        return finish(builder, QJS_INTL_DATA_ERROR);
    status = qjs_intl_compiler_text_nfd(&builder->allocator, prefix, prefix_count, &before);
    if (status)
        goto done;
    status = qjs_intl_compiler_text_nfd(&builder->allocator, codes, count, &text);
    if (status)
        goto done;
    status = qjs_intl_compiler_text_nfd(&builder->allocator, extension, extension_count, &after);
    if (status)
        goto done;
    status = lookup(builder, &text, 0, &root);
    if (status)
        goto done;
    status = lookup(builder, &after, 1, &extra);
    if (status)
        goto done;
    if (root.count > SIZE_MAX / sizeof(*cases)) {
        status = QJS_INTL_OVERFLOW;
        goto done;
    }
    if (root.count) {
        cases = builder->allocator.malloc(builder->allocator.opaque, root.count * sizeof(*cases));
        if (!cases) {
            status = QJS_INTL_NO_MEMORY;
            goto done;
        }
        /* Case is immutable-root metadata, never inferred from an allocated
           tertiary ID or altered by an earlier current-map equality rule. */
        for (i = 0; i < root.count; i++) {
            const QJSIntlRelationElement *ce = root.elements + i;
            cases[i] = (QJSIntlRelationCase){!!ce->weights[0], ce->case_value};
        }
    }
    status = qjs_intl_relations_apply(&builder->relations, strength, builder->before != 0,
                                      cases, root.count, extra.elements, extra.count, &mapping);
    if (status)
        goto done;
    status = qjs_intl_collation_map_put(&builder->map, before.codes, before.count,
                                       text.codes, text.count, mapping.elements, mapping.count, &sequence);
    if (!status)
        builder->before = 0;
done:
    if (cases)
        builder->allocator.free(builder->allocator.opaque, cases);
    qjs_intl_relation_mapping_clear(&builder->allocator, &root);
    qjs_intl_relation_mapping_clear(&builder->allocator, &extra);
    qjs_intl_relation_mapping_clear(&builder->allocator, &mapping);
    qjs_intl_compiler_text_clear(&builder->allocator, &before);
    qjs_intl_compiler_text_clear(&builder->allocator, &text);
    qjs_intl_compiler_text_clear(&builder->allocator, &after);
    return finish(builder, status);
}
