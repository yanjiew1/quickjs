/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_LIST_NATIVE_DATA_H
#define QJS_INTL_LIST_NATIVE_DATA_H
#include "list-native.h"
#include "data/native-data-reader.h"
/* Accepts a validated schema1.2 view. Locale index is the already resolved
 * section16 row; JS observable locale resolution stays in the frontend.
 * open snapshots decoded patterns/script ranges, and no longer borrows view.
 * Missing required service data returns UNSUPPORTED; malformed data returns
 * DATA_ERROR. Neither condition invents root patterns or activates coverage.
 */
QJSIntlStatus qjs_intl_native_list_open_data(const QJSIntlAllocator *,
                                           const QJSIntlDataView *,
                                           uint32_t locale_index,
                                           QJSIntlListType,
                                           QJSIntlListStyle,
                                           QJSIntlNativeList **out);
#endif
