/* Shared official boundary-file oracle, independent of algorithm state.
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
#include "unicode_boundary_test.h"

unsigned int test_unicode_boundary_file(const char *filename, UnicodeBoundaryNext *next_boundary)
{
    FILE *file = fopen(filename, "rb");
    char line[8192];
    unsigned int line_number = 0, cases = 0;
    assert(file);
    while (fgets(line, sizeof(line), file)) {
        uint16_t text[4096];
        unsigned char expected[4097] = {0}, actual[4097] = {0};
        size_t length = 0, position;
        char *token, *comment = strchr(line, '#');
        line_number++;
        assert(strchr(line, '\n') || feof(file));
        if (comment)
            *comment = '\0';
        token = strtok(line, " \t\r\n");
        if (!token)
            continue;
        do {
            if (!strcmp(token, "\xc3\xb7")) {
                expected[length] = 1;
            } else if (strcmp(token, "\xc3\x97")) {
                char *end;
                unsigned long code = strtoul(token, &end, 16);
                assert(!*end && code <= 0x10ffff && length + 2 <= 4096);
                if (code > 0xffff) {
                    code -= 0x10000;
                    text[length++] = 0xd800 + (code >> 10);
                    text[length++] = 0xdc00 + (code & 0x3ff);
                } else {
                    text[length++] = code;
                }
            }
        } while ((token = strtok(NULL, " \t\r\n")) != NULL);
        actual[0] = 1;
        position = 0;
        while (position < length) {
            size_t next = next_boundary(text, length, position);
            assert(next > position && next <= length);
            actual[next] = 1;
            position = next;
        }
        for (position = 0; position <= length; position++) {
            if (actual[position] != expected[position]) {
                fprintf(stderr, "%s:%u: wrong boundary at UTF-16 index %zu\n",
                        filename, line_number, position);
                abort();
            }
        }
        cases++;
    }
    assert(!ferror(file) && cases);
    assert(fclose(file) == 0);
    return cases;
}

