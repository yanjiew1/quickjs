/* Native Intl provider. Copyright (c) 2026 Yan-Jie Wang.
 * Compile this source only for CONFIG_INTL_NATIVE. ICU is selected separately.
 * Service AvailableLocales are the installed en/en-US rows. The selected
 * released bundle supplies each registered service and Locale inventories. */
#ifdef CONFIG_ICU
#error "Native Intl provider data requires CONFIG_ICU to be disabled"
#endif
#include "provider-native.h"
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

struct QJSIntlProvider {
    QJSIntlAllocator allocator;
    QJSIntlDataView view;
    uint32_t list_locale;
    char *default_locale;
    /* qjs_tz_resolve returns a string inside immutable compiled data. */
    const char *default_time_zone;
    QJSIntlLocaleInfoCapabilities capabilities;
    QJSIntlDataVersions versions;
};

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
    if (qjs_intl_data_open(qjs_intl_locale_metadata_blob,
                          qjs_intl_locale_metadata_blob_size, &p->view) !=
            QJS_INTL_DATA_OK) {
        status = QJS_INTL_DATA_ERROR;
        goto fail;
    }
    status = require_metadata(&p->view);
    if (status != QJS_INTL_OK) goto fail;
    status = qjs_intl_native_locale_canonicalize(&p->allocator, &p->view,
        config->default_locale.data ? config->default_locale : bytes("en-US"),
        &canonical);
    if (status != QJS_INTL_OK) goto fail;
    /* Installed default locale policy. Unknown host locales are never
     * reported as discovered/supported; ICU retains its host-default policy.
     * Restriction guarantees DefaultLocale and its fallback en both exist. */
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
    if (!p) return;
    a = p->allocator;
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
QJSIntlStatus qjs_intl_locale_available(QJSIntlProvider *p, QJSIntlService service,
                                       QJSIntlTagList *out)
{
    size_t i;
    static const char *const tags[] = { "en", "en-US" };
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!p || (unsigned int)service >= QJS_INTL_SERVICE_COUNT)
        return QJS_INTL_INVALID_ARGUMENT;
    if (service == QJS_INTL_DATE_TIME_FORMAT)
        return qjs_intl_native_provider_date_available(p, out);
    if (service == QJS_INTL_NUMBER_FORMAT) {
        QJSIntlStatus status = qjs_intl_native_provider_number_available(p);
        if (status != QJS_INTL_OK) return status;
    } else if (service == QJS_INTL_COLLATOR || service == QJS_INTL_COLLATOR_SEARCH) {
        QJSIntlStatus status = qjs_intl_native_provider_collator_available(p);
        if (status != QJS_INTL_OK) return status;
    } else if (service == QJS_INTL_SEGMENTER) {
        QJSIntlStatus status = qjs_intl_native_provider_segmenter_available(p);
        if (status != QJS_INTL_OK) return status;
    } else if (service == QJS_INTL_DURATION_FORMAT) {
        QJSIntlStatus status = qjs_intl_native_provider_duration_available(p);
        if (status != QJS_INTL_OK) return status;
    } else if (service == QJS_INTL_PLURAL_RULES) {
        QJSIntlStatus status = qjs_intl_native_provider_plural_available(p);
        if (status != QJS_INTL_OK) return status;
    } else if (service == QJS_INTL_RELATIVE_TIME_FORMAT) {
        QJSIntlStatus status = qjs_intl_native_provider_relative_available(p);
        if (status != QJS_INTL_OK) return status;
    } else if (service == QJS_INTL_DISPLAY_NAMES) {
        QJSIntlStatus status = qjs_intl_native_provider_display_available(p);
        if (status != QJS_INTL_OK) return status;
    } else if (service != QJS_INTL_LIST_FORMAT) return QJS_INTL_UNSUPPORTED;
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
    if (!same(locale, "en") && !same(locale, "en-US"))
        return QJS_INTL_UNSUPPORTED;
    return qjs_intl_native_list_open_data(&p->allocator, &p->view,
                                         p->list_locale, type, style, out);
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
    *count = p ? 2 : 0;
    *size = p ? sizeof(*p) + strlen(p->default_locale) + 1 : 0;
}

const QJSIntlDataView *qjs_intl_native_provider_view(const QJSIntlProvider *p)
{
    return p ? &p->view : NULL;
}
