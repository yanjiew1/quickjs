/* Collator's native development locale policy. No generated blob owner. */
#include "provider-native-collator.h"
#include <string.h>
static int equal(QJSIntlBytes b, const char *s)
{
    return b.data && b.length == strlen(s) && !memcmp(b.data, s, b.length);
}
static QJSIntlStatus data(QJSIntlProvider *p, QJSIntlBytes locale,
                          QJSIntlCollationData *out, uint32_t *index)
{
    const QJSIntlDataView *view = qjs_intl_native_provider_view(p);
    QJSIntlDataSection locales;
    QJSIntlDataSlice tag;
    QJSIntlStatus status;
    uint32_t i;
    if (!view) return QJS_INTL_INVALID_ARGUMENT;
    if (!equal(locale, "en") && !equal(locale, "en-US"))
        return QJS_INTL_UNSUPPORTED;
    status = qjs_intl_collation_data_init(view, out);
    if (status != QJS_INTL_OK) return status;
    if (qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales) !=
        QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < locales.record_count; i++) {
        if (qjs_intl_data_record_string(view, &locales, i, 0, &tag) !=
            QJS_INTL_DATA_OK) return QJS_INTL_DATA_ERROR;
        if (tag.length == locale.length && !memcmp(tag.data, locale.data, tag.length)) {
            *index = i;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}
QJSIntlStatus qjs_intl_native_provider_collator_defaults(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlCollatorUsage usage, QJSIntlCollatorOptions *out)
{
    QJSIntlCollationData d;
    uint32_t index = 0;
    QJSIntlStatus status;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    status = data(p, locale, &d, &index);
    return status == QJS_INTL_OK ?
        qjs_intl_native_collator_defaults(&d, index, usage, out) : status;
}
QJSIntlStatus qjs_intl_native_provider_collator_open(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlBytes collation,
    const QJSIntlCollatorOptions *options, QJSIntlNativeCollator **out)
{
    QJSIntlCollationData d;
    uint32_t index = 0;
    QJSIntlStatus status;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    status = data(p, locale, &d, &index);
    if (status != QJS_INTL_OK) return status;
    return qjs_intl_native_collator_open(qjs_intl_native_provider_allocator(p),
        &d, index, collation, options, NULL, out);
}
QJSIntlStatus qjs_intl_native_provider_collator_available(QJSIntlProvider *p)
{
    QJSIntlCollatorOptions options;
    QJSIntlNativeCollator *handle = NULL;
    QJSIntlStatus status;
    static const QJSIntlBytes locales[] = { { "en", 2 }, { "en-US", 5 } };
    QJSIntlBytes collation = { NULL, 0 };
    unsigned usage, locale;
    /* Both usage records must open real handles before en/en-US are exposed.
     * Both deliberately use the accepted shared-root comparator policy. */
    for (locale = 0; locale < 2; locale++) {
      for (usage = 0; usage < 2; usage++) {
        status = qjs_intl_native_provider_collator_defaults(p, locales[locale],
            (QJSIntlCollatorUsage)usage, &options);
        if (status != QJS_INTL_OK) return status;
        status = qjs_intl_native_provider_collator_open(p, locales[locale], collation,
                                                       &options, &handle);
        if (status != QJS_INTL_OK) return status;
        qjs_intl_native_collator_close(handle);
        handle = NULL;
      }
    }
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_provider_collator_key_values(QJSIntlProvider *p,
    QJSIntlService service, QJSIntlBytes locale, QJSIntlBytes key,
    QJSIntlTagList *out)
{
    static const char *const co[] = { NULL };
    static const char *const kn[] = { "false", "true" };
    static const char *const kf[] = { "false", "lower", "upper" };
    const char *const *values;
    const QJSIntlAllocator *a;
    QJSIntlCollatorOptions options;
    QJSIntlStatus status;
    size_t count, i;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!p || (service != QJS_INTL_COLLATOR && service != QJS_INTL_COLLATOR_SEARCH))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_native_provider_collator_defaults(p, locale,
        service == QJS_INTL_COLLATOR_SEARCH ? QJS_INTL_COLLATOR_USAGE_SEARCH :
                                            QJS_INTL_COLLATOR_SORT, &options);
    if (status != QJS_INTL_OK) return status;
    if (equal(key, "co")) { values = co; count = 1; }
    else if (equal(key, "kn")) { values = kn; count = 2; }
    else if (equal(key, "kf")) { values = kf; count = 3; }
    else return QJS_INTL_UNSUPPORTED;
    a = qjs_intl_native_provider_allocator(p);
    out->items = a->malloc(a->opaque, count * sizeof(*out->items));
    if (!out->items) return QJS_INTL_NO_MEMORY;
    memset(out->items, 0, count * sizeof(*out->items));
    for (i = 0; i < count; i++) {
        if (values[i]) {
            size_t n = strlen(values[i]);
            char *copy = a->malloc(a->opaque, n + 1);
            if (!copy) { qjs_intl_tag_list_clear(p, out); return QJS_INTL_NO_MEMORY; }
            memcpy(copy, values[i], n + 1);
            out->items[i] = (QJSIntlBytes){ copy, n };
        }
        out->count++;
    }
    return QJS_INTL_OK;
}
