/* Private native provider and lifetime contracts. Copyright (c) 2026 Yan-Jie Wang. */
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
/* Private policy update: canonical en/en-US only. Atomic allocation failure;
 * changing the default leaves metadata and all borrowing handles alive. */
QJSIntlStatus qjs_intl_native_provider_set_default_locale(QJSIntlProvider *,
                                                        QJSIntlBytes);
void qjs_intl_native_provider_memory_usage(const QJSIntlProvider *,
                                           size_t *count, size_t *bytes);
/* Borrowed immutable canonical data; provider/blob outlive its use. */
const QJSIntlDataView *qjs_intl_native_provider_view(const QJSIntlProvider *);
/* Private callback: accepts only the exact live owner view over this module's
 * immutable compiled blob with a successful whole-reader certificate. */
QJSIntlDataStatus qjs_intl_native_provider_validate_compiled_data(void *provider,
    const QJSIntlDataView *);
/* Date's installed en/en-US policy shares the internally mapped en row.
 * Cache only a successful proof over this provider's immutable compiled data. */
QJSIntlStatus qjs_intl_native_provider_date_calendar_mask(QJSIntlProvider *,
                                                        uint32_t *out);
/* Private capability inventory cache. Caller validates the exact en/en-US
 * locale policy, decimal numbering row and canonical calendar before lookup,
 * and stores only after a successful complete probe. -1 denotes the shared
 * CLDR US preferred-hour policy; explicit cycles 0..3 have separate proofs.
 * Entries use the provider allocator and die with it; access is serialized
 * under the same contract as other provider mutations. Full caches fall back
 * to uncached successful probes without another allocation. */
int qjs_intl_native_provider_date_probe_success(const QJSIntlProvider *,
    uint32_t numbering, uint32_t calendar, int requested_cycle);
QJSIntlStatus qjs_intl_native_provider_date_probe_success_store(QJSIntlProvider *,
    uint32_t numbering, uint32_t calendar, int requested_cycle);
/* Exact candidate row lookup and inherited numeric default in the compiled
 * metadata. Returned bytes are borrowed until provider destruction. Neither
 * helper grants service admission from metadata identity alone. */
QJSIntlStatus qjs_intl_native_provider_locale_row(const QJSIntlProvider *,
    QJSIntlBytes, uint32_t *);
QJSIntlStatus qjs_intl_native_provider_default_numbering(const QJSIntlProvider *,
    uint32_t, QJSIntlBytes *);
#endif
