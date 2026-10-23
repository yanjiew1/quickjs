/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "routing.h"
#include "../../cutils/cutils.h"
#include "../../unicode/libunicode.h"
#include <string.h>

static const struct ScriptFamily {
    const char *name;
    QJSIntlSegmentDictionaryFamily family;
} scripts[] = {
    {"Han", QJS_INTL_DICTIONARY_CJK},
    {"Hiragana", QJS_INTL_DICTIONARY_CJK},
    {"Katakana", QJS_INTL_DICTIONARY_CJK},
    {"Khmer", QJS_INTL_DICTIONARY_KHMER},
    {"Lao", QJS_INTL_DICTIONARY_LAO},
    {"Myanmar", QJS_INTL_DICTIONARY_MYANMAR},
    {"Thai", QJS_INTL_DICTIONARY_THAI},
};

enum { SCRIPT_COUNT = sizeof(scripts) / sizeof(scripts[0]) };

struct QJSIntlSegmentRouting {
    QJSIntlAllocator allocator;
    CharRange ranges[SCRIPT_COUNT][2];
    CharRange inherited;
};

/* Unicode ranges release through realloc(ptr, 0); Intl never requests a
   zero-sized allocation. The opaque allocator lives inside the stable cache,
   rather than in the open function's temporary stack frame. */
static void *range_realloc(void *opaque, void *ptr, size_t size)
{
    const QJSIntlAllocator *allocator = opaque;
    if (!size) {
        if (ptr)
            allocator->free(allocator->opaque, ptr);
        return NULL;
    }
    return allocator->realloc(allocator->opaque, ptr, size);
}

QJSIntlStatus qjs_intl_segment_routing_open(const QJSIntlAllocator *allocator,
                                             QJSIntlSegmentRouting **out)
{
    QJSIntlSegmentRouting *cache;
    unsigned int i, mode;
    QJSIntlStatus status;
    if (out)
        *out = NULL;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    cache = allocator->malloc(allocator->opaque, sizeof(*cache));
    if (!cache)
        return QJS_INTL_NO_MEMORY;
    memset(cache, 0, sizeof(*cache));
    cache->allocator = *allocator;
    cr_init(&cache->inherited, &cache->allocator, range_realloc);
    for (i = 0; i < SCRIPT_COUNT; i++)
        for (mode = 0; mode < 2; mode++)
            cr_init(&cache->ranges[i][mode], &cache->allocator, range_realloc);
    for (i = 0; i < SCRIPT_COUNT; i++) {
        for (mode = 0; mode < 2; mode++) {
            CharRange *range = &cache->ranges[i][mode];
            int result = unicode_script(range, scripts[i].name, mode);
            if (result || range->len <= 0 || (range->len & 1)) {
                status = result == -1 ? QJS_INTL_NO_MEMORY : QJS_INTL_DATA_ERROR;
                qjs_intl_segment_routing_close(cache);
                return status;
            }
        }
    }
    {
        int result = unicode_script(&cache->inherited, "Inherited", 1);
        if (result || cache->inherited.len <= 0 || (cache->inherited.len & 1)) {
            status = result == -1 ? QJS_INTL_NO_MEMORY : QJS_INTL_DATA_ERROR;
            qjs_intl_segment_routing_close(cache);
            return status;
        }
    }
    *out = cache;
    return QJS_INTL_OK;
}

void qjs_intl_segment_routing_close(QJSIntlSegmentRouting *cache)
{
    if (cache) {
        QJSIntlAllocator allocator = cache->allocator;
        unsigned int i, mode;
        for (i = 0; i < SCRIPT_COUNT; i++)
            for (mode = 0; mode < 2; mode++)
                cr_free(&cache->ranges[i][mode]);
        cr_free(&cache->inherited);
        allocator.free(allocator.opaque, cache);
    }
}

static int contains(const CharRange *range, uint32_t scalar)
{
    unsigned int low = 0, high = range->len;
    /* An upper-bound index is odd exactly inside a half-open range. */
    while (low < high) {
        unsigned int middle = low + (high - low) / 2;
        if (range->points[middle] <= scalar)
            low = middle + 1;
        else
            high = middle;
    }
    return low & 1;
}

unsigned int qjs_intl_segment_routing_mask(const QJSIntlSegmentRouting *cache, uint32_t scalar,
                                            int extensions)
{
    unsigned int i, result = 0;
    if (!cache || (extensions != 0 && extensions != 1) || scalar > 0x10ffff ||
        (scalar >= 0xd800 && scalar <= 0xdfff))
        return 0;
    for (i = 0; i < SCRIPT_COUNT; i++) {
        const CharRange *range = &cache->ranges[i][extensions];
        if (contains(range, scalar))
            result |= 1u << scripts[i].family;
    }
    return result;
}

int qjs_intl_segment_routing_inherited(const QJSIntlSegmentRouting *cache, uint32_t scalar)
{
    return cache && scalar <= 0x10ffff && !(scalar >= 0xd800 && scalar <= 0xdfff) &&
        contains(&cache->inherited, scalar);
}
