/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_LOCALE_NATIVE_DATA_H
#define QJS_INTL_LOCALE_NATIVE_DATA_H
#include "../native/data.h"

typedef struct QJSIntlLocaleMetadata {
    QJSIntlData data;
    QJSIntlSection aliases, likely, keys, types;
} QJSIntlLocaleMetadata;

typedef struct QJSIntlLocaleType {
    QJSIntlBytes extension, key, name, aliases, preferred, iana, region;
    unsigned int deprecated, variable;
} QJSIntlLocaleType;

/* The input must be an unchanged successfully opened shared data view. This
   validates locale-owned row structures, references and preferred-name graphs.
   Borrow immutable storage; scratch allocation is released before return.
   Outputs follow the nonaliasing/zero-on-failure contract in native/pack.h. */
QJSIntlStatus qjs_intl_locale_metadata_open(const QJSIntlData *data,
                                          const QJSIntlAllocator *allocator,
                                          QJSIntlLocaleMetadata *out);

/* Exact source facts, without alias substitution or service capability policy.
   aliases is a space-separated token list, including legacy non-BCP47 spellings.
   Borrow slices from an unchanged validated owner; queries allocate nothing and
   reset non-NULL outputs on failure. find returns UNSUPPORTED for absent exact
   extension/key/name tuples. Follow pack.h's disjoint-output contract. */
QJSIntlStatus qjs_intl_locale_type_at(const QJSIntlLocaleMetadata *, uint32_t index,
                                    QJSIntlLocaleType *);
QJSIntlStatus qjs_intl_locale_type_find(const QJSIntlLocaleMetadata *, QJSIntlBytes extension,
                                      QJSIntlBytes key, QJSIntlBytes name, uint32_t *index);

#endif
