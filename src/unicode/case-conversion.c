/* Unicode 18 SpecialCasing: final sigma, Turkish/Azeri and Lithuanian contexts.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT. */
#include "case-conversion.h"
#include "libunicode.h"

static uint32_t next(const uint16_t *input, size_t length, size_t *position)
{
    uint32_t c = input[(*position)++];
    if (c >= 0xd800 && c <= 0xdbff && *position < length &&
        input[*position] >= 0xdc00 && input[*position] <= 0xdfff) {
        c = 0x10000 + ((c - 0xd800) << 10) + input[(*position)++] - 0xdc00;
    }
    return c;
}

static BOOL following_cased(const uint16_t *input, size_t length, size_t position)
{
    while (position < length) {
        uint32_t c = next(input, length, &position);
        if (!lre_is_case_ignorable(c))
            return lre_is_cased(c);
    }
    return FALSE;
}

static BOOL following_above(const uint16_t *input, size_t length, size_t position,
                            BOOL before_dot)
{
    while (position < length) {
        uint32_t c = next(input, length, &position);
        int cc = unicode_get_cc(c);
        /* Before_Dot must see the dot itself before its class-230 barrier.
           More_Above accepts any class-230 mark before the next starter. */
        if (before_dot && c == 0x307)
            return TRUE;
        if (cc == 230)
            return !before_dot;
        if (cc == 0)
            break;
    }
    return FALSE;
}

static int put(DynBuf *out, uint32_t c)
{
    uint16_t units[2];
    size_t count = 1;
    if (c > 0xffff) {
        c -= 0x10000;
        units[0] = 0xd800 + (c >> 10);
        units[1] = 0xdc00 + (c & 0x3ff);
        count = 2;
    } else {
        units[0] = c;
    }
    return dbuf_put(out, (const uint8_t *)units, count * sizeof(*units));
}

int unicode_case_convert(DynBuf *out, const uint16_t *input, size_t length,
                         BOOL to_lower, UnicodeCaseLocale locale)
{
    size_t position = 0;
    BOOL before_cased = FALSE, after_i = FALSE, after_soft_dotted = FALSE;
    while (position < length) {
        uint32_t c = next(input, length, &position);
        uint32_t mapped[LRE_CC_RES_LEN_MAX];
        int count = -1, i, cc;
        if (to_lower && c == 0x3a3 && before_cased &&
            !following_cased(input, length, position)) {
            mapped[0] = 0x3c2;
            count = 1;
        } else if (locale == UNICODE_CASE_TURKIC) {
            if (to_lower && c == 0x130) {
                mapped[0] = 'i';
                count = 1;
            } else if (to_lower && c == 0x307 && after_i) {
                count = 0;
            } else if (to_lower && c == 'I' &&
                       !following_above(input, length, position, TRUE)) {
                mapped[0] = 0x131;
                count = 1;
            } else if (!to_lower && c == 'i') {
                mapped[0] = 0x130;
                count = 1;
            }
        } else if (locale == UNICODE_CASE_LITHUANIAN) {
            if (!to_lower && c == 0x307 && after_soft_dotted) {
                count = 0;
            } else if (to_lower && (c == 'I' || c == 'J' || c == 0x12e) &&
                       following_above(input, length, position, FALSE)) {
                mapped[0] = c == 0x12e ? 0x12f : c + ('a' - 'A');
                mapped[1] = 0x307;
                count = 2;
            } else if (to_lower && (c == 0xcc || c == 0xcd || c == 0x128)) {
                mapped[0] = 'i';
                mapped[1] = 0x307;
                mapped[2] = c == 0xcc ? 0x300 : c == 0xcd ? 0x301 : 0x303;
                count = 3;
            }
        }
        if (count < 0)
            count = lre_case_conv(mapped, c, to_lower);
        for (i = 0; i < count; i++)
            if (put(out, mapped[i]))
                return -1;

        // Track source context, not transformed characters or emitted dots.
        if (!lre_is_case_ignorable(c))
            before_cased = lre_is_cased(c);
        cc = c < 128 ? 0 : unicode_get_cc(c);
        if (cc == 0) {
            after_i = c == 'I';
            after_soft_dotted = lre_is_soft_dotted(c);
        } else if (cc == 230) {
            after_i = after_soft_dotted = FALSE;
        }
    }
    return 0;
}
