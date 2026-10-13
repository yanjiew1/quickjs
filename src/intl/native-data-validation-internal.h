/* Private provider-to-data validation dispatch. Copyright (c) 2026
 * Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NATIVE_DATA_VALIDATION_INTERNAL_H
#define QJS_INTL_NATIVE_DATA_VALIDATION_INTERNAL_H
#include "number-native-data.h"
#include "duration-native-data.h"
/* Called synchronously at the ordinary Number table-validation point.
 * A callback must validate the entire base+extra Number tables or authenticate
 * an equivalent successful proof for the exact immutable owner view. NULL
 * retains full validation. No callback or opaque is retained by Number;
 * Duration uses them only while synchronously constructing its Number owners.
 * Decoded selected-data, options and cardinal validation remain independent.
 */
typedef QJSIntlDataStatus (*QJSIntlNumberDataValidator)(void *,
    const QJSIntlDataView *);
QJSIntlStatus qjs_intl_native_number_open_data_with_validator(
    const QJSIntlAllocator *, const QJSIntlDataView *, uint32_t, uint32_t,
    const QJSIntlNumberOptions *, QJSIntlNumberDataValidator, void *,
    QJSIntlNativeNumber **);
QJSIntlStatus qjs_intl_native_number_currency_digits_with_validator(
    const QJSIntlDataView *, QJSIntlBytes, QJSIntlNumberDataValidator, void *,
    unsigned int *);
QJSIntlStatus qjs_intl_native_duration_open_data_with_validator(
    const QJSIntlAllocator *, const QJSIntlDataView *, uint32_t, uint32_t,
    const QJSIntlDurationOptions *, QJSIntlNumberDataValidator, void *,
    QJSIntlNativeDuration **);
#endif
