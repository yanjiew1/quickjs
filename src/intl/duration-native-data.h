/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_DURATION_NATIVE_DATA_H
#define QJS_INTL_DURATION_NATIVE_DATA_H
#include "duration-native.h"
#include "data/native-data-reader.h"
/* Borrowed clock slices: frontend obtains TwoDigitHours before resolving
 * GetDurationUnitOptions. view must have passed central reader plus owner110
 * gate. Missing key returns UNSUPPORTED, no locale/root fallback invented.
 */
QJSIntlStatus qjs_intl_native_duration_clock_data(const QJSIntlDataView *,
    uint32_t locale_index, uint32_t numbering_index, uint8_t *two_digit_hours,
    QJSIntlBytes separators[2]);
/* Owns shared NumberFormat handles (including owned cardinal dependencies)
 * and ListFormat handle. Copies clock data; releases view before returning.
 * Caller passes resolved options including TwoDigitHours. Missing any shared
 * Number/List/Plural/Clock data propagates UNSUPPORTED; activation external.
 */
QJSIntlStatus qjs_intl_native_duration_open_data(const QJSIntlAllocator *,
    const QJSIntlDataView *, uint32_t locale_index, uint32_t numbering_index,
    const QJSIntlDurationOptions *, QJSIntlNativeDuration **);
#endif
