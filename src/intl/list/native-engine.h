/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_LIST_NATIVE_ENGINE_H
#define QJS_INTL_LIST_NATIVE_ENGINE_H
#include "list.h"

/* Alternate predicates follow CLDR, cross-checked with ICU4X and ICU78.3.
 * Hebrew Script ranges come from Unicode data, never guessed block ranges. */
typedef enum QJSIntlListContext {
    QJS_INTL_LIST_CONTEXT_NONE = 0,
    QJS_INTL_LIST_CONTEXT_SPANISH_AND = 1,
    QJS_INTL_LIST_CONTEXT_SPANISH_OR = 2,
    QJS_INTL_LIST_CONTEXT_HEBREW_AND = 3
} QJSIntlListContext;

typedef struct QJSIntlListScriptRange {
    uint32_t first, last; /* inclusive Unicode scalar range */
} QJSIntlListScriptRange;
typedef struct QJSIntlListTemplates {
    QJSIntlBytes base[4];
    QJSIntlBytes alternate[2]; /* pair, end; required for nonzero context */
    QJSIntlListContext context;
    const QJSIntlListScriptRange *hebrew_script;
    size_t hebrew_script_count; /* required for Hebrew context */
} QJSIntlListTemplates;
/* Pattern order: pair, start, middle, end. Exact UTF8 scalar pattern strings,
 * each containing {0} and {1} once. Non-token braces are invalid. Pattern
 * apostrophes are literal (ECMA-402 PartitionPattern, not MessageFormat).
 * open copies and decodes patterns and any Script ranges. Input and allocator
 * structures remain unchanged for the call; allocator opaque lives until close and
 * every corresponding result clear. No ICU, JS, or C locale operations.
 */
QJSIntlStatus qjs_intl_native_list_open(const QJSIntlAllocator *,
                                        const QJSIntlListTemplates *,
                                        QJSIntlListFormatter **out);
#endif
