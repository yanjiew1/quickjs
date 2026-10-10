/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Calls existing MIT libunicode API (Fabrice Bellard); no implementation
 * ported from QuickJS-NG, and no generated libunicode table included here. */
#include "segmenter-unicode.h"
#include "segmenter-schema.h"
#include "cutils.h" /* libunicode.h requires BOOL/TRUE and utility declarations. */
#include "unicode/libunicode.h"
#if LIBUNICODE_UNICODE_VERSION_MAJOR != 18 || LIBUNICODE_UNICODE_VERSION_MINOR != 0 || LIBUNICODE_UNICODE_VERSION_PATCH != 0
#error Segmenter EP reuse requires exact libunicode18.0.0
#endif
struct QJSIntlSegmenterUnicode {
    QJSIntlAllocator allocator;
    CharRange ep;
};
static void *range_realloc(void *opaque, void *ptr, size_t size)
{
    QJSIntlSegmenterUnicode *s = opaque;
    if (!size) { s->allocator.free(s->allocator.opaque, ptr); return NULL; }
    return s->allocator.realloc(s->allocator.opaque, ptr, size);
}
static QJSIntlStatus contains(void *opaque, uint32_t cp, int *out)
{
    QJSIntlSegmenterUnicode *s = opaque;
    size_t low = 0, high;
    if (out) *out = 0;
    if (!s || !out || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        return QJS_INTL_INVALID_ARGUMENT;
    high = (size_t)s->ep.len;
    /* CharRange uses alternating [first,exclusive_last] endpoints. */
    while (low < high) {
        size_t mid = low + (high-low)/2;
        if (s->ep.points[mid] <= cp) low = mid + 1;
        else high = mid;
    }
    *out = (int)(low & 1);
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_segmenter_unicode_open(const QJSIntlAllocator *a,
    QJSIntlSegmenterUnicode **out)
{
    QJSIntlSegmenterUnicode *s;
    int status;
    if (out) *out = NULL;
    if (!out || !a || !a->malloc || !a->realloc || !a->free) return QJS_INTL_INVALID_ARGUMENT;
    s = a->malloc(a->opaque, sizeof(*s));
    if (!s) return QJS_INTL_NO_MEMORY;
    s->allocator = *a;
    cr_init(&s->ep, s, range_realloc);
    status = unicode_prop(&s->ep, "Extended_Pictographic");
    if (status < 0) {
        cr_free(&s->ep); a->free(a->opaque, s);
        return status == -2 ? QJS_INTL_UNSUPPORTED : QJS_INTL_NO_MEMORY;
    }
    *out = s;
    return QJS_INTL_OK;
}
void qjs_intl_native_segmenter_unicode_close(QJSIntlSegmenterUnicode *s)
{
    if (s) { cr_free(&s->ep); s->allocator.free(s->allocator.opaque, s); }
}
QJSIntlExtendedPictographicSource qjs_intl_native_segmenter_unicode_ep(QJSIntlSegmenterUnicode *s)
{
    QJSIntlExtendedPictographicSource result = { NULL, NULL, 0 };
    if (s) { result.opaque = s; result.contains = contains; result.unicode_version = QJS_INTL_SEGMENT_UNICODE_VERSION; }
    return result;
}
