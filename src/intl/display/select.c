/* Native label alternatives; CLDR lateral inheritance precedes parent lookup.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "select.h"
#include <string.h>

QJSIntlStatus qjs_intl_name_select(const QJSIntlNameData *data, uint32_t locale,
                                   QJSIntlNameGroup group, QJSIntlNameStyle style, int standalone,
                                   QJSIntlBytes code, QJSIntlNameLabel *out)
{
    QJSIntlBytes alternatives[3] = {{0}}, empty = {0};
    unsigned int count = 0, blocked = 0, i;
    uint32_t head;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (unsigned int)style > QJS_INTL_NAME_NARROW ||
        (standalone != 0 && standalone != 1) || (standalone && group != QJS_INTL_NAME_SCRIPT) ||
        (!code.data && code.length))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_name_head(data, locale, group, &head);
    if (status)
        return status;
    if (style != QJS_INTL_NAME_LONG &&
        (group == QJS_INTL_NAME_LANGUAGE || group == QJS_INTL_NAME_SCRIPT ||
         group == QJS_INTL_NAME_REGION || group == QJS_INTL_NAME_CALENDAR))
        alternatives[count++] = (QJSIntlBytes){"short", 5};
    if (standalone)
        alternatives[count++] = (QJSIntlBytes){"stand-alone", 11};
    alternatives[count++] = empty;
    while (head) {
        QJSIntlNameNode node;
        status = qjs_intl_name_node(data, head, &node);
        if (status)
            return status;
        for (i = 0; i < count; i++) {
            QJSIntlNameEntry entry;
            if (blocked & (1u << i))
                continue;
            status = qjs_intl_name_lookup(data, head, code, alternatives[i], empty, empty, &entry);
            if (status == QJS_INTL_UNSUPPORTED)
                continue;
            if (status)
                return status;
            if (entry.blocked) {
                blocked |= 1u << i;
                continue;
            }
            out->entry = entry;
            out->origin_locale = node.origin_locale;
            out->found = 1;
            return QJS_INTL_OK;
        }
        head = node.parent;
    }
    return QJS_INTL_OK;
}
