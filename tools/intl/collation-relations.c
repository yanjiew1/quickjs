/* CLDR ordered relations: copied expansions, legal weak bands and case values.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-relations.h"
#include <string.h>

void qjs_intl_relation_mapping_clear(const QJSIntlAllocator *allocator,
                                     QJSIntlRelationMapping *mapping)
{
    if (!mapping)
        return;
    if (mapping->elements)
        allocator->free(allocator->opaque, mapping->elements);
    *mapping = (QJSIntlRelationMapping){0};
}

void qjs_intl_relations_clear(QJSIntlRelations *owner)
{
    unsigned int i;
    if (!owner)
        return;
    qjs_intl_relation_mapping_clear(&owner->allocator, &owner->cursor);
    for (i = 0; i < 4; i++)
        qjs_intl_weight_graph_clear(&owner->axes[i]);
    memset(owner, 0, sizeof(*owner));
}

QJSIntlStatus qjs_intl_relations_init(QJSIntlRelations *owner, const QJSIntlAllocator *allocator,
                                      const QJSIntlRelationRoots *roots)
{
    uint32_t maxima[4];
    QJSIntlStatus status;
    unsigned int i;
    if (!owner)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(owner, 0, sizeof(*owner));
    if (!allocator || !allocator->malloc || !allocator->realloc || !allocator->free ||
        !roots || !roots->secondary_common || !roots->tertiary_common ||
        roots->secondary_common >= roots->secondary_boundary ||
        roots->tertiary_common >= roots->tertiary_boundary ||
        roots->secondary_boundary > roots->secondary_max ||
        roots->tertiary_boundary > roots->tertiary_max)
        return QJS_INTL_INVALID_ARGUMENT;
    owner->allocator = *allocator;
    owner->roots = *roots;
    maxima[0] = roots->primary_max;
    maxima[1] = roots->secondary_max;
    maxima[2] = roots->tertiary_max;
    maxima[3] = 0;
    for (i = 0; i < 4; i++) {
        status = qjs_intl_weight_graph_init(&owner->axes[i], allocator, maxima[i]);
        if (status) {
            qjs_intl_relations_clear(owner);
            return status;
        }
    }
    owner->ready = 1;
    return QJS_INTL_OK;
}

static int mutable_owner(const QJSIntlRelations *owner)
{
    return owner && owner->ready && !owner->axes[0].frozen && !owner->axes[1].frozen &&
        !owner->axes[2].frozen && !owner->axes[3].frozen;
}

static QJSIntlStatus allocate_mapping(const QJSIntlAllocator *allocator, size_t count,
                                      QJSIntlRelationMapping *out)
{
    if (count > SIZE_MAX / sizeof(*out->elements))
        return QJS_INTL_OVERFLOW;
    out->elements = allocator->malloc(allocator->opaque, count * sizeof(*out->elements));
    if (!out->elements)
        return QJS_INTL_NO_MEMORY;
    out->count = count;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_relations_reset(QJSIntlRelations *owner,
                                       const QJSIntlRelationElement *elements, size_t count)
{
    QJSIntlRelationMapping next = {0};
    QJSIntlStatus status;
    if (!mutable_owner(owner) || (!elements && count))
        return QJS_INTL_INVALID_ARGUMENT;
    status = allocate_mapping(&owner->allocator, count ? count : 1, &next);
    if (status)
        return status;
    if (count)
        memcpy(next.elements, elements, count * sizeof(*elements));
    else
        memset(next.elements, 0, sizeof(*next.elements));
    qjs_intl_relation_mapping_clear(&owner->allocator, &owner->cursor);
    owner->cursor = next;
    return QJS_INTL_OK;
}

/* Case comes from immutable root mappings, not inherited or allocated T ranks.
   Only primary CEs participate in the distribution; extension CEs are excluded. */
static void assign_case(QJSIntlRelationMapping *mapping,
                         const QJSIntlRelationCase *root, size_t root_count)
{
    size_t i, primary_count = 0, primary_at = 0, root_at = 0;
    for (i = 0; i < mapping->count; i++)
        primary_count += mapping->elements[i].weights[0] != 0;
    for (i = 0; i < mapping->count; i++) {
        QJSIntlRelationElement *ce = mapping->elements + i;
        unsigned int value = QJS_INTL_RELATION_LOWER;
        if (ce->weights[0]) {
            while (root_at < root_count && !root[root_at].primary)
                root_at++;
            if (root_at < root_count) {
                value = root[root_at++].case_value;
                if (++primary_at == primary_count) {
                    /* The final primary absorbs any remaining root primaries;
                       mixed upper/lower is distinct from either homogeneous case. */
                    while (root_at < root_count) {
                        if (root[root_at].primary &&
                            root[root_at].case_value != value)
                            value = QJS_INTL_RELATION_MIXED;
                        root_at++;
                    }
                }
            }
        } else if (!ce->weights[1] && ce->weights[2]) {
            value = QJS_INTL_RELATION_UPPER;
        }
        ce->case_value = value;
    }
}

static size_t strong_length(const QJSIntlRelationMapping *mapping, unsigned int strength)
{
    size_t length = mapping->count;
    while (length) {
        unsigned int i;
        for (i = 0; i < strength; i++)
            if (mapping->elements[length - 1].weights[i])
                return length;
        length--;
    }
    return 0;
}

QJSIntlStatus qjs_intl_relations_apply(QJSIntlRelations *owner, unsigned int strength, int before,
                                       const QJSIntlRelationCase *root, size_t root_count,
                                       const QJSIntlRelationElement *extension, size_t extension_count,
                                       QJSIntlRelationMapping *out)
{
    QJSIntlRelationMapping next = {0}, mapping = {0};
    QJSIntlStatus status;
    QJSIntlRelationElement *last;
    uint32_t reference, id;
    size_t kept, count, at;
    unsigned int i;
    if (out)
        *out = (QJSIntlRelationMapping){0};
    if (!out || !mutable_owner(owner) || !owner->cursor.count || strength > 4 ||
        (before && (!strength || strength > 3)) || (!root && root_count) ||
        (!extension && extension_count))
        return QJS_INTL_INVALID_ARGUMENT;
    for (at = 0; at < root_count; at++)
        if (root[at].primary > 1 || root[at].case_value > QJS_INTL_RELATION_UPPER)
            return QJS_INTL_INVALID_ARGUMENT;
    kept = strength ? strong_length(&owner->cursor, strength) : owner->cursor.count;
    count = kept ? kept : 1;
    if (extension_count > SIZE_MAX - count)
        return QJS_INTL_OVERFLOW;
    status = allocate_mapping(&owner->allocator, count, &next);
    if (status)
        goto fail;
    status = allocate_mapping(&owner->allocator, count + extension_count, &mapping);
    if (status)
        goto fail;
    if (kept)
        memcpy(next.elements, owner->cursor.elements, kept * sizeof(*next.elements));
    else
        memset(next.elements, 0, sizeof(*next.elements));
    last = next.elements + count - 1;
    if (strength) {
        reference = last->weights[strength - 1];
        if (before && !reference) {
            status = QJS_INTL_DATA_ERROR;
            goto fail;
        }
        /* A newly nonzero weak weight belongs above ALL ordinary weights,
           not in gap zero. This also handles the root's empty tertiary-only band. */
        if (!reference && strength == 2)
            reference = owner->roots.secondary_boundary;
        else if (!reference && strength == 3)
            reference = owner->roots.tertiary_boundary;
        status = qjs_intl_weight_insert(&owner->axes[strength - 1], reference, before,
            &(QJSIntlWeightParents){strength > 1 ? last->weights[0] : 0,
                                   strength > 2 ? last->weights[1] : 0}, &id);
        if (status)
            goto fail;
        last->weights[strength - 1] = id;
        /* All potentially failing operations precede the graph mutation. */
        for (i = strength; i < 4; i++)
            last->weights[i] = i == 1 ? owner->roots.secondary_common :
                i == 2 ? owner->roots.tertiary_common : 0;
    }
    assign_case(&next, root, root_count);
    memcpy(mapping.elements, next.elements, count * sizeof(*next.elements));
    if (extension_count)
        memcpy(mapping.elements + count, extension, extension_count * sizeof(*extension));
    qjs_intl_relation_mapping_clear(&owner->allocator, &owner->cursor);
    owner->cursor = next;
    *out = mapping;
    return QJS_INTL_OK;
fail:
    qjs_intl_relation_mapping_clear(&owner->allocator, &next);
    qjs_intl_relation_mapping_clear(&owner->allocator, &mapping);
    return status;
}
