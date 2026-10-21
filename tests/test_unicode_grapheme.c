/* UTF-16 grapheme regression and official Unicode conformance checks.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/unicode/grapheme.h"
#include "unicode_boundary_test.h"

static void check(const uint16_t *text, size_t length, const size_t *ends, size_t count)
{
    size_t position = 0, i;
    for (i = 0; i < count; i++) {
        size_t next = unicode_grapheme_next(text, length, position);
        assert(next > position && next <= length && next == ends[i]);
        position = next;
    }
    assert(position == length);
    assert(unicode_grapheme_next(text, length, position) == length);
}

static void examples(void)
{
    static const uint16_t ascii[] = {'a', 'b', '\r', '\n', 'c'};
    static const size_t ascii_ends[] = {1, 2, 4, 5};
    static const uint16_t hangul[] = {0x1100, 0x1161, 0x11a8, 'x'};
    static const size_t hangul_ends[] = {3, 4};
    static const uint16_t emoji[] = {0xd83d, 0xdc69, 0x301, 0x200d, 0xd83d, 0xde80, 'x'};
    static const size_t emoji_ends[] = {6, 7};
    static const uint16_t indic[] = {0x94d, 0x301, 0x915, 'x'};
    static const size_t indic_ends[] = {3, 4};
    static const uint16_t lone[] = {0xd800, 0x301, 0xdc00, 0, 0xd83d, 0xde00};
    static const size_t lone_ends[] = {2, 3, 4, 6};
    static const uint16_t prepend[] = {0x600, 'a', 0x600, '\r', '\n', 0x301};
    static const size_t prepend_ends[] = {2, 3, 5, 6};
    check(NULL, 0, NULL, 0);
    check(ascii, sizeof(ascii) / sizeof(*ascii), ascii_ends, 4);
    check(hangul, sizeof(hangul) / sizeof(*hangul), hangul_ends, 2);
    check(emoji, sizeof(emoji) / sizeof(*emoji), emoji_ends, 2);
    check(indic, sizeof(indic) / sizeof(*indic), indic_ends, 2);
    check(lone, sizeof(lone) / sizeof(*lone), lone_ends, 4);
    check(prepend, sizeof(prepend) / sizeof(*prepend), prepend_ends, 4);
}

static void long_runs(void)
{
    size_t i, position, length = 100000;
    uint16_t *text = malloc(length * 2 * sizeof(*text));
    assert(text);
    text[0] = 'a';
    for (i = 1; i < length; i++)
        text[i] = 0x301;
    assert(unicode_grapheme_next(text, length, 0) == length);
    /* RI parity is maintained without rescanning the run from its beginning. */
    for (i = 0; i < length; i++) {
        text[2 * i] = 0xd83c;
        text[2 * i + 1] = 0xdde6;
    }
    for (position = 0; position < 2 * length; position += 4)
        assert(unicode_grapheme_next(text, 2 * length, position) == position + 4);
    /* One long EP (Extend* ZWJ EP)* cluster, with constant-size state. */
    for (i = 0; i < length - 1; i += 3) {
        text[i] = 0xd83d;
        text[i + 1] = 0xde00;
        text[i + 2] = 0x200d;
    }
    text[length - 1] = 0xd83d;
    text[length] = 0xde00;
    assert(unicode_grapheme_next(text, length + 1, 0) == length + 1);
    free(text);
}

int main(int argc, char **argv)
{
    examples();
    long_runs();
    if (argc > 1)
        printf("Unicode grapheme conformance: %u cases\n", test_unicode_boundary_file(argv[1], unicode_grapheme_next));
    return 0;
}
