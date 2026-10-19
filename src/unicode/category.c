/* Allocation-free General_Category queries over the shared Unicode tables.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../cutils/cutils.h"
#include "libunicode.h"
#include <libunicode-table.h>
#include "unicode-encoding.h"

#define M(id) (1U << UNICODE_GC_ ## id)

static int unicode_gc_contains(uint32_t code, uint32_t mask)
{
    const uint8_t *p = unicode_gc_table;
    const uint8_t *end = p + countof(unicode_gc_table);
    uint32_t start = 0;
    if (code > 0x10ffff)
        return 0;
    while (p < end) {
        uint32_t b, length, category;
        UNICODE_GC_READ(p, b, length, category);
        length++;
        if (code - start < length) {
            /* Category 31 is an alternating Lu/Ll run, beginning with Lu. */
            if (category == 31)
                category = (code - start) & 1 ? UNICODE_GC_Ll : UNICODE_GC_Lu;
            return (mask >> category) & 1;
        }
        start += length;
    }
    return 0;
}

int lre_is_letter(uint32_t c)
{
    if (c < 128)
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    return unicode_gc_contains(c, M(Lu) | M(Ll) | M(Lt) | M(Lm) | M(Lo));
}

int lre_is_mark(uint32_t c)
{
    if (c < 128)
        return 0;
    return unicode_gc_contains(c, M(Mn) | M(Mc) | M(Me));
}


int lre_is_decimal_digit(uint32_t c)
{
    if (c < 128)
        return c >= '0' && c <= '9';
    return unicode_gc_contains(c, M(Nd));
}

int lre_is_symbol_or_separator(uint32_t c)
{
    return unicode_gc_contains(c, M(Sm) | M(Sc) | M(Sk) | M(So) | M(Zs) | M(Zl) | M(Zp));
}
