/* CLDR 49 beta4 letter-grapheme boundary rule for alphaNextToNumber.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "boundary.h"
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include <limits.h>

QJSIntlStatus qjs_intl_currency_boundary(QJSIntlBytes symbol, unsigned int *before_number,
                                       unsigned int *after_number)
{
    const uint8_t *at, *end, *next;
    unsigned int before = 0, after = 0;
    if (before_number)
        *before_number = 0;
    if (after_number)
        *after_number = 0;
    if (!before_number || !after_number || (!symbol.data && symbol.length) ||
        symbol.length > INT_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!symbol.length)
        return QJS_INTL_OK;
    at = (const uint8_t *)symbol.data;
    end = at + symbol.length;
    while (at < end) {
        int c = unicode_from_utf8(at, (int)(end - at), &next);
        unsigned int letter;
        if (c <= 0 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return QJS_INTL_INVALID_ARGUMENT;
        letter = lre_is_letter(c);
        if (at == (const uint8_t *)symbol.data)
            after = letter;
        if (letter || !lre_is_mark(c))
            before = letter;
        at = next;
    }
    *before_number = before;
    *after_number = after;
    return QJS_INTL_OK;
}
