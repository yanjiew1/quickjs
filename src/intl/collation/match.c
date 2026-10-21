/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "match.h"
#include "../native/wire.h"
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include "../../unicode/utf16.h"
#include <string.h>

/* CCC occupies the unused high byte; normalization itself receives plain code
   points. Keep the original normalized sequence intact for prefix lookback. */
#define SCALAR_MASK QJS_INTL_COLLATION_SCALAR_MASK

typedef struct NormalizationAllocator {
    const QJSIntlAllocator *allocator;
    QJSIntlStatus failure;
} NormalizationAllocator;

static void *normalize_realloc(void *opaque, void *ptr, size_t size)
{
    NormalizationAllocator *state = opaque;
    void *result;
    if (!size) {
        if (ptr)
            state->allocator->free(state->allocator->opaque, ptr);
        return NULL;
    }
    /* unicode_normalize returns an int code-point count. Bound capacity before
       that conversion, independently of the host's size_t width. */
#if SIZE_MAX > UINT32_MAX
    if (size > (size_t)INT_MAX * sizeof(uint32_t)) {
        state->failure = QJS_INTL_OVERFLOW;
        return NULL;
    }
#endif
    if (ptr)
        result = state->allocator->realloc(state->allocator->opaque, ptr, size);
    else
        result = state->allocator->malloc(state->allocator->opaque, size);
    if (!result)
        state->failure = QJS_INTL_NO_MEMORY;
    return result;
}

void qjs_intl_collation_match_clear(QJSIntlCollationMatch *it)
{
    if (!it)
        return;
    if (it->text)
        it->allocator.free(it->allocator.opaque, it->text);
    if (it->indexes)
        it->allocator.free(it->allocator.opaque, it->indexes);
    memset(it, 0, sizeof(*it));
}

QJSIntlStatus qjs_intl_collation_match_init(QJSIntlCollationMatch *it,
                                           const QJSIntlCollationContexts *data,
                                           const uint16_t *input, size_t length,
                                           const QJSIntlAllocator *allocator)
{
    return qjs_intl_collation_match_init_overlay(it, data, input, length, NULL, allocator);
}

QJSIntlStatus qjs_intl_collation_match_init_overlay(QJSIntlCollationMatch *it,
                                                   const QJSIntlCollationContexts *data,
                                                   const uint16_t *input, size_t length,
                                                   const QJSIntlCollationOverlay *overlay,
                                                   const QJSIntlAllocator *allocator)
{
    NormalizationAllocator adapter;
    QJSIntlStatus status;
    uint32_t count = 0, i, end;
    uint32_t *decoded;
    size_t position = 0;
    uint64_t bytes;
    int normalized;
    if (!it)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(it, 0, sizeof(*it));
    if (!allocator || !allocator->malloc || !allocator->realloc || !allocator->free ||
        (!input && length) || !data)
        return QJS_INTL_INVALID_ARGUMENT;
    if (overlay && (!overlay->longest_prefix || !overlay->begin || !overlay->step ||
                    !overlay->terminal || !overlay->has_children ||
                    !overlay->root_context_suppressed))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!data->section.bytes)
        return QJS_INTL_UNSUPPORTED;
    it->allocator = *allocator;
    it->data = *data;
    if (overlay) {
        it->overlay = *overlay;
        it->has_overlay = 1;
    }
    if (length > SIZE_MAX / sizeof(*input) || (uint64_t)length > (uint64_t)INT_MAX * 2)
        return QJS_INTL_OVERFLOW;
    while (position < length) {
        unicode_utf16_next(input, length, &position);
        if (count == INT_MAX)
            return QJS_INTL_OVERFLOW;
        count++;
    }
    if (!count) {
        it->ready = 1;
        return QJS_INTL_OK;
    }
    bytes = (uint64_t)count * sizeof(*decoded);
    if (bytes > SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    decoded = allocator->malloc(allocator->opaque, (size_t)bytes);
    if (!decoded)
        return QJS_INTL_NO_MEMORY;
    position = 0;
    for (i = 0; i < count; i++)
        decoded[i] = unicode_utf16_next(input, length, &position);
    adapter = (NormalizationAllocator){ allocator, QJS_INTL_OK };
    normalized = unicode_normalize(&it->text, decoded, count, UNICODE_NFD,
                                   &adapter, normalize_realloc);
    allocator->free(allocator->opaque, decoded);
    if (normalized < 0) {
        /* An arithmetic error can precede the allocator callback. DynBuf's
           persistent error propagation also covers those failures. */
        status = adapter.failure ? adapter.failure : QJS_INTL_OVERFLOW;
        goto fail;
    }
    it->length = normalized;
    bytes = ((uint64_t)it->length * 2 + 1) * sizeof(uint32_t);
    if (bytes > SIZE_MAX) {
        status = QJS_INTL_OVERFLOW;
        goto fail;
    }
    it->indexes = allocator->malloc(allocator->opaque, (size_t)bytes);
    if (!it->indexes) {
        status = QJS_INTL_NO_MEMORY;
        goto fail;
    }
    it->groups = it->indexes + it->length + 1;
    for (i = 0; i < it->length; i++) {
        uint32_t code = it->text[i];
        it->text[i] = code | (uint32_t)unicode_get_cc(code) << 24;
        it->indexes[i] = i;
    }
    it->indexes[it->length] = it->length;
    for (i = 0; i < it->length; i = end) {
        uint32_t ccc = it->text[i] >> 24, j;
        end = i + 1;
        if (ccc)
            while (end < it->length && (it->text[end] >> 24) == ccc)
                end++;
        for (j = i; j < end; j++)
            it->groups[j] = end;
    }
    it->ready = 1;
    return QJS_INTL_OK;
fail:
    qjs_intl_collation_match_clear(it);
    return status;
}

/* Committed positions form monotone successor chains. Iterative compression
   remains safe even after a contraction deletes a very long initial run. */
static uint32_t successor(QJSIntlCollationMatch *it, uint32_t position)
{
    uint32_t root = position;
    while (it->indexes[root] != root)
        root = it->indexes[root];
    while (it->indexes[position] != position) {
        uint32_t next = it->indexes[position];
        it->indexes[position] = root;
        position = next;
    }
    return root;
}

static void consume(QJSIntlCollationMatch *it, uint32_t position)
{
    it->indexes[position] = successor(it, position + 1);
}

typedef struct RootCursor {
    uint32_t first, end, depth;
} RootCursor;

static void entry_at(const QJSIntlCollationMatch *it, uint32_t index,
                     QJSIntlCollationEntry *entry)
{
    qjs_intl_collation_root_entry_at(&it->data.root, index, entry);
}

static uint32_t key_at(const QJSIntlCollationEntry *entry, uint32_t depth)
{
    return qjs_intl_read_u32(entry->keys + (size_t)depth * 4);
}

static int root_step(const QJSIntlCollationMatch *it, RootCursor *cursor, uint32_t code)
{
    QJSIntlCollationEntry entry;
    uint32_t lo, hi, first;
    if (cursor->first == cursor->end)
        return 0;
    entry_at(it, cursor->first, &entry);
    if (entry.key_count == cursor->depth)
        cursor->first++;
    lo = cursor->first;
    hi = cursor->end;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        entry_at(it, mid, &entry);
        if (key_at(&entry, cursor->depth) < code)
            lo = mid + 1;
        else
            hi = mid;
    }
    first = lo;
    hi = cursor->end;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        entry_at(it, mid, &entry);
        if (key_at(&entry, cursor->depth) <= code)
            lo = mid + 1;
        else
            hi = mid;
    }
    cursor->first = first;
    cursor->end = lo;
    cursor->depth++;
    return first != lo;
}

static int source_context(const QJSIntlCollationMatch *it, uint32_t source,
                          QJSIntlCollationContext *context)
{
    uint32_t lo = 0, hi = it->data.section.count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        qjs_intl_collation_context_at(&it->data, mid, context);
        if (context->source_index < source)
            lo = mid + 1;
        else if (context->source_index > source)
            hi = mid;
        else
            return 1;
    }
    return 0;
}

static int root_full(const QJSIntlCollationMatch *it, const RootCursor *cursor,
                       uint32_t prefix, QJSIntlCollationEntry *entry)
{
    QJSIntlCollationContext context;
    int contextual;
    if (cursor->first == cursor->end)
        return 0;
    entry_at(it, cursor->first, entry);
    if (entry->key_count != cursor->depth)
        return 0;
    contextual = source_context(it, cursor->first, &context);
    if (!prefix)
        return !contextual;
    if (!contextual || context.before.key_count != prefix)
        return 0;
    *entry = context.mapping;
    return 1;
}

typedef struct MatchCursor {
    RootCursor root;
    uint32_t node, depth;
    int suppressed;
} MatchCursor;

static int cursor_step(const QJSIntlCollationMatch *it, MatchCursor *cursor, uint32_t code)
{
    int root = root_step(it, &cursor->root, code);
    if (cursor->node != QJS_INTL_COLLATION_NODE_NONE)
        cursor->node = it->overlay.step(it->overlay.opaque, cursor->node, code);
    cursor->depth++;
    return root || cursor->node != QJS_INTL_COLLATION_NODE_NONE;
}

static int cursor_full(const QJSIntlCollationMatch *it, const MatchCursor *cursor,
                       uint32_t prefix, QJSIntlCollationMatchResult *result)
{
    QJSIntlCollationEntry entry;
    uint32_t sequence;
    int found = 0;
    if (root_full(it, &cursor->root, prefix, &entry) &&
        (!cursor->suppressed || (!prefix && entry.key_count == 1))) {
        *result = (QJSIntlCollationMatchResult){.kind = QJS_INTL_COLLATION_MATCH_ROOT,
                                              .entry = entry};
        found = 1;
    }
    /* Both providers are at the same prefix/suffix key. Override only this
       exact terminal; a short overlay must not hide a longer root contraction. */
    if (cursor->node != QJS_INTL_COLLATION_NODE_NONE &&
        (sequence = it->overlay.terminal(it->overlay.opaque, cursor->node)) !=
        QJS_INTL_COLLATION_NODE_NONE) {
        *result = (QJSIntlCollationMatchResult){.kind = QJS_INTL_COLLATION_MATCH_OVERLAY,
                                              .sequence = sequence};
        found = 1;
    }
    return found;
}

static int cursor_continues(const QJSIntlCollationMatch *it, const MatchCursor *cursor)
{
    uint32_t count = cursor->root.end - cursor->root.first;
    if (count > 1)
        return 1;
    /* An overlay checkpoint need not have a matching root terminal. The sole
       remaining root row can therefore still be a longer descendant. */
    if (count == 1 && it->has_overlay) {
        QJSIntlCollationEntry entry;
        entry_at(it, cursor->root.first, &entry);
        if (entry.key_count > cursor->root.depth)
            return 1;
    }
    return cursor->node != QJS_INTL_COLLATION_NODE_NONE &&
        it->overlay.has_children(it->overlay.opaque, cursor->node);
}

/* Full matches are checkpoints; partial contiguous matches may continue beyond
   them, but failed speculation never changes consumption or the saved cursor. */
static int contiguous(QJSIntlCollationMatch *it, uint32_t start, uint32_t prefix,
                      MatchCursor cursor, MatchCursor *best_cursor,
                      QJSIntlCollationMatchResult *best)
{
    uint32_t at = start;
    int found = 0;
    while (at < it->length && cursor_step(it, &cursor, it->text[at] & SCALAR_MASK)) {
        QJSIntlCollationMatchResult result;
        if (cursor_full(it, &cursor, prefix, &result)) {
            *best = result;
            *best_cursor = cursor;
            found = 1;
        }
        at = successor(it, at + 1);
    }
    return found;
}

static int initial_match(QJSIntlCollationMatch *it, uint32_t start,
                         uint32_t *prefix_out, MatchCursor *cursor_out,
                         QJSIntlCollationMatchResult *result)
{
    uint32_t limit = start + 1, first = it->text[start] & SCALAR_MASK;
    int suppressed = it->has_overlay &&
        it->overlay.root_context_suppressed(it->overlay.opaque, first);
    for (;;) {
        uint32_t prefix = 0, i;
        MatchCursor cursor = {{0, it->data.root.section.count, 0},
                              QJS_INTL_COLLATION_NODE_NONE, 0, suppressed};
        /* Prefixes inspect original, contiguous NFD text, including text which
           previously participated in an overlapping contraction. */
        for (i = 0; !suppressed && i < it->data.section.count; i++) {
            QJSIntlCollationContext context;
            uint32_t count, j;
            qjs_intl_collation_context_at(&it->data, i, &context);
            count = context.before.key_count;
            if (count <= prefix || count >= limit || count > start ||
                key_at(&context.mapping, 0) != first)
                continue;
            for (j = 0; j < count; j++)
                if (key_at(&context.before, j) != (it->text[start - count + j] & SCALAR_MASK))
                    break;
            if (j == count)
                prefix = count;
        }
        if (it->has_overlay) {
            uint32_t overlay_prefix = it->overlay.longest_prefix(it->overlay.opaque,
                                                                 it->text, start, limit);
            if (overlay_prefix > prefix)
                prefix = overlay_prefix;
            cursor.node = it->overlay.begin(it->overlay.opaque, first,
                                            it->text + start - prefix, prefix);
        }
        for (i = start - prefix; i < start; i++)
            root_step(it, &cursor.root, it->text[i] & SCALAR_MASK);
        if (contiguous(it, start, prefix, cursor, cursor_out, result)) {
            *prefix_out = prefix;
            return 1;
        }
        /* No fallback terminal is synthesized inside this prefix's cursor.
           In particular, ordinary c cannot seed skipping for a lone p|cd. */
        if (!prefix)
            return 0;
        limit = prefix;
    }
}

static void prepare(QJSIntlCollationMatch *it, uint32_t start,
                     QJSIntlCollationMatchResult *result)
{
    MatchCursor cursor;
    uint32_t prefix, i, at, blocked = 0;
    if (!initial_match(it, start, &prefix, &cursor, result)) {
        result->kind = QJS_INTL_COLLATION_MATCH_SCALAR;
        result->scalar = it->text[start] & SCALAR_MASK;
        consume(it, start);
        return;
    }
    at = start;
    for (i = 0; i < cursor.depth; i++) {
        uint32_t next = successor(it, at + 1);
        consume(it, at);
        at = next;
    }
    while (at < it->length && cursor_continues(it, &cursor)) {
        uint32_t ccc = it->text[at] >> 24;
        MatchCursor trial = cursor;
        QJSIntlCollationMatchResult extension;
        if (!ccc)
            break;
        /* Only a FULL single-character extension may cross skipped marks.
           Successful characters do not become blockers for later equal CCCs. */
        if (ccc > blocked && cursor_step(it, &trial, it->text[at] & SCALAR_MASK) &&
            cursor_full(it, &trial, prefix, &extension)) {
            cursor = trial;
            *result = extension;
            consume(it, at);
            at = successor(it, at + 1);
        } else {
            if (ccc > blocked)
                blocked = ccc;
            at = successor(it, it->groups[at]);
        }
    }
}

/* Match one source mapping, not one CE. Expansion decoding and implicit-weight
   construction belong to the consumer and cannot affect input consumption. */
QJSIntlStatus qjs_intl_collation_match_next(QJSIntlCollationMatch *it,
                                           QJSIntlCollationMatchResult *result,
                                           QJSIntlCollationNumber *number, int *present)
{
    uint32_t at;
    if (result)
        memset(result, 0, sizeof(*result));
    if (present)
        *present = 0;
    if (number)
        memset(number, 0, sizeof(*number));
    if (!it || !it->ready || !result || !present)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!it->length || (at = successor(it, 0)) == it->length)
        return QJS_INTL_OK;
    if (number && lre_decimal_value(it->text[at] & SCALAR_MASK) >= 0) {
        uint32_t first = at, end = at;
        do {
            consume(it, end++);
        } while (end < it->length &&
                 lre_decimal_value(it->text[end] & SCALAR_MASK) >= 0);
        /* Even an ignorable non-digit is a boundary. Do not use the CE
           stream to find runs, or remove zeros again at an ICU chunk cap. */
        while (first + 1 < end && !lre_decimal_value(it->text[first] & SCALAR_MASK))
            first++;
        number->digits = it->text + first;
        number->length = end - first;
        result->kind = QJS_INTL_COLLATION_MATCH_NUMBER;
    } else {
        prepare(it, at, result);
    }
    *present = 1;
    return QJS_INTL_OK;
}
