/* Private native development provider. Copyright (c) 2026 Yan-Jie Wang. */
#ifndef QJS_INTL_PROVIDER_NATIVE_H
#define QJS_INTL_PROVIDER_NATIVE_H
#include "provider.h"
#include "list-native-data.h"

/* List handles copy their data and allocator. They do not retain provider.
 * The allocator opaque must remain alive until handle/result destruction. */
const QJSIntlAllocator *qjs_intl_native_provider_allocator(const QJSIntlProvider *);
QJSIntlStatus qjs_intl_native_provider_list_open(QJSIntlProvider *,
    QJSIntlBytes resolved_locale, QJSIntlListType, QJSIntlListStyle,
    QJSIntlNativeList **out);
void qjs_intl_native_provider_memory_usage(const QJSIntlProvider *,
                                           size_t *count, size_t *bytes);
#endif
