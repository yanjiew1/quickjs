/* ECMA-402 measurement-unit grammar, independent of locale/provider data.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_UNIT_IDENTIFIER_H
#define QJS_INTL_UNIT_IDENTIFIER_H
#include "../common/types.h"

#define QJS_INTL_NO_UNIT UINT32_MAX

typedef struct QJSIntlUnitDefinition {
    const char *identifier, *cldr_identifier; /* Immutable NUL-terminated ASCII. */
} QJSIntlUnitDefinition;

typedef struct QJSIntlUnitIdentifier {
    uint32_t numerator, denominator; /* Definition indexes; NO_UNIT if absent. */
} QJSIntlUnitIdentifier;

uint32_t qjs_intl_unit_definition_count(void);
/* Out-of-range indexes return NULL. Returned definitions have static lifetime. */
const QJSIntlUnitDefinition *qjs_intl_unit_definition_at(uint32_t index);

/* Exact, case-sensitive sanctioned single identifier, or NO_UNIT. The input
   is a borrowed slice and need not be NUL-terminated. No operation allocates. */
uint32_t qjs_intl_unit_identifier_find(QJSIntlBytes);

/* A single sanctioned unit or exactly two separated by "-per-". No aliases,
   multiplication, case folding or arbitrary CLDR units are accepted. JS
   coercion belongs to the binding. Disjoint output is required; non-NULL
   output indexes become NO_UNIT on failure. */
QJSIntlStatus qjs_intl_unit_identifier_parse(QJSIntlBytes, QJSIntlUnitIdentifier *);

/* Static CLDR long identifier for a sanctioned single or a known precomposed
   division. NULL means invalid indexes or no precomposed form; it does not
   make an otherwise sanctioned division invalid. No locale lookup/fallback.
   Do not derive the legacy CLDR grouping prefix from the numerator's group. */
const char *qjs_intl_unit_identifier_cldr(const QJSIntlUnitIdentifier *);
#endif
