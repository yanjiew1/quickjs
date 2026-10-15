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
/* Complete successful Date defaults for one authenticated compiled locale.
 * No borrowed strings, preference lists or formatter handles are retained. */
typedef struct QJSIntlNativeDateDefaults {
    uint32_t calendar;
    unsigned int hour_cycles[3]; /* default, 12-hour family, 24-hour family */
} QJSIntlNativeDateDefaults;
int qjs_intl_native_provider_date_defaults_get(const QJSIntlProvider *,
    uint32_t locale, QJSIntlNativeDateDefaults *);
void qjs_intl_native_provider_date_defaults_store(QJSIntlProvider *,
    uint32_t locale, const QJSIntlNativeDateDefaults *);
/* Successful masks, including zero, are keyed by the actual compiled row.
 * At capacity, the complete projection is performed without storing it. */
QJSIntlStatus qjs_intl_native_provider_date_calendar_mask(QJSIntlProvider *,
    uint32_t locale, uint32_t *out);
/* Capability proofs include actual locale/numbering/calendar/effective cycle.
 * Validate identities before lookup and store only after a complete probe.
 * Entries own only allocator storage, die with the provider, and are mutated
 * under its serialized access contract. Full caches keep uncached behavior. */
int qjs_intl_native_provider_date_probe_success(const QJSIntlProvider *,
    uint32_t locale, uint32_t numbering, uint32_t calendar, unsigned int effective_cycle);
QJSIntlStatus qjs_intl_native_provider_date_probe_success_store(QJSIntlProvider *,
    uint32_t locale, uint32_t numbering, uint32_t calendar, unsigned int effective_cycle);
/* Exact candidate row lookup and inherited numeric default in the compiled
 * metadata. Returned bytes are borrowed until provider destruction. Neither
 * helper grants service admission from metadata identity alone. */
QJSIntlStatus qjs_intl_native_provider_locale_row(const QJSIntlProvider *,
    QJSIntlBytes, uint32_t *);
QJSIntlStatus qjs_intl_native_provider_default_numbering(const QJSIntlProvider *,
    uint32_t, QJSIntlBytes *);
#endif
