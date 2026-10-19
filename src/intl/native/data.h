/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_NATIVE_DATA_H
#define QJS_INTL_NATIVE_DATA_H
#include "pack.h"

typedef struct QJSIntlData {
    QJSIntlPack pack;
    QJSIntlSection strings;
    QJSIntlSection locales;
    QJSIntlSection lists;
    QJSIntlSection list_contexts;
} QJSIntlData;

/* The selected prepared profile has exactly one generated C owner. */
extern const uint8_t qjs_intl_native_bytes[];
extern const size_t qjs_intl_native_length;

/* Validate supported wire tables and references. The allocator supplies only
   temporary graph-validation memory, released before return. An open view
   borrows immutable data and requires no teardown. This does not authenticate
   a pack, canonicalize locale spellings, or establish formatter completeness.
   Required string/locale tables must be present. Lists are optional. Context
   tables require base lists and are checked against the exact CLDR defaults
   for each locale and variant, not just their structural wire schema.
   Follow pack.h's output aliasing and immutable-view contract. */
QJSIntlStatus qjs_intl_data_open(const void *bytes, size_t length,
                               const QJSIntlAllocator *allocator,
                               QJSIntlData *out);
QJSIntlStatus qjs_intl_data_string(const QJSIntlData *data, uint32_t index,
                                 QJSIntlBytes *out);
QJSIntlStatus qjs_intl_data_locale(const QJSIntlData *data, uint32_t index,
                                 QJSIntlBytes *tag, uint32_t *parent);
/* variant follows the collector's nine type/style combinations; part follows
   (2, start, middle, end). The returned template is borrowed UTF-8 text. */
QJSIntlStatus qjs_intl_data_list_template(const QJSIntlData *data, uint32_t locale,
                                        unsigned int variant, unsigned int part,
                                        QJSIntlBytes *out);

/* Context kinds follow list/native-engine.h: none, Spanish conjunction,
   Spanish disjunction, Hebrew conjunction. A disabled context has empty
   pair/end outputs. Missing context data is UNSUPPORTED, never a claim that
   base patterns alone implement all contextual behavior. */
QJSIntlStatus qjs_intl_data_list_context(const QJSIntlData *data, uint32_t locale,
                                       unsigned int variant, unsigned int *kind,
                                       QJSIntlBytes *pair, QJSIntlBytes *end);

#endif
