/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "segmenter-native-data.h"
#include "segmenter-schema.h"
#include <string.h>
struct QJSIntlNativeSegmenterData {
    QJSIntlAllocator allocator;
    QJSIntlDataView view;
    QJSIntlDataSection ranges[5];
    QJSIntlExtendedPictographicSource ep;
    uint32_t ep_source;
};
static QJSIntlStatus data_error(QJSIntlDataStatus status)
{
    return status == QJS_INTL_DATA_NOT_FOUND ? QJS_INTL_UNSUPPORTED : QJS_INTL_DATA_ERROR;
}
static uint32_t little_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int ranges_valid(const QJSIntlDataSection *s, uint32_t max)
{
    uint32_t i, last = 0, old_value = 0;
    if (s->record_width != QJS_INTL_SEGMENT_RANGE_WIDTH || !s->record_count) return 0;
    for (i = 0; i < s->record_count; i++) {
        QJSIntlDataSlice row;
        uint32_t first, end, value;
        if (qjs_intl_data_record(s, i, &row) != QJS_INTL_DATA_OK || row.length != 12) return 0;
        first = little_u32(row.data); end = little_u32(row.data+4); value = little_u32(row.data+8);
        if (first > end || end > 0x10ffff || (first <= 0xdfff && end >= 0xd800) ||
            !value || value >= max || (i && first <= last) ||
            (i && first == last + 1 && value == old_value)) return 0;
        last = end; old_value = value;
    }
    return 1;
}
static uint8_t range_property(const QJSIntlDataSection *s, uint32_t cp)
{
    uint32_t low = 0, high = s->record_count;
    while (low < high) {
        uint32_t mid = low + (high-low)/2;
        const unsigned char *row = s->bytes.data + (size_t)mid * 12;
        uint32_t first = little_u32(row), last = little_u32(row+4);
        if (cp < first) high = mid;
        else if (cp > last) low = mid + 1;
        else return (uint8_t)little_u32(row+8);
    }
    return 0;
}
static QJSIntlStatus properties(void *opaque, uint32_t cp, QJSIntlSegmentProperties *out)
{
    QJSIntlNativeSegmenterData *s = opaque;
    int ep = 0;
    QJSIntlStatus status;
    if (out) memset(out, 0, sizeof(*out));
    if (!s || !out || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        return QJS_INTL_INVALID_ARGUMENT;
    out->grapheme = range_property(&s->ranges[0], cp);
    out->word = range_property(&s->ranges[1], cp);
    out->sentence = range_property(&s->ranges[2], cp);
    out->indic_conjunct = range_property(&s->ranges[3], cp);
    if (s->ep_source == QJS_INTL_SEGMENT_EP_PACKED)
        out->extended_pictographic = range_property(&s->ranges[4], cp);
    else {
        status = s->ep.contains(s->ep.opaque, cp, &ep);
        if (status != QJS_INTL_OK || (ep != 0 && ep != 1)) {
            memset(out, 0, sizeof(*out));
            return status != QJS_INTL_OK ? status : QJS_INTL_DATA_ERROR;
        }
        out->extended_pictographic = (uint8_t)ep;
    }
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_segmenter_data_open(const QJSIntlAllocator *a,
    const QJSIntlDataView *view, const QJSIntlExtendedPictographicSource *ep,
    QJSIntlNativeSegmenterData **out)
{
    QJSIntlNativeSegmenterData value, *s;
    QJSIntlDataSection rules;
    QJSIntlDataStatus ds;
    uint32_t i;
    static const uint32_t maxima[5] = { QJS_GCB_COUNT, QJS_WB_COUNT, QJS_SB_COUNT, QJS_INCB_COUNT, 2 };
    if (out) *out = NULL;
    if (!out || !a || !a->malloc || !a->realloc || !a->free || !view || !view->data)
        return QJS_INTL_INVALID_ARGUMENT;
    if (view->length < 64 || little_u32(view->data + 36) != QJS_INTL_SEGMENT_UNICODE_VERSION ||
        !QJS_INTL_CLDR_VERSION_SUPPORTED(little_u32(view->data + 40)) || view->data[8] != 1 ||
        view->data[9] != 0 || view->data[10] != 3 || view->data[11] != 0)
        return QJS_INTL_UNSUPPORTED;
    memset(&value, 0, sizeof(value));
    value.allocator = *a; value.view = *view;
    ds = qjs_intl_data_section(view, QJS_INTL_DATA_SEGMENT_RULE, &rules);
    if (ds != QJS_INTL_DATA_OK) return data_error(ds);
    if (rules.record_width != 24 || rules.record_count != 3) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < 3; i++) {
        QJSIntlDataSlice row;
        uint32_t mode;
        if (qjs_intl_data_record(&rules, i, &row) != QJS_INTL_DATA_OK || row.length != 24)
            return QJS_INTL_DATA_ERROR;
        mode = little_u32(row.data+16);
        if (little_u32(row.data) != i || little_u32(row.data+4) != QJS_INTL_SEGMENT_UNICODE_VERSION ||
            little_u32(row.data+8) != QJS_INTL_SEGMENT_UAX29_REVISION ||
            little_u32(row.data+12) != QJS_INTL_SEGMENT_ALGORITHM_VERSION ||
            (mode != QJS_INTL_SEGMENT_EP_PACKED && mode != QJS_INTL_SEGMENT_EP_LIBUNICODE) ||
            (i && mode != value.ep_source) || little_u32(row.data+20)) return QJS_INTL_DATA_ERROR;
        value.ep_source = mode;
    }
    for (i = 0; i < 4; i++) {
        ds = qjs_intl_data_section(view, QJS_INTL_DATA_SEGMENT_GCB+i, &value.ranges[i]);
        if (ds != QJS_INTL_DATA_OK) return data_error(ds);
        if (!ranges_valid(&value.ranges[i], maxima[i])) return QJS_INTL_DATA_ERROR;
    }
    ds = qjs_intl_data_section(view, QJS_INTL_DATA_SEGMENT_EP, &value.ranges[4]);
    if (value.ep_source == QJS_INTL_SEGMENT_EP_PACKED) {
        if (ds != QJS_INTL_DATA_OK) return data_error(ds);
        if (!ranges_valid(&value.ranges[4], maxima[4])) return QJS_INTL_DATA_ERROR;
    } else {
        if (ds != QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_DATA_ERROR;
        if (!ep || !ep->contains || ep->unicode_version != QJS_INTL_SEGMENT_UNICODE_VERSION)
            return QJS_INTL_UNSUPPORTED;
        value.ep = *ep;
    }
    s = a->malloc(a->opaque, sizeof(*s));
    if (!s) return QJS_INTL_NO_MEMORY;
    *s = value; *out = s;
    return QJS_INTL_OK;
}
void qjs_intl_native_segmenter_data_close(QJSIntlNativeSegmenterData *s)
{
    if (s) s->allocator.free(s->allocator.opaque, s);
}
QJSIntlSegmentPropertySource qjs_intl_native_segmenter_data_properties(QJSIntlNativeSegmenterData *s)
{
    QJSIntlSegmentPropertySource result = { NULL, NULL, 0 };
    if (s) { result.opaque = s; result.lookup = properties; result.unicode_version = QJS_INTL_SEGMENT_UNICODE_VERSION; }
    return result;
}
