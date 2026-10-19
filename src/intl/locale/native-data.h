/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_LOCALE_NATIVE_DATA_H
#define QJS_INTL_LOCALE_NATIVE_DATA_H
#include "../native/data.h"

typedef struct QJSIntlLocaleMetadata {
    QJSIntlData data;
    QJSIntlSection aliases, likely, keys, types;
} QJSIntlLocaleMetadata;

/* The input must be an unchanged successfully opened shared data view. This
   validates locale-owned row structures, references and preferred-name graphs.
   Borrow immutable storage; scratch allocation is released before return.
   Outputs follow the nonaliasing/zero-on-failure contract in native/pack.h. */
QJSIntlStatus qjs_intl_locale_metadata_open(const QJSIntlData *data,
                                          const QJSIntlAllocator *allocator,
                                          QJSIntlLocaleMetadata *out);

#endif
