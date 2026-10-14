/* Private provider-to-data validation dispatch. Copyright (c) 2026
 * Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NATIVE_DATA_VALIDATION_INTERNAL_H
#define QJS_INTL_NATIVE_DATA_VALIDATION_INTERNAL_H
#include "number-native-data.h"
#include "duration-native-data.h"
#include "plural-native-data.h"
#include "relative-native-data.h"
#include "date-native-data.h"
/* Called synchronously at the ordinary service table-validation point.
 * A callback must validate the entire required service tables or authenticate
 * an equivalent successful proof for the exact immutable owner view. NULL
 * retains full validation. No callback or opaque is retained by a formatter;
 * Duration uses them only while synchronously constructing its Number owners.
 * Decoded selected-data, options and cardinal validation remain independent.
 */
typedef QJSIntlDataStatus (*QJSIntlDataValidator)(void *,
    const QJSIntlDataView *);
QJSIntlStatus qjs_intl_native_number_open_data_with_validator(
    const QJSIntlAllocator *, const QJSIntlDataView *, uint32_t, uint32_t,
    const QJSIntlNumberOptions *, QJSIntlDataValidator, void *,
    QJSIntlNativeNumber **);
QJSIntlStatus qjs_intl_native_number_currency_digits_with_validator(
    const QJSIntlDataView *, QJSIntlBytes, QJSIntlDataValidator, void *,
    unsigned int *);
QJSIntlStatus qjs_intl_native_duration_open_data_with_validator(
    const QJSIntlAllocator *, const QJSIntlDataView *, uint32_t, uint32_t,
    const QJSIntlDurationOptions *, QJSIntlDataValidator, void *,
    QJSIntlNativeDuration **);
QJSIntlStatus qjs_intl_native_plural_open_data_with_validator(
    const QJSIntlAllocator *, const QJSIntlDataView *, uint32_t,
    const QJSIntlPluralOptions *, QJSIntlDataValidator, void *, QJSIntlNativePlural **);
QJSIntlStatus qjs_intl_native_relative_open_data_with_validator(
    const QJSIntlAllocator *, const QJSIntlDataView *, uint32_t,
    const QJSIntlRelativeOptions *, const QJSIntlRelativeNumberBridge *,
    QJSIntlDataValidator, void *, QJSIntlNativeRelative **);
QJSIntlStatus qjs_intl_native_date_data_calendar_mask_with_validator(
    const QJSIntlDataView *, uint32_t, QJSIntlDataValidator, void *, uint32_t *);
#endif
