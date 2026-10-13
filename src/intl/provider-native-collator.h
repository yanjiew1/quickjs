/* Private native Collator adapter. Copyright (c) 2026 Yan-Jie Wang. */
#ifndef QJS_INTL_PROVIDER_NATIVE_COLLATOR_H
#define QJS_INTL_PROVIDER_NATIVE_COLLATOR_H
#include "provider-native.h"
#include "collator-native.h"
QJSIntlStatus qjs_intl_native_provider_collator_available(QJSIntlProvider *);
QJSIntlStatus qjs_intl_native_provider_collator_defaults(QJSIntlProvider *,
    QJSIntlBytes, QJSIntlCollatorUsage, QJSIntlCollatorOptions *);
QJSIntlStatus qjs_intl_native_provider_collator_open(QJSIntlProvider *,
    QJSIntlBytes, QJSIntlBytes, const QJSIntlCollatorOptions *,
    QJSIntlNativeCollator **);
/* Default-first list: co has one {NULL,0}; kn/kf have owned terminated bytes.
 * Existing qjs_intl_tag_list_clear tolerates and clears the null element. */
QJSIntlStatus qjs_intl_native_provider_collator_key_values(QJSIntlProvider *,
    QJSIntlService, QJSIntlBytes, QJSIntlBytes, QJSIntlTagList *);
#endif
