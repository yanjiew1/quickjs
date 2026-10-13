/* Native Locale methods use the installed service registry, not unactivated
 * section26 metadata coverage flags. ECMA402 7ae78cf, review 2026-10-09. */
#include "provider-native-general.h"
#include "../timezone/timezone.h"
#include <stdlib.h>
#include <string.h>
static QJSIntlBytes text(const char *s)
{
    return (QJSIntlBytes){s, s ? strlen(s) : 0};
}
static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length || !memcmp(a.data, b.data, a.length));
}
static QJSIntlStatus append(const QJSIntlAllocator *a, QJSIntlTagList *out, QJSIntlBytes v)
{
    QJSIntlBytes *items;
    char *copy;
    size_t i;
    if (!v.data || v.length == SIZE_MAX) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < out->count; i++) if (equal(out->items[i], v)) return QJS_INTL_OK;
    if (out->count >= SIZE_MAX / sizeof(*items)) return QJS_INTL_OVERFLOW;
    copy = a->malloc(a->opaque, v.length + 1);
    if (!copy) return QJS_INTL_NO_MEMORY;
    memcpy(copy, v.data, v.length); copy[v.length] = 0;
    items = a->realloc(a->opaque, out->items, (out->count + 1) * sizeof(*items));
    if (!items) { a->free(a->opaque, copy); return QJS_INTL_NO_MEMORY; }
    out->items = items;
    out->items[out->count++] = (QJSIntlBytes){copy, v.length};
    return QJS_INTL_OK;
}
static int compare(const void *a, const void *b)
{
    const QJSIntlBytes *x = a, *y = b;
    size_t n = x->length < y->length ? x->length : y->length;
    int d = n ? memcmp(x->data, y->data, n) : 0;
    return d ? d : x->length < y->length ? -1 : x->length > y->length;
}
static void remove_extensions(char *tag)
{
    char *part = strchr(tag, '-');
    while (part) {
        char *next = strchr(part + 1, '-');
        size_t n = next ? (size_t)(next - part - 1) : strlen(part + 1);
        if (n == 1) { *part = 0; return; }
        part = next;
    }
}
static QJSIntlStatus service_values(const QJSIntlAllocator *a,
    const QJSIntlDataView *view, QJSIntlProvider *provider,
    const QJSIntlLocaleInfoRequest *request, int number, QJSIntlLocaleInfoResult *out)
{
    QJSIntlTagList available = {0}, values = {0};
    QJSIntlService service = number ? QJS_INTL_NUMBER_FORMAT : QJS_INTL_COLLATOR;
    QJSIntlStatus status;
    char *canonical = NULL;
    size_t i;
    status = qjs_intl_native_locale_canonicalize(a, view, request->locale, &canonical);
    if (status != QJS_INTL_OK) goto done;
    remove_extensions(canonical);
    status = qjs_intl_locale_available(provider, service, &available);
    if (status != QJS_INTL_OK) goto done;
    /* Exact LookupMatchingLocaleByPrefix over actual installed availability.
     * No DefaultLocale or identity-only metadata row is appended here. */
    while (*canonical) {
        char *dash;
        for (i = 0; i < available.count; i++)
            if (equal(text(canonical), available.items[i])) break;
        if (i < available.count) {
            status = qjs_intl_locale_key_values(provider, service, available.items[i],
                text(number ? "nu" : "co"), &values);
            if (status != QJS_INTL_OK) goto done;
            if (!values.count || !values.items) { status = QJS_INTL_DATA_ERROR; goto done; }
            if (number) {
                if (!values.items[0].data || !values.items[0].length) { status = QJS_INTL_DATA_ERROR; goto done; }
                status = append(a, &out->list, values.items[0]);
            } else {
                for (i = 0; i < values.count; i++) {
                    QJSIntlBytes v = values.items[i];
                    if (!v.data) {
                        if (i || v.length) { status = QJS_INTL_DATA_ERROR; goto done; }
                        continue;
                    }
                    if (equal(v, text("standard")) || equal(v, text("search"))) continue;
                    status = append(a, &out->list, v);
                    if (status != QJS_INTL_OK) goto done;
                }
                if (out->list.count > 1) qsort(out->list.items, out->list.count, sizeof(*out->list.items), compare);
            }
            goto done;
        }
        dash = strrchr(canonical, '-');
        if (!dash) { *canonical = 0; break; }
        *dash = 0;
    }
    /* These are the normative no-match Locale information results, never
     * advertised as installed Collator/NumberFormat capabilities. */
    status = append(a, &out->list, text(number ? "latn" : "emoji"));
    if (!number && status == QJS_INTL_OK) status = append(a, &out->list, text("eor"));
done:
    a->free(a->opaque, canonical);
    qjs_intl_tag_list_clear(provider, &values);
    qjs_intl_tag_list_clear(provider, &available);
    if (status == QJS_INTL_OK) out->defined = 1;
    return status;
}
QJSIntlStatus qjs_intl_native_provider_locale_info_get(const QJSIntlAllocator *a,
    const QJSIntlDataView *view, QJSIntlProvider *provider,
    const QJSIntlLocaleInfoRequest *request, QJSIntlLocaleInfoField field,
    QJSIntlLocaleInfoResult *out)
{
    QJSIntlLocaleInfoCapabilities cap = {0};
    QJSIntlDataSection inventory;
    QJSIntlLocaleInfoResult result = {0};
    QJSIntlStatus status;
    size_t i;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!a || !a->malloc || !a->realloc || !a->free || !view || !provider || !request)
        return QJS_INTL_INVALID_ARGUMENT;
    if ((field == QJS_INTL_LOCALE_COLLATIONS && !request->collation.data) ||
        (field == QJS_INTL_LOCALE_NUMBERING_SYSTEMS && !request->numbering_system.data)) {
        status = service_values(a, view, provider, request,
            field == QJS_INTL_LOCALE_NUMBERING_SYSTEMS, &result);
    } else {
        /* The installed-calendar inventory is supplied by the calendar
         * implementation owner. Identity/BCP47 metadata cannot enable it. */
        if (field == QJS_INTL_LOCALE_CALENDARS && !request->calendar.data &&
            qjs_intl_data_section(view, QJS_INTL_DATA_AVAILABLE_CALENDAR, &inventory) == QJS_INTL_DATA_OK &&
            inventory.record_count) cap.calendars_ready = 1;
        /* Territorial IDs in20 come from the final TZ owner's metadata.
         * Every result is checked against the sole native TZ inventory. */
        if (field == QJS_INTL_LOCALE_TIME_ZONES &&
            qjs_intl_data_section(view, QJS_INTL_DATA_REGION_TIME_ZONE, &inventory) == QJS_INTL_DATA_OK &&
            inventory.record_count)
            cap.time_zones_ready = 1;
        status = qjs_intl_native_locale_info_get(a, view, &cap, request, field, &result);
        if (status == QJS_INTL_OK && field == QJS_INTL_LOCALE_TIME_ZONES) {
            for (i = 0; i < result.list.count; i++) {
                const char *identifier, *primary;
                QJSIntlBytes v = result.list.items[i];
                if (!v.data || qjs_tz_resolve(v.data, v.length, &identifier, &primary) != QJS_TZ_OK ||
                    !primary || strlen(primary) != v.length || memcmp(primary, v.data, v.length)) {
                    status = QJS_INTL_DATA_ERROR; break;
                }
            }
        }
    }
    if (status == QJS_INTL_OK) *out = result;
    else qjs_intl_native_locale_info_result_clear(a, &result);
    return status;
}
