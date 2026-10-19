/* List service contract, independent of backend data and JS objects.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_LIST_H
#define QJS_INTL_LIST_H
#include "../common/format.h"

typedef enum QJSIntlListType {
    QJS_INTL_LIST_CONJUNCTION,
    QJS_INTL_LIST_DISJUNCTION,
    QJS_INTL_LIST_UNIT
} QJSIntlListType;

typedef enum QJSIntlListStyle {
    QJS_INTL_LIST_LONG,
    QJS_INTL_LIST_SHORT,
    QJS_INTL_LIST_NARROW
} QJSIntlListStyle;

typedef struct QJSIntlListService QJSIntlListService;
typedef struct QJSIntlListFormatter QJSIntlListFormatter;

/* The selected compiled backend owns its available-locale inventory. Open
   copies the allocator; callbacks and opaque state must outlive all handles
   and results. Failure clears *out. Closing NULL is safe. No JS callbacks. */
QJSIntlStatus qjs_intl_list_open(const QJSIntlAllocator *allocator,
                                 QJSIntlListService **out);
void qjs_intl_list_close(QJSIntlListService *service);
/* Borrow a canonical, sorted, unique inventory without extensions or private
   use. The array and its text remain valid until service close. Root is not
   an available locale. No default selection or matching happens here. */
QJSIntlStatus qjs_intl_list_available(const QJSIntlListService *service,
                                      const QJSIntlBytes **locales, size_t *count);
/* locale indexes the inventory above. Formatters own their resolved pattern
   state and may outlive the service. Inputs remain unchanged. */
QJSIntlStatus qjs_intl_list_formatter_open(const QJSIntlListService *service,
                                           size_t locale, QJSIntlListType type,
                                           QJSIntlListStyle style,
                                           QJSIntlListFormatter **out);
void qjs_intl_list_formatter_close(QJSIntlListFormatter *formatter);
/* Borrow exact UTF-16 items for this call; preserve NUL and lone surrogates.
   The empty output must not overlap inputs. Failures reset it to all zero.
   Results outlive formatter/service close and use the original allocator.
   Elements are emitted once, including empty strings; literal pieces remain
   separate as required by DeconstructPattern. No trailing NUL is promised.
   Clearing a NULL or already cleared result is harmless. */
QJSIntlStatus qjs_intl_list_format(const QJSIntlListFormatter *formatter,
                                   const QJSIntlUTF16 *items, size_t count,
                                   QJSIntlFormatted *out);
void qjs_intl_list_result_clear(const QJSIntlAllocator *allocator,
                                QJSIntlFormatted *result);
#endif
