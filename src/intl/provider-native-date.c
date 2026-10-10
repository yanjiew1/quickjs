/* Native DateTimeFormat admitted development profile. Copyright 2026 Yan-Jie Wang.
 * ICU branches and binary service_coverage remain owned by their existing paths.
 * Complete localized calendar data is admitted by the data owner's mask.
 * The accepted shared arithmetic provider is checked before each calendar
 * is admitted. AvailableLocales stays en/en-US. */
#include "provider-native-date.h"
#include "metadata-data.h"
#include <string.h>

static QJSIntlBytes bytes(const char *s)
{
    QJSIntlBytes b = { s, s ? strlen(s) : 0 }; return b;
}
static int equal(QJSIntlBytes a, QJSIntlBytes b)
{
    return a.length == b.length && (!a.length ||
        (a.data && b.data && !memcmp(a.data, b.data, a.length)));
}
static QJSIntlStatus text(const QJSIntlDataView *v,
    const QJSIntlDataSection *s, uint32_t row, uint32_t offset, QJSIntlBytes *out)
{
    QJSIntlDataSlice b;
    if (qjs_intl_data_record_string(v, s, row, offset, &b)) return QJS_INTL_DATA_ERROR;
    out->data = (const char *)b.data; out->length = b.length;
    return QJS_INTL_OK;
}
static QJSIntlStatus locale_row(QJSIntlProvider *p, QJSIntlBytes locale,
                                uint32_t *out)
{
    const QJSIntlDataView *v = qjs_intl_native_provider_view(p);
    QJSIntlDataSection s;
    uint32_t row; QJSIntlBytes tag;
    if (!v || !out) return QJS_INTL_INVALID_ARGUMENT;
    if (!equal(locale, bytes("en")) && !equal(locale, bytes("en-US")))
        return QJS_INTL_UNSUPPORTED;
    /* Exact en/en-US profile uses the acquired inherited en date rows.
     * This choice does not confer availability on unrelated identity rows. */
    if (qjs_intl_data_section(v, QJS_INTL_DATA_LOCALE, &s)) return QJS_INTL_DATA_ERROR;
    for (row = 0; row < s.record_count; row++) {
        if (text(v, &s, row, 0, &tag)) return QJS_INTL_DATA_ERROR;
        if (equal(tag, bytes("en"))) { *out = row; return QJS_INTL_OK; }
    }
    return QJS_INTL_UNSUPPORTED;
}
static QJSIntlStatus numbering_row(QJSIntlProvider *p, QJSIntlBytes value,
                                   uint32_t *out)
{
    const QJSIntlDataView *v = qjs_intl_native_provider_view(p);
    QJSIntlDataSection s; QJSIntlBytes tag; QJSIntlDataSlice record; uint32_t row;
    if (!v || !out) return QJS_INTL_INVALID_ARGUMENT;
    if (qjs_intl_data_section(v, QJS_INTL_DATA_NUMBERING, &s)) return QJS_INTL_DATA_ERROR;
    for (row = 0; row < s.record_count; row++) {
        if (text(v, &s, row, 0, &tag)) return QJS_INTL_DATA_ERROR;
        if (!equal(value, tag)) continue;
        if (qjs_intl_data_record(&s, row, &record)) return QJS_INTL_DATA_ERROR;
        if (record.data[8] != 10 || record.data[9]) return QJS_INTL_UNSUPPORTED;
        *out = row; return QJS_INTL_OK;
    }
    return QJS_INTL_UNSUPPORTED;
}
static QJSIntlStatus append(QJSIntlProvider *p, QJSIntlTagList *out, QJSIntlBytes value)
{
    const QJSIntlAllocator *a = qjs_intl_native_provider_allocator(p);
    QJSIntlBytes *items; char *copy; size_t i;
    for (i = 0; i < out->count; i++) if (equal(out->items[i], value)) return QJS_INTL_OK;
    if (out->count >= SIZE_MAX / sizeof(*items) || value.length == SIZE_MAX)
        return QJS_INTL_OVERFLOW;
    copy = a->malloc(a->opaque, value.length + 1);
    if (!copy) return QJS_INTL_NO_MEMORY;
    if (value.length) memcpy(copy, value.data, value.length);
    copy[value.length] = 0;
    items = a->realloc(a->opaque, out->items, (out->count + 1) * sizeof(*items));
    if (!items) { a->free(a->opaque, copy); return QJS_INTL_NO_MEMORY; }
    out->items = items; out->items[out->count++] = bytes(copy); return QJS_INTL_OK;
}
static QJSIntlStatus default_numbering(QJSIntlProvider *p, uint32_t row, QJSIntlBytes *out)
{
    const QJSIntlDataView *v = qjs_intl_native_provider_view(p);
    QJSIntlDataSection s; uint32_t parent, steps;
    if (qjs_intl_data_section(v, QJS_INTL_DATA_LOCALE, &s)) return QJS_INTL_DATA_ERROR;
    for (steps = 0; steps < s.record_count; steps++) {
        if (text(v, &s, row, 36, out)) return QJS_INTL_DATA_ERROR;
        if (out->length) return QJS_INTL_OK;
        if (qjs_intl_data_record_u32(&s, row, 8, &parent)) return QJS_INTL_DATA_ERROR;
        if (parent == QJS_INTL_DATA_INDEX_NONE) return QJS_INTL_UNSUPPORTED;
        row = parent;
    }
    return QJS_INTL_DATA_ERROR;
}
QJSIntlStatus qjs_intl_native_provider_date_preferred_hour_cycle(
    QJSIntlProvider *p, QJSIntlBytes locale, unsigned int *out)
{
    const QJSIntlDataView *v = qjs_intl_native_provider_view(p);
    QJSIntlDataSection s; QJSIntlBytes key, raw; uint32_t row, scope, ignored;
    QJSIntlStatus status = locale_row(p, locale, &ignored);
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    if (status) return status;
    if (qjs_intl_data_section(v, QJS_INTL_DATA_HOUR_DEFAULT, &s)) return QJS_INTL_UNSUPPORTED;
    /* The admitted en/en-US profile uses CLDR US. Read its preferred raw
     * symbol explicitly; allowed-cycle ordering is not the preferred cycle. */
    for (row = 0; row < s.record_count; row++) {
        if (text(v, &s, row, 0, &key) ||
            qjs_intl_data_record_u32(&s, row, 8, &scope)) return QJS_INTL_DATA_ERROR;
        if (scope || !equal(key, bytes("US"))) continue;
        if (text(v, &s, row, 12, &raw) || !raw.length) return QJS_INTL_DATA_ERROR;
        switch (raw.data[0]) {
        case 'K': *out = QJS_DATE_H11; return QJS_INTL_OK;
        case 'h': *out = QJS_DATE_H12; return QJS_INTL_OK;
        case 'H': *out = QJS_DATE_H23; return QJS_INTL_OK;
        case 'k': *out = QJS_DATE_H24; return QJS_INTL_OK;
        default: return QJS_INTL_DATA_ERROR;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}
static int calendar_data_supported(void *opaque, QJSCalendarId id)
{
    QJSIntlNativeDateProviderEnvironment *state = opaque;
    return (unsigned int)id < QJS_CAL_COUNT && qjs_calendar_is_supported(id) &&
        (state->calendar_mask & (UINT32_C(1) << (unsigned int)id));
}
QJSIntlStatus qjs_intl_native_provider_date_open(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlBytes numbering, const QJSIntlDateBankOptions *o,
    QJSTzProvider *tz, QJSIntlNativeDateProviderEnvironment *state,
    QJSIntlNativeDateBank **out)
{
    QJSIntlDateEnvironment env; QJSIntlStatus status; uint32_t l, n; QJSCalendarId calendar;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!p || !o || !state) return QJS_INTL_INVALID_ARGUMENT;
    status = locale_row(p, locale, &l); if (status) return status;
    status = numbering_row(p, numbering, &n); if (status) return status;
    if (qjs_calendar_from_identifier(&calendar, o->requested.calendar.data,
        o->requested.calendar.length)) return QJS_INTL_UNSUPPORTED;
    memset(state, 0, sizeof(*state));
    status = qjs_intl_native_date_data_calendar_mask(qjs_intl_native_provider_view(p),
                                                   l, &state->calendar_mask);
    if (status) return status;
    if (!calendar_data_supported(state, calendar)) return QJS_INTL_UNSUPPORTED;
    state->bridge.upstream.opaque = tz;
    state->bridge.upstream.zone = qjs_intl_native_date_iana_zone;
    state->bridge.names_opaque = state;
    state->bridge.supports_data = calendar_data_supported;
    state->bridge.resolve_names = qjs_intl_native_date_calendar_names;
    status = qjs_intl_native_date_calendar_environment(&state->bridge, &env);
    if (status) return status;
    return qjs_intl_native_date_bank_open_data(qjs_intl_native_provider_allocator(p),
        qjs_intl_native_provider_view(p), l, n, o, &env, out);
}
static QJSIntlStatus probe_cycle(QJSIntlProvider *p, QJSIntlBytes locale,
                                 QJSIntlBytes nu, int requested_cycle, QJSIntlBytes calendar)
{
    QJSIntlDateBankOptions o; QJSIntlNativeDateBank *bank = NULL;
    QJSIntlNativeDateProviderEnvironment state;
    QJSIntlStatus status; unsigned int hc; size_t i;
    memset(&o, 0, sizeof(o)); o.requested.calendar = calendar;
    o.requested.time_zone = bytes("UTC");
    o.requested.date_style = o.requested.time_style = -1;
    status = qjs_intl_native_provider_date_preferred_hour_cycle(p, locale, &hc);
    if (status) return status;
    if (requested_cycle >= 0) hc = (unsigned int)requested_cycle;
    o.requested.hour_cycle = hc; o.requested.format_matcher = QJS_DATE_BEST_FIT;
    for (i = 0; i < QJS_DATE_FIELD_COUNT; i++) o.requested.fields[i] = -1;
    o.number_required = QJS_DATE_REQUIRE_ANY; o.number_defaults = QJS_DATE_DEFAULT_DATE;
    o.instant_defaults = QJS_DATE_DEFAULT_ALL;
    status = qjs_intl_native_provider_date_open(p, locale, nu, &o, NULL, &state, &bank);
    qjs_intl_native_date_bank_close(bank); return status;
}
static QJSIntlStatus probe(QJSIntlProvider *p, QJSIntlBytes locale, QJSIntlBytes nu)
{
    return probe_cycle(p, locale, nu, -1, bytes("gregory"));
}
QJSIntlStatus qjs_intl_native_provider_date_available(QJSIntlProvider *p, QJSIntlTagList *out)
{
    uint32_t row; QJSIntlBytes nu; QJSIntlStatus status;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!p) return QJS_INTL_INVALID_ARGUMENT;
    status = locale_row(p, bytes("en"), &row); if (status) return status;
    status = default_numbering(p, row, &nu); if (status) return status;
    status = probe(p, bytes("en"), nu); if (status) return status;
    status = append(p, out, bytes("en"));
    if (!status) status = append(p, out, bytes("en-US"));
    if (status) qjs_intl_tag_list_clear(p, out);
    return status;
}
QJSIntlStatus qjs_intl_native_provider_date_key_values(QJSIntlProvider *p,
    QJSIntlBytes locale, QJSIntlBytes key, QJSIntlTagList *out)
{
    const QJSIntlDataView *v; QJSIntlDataSection s;
    QJSIntlBytes nu, candidate; QJSIntlStatus status; uint32_t row, i; unsigned int hc;
    static const char *const cycles[] = { "h11", "h12", "h23", "h24" };
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!p) return QJS_INTL_INVALID_ARGUMENT;
    status = locale_row(p, locale, &row); if (status) return status;
    status = default_numbering(p, row, &nu); if (status) return status;
    status = probe(p, locale, nu); if (status) return status;
    if (equal(key, bytes("ca"))) {
        uint32_t mask;
        status = qjs_intl_native_date_data_calendar_mask(qjs_intl_native_provider_view(p), row, &mask);
        if (!status) status = append(p, out, bytes("gregory"));
        for (i = 0; !status && i < QJS_CAL_COUNT; i++) {
            QJSCalendarId id = (QJSCalendarId)i;
            if (!(mask & (UINT32_C(1) << i)) || !qjs_calendar_is_supported(id)) continue;
            candidate = bytes(qjs_calendar_identifier(id));
            status = probe_cycle(p, locale, nu, -1, candidate);
            if (status == QJS_INTL_UNSUPPORTED) { status = QJS_INTL_OK; continue; }
            if (!status) status = append(p, out, candidate);
        }
    } else if (equal(key, bytes("hc"))) {
        status = qjs_intl_native_provider_date_preferred_hour_cycle(p, locale, &hc);
        if (!status) status = append(p, out, bytes(cycles[hc]));
        for (i = 0; !status && i < 4; i++) {
            status = probe_cycle(p, locale, nu, (int)i, bytes("gregory"));
            if (status == QJS_INTL_UNSUPPORTED) { status = QJS_INTL_OK; continue; }
            if (!status) status = append(p, out, bytes(cycles[i]));
        }
    } else if (equal(key, bytes("nu"))) {
        status = append(p, out, nu); v = qjs_intl_native_provider_view(p);
        if (!status && qjs_intl_data_section(v, QJS_INTL_DATA_NUMBERING, &s)) status = QJS_INTL_DATA_ERROR;
        for (i = 0; !status && i < s.record_count; i++) {
            status = text(v, &s, i, 0, &candidate); if (status) break;
            status = probe(p, locale, candidate);
            if (status == QJS_INTL_UNSUPPORTED) { status = QJS_INTL_OK; continue; }
            if (!status) status = append(p, out, candidate);
        }
    } else status = QJS_INTL_UNSUPPORTED;
    if (status) qjs_intl_tag_list_clear(p, out);
    return status;
}
