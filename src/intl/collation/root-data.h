/* Root UCA mappings; storage lookup only, without comparison policy.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_COLLATION_ROOT_DATA_H
#define QJS_INTL_COLLATION_ROOT_DATA_H
#include "../native/pack.h"

typedef struct QJSIntlCollationRoot {
    QJSIntlSection section;
    const uint8_t *mappings, *keys, *elements;
    uint32_t key_count, element_count;
} QJSIntlCollationRoot;

typedef struct QJSIntlCollationEntry {
    const uint8_t *keys, *elements;
    uint32_t key_count, element_count;
} QJSIntlCollationEntry;

typedef struct QJSIntlCollationElement {
    uint16_t primary, secondary, tertiary;
    unsigned int variable;
} QJSIntlCollationElement;

/* Borrow an unchanged validated pack, independent of locale/string tables.
   Validate the optional section without allocation; missing is UNSUPPORTED.
   Views/entries borrow immutable pack bytes. Follow pack.h's disjoint-output
   contract; non-NULL outputs are reset on failure. No teardown is needed. */
QJSIntlStatus qjs_intl_collation_root_open(const QJSIntlPack *, QJSIntlCollationRoot *);
QJSIntlStatus qjs_intl_collation_root_entry_at(const QJSIntlCollationRoot *, uint32_t,
                                              QJSIntlCollationEntry *);
QJSIntlStatus qjs_intl_collation_key_at(const QJSIntlCollationEntry *, uint32_t, uint32_t *);
QJSIntlStatus qjs_intl_collation_element_at(const QJSIntlCollationEntry *, uint32_t,
                                            QJSIntlCollationElement *);
/* Exact or longest contiguous scalar-prefix lookup. Empty/missing keys return
   UNSUPPORTED. Values outside the stored scalar domain simply do not match;
   do not scan an entire remaining string on every prefix lookup. The result's
   key_count is the consumed prefix length. Normalization, implicit weights and
   discontiguous contractions belong to the comparator, not this reader. */
QJSIntlStatus qjs_intl_collation_root_find(const QJSIntlCollationRoot *, const uint32_t *, size_t,
                                          QJSIntlCollationEntry *);
QJSIntlStatus qjs_intl_collation_root_prefix(const QJSIntlCollationRoot *, const uint32_t *, size_t,
                                            QJSIntlCollationEntry *);
#endif
