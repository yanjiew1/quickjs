/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_PLURAL_NATIVE_DATA_H
#define QJS_INTL_PLURAL_NATIVE_DATA_H
#include "plural-native.h"
#include "data/plural-data-validation.h"
/* Locale index is frontend-resolved section16 index, not a guessed language.
 * Snapshot open no longer borrows data view. Missing section/binding returns
 * UNSUPPORTED; malformed tables return DATA_ERROR. Coverage remains external.
 */
QJSIntlStatus qjs_intl_native_plural_open_data(const QJSIntlAllocator *,
    const QJSIntlDataView *, uint32_t locale_index, const QJSIntlPluralOptions *,
    QJSIntlNativePlural **out);
#endif
