/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collation-reorder.h"
#include <string.h>

#define OTHERS UINT32_MAX

void qjs_intl_collation_reorder_clear(QJSIntlCollationReorder *owner)
{
    if (!owner)
        return;
    if (owner->ranks)
        owner->allocator.free(owner->allocator.opaque, owner->ranks);
    memset(owner, 0, sizeof(*owner));
}

QJSIntlStatus qjs_intl_collation_reorder_init(QJSIntlCollationReorder *owner, const QJSIntlRootInput *root,
                                             const QJSIntlAllocator *allocator)
{
    static const char *const names[] = {"space", "punct", "symbol", "currency", "digit"};
    uint32_t i;
    if (!owner)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(owner, 0, sizeof(*owner));
    if (!root || !root->fingerprint || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    owner->allocator = *allocator;
    owner->root = *root;
    owner->count = root->sections[QJS_INTL_ROOT_GROUPS].count;
    if (!owner->count)
        return QJS_INTL_DATA_ERROR;
    if ((uint64_t)owner->count * sizeof(*owner->ranks) > SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    for (i = 0; i < 5; i++) {
        QJSIntlStatus status = qjs_intl_root_input_group(root,
            (QJSIntlBytes){names[i], strlen(names[i])}, owner->core + i);
        if (status)
            return QJS_INTL_DATA_ERROR;
    }
    owner->ranks = allocator->malloc(allocator->opaque, (size_t)owner->count * sizeof(*owner->ranks));
    if (!owner->ranks)
        return QJS_INTL_NO_MEMORY;
    for (i = 0; i < owner->count; i++)
        owner->ranks[i] = i;
    owner->ready = 1;
    return QJS_INTL_OK;
}

/* Values are NUL-delimited scalar strings. Catalog names have at most eight
   ASCII characters, so malformed long tokens fail without allocating by length. */
static QJSIntlStatus next_token(const QJSIntlIRScalars *text, uint32_t *at, char token[9], size_t *length)
{
    size_t count = 0;
    while (*at < text->count) {
        uint32_t code = qjs_intl_ir_scalar(text, (*at)++);
        if (!code) {
            if (!count)
                return QJS_INTL_DATA_ERROR;
            token[count] = 0;
            *length = count;
            return QJS_INTL_OK;
        }
        if (code >= 0x80 || count == 8)
            return QJS_INTL_DATA_ERROR;
        token[count++] = (char)code;
    }
    return QJS_INTL_DATA_ERROR;
}

static int script_code(const char *text, size_t length)
{
    size_t i;
    if (length != 4 || text[0] < 'A' || text[0] > 'Z')
        return 0;
    for (i = 1; i < length; i++) {
        if (text[i] < 'a' || text[i] > 'z')
            return 0;
    }
    return 1;
}

QJSIntlStatus qjs_intl_collation_reorder_apply(QJSIntlCollationReorder *owner,
                                              const QJSIntlIRInstruction *node)
{
    uint32_t *scratch, *requests, *used, *ranks, count = 0, at = 0, rank = 0, i;
    uint64_t words;
    QJSIntlStatus status = QJS_INTL_DATA_ERROR;
    int others = 0;
    if (!owner || !owner->ready || !node || node->kind != QJS_INTL_IR_SETTING || node->auxiliary ||
        !node->fields[0].bytes || !node->fields[0].count || !node->fields[1].bytes ||
        !node->fields[1].count || node->fields[2].count)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!qjs_intl_ir_ascii_equal(node->fields, "reorder", 0))
        return QJS_INTL_UNSUPPORTED;
    /* Scratch depends on the validated group catalog, never untrusted operand
       length. At most one explicit slot per group and one others pivot. */
    words = (uint64_t)owner->count * 3 + 1;
    if (words > SIZE_MAX / sizeof(*scratch))
        return QJS_INTL_OVERFLOW;
    scratch = owner->allocator.malloc(owner->allocator.opaque, (size_t)words * sizeof(*scratch));
    if (!scratch)
        return QJS_INTL_NO_MEMORY;
    requests = scratch;
    used = requests + owner->count + 1;
    ranks = used + owner->count;
    memset(used, 0, (size_t)owner->count * sizeof(*used));
    while (at < node->fields[1].count) {
        char token[9];
        size_t length;
        uint32_t group;
        if (count >= owner->count + 1)
            goto done;
        status = next_token(node->fields + 1, &at, token, &length);
        if (status)
            goto done;
        status = QJS_INTL_DATA_ERROR;
        if (!strcmp(token, "others") || !strcmp(token, "Zzzz")) {
            if (others)
                goto done;
            others = 1;
            requests[count++] = OTHERS;
            continue;
        }
        if (!strcmp(token, "Zyyy") || !strcmp(token, "Zinh"))
            goto done;
        status = qjs_intl_root_input_group(&owner->root, (QJSIntlBytes){token, length}, &group);
        if (status) {
            if (!script_code(token, length))
                status = QJS_INTL_DATA_ERROR;
            goto done;
        }
        status = QJS_INTL_DATA_ERROR;
        if (used[group]) /* Different aliases cannot split a shared group. */
            goto done;
        used[group] = 1;
        requests[count++] = group;
    }
    if (!others) {
        if (count > owner->count)
            goto done;
        requests[count++] = OTHERS;
    }
    /* Missing core codes precede the explicit list. Only unmentioned scripts
       (including unassigned) expand at others, in source group order. */
    for (i = 0; i < 5; i++) {
        uint32_t group = owner->core[i];
        if (!used[group]) {
            ranks[group] = rank++;
            used[group] = 2;
        }
    }
    for (i = 0; i < count; i++) {
        uint32_t group = requests[i];
        if (group == OTHERS) {
            for (group = 0; group < owner->count; group++) {
                if (!used[group]) {
                    ranks[group] = rank++;
                    used[group] = 2;
                }
            }
        } else {
            ranks[group] = rank++;
            used[group] = 2;
        }
    }
    if (rank != owner->count)
        goto done;
    memcpy(owner->ranks, ranks, (size_t)owner->count * sizeof(*ranks));
    status = QJS_INTL_OK;
done:
    owner->allocator.free(owner->allocator.opaque, scratch);
    return status;
}

QJSIntlStatus qjs_intl_collation_reorder_rank(const QJSIntlCollationReorder *owner, uint32_t group,
                                             uint32_t *rank)
{
    if (rank)
        *rank = 0;
    if (!owner || !owner->ready || !rank || group >= owner->count)
        return QJS_INTL_INVALID_ARGUMENT;
    *rank = owner->ranks[group];
    return QJS_INTL_OK;
}
