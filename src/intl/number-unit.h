/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NUMBER_UNIT_H
#define QJS_INTL_NUMBER_UNIT_H
#include "provider.h"
/* Exact ECMA402 sanctioned simple identifier or simple-per-simple. */
int qjs_intl_number_unit_validate(QJSIntlBytes);
/* Compose six final templates from a numerator's cardinal forms. The exact
 * specialized per-unit template takes priority; otherwise the locale
 * {numerator}/{denominator} pattern inserts a host-resolved denominator name.
 * Output slices own separate allocations; failure zeros them and frees all.
 * maximum is a UTF8 byte bound, distinct from rendering's UTF16 bound. */
QJSIntlStatus qjs_intl_number_unit_compose(const QJSIntlAllocator *,
    const QJSIntlBytes numerator[6], QJSIntlBytes per_unit,
    QJSIntlBytes denominator, QJSIntlBytes compound_pattern,
    size_t maximum, QJSIntlBytes out[6]);
void qjs_intl_number_unit_composed_clear(const QJSIntlAllocator *, QJSIntlBytes[6]);
#endif
