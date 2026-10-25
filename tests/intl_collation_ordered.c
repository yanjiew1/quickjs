/* Parse once for both independent collation weight representations.
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
#include "intl_collation_ordered.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void qjs_test_intl_collation_ordered(const char *path, QJSIntlOrderedCompare compare, void *opaque)
{
    uint16_t previous[1024], current[1024];
    size_t previous_length = 0, previous_line = 0, line_number = 0, count = 0;
    char line[4096];
    FILE *file = fopen(path, "r");

    assert(file && compare);
    while (fgets(line, sizeof(line), file)) {
        char *at = line, *end;
        size_t length = 0;
        int order;

        line_number++;
        assert(strchr(line, '\n') || feof(file));
        while (*at == ' ' || *at == '\t')
            at++;
        if (*at == '#' || *at == '\n' || *at == '\r' || !*at)
            continue;

        while (*at != ';') {
            unsigned long code;

            errno = 0;
            code = strtoul(at, &end, 16);
            assert(end != at && !errno && code <= 0x10ffff);
            /* The official corpus also probes lone surrogate code units. */
            assert(length + 2 <= sizeof(current) / sizeof(*current));
            if (code <= 0xffff) {
                current[length++] = code;
            } else {
                current[length++] = 0xd800 + ((code - 0x10000) >> 10);
                current[length++] = 0xdc00 + ((code - 0x10000) & 0x3ff);
            }
            at = end;
            while (*at == ' ' || *at == '\t')
                at++;
        }
        assert(length);

        if (count) {
            QJSIntlStatus status = compare(opaque, previous, previous_length, current, length, &order);

            if (status || order > 0) {
                fprintf(stderr, "%s: lines %zu/%zu out of root order (status %d)\n",
                        path, previous_line, line_number, status);
                abort();
            }
        }
        memcpy(previous, current, length * sizeof(*current));
        previous_length = length;
        previous_line = line_number;
        count++;
    }

    assert(!ferror(file) && count);
    assert(!fclose(file));
    printf("checked %zu ordered root collation strings\n", count);
}
