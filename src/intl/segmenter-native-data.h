/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_SEGMENTER_NATIVE_DATA_H
#define QJS_INTL_SEGMENTER_NATIVE_DATA_H
#include "segmenter-native.h"
#include "data/native-data-reader.h"
typedef struct QJSIntlExtendedPictographicSource {
    void *opaque;
    QJSIntlStatus (*contains)(void *, uint32_t, int *);
    uint32_t unicode_version;
} QJSIntlExtendedPictographicSource;
typedef struct QJSIntlNativeSegmenterData QJSIntlNativeSegmenterData;
/* View must come from an independently integrated schema1.3 reader, not a
 * forged view. This loader independently checks its service records as well.
 * It borrows immutable blob bytes and the EP source until close. The external
 * EP source must be verified against UCD18 before provider activation. Absent
 * sections are UNSUPPORTED; malformed/empty required tables are DATA_ERROR. */
QJSIntlStatus qjs_intl_native_segmenter_data_open(const QJSIntlAllocator *,
    const QJSIntlDataView *, const QJSIntlExtendedPictographicSource *,
    QJSIntlNativeSegmenterData **out);
void qjs_intl_native_segmenter_data_close(QJSIntlNativeSegmenterData *);
QJSIntlSegmentPropertySource qjs_intl_native_segmenter_data_properties(
    QJSIntlNativeSegmenterData *);
#endif
