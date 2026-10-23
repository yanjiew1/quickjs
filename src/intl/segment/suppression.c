/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "suppression.h"
#include <string.h>

typedef struct LiteralCursor {
    size_t start;
    uint32_t state;
} LiteralCursor;

enum { MATCH_END = 1, REMOVE_BREAK = 2 };

QJSIntlStatus qjs_intl_segment_suppression_apply(const QJSIntlAllocator *allocator,
                                                 const QJSIntlSegmentProgram *program,
                                                 const QJSIntlSegmentLiteralGraph *list,
                                                 const QJSIntlSegmentText *text,
                                                 QJSIntlSegmentScan *scan)
{
    LiteralCursor *cursors;
    size_t *furthest, count, capacity, bytes, extra, live = 0, i;
    size_t covered_until = 0, anchor = 0;
    uint8_t *marks;
    unsigned int space_phase = 0;
    QJSIntlStatus status = QJS_INTL_OK;
    void *scratch;
    if (!allocator || !allocator->malloc || !allocator->free || !program ||
        program->kind != QJS_INTL_SEGMENT_SENTENCE || !program->flags || !list ||
        !text || !text->offsets || !scan || !scan->breaks || !scan->rules ||
        text->count != scan->count || (text->count && (!text->scalars || !scan->classes)))
        return QJS_INTL_INVALID_ARGUMENT;
    if (!list->words || text->count < 2)
        return QJS_INTL_OK;
    if (!list->starts || !list->max_scalars || list->max_scalars > 256)
        return QJS_INTL_INVALID_ARGUMENT;
    if (text->count == SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    count = text->count + 1;
    capacity = list->max_scalars;
    bytes = capacity * sizeof(*cursors); /* Checked list maximum is at most 256. */
    if (count > (SIZE_MAX - bytes) / sizeof(*furthest))
        return QJS_INTL_OVERFLOW;
    extra = count * sizeof(*furthest);
    bytes += extra;
    if (count > SIZE_MAX - bytes)
        return QJS_INTL_OVERFLOW;
    bytes += count;
    scratch = allocator->malloc(allocator->opaque, bytes);
    if (!scratch)
        return QJS_INTL_NO_MEMORY;
    memset(scratch, 0, bytes);
    cursors = scratch;
    furthest = (size_t *)(cursors + capacity);
    marks = (uint8_t *)(furthest + count);

    /* Start at every original scalar position. Each live cursor advances once
       per scalar and expires at the longest key length, so scratch/work cannot
       grow with an unbounded number of overlapping starts. */
    for (i = 0; i < text->count; i++) {
        size_t j, kept = 0;
        if (scan->classes[i] >= program->class_count) {
            status = QJS_INTL_INVALID_ARGUMENT;
            goto done;
        }
        /* A complete literal must not borrow context across a hard paragraph
           boundary, even if a future source list contains such a key. */
        if (program->flags[scan->classes[i]] & QJS_INTL_SEGMENT_CONTEXT_SEPARATOR) {
            live = 0;
            continue;
        }
        cursors[live++] = (LiteralCursor){i, list->root};
        for (j = 0; j < live; j++) {
            LiteralCursor cursor = cursors[j];
            uint32_t next, value;
            status = qjs_intl_segment_literal_graph_step(list, cursor.state, text->scalars[i], &next);
            if (status == QJS_INTL_UNSUPPORTED)
                continue;
            if (status)
                goto done;
            status = qjs_intl_segment_literal_graph_value(list, next, &value);
            if (!status) {
                if (value) {
                    status = QJS_INTL_DATA_ERROR;
                    goto done;
                }
                marks[i + 1] |= MATCH_END;
                furthest[cursor.start] = i + 1;
            } else if (status != QJS_INTL_UNSUPPORTED) {
                goto done;
            }
            if (i + 1 - cursor.start < capacity)
                cursors[kept++] = (LiteralCursor){cursor.start, next};
        }
        live = kept;
    }

    /* A full multi-part entry can cover internal soft candidates. A monotonic
       interval sweep avoids walking each match's range again. Separately track
       the original SATerm anchor for candidates after its allowed tail. */
    for (i = 1; i < text->count; i++) {
        unsigned int flags;
        QJSIntlSegmentRule rule;
        if (scan->classes[i - 1] >= program->class_count) {
            status = QJS_INTL_INVALID_ARGUMENT;
            goto done;
        }
        flags = program->flags[scan->classes[i - 1]];
        if (flags & QJS_INTL_SEGMENT_CONTEXT_SEPARATOR) {
            anchor = 0;
            covered_until = 0;
        } else if (flags & QJS_INTL_SEGMENT_CONTEXT_IGNORED) {
            /* Preserve invalid state too: ignored text cannot cross a barrier. */
        } else if (flags & QJS_INTL_SEGMENT_CONTEXT_TERM) {
            anchor = i;
            space_phase = 0;
        } else if (anchor && (flags & QJS_INTL_SEGMENT_CONTEXT_SPACE)) {
            space_phase = 1;
        } else if (!(anchor && !space_phase && (flags & QJS_INTL_SEGMENT_CONTEXT_CLOSE))) {
            anchor = 0;
        }
        if (furthest[i - 1] > covered_until)
            covered_until = furthest[i - 1];
        if (!scan->breaks[i] || scan->rules[i] == QJS_INTL_SEGMENT_DEFAULT_RULE)
            continue;
        status = qjs_intl_segment_program_rule(program, scan->rules[i], &rule);
        if (status)
            goto done;
        if (rule.role == QJS_INTL_SEGMENT_SOFT_SENTENCE &&
            (covered_until > i || (anchor && (marks[anchor] & MATCH_END))))
            marks[i] |= REMOVE_BREAK;
    }
    /* Publication is the only mutation of the owned scan. In particular, a
       failed lookup or physical allocation cannot leave partial removals. */
    for (i = 1; i < text->count; i++)
        if (marks[i] & REMOVE_BREAK)
            scan->breaks[i] = 0;
    status = QJS_INTL_OK;
done:
    allocator->free(allocator->opaque, scratch);
    return status;
}
