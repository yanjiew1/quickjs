/* Default UTF-16 sentence boundaries and official Unicode conformance.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/unicode/sentence.h"
#include "unicode_boundary_test.h"

static void check(const uint16_t *text, size_t length, const size_t *ends, size_t count)
{
    size_t position = 0, i;
    for (i = 0; i < count; i++) {
        size_t next = unicode_sentence_next(text, length, position);
        assert(next > position && next <= length && next == ends[i]);
        position = next;
    }
    assert(position == length);
    assert(unicode_sentence_next(text, length, position) == length);
}

static void examples(void)
{
    static const uint16_t ordinary[] = {'O', 'n', 'e', '.', ' ', 'T', 'w', 'o', '!'};
    static const size_t ordinary_ends[] = {5, 9};
    static const uint16_t numeric[] = {'1', '.', '2', ' ', 'U', '.', 'S', '.', ' ', 'a'};
    static const size_t numeric_ends[] = {10};
    static const uint16_t close[] = {'a', '!', '"', ' ', 'B'};
    static const size_t close_ends[] = {4, 5};
    static const uint16_t paragraph[] = {'A', '\r', '\n', 0x301, 'B', 0x2028, 'C'};
    static const size_t paragraph_ends[] = {3, 6, 7};
    static const uint16_t lone[] = {0xd800, 0x301, 0xdc00, 0, 0xd83d, 0xde00, '.', ' ', 'B'};
    static const size_t lone_ends[] = {8, 9};
    check(NULL, 0, NULL, 0);
    check(ordinary, sizeof(ordinary) / sizeof(*ordinary), ordinary_ends, 2);
    check(numeric, sizeof(numeric) / sizeof(*numeric), numeric_ends, 1);
    check(close, sizeof(close) / sizeof(*close), close_ends, 2);
    check(paragraph, sizeof(paragraph) / sizeof(*paragraph), paragraph_ends, 3);
    check(lone, sizeof(lone) / sizeof(*lone), lone_ends, 2);
}

static void long_runs(void)
{
    size_t i, length = 100003;
    uint16_t *text = malloc(length * sizeof(*text));
    assert(text);
    text[0] = 'a';
    text[1] = '.';
    for (i = 2; i < length - 1; i++)
        text[i] = ')';
    text[length - 1] = 'b';
    /* SB8 lookahead is cached through the whole Close* prefix, rather than
       repeated at each closing punctuation character (quadratic work). */
    assert(unicode_sentence_next(text, length, 0) == length);
    text[length - 1] = 'B';
    assert(unicode_sentence_next(text, length, 0) == length - 1);
    for (i = 2; i < length - 1; i++)
        text[i] = ' ';
    assert(unicode_sentence_next(text, length, 0) == length - 1);
    text[length - 1] = 'b';
    assert(unicode_sentence_next(text, length, 0) == length);
    free(text);
}

int main(int argc, char **argv)
{
    examples();
    long_runs();
    if (argc > 1)
        printf("Unicode sentence conformance: %u cases\n",
               test_unicode_boundary_file(argv[1], unicode_sentence_next));
    return 0;
}
