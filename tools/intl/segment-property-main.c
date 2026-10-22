/* Pinned C property ranges for host segmentation preparation, not runtime JSON.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "segment-property.h"
#include "../../src/cutils/cutils.h"
#include "../../src/unicode/libunicode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *allocate(void *opaque, size_t size)
{
    (void)opaque;
    return malloc(size);
}

static void *resize(void *opaque, void *pointer, size_t size)
{
    (void)opaque;
    return realloc(pointer, size);
}

static void release(void *opaque, void *pointer)
{
    (void)opaque;
    free(pointer);
}

int main(int argc, char **argv)
{
    QJSIntlAllocator allocator = {NULL, allocate, resize, release};
    QJSIntlSegmentRanges ranges = {0};
    QJSIntlBytes property = {0}, value;
    QJSIntlStatus status;
    const char *separator;
    uint32_t i;
    int failed;
    if (argc != 2) {
        fprintf(stderr, "usage: intl-segment-property [property=]value\n");
        return 2;
    }
    separator = strchr(argv[1], '=');
    if (separator)
        property = (QJSIntlBytes){argv[1], (size_t)(separator - argv[1])};
    value.data = separator ? separator + 1 : argv[1];
    value.length = strlen(value.data);
    status = qjs_intl_segment_property(&allocator, property, value, &ranges);
    if (status) {
        fprintf(stderr, "Unicode property query failed (status %d)\n", (int)status);
        return 1;
    }
    printf("{\"unicode\":\"%d.%d.%d\",\"ranges\":[", LIBUNICODE_UNICODE_VERSION_MAJOR,
           LIBUNICODE_UNICODE_VERSION_MINOR, LIBUNICODE_UNICODE_VERSION_PATCH);
    for (i = 0; i < ranges.count; i += 2)
        printf("%s[%u,%u]", i ? "," : "", (unsigned int)ranges.points[i],
               (unsigned int)ranges.points[i + 1]);
    puts("]}");
    failed = fflush(stdout) || ferror(stdout);
    qjs_intl_segment_ranges_clear(&allocator, &ranges);
    return failed ? 1 : 0;
}
