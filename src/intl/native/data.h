/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NATIVE_DATA_H
#define QJS_INTL_NATIVE_DATA_H
#include "pack.h"

typedef struct QJSIntlData {
    QJSIntlPack pack;
    QJSIntlSection strings;
    QJSIntlSection locales;
} QJSIntlData;

/* The selected prepared profile has exactly one generated C owner. */
extern const uint8_t qjs_intl_native_bytes[];
extern const size_t qjs_intl_native_length;

/* Validate shared string/locale tables and references. The allocator supplies only
   temporary graph-validation memory, released before return. An open view
   borrows immutable data and requires no teardown. This does not authenticate
   a pack, canonicalize locale spellings, or establish formatter completeness.
   Required string/locale tables must be present. Service sections have
   separate owners; their contents are not admitted by this operation.
   Follow pack.h's output aliasing and immutable-view contract. */
QJSIntlStatus qjs_intl_data_open(const void *bytes, size_t length,
                               const QJSIntlAllocator *allocator,
                               QJSIntlData *out);
QJSIntlStatus qjs_intl_data_string(const QJSIntlData *data, uint32_t index,
                                 QJSIntlBytes *out);
QJSIntlStatus qjs_intl_data_locale(const QJSIntlData *data, uint32_t index,
                                 QJSIntlBytes *tag, uint32_t *parent);

#endif
