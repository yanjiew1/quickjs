/* Raw CE emission over the shared normalized-text/context matcher.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "elements.h"
#include <string.h>

void qjs_intl_collation_elements_clear(QJSIntlCollationElements *it)
{
    if (!it)
        return;
    qjs_intl_collation_match_clear(&it->match);
    memset(it, 0, sizeof(*it));
}

QJSIntlStatus qjs_intl_collation_elements_init(QJSIntlCollationElements *it,
                                              const QJSIntlCollationContexts *data,
                                              const uint16_t *input, size_t length,
                                              const QJSIntlAllocator *allocator)
{
    QJSIntlCollationKeyIndex keys;
    QJSIntlStatus status;
    if (!it)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(it, 0, sizeof(*it));
    status = qjs_intl_collation_contexts_keys(data, &keys);
    if (status)
        return status;
    it->data = *data;
    return qjs_intl_collation_match_init(&it->match, &keys, input, length, allocator);
}

static QJSIntlStatus elements_next(QJSIntlCollationElements *it,
                                   QJSIntlCollationElement *out,
                                   QJSIntlCollationNumber *number, int *present)
{
    uint32_t count;
    if (out)
        memset(out, 0, sizeof(*out));
    if (present)
        *present = 0;
    if (number)
        memset(number, 0, sizeof(*number));
    if (!it || !it->match.ready || !out || !present)
        return QJS_INTL_INVALID_ARGUMENT;
    count = it->use_implicit ? 2 : it->pending.element_count;
    if (it->pending_at == count) {
        QJSIntlCollationMatchResult result;
        QJSIntlStatus status = qjs_intl_collation_match_next(&it->match, &result, number, present);
        if (status || !*present || result.kind == QJS_INTL_COLLATION_MATCH_NUMBER)
            return status;
        it->use_implicit = result.kind == QJS_INTL_COLLATION_MATCH_SCALAR;
        if (it->use_implicit)
            qjs_intl_collation_implicit(result.scalar, it->implicit);
        else
            qjs_intl_collation_contexts_mapping(&it->data, result.mapping_id, &it->pending);
        it->pending_at = 0;
    }
    if (it->use_implicit)
        *out = it->implicit[it->pending_at++];
    else
        qjs_intl_collation_element_at(&it->pending, it->pending_at++, out);
    *present = 1;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_elements_next(QJSIntlCollationElements *it,
                                              QJSIntlCollationElement *out, int *present)
{
    return elements_next(it, out, NULL, present);
}

QJSIntlStatus qjs_intl_collation_elements_next_numeric(QJSIntlCollationElements *it,
                                                      QJSIntlCollationElement *out,
                                                      QJSIntlCollationNumber *number,
                                                      int *present)
{
    if (!number) {
        if (present)
            *present = 0;
        if (out)
            memset(out, 0, sizeof(*out));
        return QJS_INTL_INVALID_ARGUMENT;
    }
    return elements_next(it, out, number, present);
}
