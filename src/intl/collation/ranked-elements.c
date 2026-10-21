/* Keep logical rank32 emission separate from the raw16 diagnostic reader.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "ranked-elements.h"
#include <string.h>

void qjs_intl_collation_ranked_elements_clear(QJSIntlCollationRankedElements *it)
{
    if (!it)
        return;

    qjs_intl_collation_match_clear(&it->match);
    memset(it, 0, sizeof(*it));
}

QJSIntlStatus qjs_intl_collation_ranked_elements_init(QJSIntlCollationRankedElements *it,
                                                      const QJSIntlCompiledCollationProgram *program,
                                                      const uint16_t *input, size_t length, int numeric,
                                                      const QJSIntlAllocator *allocator)
{
    QJSIntlCollationOverlay overlay;
    QJSIntlStatus status;

    if (!it)
        return QJS_INTL_INVALID_ARGUMENT;

    memset(it, 0, sizeof(*it));
    if (!program || !program->ready)
        return QJS_INTL_INVALID_ARGUMENT;

    it->program = *program;
    it->numeric = !!numeric;

    /* The callback owner is inside the final iterator, not the caller's
       descriptor or a temporary that would disappear after initialization. */
    status = qjs_intl_collation_trie_overlay(&it->program.trie, &overlay);
    if (!status)
        status = qjs_intl_collation_match_init_overlay(&it->match, &it->program.root.keys,
                                                      input, length, &overlay, allocator);
    if (status)
        qjs_intl_collation_ranked_elements_clear(it);

    return status;
}

QJSIntlStatus qjs_intl_collation_ranked_elements_next(QJSIntlCollationRankedElements *it,
                                                      QJSIntlCollationRankedToken *out, int *present)
{
    QJSIntlStatus status;

    if (out)
        memset(out, 0, sizeof(*out));
    if (present)
        *present = 0;

    if (!it || !it->match.ready || !out || !present)
        return QJS_INTL_INVALID_ARGUMENT;

    while (!it->pending_count) {
        QJSIntlCollationMatchResult result;
        int matched;

        status = qjs_intl_collation_match_next(&it->match, &result,
                                              it->numeric ? &out->number : NULL, &matched);
        if (status || !matched)
            return status;

        if (result.kind == QJS_INTL_COLLATION_MATCH_NUMBER) {
            *present = 1;
            return QJS_INTL_OK;
        }

        if (result.kind == QJS_INTL_COLLATION_MATCH_SCALAR) {
            status = qjs_intl_compiled_collation_implicit(&it->program, result.scalar, &out->element);
            if (!status)
                *present = 1;
            return status;
        }

        it->pending_overlay = result.kind == QJS_INTL_COLLATION_MATCH_OVERLAY;
        if (it->pending_overlay)
            status = qjs_intl_compiled_collation_mapping(&it->program, result.sequence,
                                                        &it->pending_at, &it->pending_count);
        else
            status = qjs_intl_collation_base_mapping(&it->program.root, result.mapping_id,
                                                     &it->pending_at, &it->pending_count);
        if (status)
            return status;

        /* Empty overlay output is a successful consumed mapping. It must not
           fall back to root data or masquerade as exhaustion before later text. */
    }

    if (it->pending_overlay)
        status = qjs_intl_compiled_collation_element(&it->program, it->pending_at, &out->element);
    else
        status = qjs_intl_compiled_collation_root_element(&it->program, it->pending_at, &out->element);

    if (!status) {
        it->pending_at++;
        it->pending_count--;
        *present = 1;
    }

    return status;
}
