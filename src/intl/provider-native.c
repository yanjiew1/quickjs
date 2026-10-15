/* Native Intl provider. Copyright (c) 2026 Yan-Jie Wang.
 * Compile this source only for CONFIG_INTL_NATIVE. ICU is selected separately.
 * Services independently admit compiled locale rows; most initially retain
 * the English inventory. Metadata identity alone does not grant availability. */
#ifdef CONFIG_ICU
#error "Native Intl provider data requires CONFIG_ICU to be disabled"
#endif
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <stdatomic.h>
#endif
#include "provider-native.h"
#include "native-data-validation-internal.h"
#include "provider-native-display.h"
#include "provider-native-relative.h"
#include "provider-native-plural.h"
#include "provider-native-duration.h"
#include "provider-native-segmenter.h"
#include "provider-native-collator.h"
#include "provider-native-number.h"
#include "provider-native-date.h"
#include "native-locale-info.h"
#include "provider-native-general.h"
#include "data/locale-metadata.h" /* externs; generated .c has the only owner */
#include "../timezone/timezone.h"
#include <string.h>

/* Capability proofs only: actual formatter banks always open independently.
 * At capacity, probes keep their normal uncached behavior. */
#define DATE_PROBE_CACHE_LIMIT 256
struct DateProbeSuccess {
    struct DateProbeSuccess *next;
    uint32_t numbering, calendar;
    int requested_cycle;
};

struct QJSIntlProvider {
    QJSIntlAllocator allocator;
    QJSIntlDataView view;
    uint32_t list_locale;
    uint32_t date_calendar_mask;
    unsigned char date_calendar_mask_ready;
    struct DateProbeSuccess *date_probe_success;
    size_t date_probe_success_count;
    char *default_locale;
    /* qjs_tz_resolve returns a string inside immutable compiled data. */
    const char *default_time_zone;
    QJSIntlLocaleInfoCapabilities capabilities;
    QJSIntlDataVersions versions;
};

/* Only this module's immutable compiled blob is cached. No shared
 * view or provider resources are retained. Racing cold validations may
 * duplicate work; only reader success is retained, even if the provider
 * subsequently fails its own admission or allocation. */
#ifdef _WIN32
static volatile LONG compiled_metadata_valid = 0;
static volatile LONG compiled_service_valid = 0;
static volatile LONG compiled_locale_service_valid[256];
static int compiled_metadata_is_valid(void)
{
    return InterlockedCompareExchange(&compiled_metadata_valid, 0, 0);
}
static void compiled_metadata_mark_valid(void)
{
    InterlockedExchange(&compiled_metadata_valid, 1);
}
#else
static _Atomic unsigned int compiled_metadata_valid = 0;
static _Atomic unsigned int compiled_service_valid = 0;
static _Atomic unsigned int compiled_locale_service_valid[256];
static int compiled_metadata_is_valid(void)
{
    return atomic_load_explicit(&compiled_metadata_valid,
                                memory_order_acquire);
}
static void compiled_metadata_mark_valid(void)
{
    atomic_store_explicit(&compiled_metadata_valid, 1,
                          memory_order_release);
}
#endif
/* These probes certify the fixed English en/en-US service inventory.
 * A broader inventory must bind proofs to its actual locale/service rows.
 * A successful, option-independent service probe authenticates only this
 * immutable compiled bundle. No handles, allocators or failures are cached.
 * Concurrent cold probes may overlap; publishing a success never blocks. */
_Static_assert(QJS_INTL_SERVICE_COUNT <= 32, "service proof mask capacity");
static int compiled_service_is_valid(QJSIntlService service)
{
#ifdef _WIN32
    unsigned int bits = (unsigned int)InterlockedCompareExchange(&compiled_service_valid, 0, 0);
#else
    unsigned int bits = atomic_load_explicit(&compiled_service_valid, memory_order_acquire);
#endif
    return (bits & (1u << (unsigned int)service)) != 0;
}
static void compiled_service_mark_valid(QJSIntlService service)
{
    unsigned int bit = 1u << (unsigned int)service;
#ifdef _WIN32
    LONG before, after;
    do {
        before = InterlockedCompareExchange(&compiled_service_valid, 0, 0);
        after = (LONG)((unsigned int)before | bit);
    } while (InterlockedCompareExchange(&compiled_service_valid, after, before) != before);
#else
    atomic_fetch_or_explicit(&compiled_service_valid, bit, memory_order_release);
#endif
}
static QJSIntlDataStatus open_compiled_metadata(QJSIntlDataView *out)
{
    QJSIntlDataView view;
    QJSIntlDataStatus status;
    if (!out) return QJS_INTL_DATA_INVALID_ARGUMENT;
    out->data = NULL;
    out->length = 0;
    if (!compiled_metadata_is_valid()) {
        status = qjs_intl_data_open(qjs_intl_locale_metadata_blob,
            qjs_intl_locale_metadata_blob_size, &view);
        if (status != QJS_INTL_DATA_OK) return status;
        compiled_metadata_mark_valid();
    } else {
        view.data = qjs_intl_locale_metadata_blob;
        view.length = qjs_intl_locale_metadata_blob_size;
    }
    *out = view;
    return QJS_INTL_DATA_OK;
}

static QJSIntlBytes bytes(const char *s)
{
    QJSIntlBytes result = { s, s ? strlen(s) : 0 };
    return result;
}
static int same(QJSIntlBytes value, const char *s)
{
    size_t n = strlen(s);
    return value.data && value.length == n && !memcmp(value.data, s, n);
}
static uint32_t header_word(const QJSIntlDataView *view, size_t offset)
{
    const unsigned char *p = view->data + offset;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static QJSIntlStatus require_metadata(const QJSIntlDataView *view)
{
    static const uint32_t required[] = {
        QJS_INTL_DATA_WEEK, QJS_INTL_DATA_PREFERENCE,
        QJS_INTL_DATA_LIST, QJS_INTL_DATA_SCRIPT_DIRECTION
    };
    QJSIntlDataSection section;
    QJSIntlDataStatus status;
    size_t i;
    /* Reader acceptance alone does not establish this profile's metadata. */
    if (header_word(view, QJS_INTL_H_UNICODE_VERSION) != (18u << 16) ||
        header_word(view, QJS_INTL_H_CLDR_VERSION) != QJS_INTL_CLDR_RELEASE_48_2)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < sizeof(required) / sizeof(required[0]); i++) {
        status = qjs_intl_data_section(view, required[i], &section);
        if (status == QJS_INTL_DATA_NOT_FOUND) return QJS_INTL_UNSUPPORTED;
        if (status != QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
        if (!section.record_count) return QJS_INTL_UNSUPPORTED;
    }
    return QJS_INTL_OK;
}
static QJSIntlStatus find_list_locale(QJSIntlProvider *p)
{
    QJSIntlDataSection locales;
    QJSIntlDataSlice tag;
    uint32_t i;
    if (qjs_intl_data_section(&p->view, QJS_INTL_DATA_LOCALE, &locales) !=
            QJS_INTL_DATA_OK)
        return QJS_INTL_UNSUPPORTED;
    for (i = 0; i < locales.record_count; i++) {
        if (qjs_intl_data_record_string(&p->view, &locales, i, 0, &tag) !=
                QJS_INTL_DATA_OK)
            return QJS_INTL_DATA_ERROR;
        if (tag.length == 2 && !memcmp(tag.data, "en", 2)) {
            p->list_locale = i;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}
QJSIntlStatus qjs_intl_provider_new(const QJSIntlProviderConfig *config,
                                    QJSIntlProvider **out)
{
    QJSIntlProvider *p;
    QJSIntlStatus status;
    QJSIntlNativeList *list = NULL;
    char *canonical = NULL;
    const char *canonical_zone = "UTC";
    unsigned int type, style;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!config || config->backend != QJS_INTL_BACKEND_NATIVE ||
        !config->allocator.malloc || !config->allocator.realloc ||
        !config->allocator.free ||
        (!config->default_locale.data && config->default_locale.length) ||
        (!config->default_time_zone.data && config->default_time_zone.length))
        return QJS_INTL_INVALID_ARGUMENT;
    p = config->allocator.malloc(config->allocator.opaque, sizeof(*p));
    if (!p) return QJS_INTL_NO_MEMORY;
    memset(p, 0, sizeof(*p));
    p->allocator = config->allocator;
    if (open_compiled_metadata(&p->view) != QJS_INTL_DATA_OK) {
        status = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    status = require_metadata(&p->view);
    if (status != QJS_INTL_OK) goto fail;
    status = qjs_intl_native_locale_canonicalize(&p->allocator, &p->view,
        config->default_locale.data ? config->default_locale : bytes("en-US"),
        &canonical);
    if (status != QJS_INTL_OK) goto fail;
    /* Native host configuration admits en/en-US with real service data.
     * The frontend selects the realm's initial en-US policy or an explicit
     * embedding setting; this provider does not discover an OS locale.
     * DefaultLocale and its required less narrow fallback both exist. */
    if (strcmp(canonical, "en-US") && strcmp(canonical, "en")) {
        status = QJS_INTL_UNSUPPORTED;
        goto fail;
    }
    if (config->default_time_zone.data &&
        qjs_tz_resolve(config->default_time_zone.data,
                       config->default_time_zone.length, &canonical_zone, NULL)) {
        status = QJS_INTL_UNSUPPORTED;
        goto fail;
    }
    p->default_locale = canonical;
    p->default_time_zone = canonical_zone;
    canonical = NULL;
    p->versions.provider = bytes("native");
    p->versions.unicode = bytes("18.0.0");
    p->versions.cldr = bytes("48.2.0");
    p->versions.collation = bytes("17.0.0");
    p->versions.tzdata = bytes("2026e");
    p->versions.input_manifest_sha256 = bytes("8b9c06ad8183237e11bbb9145ffa988a452a88ef2d5cea9b3478fad2cbe48cf2");
    /* The sole released embedding and its input manifest digest (section2)
     * are pinned by the source stage. Record the validated schema version. */
    p->versions.schema_version =
        ((uint32_t)p->view.data[8] | (uint32_t)p->view.data[9] << 8) << 16 |
        (uint32_t)p->view.data[10] | (uint32_t)p->view.data[11] << 8;
    status = find_list_locale(p);
    if (status != QJS_INTL_OK) goto fail;
    /* Admit this small locale policy only with every ListFormat option row.
     * No metadata coverage bit is added or inferred from unrelated engines. */
    for (type = 0; type < 3; type++) {
        for (style = 0; style < 3; style++) {
            status = qjs_intl_native_list_open_data(&p->allocator, &p->view,
                p->list_locale, (QJSIntlListType)type,
                (QJSIntlListStyle)style, &list);
            if (status != QJS_INTL_OK) goto fail;
            qjs_intl_native_list_close(list);
            list = NULL;
        }
    }
    *out = p;
    return QJS_INTL_OK;
fail:
    qjs_intl_native_list_close(list);
    p->allocator.free(p->allocator.opaque, canonical);
    qjs_intl_provider_free(p);
    return status;
}
void qjs_intl_provider_free(QJSIntlProvider *p)
{
    QJSIntlAllocator a;
    struct DateProbeSuccess *entry, *next;
    if (!p) return;
    a = p->allocator;
    for (entry = p->date_probe_success; entry; entry = next) {
        next = entry->next;
        a.free(a.opaque, entry);
    }
    a.free(a.opaque, p->default_locale);
    a.free(a.opaque, p);
}

QJSIntlStatus qjs_intl_native_provider_set_default_locale(QJSIntlProvider *p,
                                                        QJSIntlBytes locale)
{
    char *copy, *old;
    if (!p || !locale.data)
        return QJS_INTL_INVALID_ARGUMENT;
    /* The frontend canonicalizes and removes 'u'. Admit only the profile with
       proved patterns and fallback closure. Keep metadata/handle lifetimes. */
    if (!same(locale, "en") && !same(locale, "en-US"))
        return QJS_INTL_UNSUPPORTED;
    copy = p->allocator.malloc(p->allocator.opaque, locale.length + 1);
    if (!copy)
        return QJS_INTL_NO_MEMORY;
    memcpy(copy, locale.data, locale.length);
    copy[locale.length] = 0;
    old = p->default_locale;
    p->default_locale = copy;
    p->allocator.free(p->allocator.opaque, old);
    return QJS_INTL_OK;
}

const QJSIntlAllocator *qjs_intl_native_provider_allocator(const QJSIntlProvider *p)
{
    return p ? &p->allocator : NULL;
}
QJSIntlStatus qjs_intl_locale_canonicalize(QJSIntlProvider *p, QJSIntlBytes tag,
                                           char **out)
{
    if (!p) { if (out) *out = NULL; return QJS_INTL_INVALID_ARGUMENT; }
    return qjs_intl_native_locale_canonicalize(&p->allocator, &p->view, tag, out);
}
QJSIntlStatus qjs_intl_locale_canonicalize_uvalue(QJSIntlProvider *p,
    QJSIntlBytes key, QJSIntlBytes value, char **out)
{
    if (!p) { if (out) *out = NULL; return QJS_INTL_INVALID_ARGUMENT; }
    return qjs_intl_native_locale_canonicalize_uvalue(&p->allocator, &p->view,
                                                    key, value, out);
}
QJSIntlStatus qjs_intl_locale_maximize(QJSIntlProvider *p, QJSIntlBytes tag,
                                      char **out)
{
    if (!p) { if (out) *out = NULL; return QJS_INTL_INVALID_ARGUMENT; }
    return qjs_intl_native_locale_maximize(&p->allocator, &p->view, tag, out);
}
QJSIntlStatus qjs_intl_locale_minimize(QJSIntlProvider *p, QJSIntlBytes tag,
                                      char **out)
{
    if (!p) { if (out) *out = NULL; return QJS_INTL_INVALID_ARGUMENT; }
    return qjs_intl_native_locale_minimize(&p->allocator, &p->view, tag, out);
}
QJSIntlStatus qjs_intl_locale_info_get(QJSIntlProvider *p,
    const QJSIntlLocaleInfoRequest *request, QJSIntlLocaleInfoField field,
    QJSIntlLocaleInfoResult *out)
{
    if (!p) { if (out) memset(out, 0, sizeof(*out)); return QJS_INTL_INVALID_ARGUMENT; }
    return qjs_intl_native_provider_locale_info_get(&p->allocator, &p->view,
                                                   p, request, field, out);
}
void qjs_intl_locale_info_result_clear(QJSIntlProvider *p,
                                       QJSIntlLocaleInfoResult *result)
{
    if (p) qjs_intl_native_locale_info_result_clear(&p->allocator, result);
}
void qjs_intl_tag_list_clear(QJSIntlProvider *p, QJSIntlTagList *list)
{
    size_t i;
    if (!p || !list) return;
    for (i = 0; i < list->count; i++)
        p->allocator.free(p->allocator.opaque, (void *)list->items[i].data);
    p->allocator.free(p->allocator.opaque, list->items);
    memset(list, 0, sizeof(*list));
}
void qjs_intl_owned_tag_clear(QJSIntlProvider *p, char *tag)
{
    if (p) p->allocator.free(p->allocator.opaque, tag);
}
/* The probe receives the exact tag stored at its authenticated locale row.
 * It must establish an option-independent service proof, without retaining
 * borrowed data or making the result depend on mutable provider defaults. */
typedef QJSIntlStatus (*LocaleProbe)(QJSIntlProvider *, QJSIntlBytes, uint32_t);
static QJSIntlStatus available_compiled_locales(QJSIntlProvider *, QJSIntlService,
    LocaleProbe, QJSIntlTagList *);
static QJSIntlStatus list_locale_available(QJSIntlProvider *p,
    QJSIntlBytes locale, uint32_t row)
{
    unsigned int type, style;
    (void)row; /* list_open resolves the same authenticated tag to its row. */
    for (type = 0; type < 3; type++) {
        for (style = 0; style < 3; style++) {
            QJSIntlNativeList *list = NULL;
            QJSIntlStatus status = qjs_intl_native_provider_list_open(p, locale,
                (QJSIntlListType)type, (QJSIntlListStyle)style, &list);
            qjs_intl_native_list_close(list);
            if (status != QJS_INTL_OK) return status;
        }
    }
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_locale_available(QJSIntlProvider *p, QJSIntlService service,
                                       QJSIntlTagList *out)
{
    size_t i;
    static const char *const tags[] = { "en", "en-US" };
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!p || (unsigned int)service >= QJS_INTL_SERVICE_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    if (service == QJS_INTL_NUMBER_FORMAT)
        return available_compiled_locales(p, service,
            qjs_intl_native_provider_number_locale_available, out);
    if (service == QJS_INTL_PLURAL_RULES)
        return available_compiled_locales(p, service,
            qjs_intl_native_provider_plural_locale_available, out);
    if (service == QJS_INTL_LIST_FORMAT)
        return available_compiled_locales(p, service, list_locale_available, out);
    if (service == QJS_INTL_DATE_TIME_FORMAT)
        return qjs_intl_native_provider_date_available(p, out);
    if (qjs_intl_native_provider_validate_compiled_data(p, &p->view) != QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    if (!compiled_service_is_valid(service)) {
        if (service == QJS_INTL_COLLATOR || service == QJS_INTL_COLLATOR_SEARCH) {
            QJSIntlStatus status = qjs_intl_native_provider_collator_available(p);
            if (status != QJS_INTL_OK) return status;
        } else if (service == QJS_INTL_SEGMENTER) {
            QJSIntlStatus status = qjs_intl_native_provider_segmenter_available(p);
            if (status != QJS_INTL_OK) return status;
        } else if (service == QJS_INTL_DURATION_FORMAT) {
            QJSIntlStatus status = qjs_intl_native_provider_duration_available(p);
            if (status != QJS_INTL_OK) return status;
        } else if (service == QJS_INTL_RELATIVE_TIME_FORMAT) {
            QJSIntlStatus status = qjs_intl_native_provider_relative_available(p);
            if (status != QJS_INTL_OK) return status;
        } else if (service == QJS_INTL_DISPLAY_NAMES) {
            QJSIntlStatus status = qjs_intl_native_provider_display_available(p);
            if (status != QJS_INTL_OK) return status;
        } else return QJS_INTL_UNSUPPORTED;
        compiled_service_mark_valid(service);
    }
    out->items = p->allocator.malloc(p->allocator.opaque, sizeof(*out->items) * 2);
    if (!out->items) return QJS_INTL_NO_MEMORY;
    memset(out->items, 0, sizeof(*out->items) * 2);
    for (i = 0; i < 2; i++) {
        char *copy = p->allocator.malloc(p->allocator.opaque, strlen(tags[i]) + 1);
        if (!copy) { qjs_intl_tag_list_clear(p, out); return QJS_INTL_NO_MEMORY; }
        strcpy(copy, tags[i]);
        out->items[i] = bytes(copy);
        out->count++;
    }
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_locale_key_values(QJSIntlProvider *p, QJSIntlService service,
    QJSIntlBytes locale, QJSIntlBytes key, QJSIntlTagList *out)
{
    if (service == QJS_INTL_RELATIVE_TIME_FORMAT &&
        !same(locale, "en") && !same(locale, "en-US")) {
        /* Number's expanded inventory does not admit Relative locales. */
        if (!out) return QJS_INTL_INVALID_ARGUMENT;
        memset(out, 0, sizeof(*out));
        return p ? QJS_INTL_UNSUPPORTED : QJS_INTL_INVALID_ARGUMENT;
    }
    if (service == QJS_INTL_NUMBER_FORMAT || service == QJS_INTL_RELATIVE_TIME_FORMAT)
        return qjs_intl_native_provider_number_key_values(p, locale, key, out);
    if (service == QJS_INTL_COLLATOR || service == QJS_INTL_COLLATOR_SEARCH)
        return qjs_intl_native_provider_collator_key_values(p, service, locale, key, out);
    if (service == QJS_INTL_DATE_TIME_FORMAT)
        return qjs_intl_native_provider_date_key_values(p, locale, key, out);
    if (service == QJS_INTL_DURATION_FORMAT)
        return qjs_intl_native_provider_duration_key_values(p, locale, key, out);
    (void)p; (void)service; (void)locale; (void)key;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    return QJS_INTL_UNSUPPORTED;
}
QJSIntlStatus qjs_intl_locale_time_zones(QJSIntlProvider *p, QJSIntlBytes region,
                                       QJSIntlTagList *out)
{
    (void)p; (void)region;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    return QJS_INTL_UNSUPPORTED;
}
QJSIntlStatus qjs_intl_native_provider_list_open(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlListType type, QJSIntlListStyle style,
    QJSIntlNativeList **out)
{
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!p) return QJS_INTL_INVALID_ARGUMENT;
    {
        uint32_t row;
        QJSIntlStatus status = qjs_intl_native_provider_locale_row(p, locale, &row);
        if (status != QJS_INTL_OK) return status;
        return qjs_intl_native_list_open_data(&p->allocator, &p->view,
                                             row, type, style, out);
    }
}
void qjs_intl_formatted_clear(QJSIntlProvider *p, QJSIntlFormatted *result)
{
    if (p) qjs_intl_native_list_result_clear(&p->allocator, result);
}
QJSIntlBytes qjs_intl_provider_default_locale(const QJSIntlProvider *p)
{
    return bytes(p ? p->default_locale : NULL);
}
QJSIntlBytes qjs_intl_provider_default_time_zone(const QJSIntlProvider *p)
{
    return bytes(p ? p->default_time_zone : NULL);
}
const QJSIntlDataVersions *qjs_intl_provider_versions(const QJSIntlProvider *p)
{
    return p ? &p->versions : NULL;
}

void qjs_intl_native_provider_memory_usage(const QJSIntlProvider *p,
                                           size_t *count, size_t *size)
{
    if (!count || !size) return;
    *count = p ? 2 + p->date_probe_success_count : 0;
    *size = p ? sizeof(*p) + strlen(p->default_locale) + 1 +
        p->date_probe_success_count * sizeof(struct DateProbeSuccess) : 0;
}

const QJSIntlDataView *qjs_intl_native_provider_view(const QJSIntlProvider *p)
{
    return p ? &p->view : NULL;
}

static QJSIntlDataStatus validate_compiled_view(const QJSIntlProvider *p,
    const QJSIntlDataView *view)
{
    /* Reader success includes all installed service validators, including
     * Number, Plural, Relative and Date. The
     * certificate describes only the compiled blob, never decoded snapshots.
     * Reject copied views even when their pointer/length happen to match. */
    if (!p || view != &p->view ||
        view->data != qjs_intl_locale_metadata_blob ||
        view->length != qjs_intl_locale_metadata_blob_size ||
        !compiled_metadata_is_valid())
        return QJS_INTL_DATA_INVALID_ARGUMENT;
    return QJS_INTL_DATA_OK;
}

QJSIntlDataStatus qjs_intl_native_provider_validate_compiled_data(void *opaque,
    const QJSIntlDataView *view)
{
    return validate_compiled_view(opaque, view);
}

QJSIntlStatus qjs_intl_native_provider_date_calendar_mask(QJSIntlProvider *p,
                                                        uint32_t *out)
{
    QJSIntlStatus status;
    uint32_t mask;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = 0;
    if (!p) return QJS_INTL_INVALID_ARGUMENT;
    if (!p->date_calendar_mask_ready) {
        /* Date en/en-US both map to the en row found by find_list_locale.
         * The provider opens only the immutable compiled blob and retains
         * that view for its whole lifetime. Caller-owned views still use the
         * uncached data utility and must be validated on every call. */
        status = qjs_intl_native_date_data_calendar_mask_with_validator(&p->view,
            p->list_locale, qjs_intl_native_provider_validate_compiled_data, p, &mask);
        if (status != QJS_INTL_OK) return status;
        p->date_calendar_mask = mask;
        p->date_calendar_mask_ready = 1;
    }
    *out = p->date_calendar_mask;
    return QJS_INTL_OK;
}

int qjs_intl_native_provider_date_probe_success(const QJSIntlProvider *p,
    uint32_t numbering, uint32_t calendar, int requested_cycle)
{
    const struct DateProbeSuccess *entry;
    if (!p) return 0;
    for (entry = p->date_probe_success; entry; entry = entry->next) {
        if (entry->numbering == numbering && entry->calendar == calendar &&
            entry->requested_cycle == requested_cycle) return 1;
    }
    return 0;
}

QJSIntlStatus qjs_intl_native_provider_date_probe_success_store(QJSIntlProvider *p,
    uint32_t numbering, uint32_t calendar, int requested_cycle)
{
    struct DateProbeSuccess *entry;
    if (!p) return QJS_INTL_INVALID_ARGUMENT;
    if (qjs_intl_native_provider_date_probe_success(p, numbering, calendar,
                                                   requested_cycle))
        return QJS_INTL_OK;
    if (p->date_probe_success_count == DATE_PROBE_CACHE_LIMIT)
        return QJS_INTL_OK;
    entry = p->allocator.malloc(p->allocator.opaque, sizeof(*entry));
    if (!entry) return QJS_INTL_NO_MEMORY;
    entry->numbering = numbering;
    entry->calendar = calendar;
    entry->requested_cycle = requested_cycle;
    entry->next = p->date_probe_success;
    p->date_probe_success = entry;
    p->date_probe_success_count++;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_native_provider_locale_row(const QJSIntlProvider *p,
    QJSIntlBytes locale, uint32_t *out)
{
    QJSIntlDataSection rows;
    QJSIntlDataSlice tag;
    uint32_t lo = 0, hi;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = QJS_INTL_DATA_INDEX_NONE;
    if (!p || !locale.data || !locale.length) return QJS_INTL_INVALID_ARGUMENT;
    if (same(locale, "root")) return QJS_INTL_UNSUPPORTED;
    if (validate_compiled_view(p, &p->view) ||
        qjs_intl_data_section(&p->view, QJS_INTL_DATA_LOCALE, &rows)) return QJS_INTL_DATA_ERROR;
    hi = rows.record_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        size_t common;
        int order;
        if (qjs_intl_data_record_string(&p->view, &rows, mid, 0, &tag)) return QJS_INTL_DATA_ERROR;
        common = locale.length < tag.length ? locale.length : tag.length;
        order = memcmp(locale.data, tag.data, common);
        if (!order) order = locale.length < tag.length ? -1 : locale.length > tag.length;
        if (order > 0) lo = mid + 1; else hi = mid;
    }
    if (lo == rows.record_count) return QJS_INTL_UNSUPPORTED;
    if (qjs_intl_data_record_string(&p->view, &rows, lo, 0, &tag)) return QJS_INTL_DATA_ERROR;
    if (locale.length != tag.length || memcmp(locale.data, tag.data, locale.length))
        return QJS_INTL_UNSUPPORTED;
    *out = lo;
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_provider_default_numbering(const QJSIntlProvider *p,
    uint32_t row, QJSIntlBytes *out)
{
    QJSIntlDataSection rows;
    QJSIntlDataSlice value;
    uint32_t steps, parent;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = (QJSIntlBytes){NULL, 0};
    if (!p) return QJS_INTL_INVALID_ARGUMENT;
    if (validate_compiled_view(p, &p->view) ||
        qjs_intl_data_section(&p->view, QJS_INTL_DATA_LOCALE, &rows)) return QJS_INTL_DATA_ERROR;
    if (row >= rows.record_count) return QJS_INTL_INVALID_ARGUMENT;
    for (steps = 0; steps < rows.record_count; steps++) {
        if (qjs_intl_data_record_string(&p->view, &rows, row, 36, &value)) return QJS_INTL_DATA_ERROR;
        if (value.length) {
            *out = (QJSIntlBytes){(const char *)value.data, value.length};
            return QJS_INTL_OK;
        }
        if (qjs_intl_data_record_u32(&rows, row, 8, &parent)) return QJS_INTL_DATA_ERROR;
        if (parent == QJS_INTL_DATA_INDEX_NONE) return QJS_INTL_UNSUPPORTED;
        row = parent;
    }
    return QJS_INTL_DATA_ERROR;
}
static int locale_service_is_valid(const QJSIntlProvider *p,
    uint32_t row, QJSIntlService service)
{
    unsigned int bits;
    QJSIntlDataSection locales;
    if (!p || row >= 256 || (unsigned int)service >= QJS_INTL_SERVICE_COUNT ||
        validate_compiled_view(p, &p->view) ||
        qjs_intl_data_section(&p->view, QJS_INTL_DATA_LOCALE, &locales) ||
        row >= locales.record_count) return 0;
#ifdef _WIN32
    bits = (unsigned int)InterlockedCompareExchange(&compiled_locale_service_valid[row], 0, 0);
#else
    bits = atomic_load_explicit(&compiled_locale_service_valid[row], memory_order_acquire);
#endif
    return (bits & (1u << (unsigned int)service)) != 0;
}
static void locale_service_mark_valid(QJSIntlProvider *p,
    uint32_t row, QJSIntlService service)
{
    unsigned int bit;
    QJSIntlDataSection locales;
    if (!p || row >= 256 || (unsigned int)service >= QJS_INTL_SERVICE_COUNT ||
        qjs_intl_native_provider_validate_compiled_data(p, &p->view) ||
        qjs_intl_data_section(&p->view, QJS_INTL_DATA_LOCALE, &locales) ||
        row >= locales.record_count) return;
    bit = 1u << (unsigned int)service;
#ifdef _WIN32
    {
        LONG before, after;
        do {
            before = InterlockedCompareExchange(&compiled_locale_service_valid[row], 0, 0);
            after = (LONG)((unsigned int)before | bit);
        } while (InterlockedCompareExchange(&compiled_locale_service_valid[row], after, before) != before);
    }
#else
    atomic_fetch_or_explicit(&compiled_locale_service_valid[row], bit, memory_order_release);
#endif
}

static QJSIntlStatus available_compiled_locales(QJSIntlProvider *p,
    QJSIntlService service, LocaleProbe probe, QJSIntlTagList *out)
{
    const QJSIntlAllocator *allocator;
    const QJSIntlDataView *view;
    QJSIntlDataSection locales;
    QJSIntlDataSlice tag;
    QJSIntlStatus status;
    uint32_t row;
    size_t pass, count = 0;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!p) return QJS_INTL_INVALID_ARGUMENT;
    allocator = qjs_intl_native_provider_allocator(p);
    view = qjs_intl_native_provider_view(p);
    if (qjs_intl_native_provider_validate_compiled_data(p, view) ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales)) return QJS_INTL_DATA_ERROR;
    /* Probe first, then allocate only the owned result. Warm passes reuse
       exact positive row proofs. Rows beyond the bounded cache are rechecked. */
    for (pass = 0; pass < 2; pass++) {
        for (row = 0; row < locales.record_count; row++) {
            QJSIntlBytes locale;
            char *copy;
            if (qjs_intl_data_record_string(view, &locales, row, 0, &tag)) {
                status = QJS_INTL_DATA_ERROR; goto fail;
            }
            locale = (QJSIntlBytes){(const char *)tag.data, tag.length};
            if (same(locale, "root")) continue;
            if (locale_service_is_valid(p, row, service)) {
                status = QJS_INTL_OK;
            } else {
                status = probe(p, locale, row);
                if (status == QJS_INTL_OK) locale_service_mark_valid(p, row, service);
            }
            if (status == QJS_INTL_UNSUPPORTED) continue;
            if (status != QJS_INTL_OK) goto fail;
            if (!pass) { count++; continue; }
            copy = allocator->malloc(allocator->opaque, tag.length + 1);
            if (!copy) { status = QJS_INTL_NO_MEMORY; goto fail; }
            memcpy(copy, tag.data, tag.length); copy[tag.length] = 0;
            out->items[out->count++] = (QJSIntlBytes){copy, tag.length};
        }
        if (!pass) {
            if (!count) return QJS_INTL_UNSUPPORTED;
            if (count > SIZE_MAX / sizeof(*out->items)) return QJS_INTL_OVERFLOW;
            out->items = allocator->malloc(allocator->opaque, count * sizeof(*out->items));
            if (!out->items) return QJS_INTL_NO_MEMORY;
        }
    }
    return QJS_INTL_OK;
 fail:
    qjs_intl_tag_list_clear(p, out);
    return status;
}
