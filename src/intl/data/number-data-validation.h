/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_DATA_VALIDATION_H
#define QJS_INTL_NUMBER_DATA_VALIDATION_H
#include "native-data-reader.h"
#include "number-data-schema.h"
/* Allocation-free owner gate. Call only after the reader admits exact widths
 * 80..88 and has validated pool refs, metadata16/17 and version pins.
 * Absence of all nine sections succeeds; service coverage stays external.
 * Link number-native.c for its UTF8/token grammar gate plus shared decimal
 * and plural dependencies. This source never changes the central reader.
 */
QJSIntlDataStatus qjs_intl_number_data_validate(const QJSIntlDataView *);
#endif
