/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_RELATIVE_NATIVE_DATA_H
#define QJS_INTL_RELATIVE_NATIVE_DATA_H
#include "relative-native.h"
#include "data/plural-data-validation.h"
#include "data/relative-data-validation.h"
/* Requires root's final additive1.3 generic reader admitting IDs70/71.
 * Full service lattice/order/ref validation is performed before lookup.
 * Locale index is resolved by frontend against section16, not guessed.
 * Open snapshots all service data and cardinal rules; view can then die. */
QJSIntlStatus qjs_intl_native_relative_open_data(const QJSIntlAllocator *,
    const QJSIntlDataView *, uint32_t locale_index,
    const QJSIntlRelativeOptions *, const QJSIntlRelativeNumberBridge *,
    QJSIntlNativeRelative **out);
#endif
