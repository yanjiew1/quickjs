/* Default UTF-16 word boundaries and official Unicode conformance.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/unicode/word.h"
#include "unicode_boundary_test.h"

static void check(const uint16_t *text, size_t length, const size_t *ends, size_t count)
{
    size_t position = 0, i;
    for (i = 0; i < count; i++) {
        size_t next = unicode_word_next(text, length, position);
        assert(next > position && next <= length && next == ends[i]);
        position = next;
    }
    assert(position == length);
    assert(unicode_word_next(text, length, position) == length);
}

static void examples(void)
{
    static const uint16_t punctuation[] = {'c', 'a', 'n', '\'', 't', ' ', '3', '.', '2', '!'};
    static const size_t punctuation_ends[] = {5, 6, 9, 10};
    static const uint16_t ignored[] = {'a', 0x301, ':', 0x301, 'b', '\r', '\n', 0x301, 'c'};
    static const size_t ignored_ends[] = {5, 7, 8, 9};
    static const uint16_t space[] = {' ', 0x301, ' ', ' ', 'x'};
    static const size_t space_ends[] = {2, 4, 5};
    static const uint16_t hebrew[] = {0x5d0, '"', 0x5d1, ' ', 0x5d0, '\''};
    static const size_t hebrew_ends[] = {3, 4, 6};
    static const uint16_t lone[] = {0xd800, 0x301, 0xdc00, 0, 0xd83d, 0xde00};
    static const size_t lone_ends[] = {2, 3, 4, 6};
    check(NULL, 0, NULL, 0);
    check(punctuation, sizeof(punctuation) / sizeof(*punctuation), punctuation_ends, 4);
    check(ignored, sizeof(ignored) / sizeof(*ignored), ignored_ends, 4);
    check(space, sizeof(space) / sizeof(*space), space_ends, 3);
    check(hebrew, sizeof(hebrew) / sizeof(*hebrew), hebrew_ends, 3);
    check(lone, sizeof(lone) / sizeof(*lone), lone_ends, 4);
}

static void long_runs(void)
{
    size_t i, position, length = 100000;
    uint16_t *text = malloc(2 * length * sizeof(*text));
    assert(text);
    /* Failed punctuation lookahead must not repeatedly rescan this run. */
    text[0] = 'a';
    text[1] = ':';
    for (i = 2; i < length - 1; i++)
        text[i] = 0x301;
    text[length - 1] = '!';
    assert(unicode_word_next(text, length, 0) == 1);
    assert(unicode_word_next(text, length, 1) == length - 1);
    assert(unicode_word_next(text, length, length - 1) == length);
    text[length - 1] = 'b';
    assert(unicode_word_next(text, length, 0) == length);
    for (i = 0; i < length; i++) {
        text[2 * i] = 0xd83c;
        text[2 * i + 1] = 0xdde6;
    }
    for (position = 0; position < 2 * length; position += 4)
        assert(unicode_word_next(text, 2 * length, position) == position + 4);
    free(text);
}

int main(int argc, char **argv)
{
    examples();
    long_runs();
    if (argc > 1)
        printf("Unicode word conformance: %u cases\n",
               test_unicode_boundary_file(argv[1], unicode_word_next));
    return 0;
}
