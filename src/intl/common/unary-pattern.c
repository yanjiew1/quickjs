/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "unary-pattern.h"
#include <string.h>

QJSIntlStatus qjs_intl_unary_pattern_parse(QJSIntlBytes text, QJSIntlUnaryPattern *out)
{
    size_t i, number = 0;
    int seen = 0;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (!text.data && text.length) || text.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < text.length; i++) {
        if (text.data[i] == '}')
            return QJS_INTL_DATA_ERROR;
        if (text.data[i] != '{')
            continue;
        if (seen || text.length - i < 3 || text.data[i + 1] != '0' || text.data[i + 2] != '}')
            return QJS_INTL_DATA_ERROR;
        seen = 1;
        number = i;
        i += 2;
    }
    if (!seen)
        return QJS_INTL_UNSUPPORTED;
    out->prefix = (QJSIntlBytes){text.data, number};
    out->suffix = (QJSIntlBytes){text.data + number + 3, text.length - number - 3};
    return QJS_INTL_OK;
}
