/* Export validated TZif views for the build-host embedding generator.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
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
#include "../../src/timezone/tzif.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static void print_rule(const QJSTzifRule *rule)
{
    printf("[%d,%d,%d,%d,%" PRId32 "]", rule->kind, rule->day,
           rule->month, rule->week, rule->seconds);
}

int main(int argc, char **argv)
{
    unsigned char *bytes = NULL;
    FILE *file;
    long length;
    char line[128];
    int first = 1, result = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: timezone-describe PACK < EXTENTS\n");
        return 1;
    }
    file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0 ||
        length > 16 * 1024 * 1024 || fseek(file, 0, SEEK_SET))
        goto done;
    bytes = malloc((size_t)length);
    if (!bytes || fread(bytes, 1, (size_t)length, file) != (size_t)length)
        goto done;
    printf("[");
    while (fgets(line, sizeof(line), stdin)) {
        QJSTzifState state;
        unsigned long long offset, size;
        char *end;

        errno = 0;
        offset = strtoull(line, &end, 10);
        if (errno || end == line || *end != ':')
            goto done;
        size = strtoull(end + 1, &end, 10);
        if (errno || (*end != '\n' && *end) ||
            offset > (unsigned long long)length ||
            size > (unsigned long long)length - offset ||
            qjs_tzif_parse(&state, bytes + (size_t)offset, (size_t)size))
            goto done;
        printf("%s{\"times\":%zu,\"indices\":%zu,\"types\":%zu,"
               "\"characters\":%zu,\"time_count\":%" PRIu32 ","
               "\"type_count\":%" PRIu32 ",\"character_count\":%" PRIu32 ","
               "\"future\":%d,\"standard_offset\":%" PRId32 ","
               "\"daylight_offset\":%" PRId32 ",\"start\":",
               first ? "" : ",", (size_t)(state.times - bytes),
               (size_t)(state.indices - bytes), (size_t)(state.types - bytes),
               (size_t)(state.characters - bytes), state.time_count,
               state.type_count, state.character_count, state.future,
               state.standard_offset, state.daylight_offset);
        print_rule(&state.start);
        printf(",\"end\":");
        print_rule(&state.end);
        printf("}");
        first = 0;
    }
    if (ferror(stdin))
        goto done;
    printf("]\n");
    if (!ferror(stdout))
        result = 0;
done:
    if (result)
        fprintf(stderr, "Invalid or unreadable timezone pack/extent\n");
    free(bytes);
    fclose(file);
    return result;
}
