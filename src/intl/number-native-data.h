/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_NATIVE_DATA_H
#define QJS_INTL_NUMBER_NATIVE_DATA_H
#include "number-native.h"
#include "data/native-data-reader.h"
#include "data/number-data-schema.h"
#include "data/number-extra-schema.h"
/* Frontend already resolved locale16/numbering17 indices. Open copies all
 * decoded data, opens and owns a cardinal handle for unit/name/compact, and
 * releases the borrowed binary before return. Missing data -> UNSUPPORTED.
 * Compound data uses83 numerator and121/122 denominator/per grammar.
 * Ranges use86; compact uses120. No section enables service coverage.
 */
QJSIntlStatus qjs_intl_native_number_open_data(const QJSIntlAllocator *,
    const QJSIntlDataView *, uint32_t locale_index, uint32_t numbering_index,
    const QJSIntlNumberOptions *, QJSIntlNativeNumber **);
QJSIntlStatus qjs_intl_native_number_currency_digits(const QJSIntlDataView *,
    QJSIntlBytes currency, unsigned int *out);
/* Private adapter ownership transfer: used only after successful open_data. */
void qjs_intl_native_number_take_cardinal(QJSIntlNativeNumber *, QJSIntlNativePlural *);
#endif
