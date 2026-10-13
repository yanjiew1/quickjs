/* Private native Segmenter adapter. Copyright (c) 2026 Yan-Jie Wang. */
#ifndef QJS_INTL_PROVIDER_NATIVE_SEGMENTER_H
#define QJS_INTL_PROVIDER_NATIVE_SEGMENTER_H
#include "provider-native.h"
#include "segmenter-native-data.h"
#include "segmenter-unicode.h"
typedef struct QJSIntlProviderSegmenter QJSIntlProviderSegmenter;
QJSIntlStatus qjs_intl_native_provider_segmenter_available(QJSIntlProvider *);
QJSIntlStatus qjs_intl_native_provider_segmenter_open(QJSIntlProvider *,
    QJSIntlBytes, QJSIntlSegmentGranularity, QJSIntlProviderSegmenter **);
void qjs_intl_native_provider_segmenter_close(QJSIntlProviderSegmenter *);
QJSIntlStatus qjs_intl_native_provider_segments_new(QJSIntlProviderSegmenter *,
    QJSIntlUTF16, QJSIntlNativeSegments **);
#endif
