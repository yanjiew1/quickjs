/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "format-builder.h"
#include "../../cutils/cutils.h"
#include <limits.h>

static int next(QJSIntlBytes text, size_t *at)
{
    const uint8_t *end;
    int c;
    if (*at == text.length || text.length - *at > INT_MAX)
        return -1;
    c = unicode_from_utf8((const uint8_t *)text.data + *at, (int)(text.length - *at), &end);
    if (c <= 0 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
        return -1;
    *at = (const char *)end - text.data;
    return c;
}

static int white_space(int c)
{
    return c == 0x20 || (c >= 0x9 && c <= 0xd) || c == 0x85 || c == 0xa0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
           c == 0x205f || c == 0x3000;
}

/* Append scalar text, optionally tracking Unicode nonspace boundaries for
   the caller's semantic affix classification. */
QJSIntlStatus qjs_intl_format_builder_text(QJSIntlFormatBuilder *builder, QJSIntlBytes text,
                                           size_t *first, size_t *last)
{
    size_t at = 0;
    if (!text.data && text.length)
        return QJS_INTL_INVALID_ARGUMENT;
    while (at < text.length) {
        int c = next(text, &at);
        size_t width;
        if (c < 0)
            return QJS_INTL_DATA_ERROR;
        width = c > 0xffff ? 2 : 1;
        if (width > builder->limit - builder->length)
            return QJS_INTL_OVERFLOW;
        if (first && !white_space(c)) {
            if (*first == SIZE_MAX)
                *first = builder->length;
            *last = builder->length + width;
        }
        if (builder->result.text) {
            uint16_t *out = builder->result.text + builder->length;
            if (c > 0xffff) {
                c -= 0x10000;
                out[0] = 0xd800 + (c >> 10);
                out[1] = 0xdc00 + (c & 1023);
            } else
                out[0] = c;
        }
        builder->length += width;
    }
    return QJS_INTL_OK;
}

