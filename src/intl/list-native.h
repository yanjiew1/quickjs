/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_LIST_NATIVE_H
#define QJS_INTL_LIST_NATIVE_H
#include "provider.h"

typedef enum QJSIntlListType {
    QJS_INTL_LIST_CONJUNCTION = 0,
    QJS_INTL_LIST_DISJUNCTION = 1,
    QJS_INTL_LIST_UNIT = 2
} QJSIntlListType;
typedef enum QJSIntlListStyle {
    QJS_INTL_LIST_LONG = 0,
    QJS_INTL_LIST_SHORT = 1,
    QJS_INTL_LIST_NARROW = 2
} QJSIntlListStyle;
/* Alternate row predicates follow the pinned ICU78.3 rules; Hebrew Script
 * ranges are generated from pinned Unicode18, never a guessed block range. */
typedef enum QJSIntlListContext {
    QJS_INTL_LIST_CONTEXT_NONE = 0,
    QJS_INTL_LIST_CONTEXT_SPANISH_AND = 1,
    QJS_INTL_LIST_CONTEXT_SPANISH_OR = 2,
    QJS_INTL_LIST_CONTEXT_HEBREW_AND = 3
} QJSIntlListContext;

typedef struct QJSIntlNativeList QJSIntlNativeList;
typedef struct QJSIntlListScriptRange {
    uint32_t first, last;         /* inclusive Unicode scalar range */
} QJSIntlListScriptRange;
typedef struct QJSIntlListTemplates {
    QJSIntlBytes base[4];
    QJSIntlBytes alternate[4];    /* required for nonzero context */
    QJSIntlListContext context;
    const QJSIntlListScriptRange *hebrew_script;
    size_t hebrew_script_count;   /* required for Hebrew context */
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
                                      QJSIntlNativeList **out);
void qjs_intl_native_list_close(QJSIntlNativeList *);
/* JS conversion/iterator operations remain in the frontend. This operation
 * borrows exact UTF16 slices for this call and owns the returned text/parts.
 * Every element is emitted once, including zero-length strings. Literal
 * pieces remain separate, matching DeconstructPattern. Output must start
 * empty and must not overlap inputs; every failure resets it to all-zero.
 * No trailing NUL is promised. Inputs remain unchanged for the call.
 */
QJSIntlStatus qjs_intl_native_list_format(const QJSIntlNativeList *,
                                        const QJSIntlUTF16 *items,
                                        size_t count, QJSIntlFormatted *out);
/* Results survive handle close; clear with the same allocator callbacks and
 * opaque used by open. Accepts NULL result; repeat clear is harmless.
 */
void qjs_intl_native_list_result_clear(const QJSIntlAllocator *,
                                      QJSIntlFormatted *);
#endif
