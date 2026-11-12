/* Unicode 18 SpecialCasing and Greek locale uppercase tailoring.
 * Copyright (c) 2026 Yan-Jie Wang.
 * SPDX-License-Identifier: MIT AND Unicode-3.0
 * Greek tailoring is adapted from ICU4X, Copyright (c) 2020-2024 Unicode, Inc.;
 * see the Unicode license in greek-case-data.h.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "case-conversion.h"
#include "libunicode.h"
#include "utf16.h"
#include "greek-case-data.h"

/* Greek uppercase tailoring follows ICU4X's Greek case mapping design:
   unicode-org/icu4x, commit 56e2150c77decde2bfaf34b8265441714549e005.
   The corresponding Unicode license is in greek-case-data.h. */
enum {
    GREEK_IOTA = 0x10,
    GREEK_DIAERESIS = 0x20,
    GREEK_ACCENT = 0x40,
    GREEK_MARK = 0x100,
    GREEK_COMBINING_ACCENT = 0x200,
};

static unsigned int greek_letter(uint32_t c)
{
    if (c >= 0x370 && c < 0x370 + countof(greek_case_370))
        return greek_case_370[c - 0x370];
    if (c >= 0x1f00 && c < 0x1f00 + countof(greek_case_1f00))
        return greek_case_1f00[c - 0x1f00];
    return c == 0x2126 ? 7 : 0;
}

static unsigned int greek_mark(uint32_t c)
{
    switch (c) {
    case 0x300: case 0x301: case 0x302: case 0x303: case 0x311: case 0x342:
        return GREEK_MARK | GREEK_ACCENT | GREEK_COMBINING_ACCENT;
    case 0x344:
        return GREEK_MARK | GREEK_ACCENT | GREEK_DIAERESIS | GREEK_COMBINING_ACCENT;
    case 0x308:
        return GREEK_MARK | GREEK_DIAERESIS;
    case 0x345:
        return GREEK_MARK | GREEK_IOTA;
    case 0x304: case 0x306: case 0x313: case 0x314: case 0x343:
        return GREEK_MARK;
    default:
        return 0;
    }
}

static unsigned int greek_following_marks(const uint16_t *input, size_t length,
                                           size_t position)
{
    unsigned int marks = 0, mark;
    while (position < length) {
        uint32_t c = unicode_utf16_next(input, length, &position);
        mark = greek_mark(c);
        if (!mark)
            break;
        marks |= mark;
    }
    return marks;
}

static BOOL following_cased(const uint16_t *input, size_t length, size_t position)
{
    while (position < length) {
        uint32_t c = unicode_utf16_next(input, length, &position);
        if (!lre_is_case_ignorable(c))
            return lre_is_cased(c);
    }
    return FALSE;
}

static BOOL following_above(const uint16_t *input, size_t length, size_t position,
                            BOOL before_dot)
{
    while (position < length) {
        uint32_t c = unicode_utf16_next(input, length, &position);
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
    BOOL previous_greek = FALSE;
    unsigned int previous_vowel = 0;
    while (position < length) {
        uint32_t c = unicode_utf16_next(input, length, &position);
        uint32_t mapped[LRE_CC_RES_LEN_MAX];
        int count = -1, i, cc;
        if (!to_lower && locale == UNICODE_CASE_GREEK) {
            static const uint16_t vowels[] = {
                0, 0x391, 0x395, 0x397, 0x399, 0x39f, 0x3a5, 0x3a9, 0x3d2,
            };
            unsigned int letter = greek_letter(c), mark = greek_mark(c);
            if (mark && previous_greek) {
                if (mark & GREEK_IOTA)
                    previous_vowel = 0;
                else
                    count = 0;
            } else {
                unsigned int vowel = letter & 15;
                if (letter && !(letter & 0x80) && vowel <= 8) {
                    unsigned int pre = letter, all = letter |
                        greek_following_marks(input, length, position);
                    unsigned int next_vowel = all;
                    if (!(all & GREEK_DIAERESIS) && (vowel == 4 || vowel == 6) &&
                        !(previous_vowel & GREEK_DIAERESIS)) {
                        if (previous_vowel & GREEK_COMBINING_ACCENT)
                            all |= GREEK_DIAERESIS;
                        else if (previous_vowel & GREEK_ACCENT)
                            pre |= GREEK_DIAERESIS;
                    }
                    count = 0;
                    if (vowel == 3 && (all & GREEK_ACCENT) &&
                        !(all & GREEK_IOTA) && !before_cased &&
                        !following_cased(input, length, position)) {
                        if (pre & GREEK_ACCENT)
                            mapped[count++] = 0x389;
                        else {
                            mapped[count++] = 0x397;
                            mapped[count++] = 0x301;
                        }
                    } else if ((vowel == 4 || vowel == 6) &&
                               (pre & GREEK_DIAERESIS)) {
                        mapped[count++] = vowel == 4 ? 0x3aa : 0x3ab;
                        all &= ~GREEK_DIAERESIS;
                    } else {
                        mapped[count++] = vowels[vowel];
                    }
                    if (all & GREEK_DIAERESIS)
                        mapped[count++] = 0x308;
                    if (pre & GREEK_IOTA)
                        mapped[count++] = 0x399;
                    previous_vowel = next_vowel;
                } else {
                    previous_vowel = 0;
                    if (letter == 0x81) {
                        mapped[0] = 0x3a1;
                        count = 1;
                    }
                }
                previous_greek = letter != 0;
            }
        }
        if (count < 0 && to_lower && c == 0x3a3 && before_cased &&
            !following_cased(input, length, position)) {
            mapped[0] = 0x3c2;
            count = 1;
        } else if (count < 0 && locale == UNICODE_CASE_TURKIC) {
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
        } else if (count < 0 && locale == UNICODE_CASE_LITHUANIAN) {
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
