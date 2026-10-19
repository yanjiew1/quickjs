/* Literal two-operand templates shared by native Intl consumers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "pair-pattern.h"
#include <string.h>

QJSIntlStatus qjs_intl_pair_pattern_parse(QJSIntlBytes text,
                                        QJSIntlPairPattern *out)
{
    size_t i, positions[2] = {0};
    unsigned int seen = 0, first = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!text.data && text.length) || text.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < text.length; i++) {
        unsigned int operand;
        if (text.data[i] == '}')
            return QJS_INTL_DATA_ERROR;
        if (text.data[i] != '{')
            continue;
        if (text.length - i < 3 || text.data[i + 2] != '}' ||
            (text.data[i + 1] != '0' && text.data[i + 1] != '1'))
            return QJS_INTL_DATA_ERROR;
        operand = text.data[i + 1] - '0';
        if (seen & (1u << operand))
            return QJS_INTL_DATA_ERROR;
        if (!seen)
            first = operand;
        seen |= 1u << operand;
        positions[operand] = i;
        i += 2;
    }
    if (seen != 3)
        return QJS_INTL_UNSUPPORTED;
    out->prefix = (QJSIntlBytes){text.data, positions[first]};
    out->separator = (QJSIntlBytes){text.data + positions[first] + 3,
                                    positions[1 - first] - positions[first] - 3};
    out->suffix = (QJSIntlBytes){text.data + positions[1 - first] + 3,
                                 text.length - positions[1 - first] - 3};
    out->first = first;
    return QJS_INTL_OK;
}

