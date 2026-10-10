/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_SEGMENTER_UNICODE_H
#define QJS_INTL_SEGMENTER_UNICODE_H
#include "segmenter-native-data.h"
typedef struct QJSIntlSegmenterUnicode QJSIntlSegmenterUnicode;
/* Reuses libunicode's existing EP table via its CharRange API; it does not
 * copy a generated table into another translation unit. Creates a provider
 * cache once. Activation requires root's exhaustive UCD18 comparison gate.
 * Compilation fails with libunicode version other than exactly18.0.0. */
QJSIntlStatus qjs_intl_native_segmenter_unicode_open(const QJSIntlAllocator *,
    QJSIntlSegmenterUnicode **out);
void qjs_intl_native_segmenter_unicode_close(QJSIntlSegmenterUnicode *);
QJSIntlExtendedPictographicSource qjs_intl_native_segmenter_unicode_ep(QJSIntlSegmenterUnicode *);
#endif
