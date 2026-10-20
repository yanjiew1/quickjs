/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COMMON_TIMEZONE_SYNTAX_H
#define QJS_INTL_COMMON_TIMEZONE_SYNTAX_H
#include "types.h"

/* Source identity grammar, not ECMA primary-zone canonicalization. */
static inline int qjs_intl_timezone_source_identity_valid(QJSIntlBytes text, int metazone)
{
    size_t i;
    unsigned int segment = 0;
    if (!text.length)
        return 0;
    for (i = 0; i < text.length; i++) {
        unsigned int c = (uint8_t)text.data[i];
        int letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (metazone && !i && !letter)
            return 0;
        if (letter || (c >= '0' && c <= '9') || c == '_' || (!metazone && (c == '+' || c == '.' || c == '-')))
            segment = 1;
        else if (!metazone && c == '/' && segment)
            segment = 0;
        else
            return 0;
    }
    return segment;
}
#endif
