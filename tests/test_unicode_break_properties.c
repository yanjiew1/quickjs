/* Check boundary properties, optionally against every source UCD code point.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/unicode/break-properties.h"

typedef struct PropertyName {
    const char *name;
    unsigned int value;
} PropertyName;

static const PropertyName names[] = {
#define UNICODE_GRAPHEME_BREAK
#define DEF(name) { "GCB:" #name, UNICODE_GCB_ ## name },
#include "../src/unicode/break-properties-def.h"
#undef DEF
#undef UNICODE_GRAPHEME_BREAK
#define UNICODE_WORD_BREAK
#define DEF(name) { "WB:" #name, UNICODE_WB_ ## name << UNICODE_BREAK_WB_SHIFT },
#include "../src/unicode/break-properties-def.h"
#undef DEF
#undef UNICODE_WORD_BREAK
#define UNICODE_SENTENCE_BREAK
#define DEF(name) { "SB:" #name, UNICODE_SB_ ## name << UNICODE_BREAK_SB_SHIFT },
#include "../src/unicode/break-properties-def.h"
#undef DEF
#undef UNICODE_SENTENCE_BREAK
#define UNICODE_INDIC_CONJUNCT_BREAK
#define DEF(name) { "InCB:" #name, UNICODE_INCB_ ## name << UNICODE_BREAK_INCB_SHIFT },
#include "../src/unicode/break-properties-def.h"
#undef DEF
#undef UNICODE_INDIC_CONJUNCT_BREAK
    { "EP:Extended_Pictographic", UNICODE_BREAK_EP }
};

/* Deliberately parse source ranges directly into a dense oracle, independently
   of the generator's transition/index representation and parsing helpers. */
static void read_property(uint16_t *expected, const char *directory,
                           const char *filename, const char *prefix, const char *filter)
{
    char path[2048], line[4096];
    FILE *file;
    assert(snprintf(path, sizeof(path), "%s/%s", directory, filename) < sizeof(path));
    file = fopen(path, "rb");
    assert(file);
    while (fgets(line, sizeof(line), file)) {
        char name[80], key[100], *comment, *field;
        unsigned int first, last, code;
        size_t i;
        comment = strchr(line, '#');
        if (comment)
            *comment = '\0';
        field = strchr(line, ';');
        if (!field)
            continue;
        if (sscanf(line, "%x..%x", &first, &last) != 2) {
            assert(sscanf(line, "%x", &first) == 1);
            last = first;
        }
        assert(first <= last && last <= 0x10ffff);
        assert(sscanf(field + 1, " %79[^; \t\r\n]", name) == 1);
        if (filter && strcmp(name, filter))
            continue;
        if (filter && !strcmp(filter, "InCB")) {
            field = strchr(field + 1, ';');
            assert(field && sscanf(field + 1, " %79s", name) == 1);
        }
        assert(snprintf(key, sizeof(key), "%s:%s", prefix, name) < sizeof(key));
        for (i = 0; i < sizeof(names) / sizeof(*names); i++)
            if (!strcmp(names[i].name, key))
                break;
        assert(i < sizeof(names) / sizeof(*names));
        for (code = first; code <= last; code++)
            expected[code] |= names[i].value;
    }
    assert(!ferror(file));
    assert(fclose(file) == 0);
}

static void check_ucd(const char *directory)
{
    uint16_t *expected = calloc(0x110000, sizeof(*expected));
    uint32_t code;
    assert(expected);
    read_property(expected, directory, "auxiliary/GraphemeBreakProperty.txt", "GCB", NULL);
    read_property(expected, directory, "auxiliary/WordBreakProperty.txt", "WB", NULL);
    read_property(expected, directory, "auxiliary/SentenceBreakProperty.txt", "SB", NULL);
    read_property(expected, directory, "DerivedCoreProperties.txt", "InCB", "InCB");
    read_property(expected, directory, "emoji-data.txt", "EP", "Extended_Pictographic");
    for (code = 0; code <= 0x10ffff; code++) {
        uint16_t actual = unicode_break_properties(code);
        if (actual != expected[code]) {
            fprintf(stderr, "U+%06x: expected %04x, got %04x\n", code, expected[code], actual);
            abort();
        }
    }
    free(expected);
}

int main(int argc, char **argv)
{
    uint16_t value;
    /* These semantic examples also guard the fixed enum/bit mapping, rather
       than deriving every expectation from the source-name definitions. */
    assert((unicode_break_properties('\r') & UNICODE_BREAK_GCB_MASK) == UNICODE_GCB_CR);
    assert((unicode_break_properties('\n') & UNICODE_BREAK_SB_MASK) ==
           (UNICODE_SB_LF << UNICODE_BREAK_SB_SHIFT));
    assert((unicode_break_properties('A') & UNICODE_BREAK_WB_MASK) ==
           (UNICODE_WB_ALetter << UNICODE_BREAK_WB_SHIFT));
    assert((unicode_break_properties('9') & UNICODE_BREAK_WB_MASK) ==
           (UNICODE_WB_Numeric << UNICODE_BREAK_WB_SHIFT));
    value = unicode_break_properties(0x94d);
    assert((value & UNICODE_BREAK_INCB_MASK) == (UNICODE_INCB_Linker << UNICODE_BREAK_INCB_SHIFT));
    assert(unicode_break_properties(0x1f600) & UNICODE_BREAK_EP);
    /* EP intentionally includes reserved code points; surrogates and input
       beyond the Unicode domain are accepted as neutral boundary properties. */
    assert(unicode_break_properties(0x1fc00) & UNICODE_BREAK_EP);
    assert(!unicode_break_properties(0xd800));
    assert(!unicode_break_properties(0xdfff));
    assert(!unicode_break_properties(0x110000));
    assert(!unicode_break_properties(UINT32_MAX));
    if (argc > 1)
        check_ucd(argv[1]);
    return 0;
}
